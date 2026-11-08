import argparse
import contextlib
import hashlib
import io
import os
import subprocess
import sys
import tempfile
import threading
import time

from _version import __version__


def _debug(message):
	if os.environ.get('SOURCETRAIL_PYTHON_DAEMON_DEBUG') == '1':
		sys.stderr.write('[sourcetrail-daemon] ' + message + '\n')
		sys.stderr.flush()


def _daemon_enabled():
	value = os.environ.get('SOURCETRAIL_PYTHON_DAEMON', '1')
	return value not in ('0', 'false', 'False', '')


def _daemon_address(database_path):
	digest = hashlib.sha1(os.path.abspath(database_path).encode('utf-8')).hexdigest()[:16]
	if os.name == 'nt':
		address, family = '\\\\.\\pipe\\sourcetrail-py-' + digest, 'AF_PIPE'
	else:
		address, family = os.path.join(tempfile.gettempdir(), 'sourcetrail-py-' + digest + '.sock'), 'AF_UNIX'
	_debug('address ' + address)
	return address, family


def _start_daemon(database_path):
	kwargs = {
		'cwd': os.getcwd(),
		'stdin': subprocess.DEVNULL,
		'stdout': subprocess.DEVNULL,
		'stderr': subprocess.DEVNULL,
		'close_fds': True,
	}
	if os.name == 'nt':
		kwargs['creationflags'] = subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP
	else:
		kwargs['start_new_session'] = True

	subprocess.Popen(
		[sys.executable, '-X', 'utf8', os.path.abspath(__file__), '--serve', os.path.abspath(database_path)],
		**kwargs
	)


def _connect(database_path):
	from multiprocessing.connection import Client

	address, family = _daemon_address(database_path)
	for attempt in range(2):
		try:
			return Client(address, family=family)
		except Exception as error:
			_debug('connect attempt failed: ' + str(error))
			if attempt == 0:
				_start_daemon(database_path)
				_debug('daemon started')
				deadline = time.time() + 5.0
				retry_error_reported = False
				while time.time() < deadline:
					time.sleep(0.05)
					try:
						return Client(address, family=family)
					except Exception as retryError:
						if not retry_error_reported:
							_debug('connect retry failed: ' + str(retryError))
							retry_error_reported = True
	return None


def _request_index(args):
	database_path = os.path.abspath(args.database_file_path)
	source_path = os.path.abspath(args.source_file_path)
	environment_path = args.environment_path
	if environment_path is not None and not os.path.isabs(environment_path):
		environment_path = os.path.join(os.getcwd(), environment_path)

	connection = _connect(database_path)
	if connection is None:
		_debug('falling back to local indexing')
		return None
	_debug('connected to daemon')

	request = {
		'source_file_path': source_path,
		'database_file_path': database_path,
		'environment_path': environment_path,
		'working_directory': os.getcwd(),
		'verbose': bool(args.verbose),
		'shallow': bool(args.shallow),
	}
	try:
		connection.send(request)
		response = connection.recv()
	except Exception as error:
		print('ERROR: Python indexer daemon failed: ' + str(error))
		return 1
	finally:
		connection.close()

	_debug('daemon response code ' + str(response.get('code')))
	if response.get('stdout'):
		sys.stdout.write(response['stdout'])
	if response.get('stderr'):
		sys.stderr.write(response['stderr'])
	return int(response.get('code', 1))


def _process_request(request, indexer, shallow_indexer, srctrl):
	source_path = request.get('source_file_path')
	database_path = request.get('database_file_path')
	environment_path = request.get('environment_path')
	working_directory = request.get('working_directory') or os.getcwd()
	verbose = bool(request.get('verbose'))
	shallow = bool(request.get('shallow'))

	stdout = io.StringIO()
	stderr = io.StringIO()
	code = 0
	with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
		try:
			# Open and close per request: the C++ side merges and swaps the temporary
			# databases right after the commands, and a long-lived SQLite connection
			# from a daemon blocks that (Windows file locks). The imports and jedi
			# caches stay warm, which is where most of the startup saving comes from.
			if not srctrl.open(os.path.abspath(database_path)):
				raise RuntimeError(srctrl.getLastError())
			if not indexer.isSourcetrailDBVersionCompatible(True):
				raise RuntimeError('Sourcetrail database version is not compatible')
			srctrl.beginTransaction()
			if shallow:
				client = shallow_indexer.AstVisitorClient()
				shallow_indexer.indexSourceFile(source_path, environment_path, working_directory, client, verbose)
			else:
				client = indexer.AstVisitorClient()
				indexer.indexSourceFile(source_path, environment_path, working_directory, client, verbose)
			srctrl.commitTransaction()
		except Exception as error:
			code = 1
			stderr.write(str(error))
			if hasattr(srctrl, 'rollbackTransaction'):
				try:
					srctrl.rollbackTransaction()
				except Exception:
					pass
		finally:
			try:
				srctrl.close()
			except Exception:
				pass

	return {'code': code, 'stdout': stdout.getvalue(), 'stderr': stderr.getvalue()}


def _serve(database_path):
	from multiprocessing.connection import Listener

	import indexer
	import shallow_indexer
	import sourcetraildb as srctrl

	address, family = _daemon_address(database_path)
	if family == 'AF_UNIX' and os.path.exists(address):
		try:
			os.remove(address)
		except OSError:
			pass

	_debug('serving ' + address + ' for ' + database_path)
	try:
		listener = Listener(address, family=family)
	except Exception as error:
		# Another daemon already owns this database.
		_debug('listener failed: ' + str(error))
		return 0

	_debug('listening')

	try:
		idle_timeout = float(os.environ.get('SOURCETRAIL_PYTHON_DAEMON_IDLE', '60'))
	except ValueError:
		idle_timeout = 60.0

	last_activity = [time.time()]

	def watchdog():
		while True:
			time.sleep(1.0)
			if time.time() - last_activity[0] > idle_timeout:
				os._exit(0)

	threading.Thread(target=watchdog, daemon=True).start()

	while True:
		try:
			connection = listener.accept()
			_debug('accepted connection')
		except Exception:
			return 1

		try:
			request = connection.recv()
			_debug('request ' + str(request.get('source_file_path')))
			last_activity[0] = time.time()
			connection.send(_process_request(request, indexer, shallow_indexer, srctrl))
		except Exception as error:
			try:
				connection.send({'code': 1, 'stdout': '', 'stderr': str(error)})
			except Exception:
				pass
		finally:
			try:
				connection.close()
			except Exception:
				pass


def main():
	if len(sys.argv) >= 2 and sys.argv[1] == '--serve':
		if len(sys.argv) < 3:
			return 1
		return _serve(sys.argv[2])

	parser = argparse.ArgumentParser(description='Python source code indexer that generates a Sourcetrail compatible database.')
	parser.add_argument('--version', action='version', version='SourcetrailPythonIndexer {version}'.format(version=__version__))

	subparsers = parser.add_subparsers(title='commands', dest='command')

	indexCommandName = 'index'
	parserIndex = subparsers.add_parser(
		indexCommandName,
		help='Index a Python source file and store the indexed data to a Sourcetrail database file. Run "' + indexCommandName + ' -h" for more info on available arguments.'
	)
	parserIndex.add_argument('--source-file-path', help='path to the source file to index', type=str, required=True)
	parserIndex.add_argument('--database-file-path', help='path to the generated Sourcetrail database file', type=str, required=True)
	parserIndex.add_argument(
		'--environment-path',
		help='path to the Python executable or the directory that contains the Python environment that should be used to resolve dependencies within the indexed source '
			'code (if not specified the path to the currently used interpreter is used)',
		type=str,
		required=False
	)
	parserIndex.add_argument('--clear', help='clear the database before indexing', action='store_true', required=False)
	parserIndex.add_argument('--verbose', help='enable verbose console output', action='store_true', required=False)
	parserIndex.add_argument('--shallow', help='use a quick indexing mode that matches references by name and ignores most of the context', action='store_true', required=False)

	checkEnvironmentCommandName = 'check-environment'
	parserCheckEnvironment = subparsers.add_parser(
		checkEnvironmentCommandName,
		help='Check if the provided path specifies a valid Python environment. This command exits with code "0" if a valid Python environment has been provided, otherwise '
			'code "1" is returned. Run "' + checkEnvironmentCommandName + ' -h" for more info on available arguments.'
	)
	parserCheckEnvironment.add_argument(
		'--environment-path',
		help='path to the Python executable or the directory that contains the Python environment that should be checked for compatibility',
		type=str,
		required=True
	)

	args = parser.parse_args() # code exits here for "--version" and "--help"

	if args.command == indexCommandName:
		if _daemon_enabled() and not args.clear:
			code = _request_index(args)
			if code is not None:
				return code
		processIndexCommand(args)
	elif args.command == checkEnvironmentCommandName:
		processCheckEnvironmentCommand(args)
	else:
		os.write(2, b"Error: No command has been specified.") # write to stderr
		return 1
	return 0


def processIndexCommand(args):
	import indexer
	import sourcetraildb as srctrl

	workingDirectory = os.getcwd()

	if not indexer.isSourcetrailDBVersionCompatible(True):
		return

	databaseFilePath = os.path.abspath(args.database_file_path)
	sourceFilePath = os.path.abspath(args.source_file_path)

	environmentPath = args.environment_path
	if environmentPath is not None and not os.path.isabs(environmentPath):
		environmentPath = os.path.join(workingDirectory, environmentPath)

	if not srctrl.open(databaseFilePath):
		print('ERROR: ' + srctrl.getLastError())

	if args.clear:
		if args.verbose:
			print('INFO: Clearing database...')
		if not srctrl.clear():
			print('ERROR: ' + srctrl.getLastError())
		else:
			if args.verbose:
				print('INFO: Clearing done.')

	if args.verbose:
		if srctrl.isEmpty():
			print('INFO: Loaded database is empty.')
		else:
			print('INFO: Loaded database contains data.')

	srctrl.beginTransaction()
	indexSourceFile(sourceFilePath, environmentPath, workingDirectory, args.verbose, args.shallow)
	srctrl.commitTransaction()

	if not srctrl.close():
		print('ERROR: ' + srctrl.getLastError())


def processCheckEnvironmentCommand(args):
	import indexer

	workingDirectory = os.getcwd()

	environmentPath = args.environment_path
	if environmentPath is not None and not os.path.isabs(environmentPath):
		environmentPath = os.path.join(workingDirectory, environmentPath)

	message = indexer.isValidEnvironment(environmentPath)
	if not message:
		print('The provided path is a valid Python environment.')
	else:
		print('The provided path is not a valid Python environment: ' + message)


def indexSourceFile(sourceFilePath, environmentPath, workingDirectory, verbose, shallow):
	if shallow:
		import shallow_indexer

		astVisitorClient = shallow_indexer.AstVisitorClient()
		shallow_indexer.indexSourceFile(sourceFilePath, environmentPath, workingDirectory, astVisitorClient, verbose)
	else:
		import indexer

		astVisitorClient = indexer.AstVisitorClient()
		indexer.indexSourceFile(sourceFilePath, environmentPath, workingDirectory, astVisitorClient, verbose)


if __name__ == '__main__':
	sys.exit(main())
