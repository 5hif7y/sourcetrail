// Writes a small database through the C# binding: two overloads, a call
// between them, a local, a signature, a qualifier and an atomic range. Then
// checks that a bad name is rejected with the shim's message. Exit code 0
// means every call succeeded.

using System;
using Sourcetrail.Writer;

static class Program
{
    static int Main(string[] args)
    {
        if (args.Length != 1)
        {
            Console.Error.WriteLine("usage: SodbSmoke <scratch.srctrldb>");
            return 2;
        }
        System.IO.File.Delete(args[0]);
        Console.WriteLine($"ABI v{Writer.AbiVersion}");

        using (var w = new Writer(args[0]))
        {
            w.Begin();
            int file = w.File("Smoke.cs");
            w.FileLanguage(file, "csharp");

            var scaleDouble = new[] { new NameElement("Demo"), new NameElement("Square"), new NameElement("Scale", "Square", "(double)") };
            var scaleInt = new[] { new NameElement("Demo"), new NameElement("Square"), new NameElement("Scale", "Square", "(int)") };
            int a = w.Symbol(".", scaleDouble, SymbolKind.Method, DefinitionKind.Explicit);
            int b = w.Symbol(".", scaleInt, SymbolKind.Method, DefinitionKind.Explicit);
            if (a == b) throw new SodbException("overloads must be two symbols");
            int console = w.Symbol(".", new[] { new NameElement("System"), new NameElement("Console") }, SymbolKind.Class, DefinitionKind.None);

            w.SymbolLocation(a, file, new SourceRange(3, 19, 3, 23));
            w.ScopeLocation(a, file, new SourceRange(3, 5, 3, 60));
            w.SignatureLocation(a, file, new SourceRange(3, 5, 3, 40));
            w.SymbolLocation(b, file, new SourceRange(4, 19, 4, 23));
            int call = w.Reference(b, a, ReferenceKind.Call);
            w.ReferenceLocation(call, file, new SourceRange(4, 40, 4, 44));
            w.ReferenceIsAmbiguous(call);
            w.QualifierLocation(console, file, new SourceRange(5, 9, 5, 15));
            int local = w.LocalSymbol("Smoke.cs:3:30");
            w.LocalSymbolLocation(local, file, new SourceRange(3, 30, 3, 35));
            w.AtomicRange(file, new SourceRange(1, 1, 2, 3));

            try
            {
                w.Symbol(".", new[] { new NameElement("") }, SymbolKind.Function, DefinitionKind.Explicit);
                throw new InvalidOperationException("an empty name must be rejected");
            }
            catch (SodbException e) when (e.Message.Contains("empty"))
            {
                Console.WriteLine($"rejected as expected: {e.Message}");
            }

            w.Commit();
        }
        Console.WriteLine("ok");
        return 0;
    }
}
