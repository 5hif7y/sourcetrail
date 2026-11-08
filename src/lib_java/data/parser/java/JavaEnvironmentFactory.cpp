#include "JavaEnvironmentFactory.h"

#include "ApplicationSettings.h"
#include "JavaEnvironment.h"
#include "logging.h"
#include "utilityApp.h"

#include <algorithm>
#include <string>

namespace
{

template <typename Ret, typename... Args>
std::function<Ret(Args...)> loadFunctionFromLibrary(boost::dll::shared_library *sharedLibrary,
	const FilePath& libraryPath, const std::string& functionName, std::string *errorString)
{
	try {
		sharedLibrary->load(libraryPath.getPath());
	}
	catch (const boost::dll::fs::system_error &systemError) {
		*errorString = "Cannot load library '" + libraryPath.str() + "': " + systemError.what();
		return nullptr;
	}
	try {
		std::function<Ret(Args...)> functionId = sharedLibrary->get<Ret(Args...)>(functionName);
		return functionId;
	}
	catch (const boost::dll::fs::system_error &systemError) {
		*errorString = "Cannot get symbol '" + functionName + "' from library '" + libraryPath.str() + "': " + systemError.what();
		return nullptr;
	}
}

}

boost::dll::shared_library JavaEnvironmentFactory::s_jvmLibrary;
std::shared_ptr<JavaEnvironmentFactory> JavaEnvironmentFactory::s_instance;
std::string JavaEnvironmentFactory::s_classPath;

void JavaEnvironmentFactory::createInstance(const std::string &classPath, std::string *errorString)
{
	if (s_instance)
	{
		if (classPath == s_classPath)
		{
			return;
		}
		else
		{
			LOG_ERROR("java classpath cannot be changed!");
			// todo: implement destroying the old factory instance and create a new one.
			// may be not so easy... there can only be one java env per process (which can never be
			// destroyed) -.-
			return;
		}
	}

	std::function<jint(JavaVM**, void**, void*)> createInstanceFunction;
	const FilePath javaPath = ApplicationSettings::getInstance()->getJavaPath();

	if (javaPath.empty())
	{
		*errorString = "No Java Path provided in preferences.";
		return;
	}

	createInstanceFunction = loadFunctionFromLibrary<jint, JavaVM**, void**, void*>(
		&s_jvmLibrary, javaPath, "JNI_CreateJavaVM", errorString);

	if (!createInstanceFunction && !errorString->empty())
	{
		return;
	}

	s_classPath = classPath;

	JavaVM* jvm = nullptr;	  // Pointer to the JVM (Java Virtual Machine)
	JNIEnv* env = nullptr;	  // Pointer to native interface

	JavaVMInitArgs vm_args;									  // Initialization arguments
	std::vector<JavaVMOption> options;						  // JVM invocation options
	std::string classPathOption = "-Djava.class.path=" + classPath;
	options.push_back({ .optionString = const_cast<char*>(classPathOption.c_str()) });
	// Cap the heap explicitly. Without -Xmx the JVM grows until the OS refuses, and
	// with several threads resolving bindings concurrently it used to spend most of
	// its time in GC before failing with an uncaught OutOfMemoryError. The default
	// scales with physical RAM (25%, 256..2048 MB) so the same binary works on a
	// 2 GB embedded board and on a desktop; SOURCETRAIL_JAVA_HEAP overrides it.
	const int javaHeapMb = utility::getJavaHeapSizeMb();
	const int javaInitialHeapMb = std::max(1, std::min(64, javaHeapMb / 4));
	std::string javaInitialHeapOption = "-Xms" + std::to_string(javaInitialHeapMb) + "m";
	std::string javaMaxHeapOption = "-Xmx" + std::to_string(javaHeapMb) + "m";
	options.push_back({ .optionString = const_cast<char*>(javaInitialHeapOption.c_str()) });
	options.push_back({ .optionString = const_cast<char*>(javaMaxHeapOption.c_str()) });

	// Prevent JVM warning:
	// - A restricted method in java.lang.System has been called
	// - java.lang.System::load has been called by ... in an unnamed module ...
	// - Use --enable-native-access=ALL-UNNAMED to avoid a warning for callers in this module
	// - Restricted methods will be blocked in a future release unless native access is enabled
	// See https://docs.oracle.com/en/java/javase/24/core/restricted-methods.html for further details.
	options.push_back({ .optionString = const_cast<char*>("--enable-native-access=ALL-UNNAMED") });

	// options.push_back({ const_cast<char*>("-verbose:jni") });

	// Allow attaching a debugger (default Eclipse port):
	// options.push_back({ .optionString = const_cast<char*>("-agentlib:jdwp=transport=dt_socket,server=y,suspend=y,address=*:8000") });

	// use these options to enable profiling in VisualVM
	//options.push_back({ const_cast<char*>("-Dcom.sun.management.jmxremote") });
	//options.push_back({ const_cast<char*>("-Dcom.sun.management.jmxremote.port=9010") });
	//options.push_back({ const_cast<char*>("-Dcom.sun.management.jmxremote.local.only=false") });
	//options.push_back({ const_cast<char*>("-Dcom.sun.management.jmxremote.authenticate=false") });
	//options.push_back({ const_cast<char*>("-Dcom.sun.management.jmxremote.ssl=false") });

	vm_args.version = JNI_VERSION_1_8;
	vm_args.nOptions = static_cast<jint>(options.size());
	vm_args.options = options.data();
	vm_args.ignoreUnrecognized = JNI_FALSE; // invalid options make the JVM init fail

	jint rc = createInstanceFunction(&jvm, reinterpret_cast<void**>(&env), &vm_args);

	if (rc != JNI_OK)
	{
		if (rc == JNI_EVERSION)
		{
			*errorString = "JVM is outdated and doesn't meet requirements";
		}
		else if (rc == JNI_ENOMEM)
		{
			*errorString = "not enough memory for JVM";
		}
		else if (rc == JNI_EINVAL)
		{
			*errorString = "invalid argument for launching JVM";
		}
		else if (rc == JNI_EEXIST)
		{
			*errorString = "the process can only launch one JVM an not more";
		}
		else
		{
			*errorString = "could not create the JVM instance (error code " + std::to_string(rc) + ")";
		}
	}
	else
	{
		jvm->DetachCurrentThread();
		s_instance = std::shared_ptr<JavaEnvironmentFactory>(new JavaEnvironmentFactory(jvm));
	}
}

std::shared_ptr<JavaEnvironmentFactory> JavaEnvironmentFactory::getInstance()
{
	return s_instance;
}

JavaEnvironmentFactory::~JavaEnvironmentFactory()
{
	// Intentionally do NOT call m_jvm->DestroyJavaVM().
	//
	// DestroyJavaVM() blocks until every non-daemon Java thread has terminated.
	// After indexing a Java project the process got stuck here: sourcetrail_cli
	// never exited (it had to be killed) and Sourcetrail.exe hung on close. This
	// factory is the process-wide singleton described above ("there can only be
	// one java env per process which can never be destroyed"), so it is only
	// destroyed during static teardown, right before the OS reclaims the JVM.
	// Skipping the call costs nothing and removes the hang.
}

std::shared_ptr<JavaEnvironment> JavaEnvironmentFactory::createEnvironment()
{
	JNIEnv *env = aidkit::concurrent::access([jvm = m_jvm](auto &threadIdToEnvAndUserCount)
	{
		const std::thread::id currentThreadId = std::this_thread::get_id();

		auto it = threadIdToEnvAndUserCount.find(currentThreadId);
		if (it != threadIdToEnvAndUserCount.end())
		{
			return it->second.first;
		}
		else
		{
			JNIEnv *env = nullptr;
			jvm->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr);
			threadIdToEnvAndUserCount.insert(std::pair(currentThreadId, std::pair(env, 0)));

			return env;
		}
	}, m_threadIdToEnvAndUserCount);

	return std::shared_ptr<JavaEnvironment>(new JavaEnvironment(m_jvm, env));
}


JavaEnvironmentFactory::JavaEnvironmentFactory(JavaVM* jvm)
	: m_jvm(jvm)
{
}

void JavaEnvironmentFactory::registerEnvironment()
{
	aidkit::concurrent::access([](auto &threadIdToEnvAndUserCount)
	{
		const std::thread::id currentThreadId = std::this_thread::get_id();

		auto it = threadIdToEnvAndUserCount.find(currentThreadId);
		if (it != threadIdToEnvAndUserCount.end())
		{
			it->second.second++;
		}
		else
		{
			LOG_ERROR("something went horribly wrong while registering a java environment");
		}
	}, m_threadIdToEnvAndUserCount);
}

void JavaEnvironmentFactory::unregisterEnvironment()
{
	aidkit::concurrent::access([](auto &threadIdToEnvAndUserCount)
	{
		const std::thread::id currentThreadId = std::this_thread::get_id();

		auto it = threadIdToEnvAndUserCount.find(currentThreadId);
		if (it != threadIdToEnvAndUserCount.end())
		{
			it->second.second--;
			// Keep the thread attached. Detaching after every file forced a fresh
			// AttachCurrentThread (and a JVM safepoint) for the next one; on large
			// projects that dominated indexing. createEnvironment() reuses this
			// entry for the next file on the same thread.
		}
		else
		{
			LOG_ERROR("something went horribly wrong while unregistering a java environment");
		}
	}, m_threadIdToEnvAndUserCount);
}
