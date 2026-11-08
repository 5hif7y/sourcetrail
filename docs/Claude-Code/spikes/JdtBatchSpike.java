import org.eclipse.jdt.core.JavaCore;
import org.eclipse.jdt.core.compiler.IProblem;
import org.eclipse.jdt.core.dom.AST;
import org.eclipse.jdt.core.dom.ASTParser;
import org.eclipse.jdt.core.dom.ASTVisitor;
import org.eclipse.jdt.core.dom.CompilationUnit;
import org.eclipse.jdt.core.dom.FileASTRequestor;
import org.eclipse.jdt.core.dom.SimpleName;

import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Hashtable;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;
import java.util.stream.Stream;

/**
 * Compares the production path (a fresh ASTParser per file, createAST) with JDT's
 * batch API (one ASTParser and one setEnvironment for N files, createASTs).
 *
 * Usage: java JdtBatchSpike <source-root> [limit] [batch-size]
 */
public class JdtBatchSpike
{
	static String s_sourceRoot;
	static Hashtable<String, String> s_options;

	public static void main(String[] args) throws Exception
	{
		final Path root = Paths.get(args[0]).toAbsolutePath();
		final int limit = args.length > 1 ? Integer.parseInt(args[1]) : 200;
		final int batchSize = args.length > 2 ? Integer.parseInt(args[2]) : 50;

		s_sourceRoot = root.toString();
		s_options = new Hashtable<>();
		s_options.put(JavaCore.COMPILER_SOURCE, "21");
		s_options.put(JavaCore.COMPILER_COMPLIANCE, "21");
		s_options.put(JavaCore.COMPILER_CODEGEN_TARGET_PLATFORM, "21");

		final List<Path> files;
		try (Stream<Path> stream = Files.walk(root))
		{
			files = stream.filter(p -> p.toString().endsWith(".java"))
							.sorted()
							.limit(limit)
							.collect(Collectors.toList());
		}
		System.out.println("files: " + files.size() + ", sourceRoot: " + s_sourceRoot + ", batch: " + batchSize);

		// Warm up JIT / JDT classes before measuring.
		parsePerFile(files.subList(0, Math.min(3, files.size())));
		parseBatch(files.subList(0, Math.min(3, files.size())), batchSize);

		final long t1 = System.nanoTime();
		final Stats perFile = parsePerFile(files);
		final long t2 = System.nanoTime();
		final Stats batch = parseBatch(files, batchSize);
		final long t3 = System.nanoTime();

		final double perMs = (t2 - t1) / 1_000_000.0;
		final double batchMs = (t3 - t2) / 1_000_000.0;
		System.out.printf("per-file: %8.0f ms total, %6.1f ms/file, bindings=%d, problems=%d%n",
						  perMs, perMs / files.size(), perFile.bindings, perFile.problems);
		System.out.printf("batch:    %8.0f ms total, %6.1f ms/file, bindings=%d, problems=%d%n",
						  batchMs, batchMs / files.size(), batch.bindings, batch.problems);
		System.out.printf("speedup:  %.2fx%n", perMs / batchMs);
		System.out.println("binding delta (batch - per-file): " + (batch.bindings - perFile.bindings));
	}

	static final class Stats
	{
		long bindings;
		long problems;
	}

	static void configure(ASTParser parser)
	{
		parser.setResolveBindings(true);
		parser.setBindingsRecovery(true);
		parser.setStatementsRecovery(true);
		parser.setKind(ASTParser.K_COMPILATION_UNIT);
		parser.setCompilerOptions(s_options);
		parser.setEnvironment(new String[0], new String[] { s_sourceRoot }, null, true);
	}

	static Stats parsePerFile(List<Path> files) throws Exception
	{
		final Stats stats = new Stats();
		for (Path file: files)
		{
			final String source = new String(Files.readAllBytes(file), StandardCharsets.UTF_8);
			final ASTParser parser = ASTParser.newParser(AST.getJLSLatest());
			configure(parser);
			parser.setUnitName(file.getFileName().toString());
			parser.setSource(source.toCharArray());
			final CompilationUnit cu = (CompilationUnit)parser.createAST(null);
			if (cu == null)
			{
				throw new IllegalStateException("null AST for " + file);
			}
			stats.bindings += countBindings(cu);
			stats.problems += cu.getProblems().length;
		}
		return stats;
	}

	static Stats parseBatch(List<Path> files, int batchSize) throws Exception
	{
		final Stats stats = new Stats();
		for (int start = 0; start < files.size(); start += batchSize)
		{
			final List<Path> chunk = files.subList(start, Math.min(start + batchSize, files.size()));
			final String[] paths = new String[chunk.size()];
			final String[] encodings = new String[chunk.size()];
			for (int i = 0; i < chunk.size(); i++)
			{
				paths[i] = chunk.get(i).toString();
				encodings[i] = "UTF-8";
			}

			final Map<String, long[]> results = new LinkedHashMap<>();
			final ASTParser parser = ASTParser.newParser(AST.getJLSLatest());
			configure(parser);
			parser.createASTs(paths, encodings, new String[0], new FileASTRequestor()
			{
				@Override
				public void acceptAST(String sourceFilePath, CompilationUnit ast)
				{
					results.put(sourceFilePath, new long[] { countBindings(ast), ast.getProblems().length });
				}
			}, null);

			for (long[] r: results.values())
			{
				stats.bindings += r[0];
				stats.problems += r[1];
			}
		}
		return stats;
	}

	static long countBindings(CompilationUnit cu)
	{
		final long[] count = { 0 };
		cu.accept(new ASTVisitor()
		{
			@Override
			public boolean visit(SimpleName node)
			{
				if (node.resolveBinding() != null)
				{
					count[0]++;
				}
				return true;
			}
		});
		return count[0];
	}
}
