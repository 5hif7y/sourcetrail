//! SCIP symbols to Sourcetrail names and kinds.
//!
//! A global SCIP symbol is `<scheme> <manager> <package> <version>
//! <descriptors>`. Each descriptor becomes one level of the Sourcetrail name;
//! the package name, when there is one (a Rust crate), is the first level.
//! The name depends only on the symbol string, so the same symbol gets the
//! same Sourcetrail node in every batch.

use crate::writer::{NameElement, SymbolKind};
use protobuf::Enum;
use scip::types::descriptor::Suffix;
use scip::types::symbol_information::Kind;
use scip::types::SymbolInformation;

/// What a SCIP symbol becomes in Sourcetrail.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Target {
    /// A node with a qualified name and a kind guessed from its last
    /// descriptor (the SymbolInformation kind, when there is one, wins).
    Global {
        name: Vec<NameElement>,
        kind: Option<SymbolKind>,
    },
    /// A local symbol (a `local N` symbol or a parameter). The string is a
    /// globally unique name for it.
    Local(String),
}

/// Parses a SCIP symbol. `document` is the relative path of the document the
/// symbol appears in; it makes `local N` names unique. Returns None for
/// symbols that cannot be parsed or have no descriptors.
pub fn parse(symbol: &str, document: &str) -> Option<Target> {
    if scip::symbol::is_local_symbol(symbol) {
        return Some(Target::Local(format!("{document}#{symbol}")));
    }
    let parsed = scip::symbol::parse_symbol(symbol).ok()?;
    let descriptors = &parsed.descriptors;
    if descriptors.is_empty() {
        return None;
    }

    let mut name = Vec::new();
    if let Some(package) = parsed.package.as_ref() {
        if !package.name.is_empty() {
            name.push(NameElement {
                name: package.name.clone(),
                ..Default::default()
            });
        }
    }

    let mut kind = None;
    let mut parent_is_type = false;
    for (i, d) in descriptors.iter().enumerate() {
        let suffix = d.suffix.value();
        let last = i + 1 == descriptors.len();
        if suffix == Suffix::Parameter.value() || suffix == Suffix::Local.value() {
            // Parameters are locals of their function in Sourcetrail.
            return Some(Target::Local(symbol.to_string()));
        }
        if d.name.is_empty() {
            continue;
        }
        let mut element = NameElement {
            name: d.name.clone(),
            ..Default::default()
        };
        if suffix == Suffix::Method.value() {
            // The disambiguator tells overloads apart, e.g. `(+1)`.
            element.postfix = if d.disambiguator.is_empty() {
                "()".to_string()
            } else {
                format!("({})", d.disambiguator)
            };
        }
        if last {
            kind = kind_from_suffix(suffix, parent_is_type);
        }
        // Type parameters (`impl#[T]`) do not change what the member hangs
        // from.
        if suffix != Suffix::TypeParameter.value() {
            parent_is_type = suffix == Suffix::Type.value();
        }
        name.push(element);
    }
    if name.is_empty() {
        return None;
    }
    Some(Target::Global { name, kind })
}

fn kind_from_suffix(suffix: i32, parent_is_type: bool) -> Option<SymbolKind> {
    if suffix == Suffix::Namespace.value() {
        Some(SymbolKind::Namespace)
    } else if suffix == Suffix::Type.value() {
        Some(SymbolKind::Type)
    } else if suffix == Suffix::Term.value() {
        Some(if parent_is_type {
            SymbolKind::Field
        } else {
            SymbolKind::GlobalVariable
        })
    } else if suffix == Suffix::Method.value() {
        Some(if parent_is_type {
            SymbolKind::Method
        } else {
            SymbolKind::Function
        })
    } else if suffix == Suffix::TypeParameter.value() {
        Some(SymbolKind::TypeParameter)
    } else if suffix == Suffix::Macro.value() {
        Some(SymbolKind::Macro)
    } else {
        None
    }
}

/// The Sourcetrail kind for a SymbolInformation kind; it wins over the guess
/// from the descriptor. When SCIP gives no kind, or one without an
/// equivalent, the guess is kept.
pub fn kind_from_info(info: &SymbolInformation, guess: Option<SymbolKind>) -> Option<SymbolKind> {
    let Ok(kind) = info.kind.enum_value() else {
        return guess;
    };
    use Kind::*;
    let mapped = match kind {
        Class | SingletonClass | Object => SymbolKind::Class,
        Struct => SymbolKind::Struct,
        Interface | Trait | Protocol | TypeClass | Mixin => SymbolKind::Interface,
        Enum => SymbolKind::Enum,
        EnumMember => SymbolKind::EnumConstant,
        Union => SymbolKind::Union,
        TypeAlias | AssociatedType | Delegate => SymbolKind::Typedef,
        Type | TypeFamily | Concept => SymbolKind::Type,
        TypeParameter => SymbolKind::TypeParameter,
        Module | Library | File => SymbolKind::Module,
        Namespace | PackageObject => SymbolKind::Namespace,
        Package => SymbolKind::Package,
        Function => SymbolKind::Function,
        Method | AbstractMethod | Constructor | Getter | Setter | Accessor | StaticMethod
        | TraitMethod | ProtocolMethod | PureVirtualMethod | MethodSpecification
        | TypeClassMethod | SingletonMethod | MethodAlias | Operator | Extension => {
            SymbolKind::Method
        }
        Field | StaticField | StaticDataMember | Property | StaticProperty | Event
        | StaticEvent => SymbolKind::Field,
        Constant | Variable | StaticVariable | Value => match guess {
            Some(SymbolKind::Field) => SymbolKind::Field,
            _ => SymbolKind::GlobalVariable,
        },
        Macro => SymbolKind::Macro,
        Attribute => SymbolKind::Annotation,
        _ => return guess,
    };
    Some(mapped)
}

/// The name delimiter for a language: `::` for Rust and C/C++, `.` otherwise.
pub fn delimiter_for(language: &str, scheme: &str) -> &'static str {
    let l = language.to_ascii_lowercase();
    if l == "rust"
        || l == "c"
        || l == "cpp"
        || l == "c++"
        || scheme == "rust-analyzer"
        || scheme == "scip-clang"
    {
        "::"
    } else {
        "."
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn names(t: &Target) -> Vec<String> {
        match t {
            Target::Global { name, .. } => name
                .iter()
                .map(|e| format!("{}{}", e.name, e.postfix))
                .collect(),
            Target::Local(s) => vec![format!("local:{s}")],
        }
    }

    #[test]
    fn rust_method() {
        let t = parse(
            "rust-analyzer cargo demo 0.1.0 shapes/Box#grow().",
            "src/lib.rs",
        )
        .unwrap();
        assert_eq!(names(&t), ["demo", "shapes", "Box", "grow()"]);
        assert!(matches!(
            t,
            Target::Global {
                kind: Some(SymbolKind::Method),
                ..
            }
        ));
    }

    #[test]
    fn free_function_and_field() {
        let f = parse("rust-analyzer cargo demo 0.1.0 util/helper().", "a.rs").unwrap();
        assert!(matches!(
            f,
            Target::Global {
                kind: Some(SymbolKind::Function),
                ..
            }
        ));
        let g = parse("rust-analyzer cargo demo 0.1.0 shapes/Box#width.", "a.rs").unwrap();
        assert!(matches!(
            g,
            Target::Global {
                kind: Some(SymbolKind::Field),
                ..
            }
        ));
    }

    #[test]
    fn overloads_differ() {
        let a = parse("scip-dotnet nuget . . App/Shape#Area().", "a.cs").unwrap();
        let b = parse("scip-dotnet nuget . . App/Shape#Area(+1).", "a.cs").unwrap();
        assert_eq!(names(&a), ["App", "Shape", "Area()"]);
        assert_eq!(names(&b), ["App", "Shape", "Area(+1)"]);
    }

    #[test]
    fn locals_and_parameters() {
        assert_eq!(
            parse("local 3", "src/a.rs"),
            Some(Target::Local("src/a.rs#local 3".into()))
        );
        let p = parse("rust-analyzer cargo demo 0.1.0 util/helper().(x)", "a.rs").unwrap();
        assert!(matches!(p, Target::Local(_)));
    }

    #[test]
    fn delimiters() {
        assert_eq!(delimiter_for("rust", ""), "::");
        assert_eq!(delimiter_for("", "rust-analyzer"), "::");
        assert_eq!(delimiter_for("CSharp", "scip-dotnet"), ".");
    }
}
