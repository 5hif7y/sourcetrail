package com.sourcetrail;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Hashtable;
import java.util.List;
import java.util.jar.JarFile;
import java.util.zip.ZipEntry;
import org.eclipse.jdt.core.JavaCore;
import org.eclipse.jdt.core.compiler.IProblem;
import org.eclipse.jdt.core.dom.AST;
import org.eclipse.jdt.core.dom.ASTParser;
import org.eclipse.jdt.core.dom.ASTVisitor;
import org.eclipse.jdt.core.dom.BlockComment;
import org.eclipse.jdt.core.dom.Comment;
import org.eclipse.jdt.core.dom.CompilationUnit;
import org.eclipse.jdt.core.dom.LineComment;
import org.eclipse.jdt.core.dom.PackageDeclaration;

public class JavaIndexer
{
	public static void processFile(
		int address,
		String filePath,
		String fileContent,
		String languageStandard,
		String classPath,
		int verbose)
	{
		processFile(
			new JavaIndexerAstVisitorClient(address),
			filePath,
			fileContent,
			languageStandard,
			classPath,
			verbose);
	}

	/**
	 * Keep only the source roots that contain the file being indexed.
	 *
	 * JDT walks every source root recursively on each package/module lookup, so the
	 * cost of indexing ONE file grows with the number of files in ALL roots. On Kafka
	 * (6177 files spread over 134 roots) that made indexing crawl: after five minutes
	 * the process was still burning CPU inside ClasspathDirectory.computeDirectoryList
	 * and never finished. Passing only the file's own root keeps resolution inside its
	 * module and reduces the per-file cost to that module's size.
	 *
	 * Falls back to the full list when no root matches, so a file outside the declared
	 * roots is still indexed with the previous behaviour.
	 */
	/**
	 * Keep only the source roots that contain the file being indexed.
	 *
	 * JDT walks every source root recursively on each package/module lookup, so the
	 * cost of indexing ONE file grows with the number of files in ALL roots. On Kafka
	 * (6177 files spread over 134 roots) that made indexing crawl: after five minutes
	 * the process was still burning CPU inside ClasspathDirectory.computeDirectoryList
	 * and never finished. Passing only the file's own root keeps resolution inside its
	 * module and reduces the per-file cost to that module's size.
	 *
	 * Falls back to the full list when no root matches, so a file outside the declared
	 * roots is still indexed with the previous behaviour.
	 */
	private static List<String> filterSourcesForFile(List<String> sources, Path file)
	{
		if (sources.size() <= 1)
		{
			return sources;
		}

		final String filePath = file.toAbsolutePath().normalize().toString().replace('\\', '/');
		List<String> matching = new ArrayList<>();
		for (String source: sources)
		{
			final String sourcePath = Paths.get(source).toAbsolutePath().normalize().toString().replace('\\', '/');
			if (filePath.equals(sourcePath) || filePath.startsWith(sourcePath + "/"))
			{
				matching.add(source);
			}
		}
		return matching.isEmpty() ? sources : matching;
	}

	public static void processFile(
		AstVisitorClient astVisitorClient,
		String filePath,
		String fileContent,
		String languageStandard,
		String classPath,
		int verbose)
	{
		try
		{
			astVisitorClient.logInfo("indexing source file: " + filePath);

			var parser = ASTParser.newParser(AST.getJLSLatest());
			parser.setResolveBindings(true);	  // solve "bindings" like the declaration of the type used in a var decl
			parser.setKind(ASTParser.K_COMPILATION_UNIT);	  // specify to parse the entire compilation unit
			parser.setBindingsRecovery(true);	  // also return bindings that are not resolved completely
			parser.setStatementsRecovery(true);

			{
				astVisitorClient.logInfo("using language standard " + languageStandard);

				Hashtable<String, String> options = JavaCore.getOptions();
				options.put(JavaCore.COMPILER_PB_ENABLE_PREVIEW_FEATURES, JavaCore.DISABLED);
				options.put(JavaCore.COMPILER_PB_REPORT_PREVIEW_FEATURES, JavaCore.IGNORE);
				options.put(JavaCore.COMPILER_SOURCE, languageStandard);
				options.put(JavaCore.COMPILER_CODEGEN_TARGET_PLATFORM, languageStandard);
				options.put(JavaCore.COMPILER_COMPLIANCE, languageStandard);
				parser.setCompilerOptions(options);
			}

			Path path = Paths.get(filePath);
			parser.setUnitName(path.getFileName().toString());

			List<String> classpath = new ArrayList<>();
			List<String> sources = new ArrayList<>();

			for (String classPathEntry: classPath.split("\\" + Utility.getClassPathSeparator()))
			{
				if (classPathEntry.endsWith(".jar"))
				{
					classpath.add(classPathEntry);
				}
				else if (classPathEntry.endsWith(".aar"))
				{
					File extractedJarFile = extractClassesJarFileFromAarFile(
						Paths.get(classPathEntry), astVisitorClient);
					if (extractedJarFile != null)
					{
						classpath.add(extractedJarFile.getAbsolutePath());
					}
				}
				else if (!classPathEntry.isEmpty())
				{
					sources.add(classPathEntry);
				}
			}

			sources = filterSourcesForFile(sources, path);

			parser.setEnvironment(classpath.toArray(new String[0]), sources.toArray(new String[0]), null, true);
			parser.setSource(fileContent.toCharArray());

			var compilationUnit = (CompilationUnit)parser.createAST(null);

			ASTVisitor visitor;
			if (verbose != 0)
				visitor = new VerboseContextAwareAstVisitor(astVisitorClient, path.toFile(), fileContent, compilationUnit);
			else
				visitor = new ContextAwareAstVisitor(astVisitorClient, path.toFile(), fileContent, compilationUnit);

			astVisitorClient.logInfo("starting AST traversal");

			compilationUnit.accept(visitor);

			for (IProblem problem: compilationUnit.getProblems())
			{
				if (problem.isError())
				{
					Range range = new Range(
						compilationUnit.getLineNumber(problem.getSourceStart()),
						compilationUnit.getColumnNumber(problem.getSourceStart() + 1),
						compilationUnit.getLineNumber(problem.getSourceEnd()),
						compilationUnit.getColumnNumber(problem.getSourceEnd()) + 1);

					astVisitorClient.recordError(problem.getMessage(), false, true, range);
				}
			}

			for (Object commentObject: compilationUnit.getCommentList())
			{
				if ((commentObject instanceof LineComment) || (commentObject instanceof BlockComment))
				{
					((Comment)commentObject).accept(visitor);
				}
			}
		}
		catch (Throwable e)
		{
			// Catch Throwable, not Exception: OutOfMemoryError and LinkageError are
			// Errors, and letting one escape leaves a pending JNI exception that
			// breaks every subsequent call on this thread.
			StringWriter sw = new StringWriter();
			PrintWriter pw = new PrintWriter(sw);
			e.printStackTrace(pw);
			astVisitorClient.logError(sw.toString());
		}
	}

	public static String getPackageName(String fileContent)
	{
		String packageName = "";

		ASTParser parser = ASTParser.newParser(AST.getJLSLatest());
		parser.setKind(ASTParser.K_COMPILATION_UNIT);	  // specify to parse the entire compilation unit
		parser.setSource(fileContent.toCharArray());
		CompilationUnit cu = (CompilationUnit)parser.createAST(null);
		PackageDeclaration packageDeclaration = cu.getPackage();
		if (packageDeclaration != null)
		{
			packageName = packageDeclaration.getName().getFullyQualifiedName();
		}
		return packageName;
	}

	public static void clearCaches()
	{
		Runtime.getRuntime().gc();
	}

	private static File extractClassesJarFileFromAarFile(
		Path aarFilePath, AstVisitorClient astVisitorClient) throws IOException
	{
		try (JarFile jarFile = new JarFile(aarFilePath.toString()))
		{
			ZipEntry classesJarEntry = jarFile.getEntry("classes.jar");
			if (classesJarEntry != null)
			{
				try (InputStream inputStream = jarFile.getInputStream(classesJarEntry))
				{
					File tempFile = File.createTempFile(
						"jar_file_from_" + Utility.getFilenameWithoutExtension(aarFilePath) + "_", ".jar");
					tempFile.deleteOnExit();

					byte[] buffer = new byte[8 * 1024];

					try (OutputStream output = new FileOutputStream(tempFile))
					{
						int bytesRead;
						while ((bytesRead = inputStream.read(buffer)) != -1)
						{
							output.write(buffer, 0, bytesRead);
						}
					}
					astVisitorClient.logInfo(
						"Extracted classes.jar file from \"" + aarFilePath.toString() + "\" to \"" +
						tempFile.getAbsolutePath() + "\". "
						+ "This file will be automatically deleted when the session ends.");

					return tempFile;
				}
			}
			else
			{
				astVisitorClient.logError(
					"Classpath entry \"" + aarFilePath +
					"\" is malformed. No internal \"classes.jar\" entry could be found.");
			}
			jarFile.close();

			return null;
		}
	}

	// the following methods are defined in the native c++ code

	static public native boolean getInterrupted(int address);

	static public native void logInfo(int address, String info);

	static public native void logWarning(int address, String warning);

	static public native void logError(int address, String error);

	static public native void recordSymbol(
		int address, String symbolName, int symbolType, int access, int definitionKind);

	static public native void recordSymbolWithLocation(
		int address,
		String symbolName,
		int symbolType,
		int beginLine,
		int beginColumn,
		int endLine,
		int endColumn,
		int access,
		int definitionKind);

	static public native void recordSymbolWithLocationAndScope(
		int address,
		String symbolName,
		int symbolType,
		int beginLine,
		int beginColumn,
		int endLine,
		int endColumn,
		int scopeBeginLine,
		int scopeBeginColumn,
		int scopeEndLine,
		int scopeEndColumn,
		int access,
		int definitionKind);

	static public native void recordSymbolWithLocationAndScopeAndSignature(
		int address,
		String symbolName,
		int symbolType,
		int beginLine,
		int beginColumn,
		int endLine,
		int endColumn,
		int scopeBeginLine,
		int scopeBeginColumn,
		int scopeEndLine,
		int scopeEndColumn,
		int signatureBeginLine,
		int signatureBeginColumn,
		int signatureEndLine,
		int signatureEndColumn,
		int access,
		int definitionKind);

	static public native void recordReference(
		int address,
		int referenceKind,
		String referencedName,
		String contextName,
		int beginLine,
		int beginColumn,
		int endLine,
		int endColumn);

	static public native void recordQualifierLocation(
		int address, String qualifierName, int beginLine, int beginColumn, int endLine, int endColumn);

	static public native void recordLocalSymbol(
		int address, String symbolName, int beginLine, int beginColumn, int endLine, int endColumn);

	static public native void recordComment(
		int address, int beginLine, int beginColumn, int endLine, int endColumn);

	static public native void recordError(
		int address,
		String message,
		int fatal,
		int indexed,
		int beginLine,
		int beginColumn,
		int endLine,
		int endColumn);
}