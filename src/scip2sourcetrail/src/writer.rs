//! Safe wrapper over the C ABI of the SourcetrailDB writer shim
//! (src/odin_writer/sourcetrail_odin_writer.h, ABI v2).

use std::ffi::{c_char, c_int, c_void, CStr, CString};
use std::path::Path;

#[repr(C)]
struct NameElementC {
    prefix: *const c_char,
    name: *const c_char,
    postfix: *const c_char,
}

extern "C" {
    fn sodb_abi_version() -> c_int;
    fn sodb_open(path: *const c_char) -> *mut c_void;
    fn sodb_close(w: *mut c_void);
    fn sodb_begin(w: *mut c_void) -> c_int;
    fn sodb_commit(w: *mut c_void) -> c_int;
    fn sodb_symbol_ex(
        w: *mut c_void,
        delimiter: *const c_char,
        elements: *const NameElementC,
        count: c_int,
        symbol_kind: c_int,
        definition_kind: c_int,
    ) -> c_int;
    fn sodb_symbol_location(
        w: *mut c_void,
        id: c_int,
        file: c_int,
        sl: c_int,
        sc: c_int,
        el: c_int,
        ec: c_int,
    ) -> c_int;
    fn sodb_symbol_scope_location(
        w: *mut c_void,
        id: c_int,
        file: c_int,
        sl: c_int,
        sc: c_int,
        el: c_int,
        ec: c_int,
    ) -> c_int;
    fn sodb_reference(w: *mut c_void, source: c_int, target: c_int, kind: c_int) -> c_int;
    fn sodb_reference_location(
        w: *mut c_void,
        id: c_int,
        file: c_int,
        sl: c_int,
        sc: c_int,
        el: c_int,
        ec: c_int,
    ) -> c_int;
    fn sodb_file(w: *mut c_void, path: *const c_char) -> c_int;
    fn sodb_file_language(w: *mut c_void, file: c_int, language: *const c_char) -> c_int;
    fn sodb_error(
        w: *mut c_void,
        message: *const c_char,
        fatal: c_int,
        file: c_int,
        sl: c_int,
        sc: c_int,
        el: c_int,
        ec: c_int,
    ) -> c_int;
    fn sodb_last_error(w: *mut c_void) -> *const c_char;
    fn sodb_local_symbol(w: *mut c_void, name: *const c_char) -> c_int;
    fn sodb_local_symbol_location(
        w: *mut c_void,
        id: c_int,
        file: c_int,
        sl: c_int,
        sc: c_int,
        el: c_int,
        ec: c_int,
    ) -> c_int;
}

/// Symbol kinds (SODB_SYMBOL_*).
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
#[repr(i32)]
pub enum SymbolKind {
    Type = 0,
    BuiltinType = 1,
    Module = 2,
    Namespace = 3,
    Package = 4,
    Struct = 5,
    Class = 6,
    Interface = 7,
    Annotation = 8,
    GlobalVariable = 9,
    Field = 10,
    Function = 11,
    Method = 12,
    Enum = 13,
    EnumConstant = 14,
    Typedef = 15,
    TypeParameter = 16,
    Macro = 17,
    Union = 18,
}

impl SymbolKind {
    pub fn is_callable(self) -> bool {
        matches!(self, SymbolKind::Function | SymbolKind::Method)
    }

    pub fn is_type(self) -> bool {
        matches!(
            self,
            SymbolKind::Type
                | SymbolKind::BuiltinType
                | SymbolKind::Struct
                | SymbolKind::Class
                | SymbolKind::Interface
                | SymbolKind::Enum
                | SymbolKind::Typedef
                | SymbolKind::TypeParameter
                | SymbolKind::Union
        )
    }
}

/// Reference kinds (SODB_REFERENCE_*).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(i32)]
pub enum ReferenceKind {
    TypeUsage = 0,
    Usage = 1,
    Call = 2,
    Inheritance = 3,
    Override = 4,
    Import = 8,
    MacroUsage = 9,
}

/// Definition kinds (SODB_DEFINITION_*).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(i32)]
pub enum DefinitionKind {
    None = 0,
    Explicit = 2,
}

/// One level of a qualified name.
#[derive(Clone, Debug, Default, PartialEq, Eq, Hash)]
pub struct NameElement {
    pub prefix: String,
    pub name: String,
    pub postfix: String,
}

/// A source range in Sourcetrail coordinates: 1-based, both ends inclusive.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct Range {
    pub start_line: i32,
    pub start_column: i32,
    pub end_line: i32,
    pub end_column: i32,
}

pub struct Writer {
    handle: *mut c_void,
}

fn cstring(s: &str) -> CString {
    // Interior NULs cannot cross the C ABI; they never occur in source paths
    // or symbol names, so dropping them is enough.
    CString::new(s.replace('\0', "")).unwrap()
}

impl Writer {
    /// The ABI version of the loaded shim.
    pub fn abi_version() -> i32 {
        unsafe { sodb_abi_version() }
    }

    pub fn open(path: &Path) -> Result<Writer, String> {
        if Self::abi_version() < 2 {
            return Err(format!(
                "the writer shim implements ABI v{}, v2 is required",
                Self::abi_version()
            ));
        }
        let p = cstring(&path.to_string_lossy());
        let handle = unsafe { sodb_open(p.as_ptr()) };
        if handle.is_null() {
            return Err(format!("cannot open the database {}", path.display()));
        }
        Ok(Writer { handle })
    }

    pub fn last_error(&self) -> String {
        unsafe {
            let p = sodb_last_error(self.handle);
            if p.is_null() {
                String::new()
            } else {
                CStr::from_ptr(p).to_string_lossy().into_owned()
            }
        }
    }

    fn check(&self, ok: c_int, what: &str) -> Result<(), String> {
        if ok == 0 {
            Err(format!("{what}: {}", self.last_error()))
        } else {
            Ok(())
        }
    }

    fn id(&self, id: c_int, what: &str) -> Result<i32, String> {
        if id == 0 {
            Err(format!("{what}: {}", self.last_error()))
        } else {
            Ok(id)
        }
    }

    pub fn begin(&mut self) -> Result<(), String> {
        let ok = unsafe { sodb_begin(self.handle) };
        self.check(ok, "begin")
    }

    pub fn commit(&mut self) -> Result<(), String> {
        let ok = unsafe { sodb_commit(self.handle) };
        self.check(ok, "commit")
    }

    pub fn symbol(
        &mut self,
        delimiter: &str,
        name: &[NameElement],
        kind: Option<SymbolKind>,
        definition: DefinitionKind,
    ) -> Result<i32, String> {
        let owned: Vec<(CString, CString, CString)> = name
            .iter()
            .map(|e| (cstring(&e.prefix), cstring(&e.name), cstring(&e.postfix)))
            .collect();
        let elements: Vec<NameElementC> = owned
            .iter()
            .map(|(p, n, s)| NameElementC {
                prefix: p.as_ptr(),
                name: n.as_ptr(),
                postfix: s.as_ptr(),
            })
            .collect();
        let d = cstring(delimiter);
        let id = unsafe {
            sodb_symbol_ex(
                self.handle,
                d.as_ptr(),
                elements.as_ptr(),
                elements.len() as c_int,
                kind.map_or(-1, |k| k as c_int),
                definition as c_int,
            )
        };
        self.id(id, "symbol")
    }

    pub fn symbol_location(&mut self, symbol: i32, file: i32, r: Range) -> Result<(), String> {
        let ok = unsafe {
            sodb_symbol_location(
                self.handle,
                symbol,
                file,
                r.start_line,
                r.start_column,
                r.end_line,
                r.end_column,
            )
        };
        self.check(ok, "symbol location")
    }

    pub fn symbol_scope_location(
        &mut self,
        symbol: i32,
        file: i32,
        r: Range,
    ) -> Result<(), String> {
        let ok = unsafe {
            sodb_symbol_scope_location(
                self.handle,
                symbol,
                file,
                r.start_line,
                r.start_column,
                r.end_line,
                r.end_column,
            )
        };
        self.check(ok, "scope location")
    }

    pub fn reference(
        &mut self,
        source: i32,
        target: i32,
        kind: ReferenceKind,
    ) -> Result<i32, String> {
        let id = unsafe { sodb_reference(self.handle, source, target, kind as c_int) };
        self.id(id, "reference")
    }

    pub fn reference_location(
        &mut self,
        reference: i32,
        file: i32,
        r: Range,
    ) -> Result<(), String> {
        let ok = unsafe {
            sodb_reference_location(
                self.handle,
                reference,
                file,
                r.start_line,
                r.start_column,
                r.end_line,
                r.end_column,
            )
        };
        self.check(ok, "reference location")
    }

    pub fn file(&mut self, path: &str) -> Result<i32, String> {
        let p = cstring(path);
        let id = unsafe { sodb_file(self.handle, p.as_ptr()) };
        self.id(id, "file")
    }

    pub fn file_language(&mut self, file: i32, language: &str) -> Result<(), String> {
        let l = cstring(language);
        let ok = unsafe { sodb_file_language(self.handle, file, l.as_ptr()) };
        self.check(ok, "file language")
    }

    pub fn error(&mut self, message: &str, fatal: bool, file: i32, r: Range) -> Result<(), String> {
        let m = cstring(message);
        let ok = unsafe {
            sodb_error(
                self.handle,
                m.as_ptr(),
                fatal as c_int,
                file,
                r.start_line,
                r.start_column,
                r.end_line,
                r.end_column,
            )
        };
        self.check(ok, "error")
    }

    pub fn local_symbol(&mut self, name: &str) -> Result<i32, String> {
        let n = cstring(name);
        let id = unsafe { sodb_local_symbol(self.handle, n.as_ptr()) };
        self.id(id, "local symbol")
    }

    pub fn local_symbol_location(&mut self, local: i32, file: i32, r: Range) -> Result<(), String> {
        let ok = unsafe {
            sodb_local_symbol_location(
                self.handle,
                local,
                file,
                r.start_line,
                r.start_column,
                r.end_line,
                r.end_column,
            )
        };
        self.check(ok, "local location")
    }
}

impl Drop for Writer {
    fn drop(&mut self) {
        unsafe { sodb_close(self.handle) };
    }
}
