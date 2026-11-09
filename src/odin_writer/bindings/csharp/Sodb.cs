// P/Invoke binding of the SourcetrailDB writer shim, ABI v2
// (src/odin_writer/sourcetrail_odin_writer.h).
//
// A base for a C# indexer built on Roslyn that writes a Sourcetrail database
// directly, without SCIP. The shim library (sourcetrail_odin_writer.dll, or
// libsourcetrail_odin_writer.so/.dylib) must be next to the program or on
// the library search path.

using System;
using System.Runtime.InteropServices;

namespace Sourcetrail.Writer
{
    public enum SymbolKind
    {
        None = -1,
        Type = 0, BuiltinType = 1, Module = 2, Namespace = 3, Package = 4, Struct = 5,
        Class = 6, Interface = 7, Annotation = 8, GlobalVariable = 9, Field = 10,
        Function = 11, Method = 12, Enum = 13, EnumConstant = 14, Typedef = 15,
        TypeParameter = 16, Macro = 17, Union = 18,
    }

    public enum ReferenceKind
    {
        TypeUsage = 0, Usage = 1, Call = 2, Inheritance = 3, Override = 4,
        TypeArgument = 5, TemplateSpecialization = 6, Include = 7, Import = 8,
        MacroUsage = 9, AnnotationUsage = 10,
    }

    public enum DefinitionKind { None = 0, Implicit = 1, Explicit = 2 }

    /// <summary>One level of a qualified name; Postfix carries the signature.</summary>
    public readonly struct NameElement
    {
        public readonly string Prefix, Name, Postfix;
        public NameElement(string name, string prefix = "", string postfix = "")
        {
            Name = name; Prefix = prefix; Postfix = postfix;
        }
    }

    /// <summary>1-based lines and columns, both ends inclusive.</summary>
    public readonly struct SourceRange
    {
        public readonly int StartLine, StartColumn, EndLine, EndColumn;
        public SourceRange(int startLine, int startColumn, int endLine, int endColumn)
        {
            StartLine = startLine; StartColumn = startColumn; EndLine = endLine; EndColumn = endColumn;
        }
    }

    public sealed class SodbException : Exception
    {
        public SodbException(string message) : base(message) { }
    }

    internal static class Native
    {
        const string Lib = "sourcetrail_odin_writer";

        [StructLayout(LayoutKind.Sequential)]
        internal struct NameElementC { public IntPtr Prefix, Name, Postfix; }

        [DllImport(Lib)] internal static extern int sodb_abi_version();
        [DllImport(Lib)] internal static extern IntPtr sodb_open(byte[] path);
        [DllImport(Lib)] internal static extern void sodb_close(IntPtr w);
        [DllImport(Lib)] internal static extern int sodb_begin(IntPtr w);
        [DllImport(Lib)] internal static extern int sodb_commit(IntPtr w);
        [DllImport(Lib)] internal static extern int sodb_symbol_ex(IntPtr w, byte[] delimiter, NameElementC[] elements, int count, int symbolKind, int definitionKind);
        [DllImport(Lib)] internal static extern int sodb_symbol_location(IntPtr w, int id, int file, int sl, int sc, int el, int ec);
        [DllImport(Lib)] internal static extern int sodb_symbol_scope_location(IntPtr w, int id, int file, int sl, int sc, int el, int ec);
        [DllImport(Lib)] internal static extern int sodb_symbol_signature_location(IntPtr w, int id, int file, int sl, int sc, int el, int ec);
        [DllImport(Lib)] internal static extern int sodb_qualifier_location(IntPtr w, int id, int file, int sl, int sc, int el, int ec);
        [DllImport(Lib)] internal static extern int sodb_reference(IntPtr w, int source, int target, int kind);
        [DllImport(Lib)] internal static extern int sodb_reference_location(IntPtr w, int id, int file, int sl, int sc, int el, int ec);
        [DllImport(Lib)] internal static extern int sodb_reference_ambiguous(IntPtr w, int id);
        [DllImport(Lib)] internal static extern int sodb_file(IntPtr w, byte[] path);
        [DllImport(Lib)] internal static extern int sodb_file_language(IntPtr w, int file, byte[] language);
        [DllImport(Lib)] internal static extern int sodb_local_symbol(IntPtr w, byte[] name);
        [DllImport(Lib)] internal static extern int sodb_local_symbol_location(IntPtr w, int id, int file, int sl, int sc, int el, int ec);
        [DllImport(Lib)] internal static extern int sodb_atomic_source_range(IntPtr w, int file, int sl, int sc, int el, int ec);
        [DllImport(Lib)] internal static extern int sodb_error(IntPtr w, byte[] message, int fatal, int file, int sl, int sc, int el, int ec);
        [DllImport(Lib)] internal static extern IntPtr sodb_last_error(IntPtr w);
    }

    /// <summary>A Sourcetrail database being written. Not thread-safe.</summary>
    public sealed class Writer : IDisposable
    {
        IntPtr _handle;

        public static int AbiVersion => Native.sodb_abi_version();

        // NUL-terminated UTF-8, which is what the shim expects.
        static byte[] Utf8(string s) => System.Text.Encoding.UTF8.GetBytes((s ?? "") + "\0");

        public Writer(string databasePath)
        {
            if (AbiVersion < 2)
                throw new SodbException($"the writer shim implements ABI v{AbiVersion}, v2 is required");
            _handle = Native.sodb_open(Utf8(databasePath));
            if (_handle == IntPtr.Zero)
                throw new SodbException($"cannot open the database {databasePath}");
        }

        public string LastError =>
            Marshal.PtrToStringUTF8(Native.sodb_last_error(_handle)) ?? "";

        void Check(int ok, string what)
        {
            if (ok == 0) throw new SodbException($"{what}: {LastError}");
        }

        int Id(int id, string what)
        {
            if (id == 0) throw new SodbException($"{what}: {LastError}");
            return id;
        }

        public void Begin() => Check(Native.sodb_begin(_handle), "begin");
        public void Commit() => Check(Native.sodb_commit(_handle), "commit");

        public int Symbol(string delimiter, NameElement[] name, SymbolKind kind, DefinitionKind definition)
        {
            // The strings must stay pinned while the shim reads them.
            var handles = new GCHandle[name.Length * 3];
            var elements = new Native.NameElementC[name.Length];
            try
            {
                for (int i = 0; i < name.Length; i++)
                {
                    IntPtr Pin(int slot, string s)
                    {
                        handles[slot] = GCHandle.Alloc(Utf8(s), GCHandleType.Pinned);
                        return handles[slot].AddrOfPinnedObject();
                    }
                    elements[i].Prefix = Pin(3 * i, name[i].Prefix);
                    elements[i].Name = Pin(3 * i + 1, name[i].Name);
                    elements[i].Postfix = Pin(3 * i + 2, name[i].Postfix);
                }
                return Id(Native.sodb_symbol_ex(_handle, Utf8(delimiter), elements, name.Length, (int)kind, (int)definition), "symbol");
            }
            finally
            {
                foreach (var h in handles)
                    if (h.IsAllocated) h.Free();
            }
        }

        public void SymbolLocation(int symbol, int file, SourceRange r) =>
            Check(Native.sodb_symbol_location(_handle, symbol, file, r.StartLine, r.StartColumn, r.EndLine, r.EndColumn), "symbol location");
        public void ScopeLocation(int symbol, int file, SourceRange r) =>
            Check(Native.sodb_symbol_scope_location(_handle, symbol, file, r.StartLine, r.StartColumn, r.EndLine, r.EndColumn), "scope location");
        public void SignatureLocation(int symbol, int file, SourceRange r) =>
            Check(Native.sodb_symbol_signature_location(_handle, symbol, file, r.StartLine, r.StartColumn, r.EndLine, r.EndColumn), "signature location");
        public void QualifierLocation(int symbol, int file, SourceRange r) =>
            Check(Native.sodb_qualifier_location(_handle, symbol, file, r.StartLine, r.StartColumn, r.EndLine, r.EndColumn), "qualifier location");
        public int Reference(int source, int target, ReferenceKind kind) =>
            Id(Native.sodb_reference(_handle, source, target, (int)kind), "reference");
        public void ReferenceLocation(int reference, int file, SourceRange r) =>
            Check(Native.sodb_reference_location(_handle, reference, file, r.StartLine, r.StartColumn, r.EndLine, r.EndColumn), "reference location");
        public void ReferenceIsAmbiguous(int reference) =>
            Check(Native.sodb_reference_ambiguous(_handle, reference), "ambiguous reference");
        public int File(string path) => Id(Native.sodb_file(_handle, Utf8(path)), "file");
        public void FileLanguage(int file, string language) =>
            Check(Native.sodb_file_language(_handle, file, Utf8(language)), "file language");
        public int LocalSymbol(string uniqueName) => Id(Native.sodb_local_symbol(_handle, Utf8(uniqueName)), "local symbol");
        public void LocalSymbolLocation(int local, int file, SourceRange r) =>
            Check(Native.sodb_local_symbol_location(_handle, local, file, r.StartLine, r.StartColumn, r.EndLine, r.EndColumn), "local location");
        public void AtomicRange(int file, SourceRange r) =>
            Check(Native.sodb_atomic_source_range(_handle, file, r.StartLine, r.StartColumn, r.EndLine, r.EndColumn), "atomic range");
        public void Error(string message, bool fatal, int file, SourceRange r) =>
            Check(Native.sodb_error(_handle, Utf8(message), fatal ? 1 : 0, file, r.StartLine, r.StartColumn, r.EndLine, r.EndColumn), "error");

        public void Dispose()
        {
            if (_handle != IntPtr.Zero)
            {
                Native.sodb_close(_handle);
                _handle = IntPtr.Zero;
            }
        }
    }
}
