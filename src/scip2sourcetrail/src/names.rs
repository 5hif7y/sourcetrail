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
        /// The symbol is another name for an existing node (rust-analyzer's
        /// `impl#[T]` is `Self`, i.e. `T`): no kind is recorded for it.
        alias: bool,
        /// A method of `impl Trait for T` (rust-analyzer): the trait's simple
        /// name. `name` is then `T::method` and T implements the trait.
        trait_impl: Option<String>,
    },
    /// A local symbol (a `local N` symbol or a parameter). The string is a
    /// globally unique name for it.
    Local(String),
}

/// A descriptor reduced to what the name needs.
struct Level {
    name: String,
    suffix: i32,
    disambiguator: String,
}

/// `Add<Self>` (written with backticks in SCIP) -> `Add`.
fn simple_type_name(name: &str) -> String {
    let n = name.trim_matches('`');
    n.split('<').next().unwrap_or(n).trim().to_string()
}

/// rust-analyzer names a crate root `crate/`, and an impl block `impl#` with
/// its self type and trait as type parameters: `impl#[Square]new().` and
/// `impl#[Square][Area]area().`. This turns them into what a Rust programmer
/// writes: `Square::new` and `Square::area` (for Area). Returns the levels,
/// whether the symbol is the `Self` alias, and the trait.
fn rust_levels(mut levels: Vec<Level>) -> (Vec<Level>, bool, Option<String>) {
    let namespace = Suffix::Namespace.value();
    let type_ = Suffix::Type.value();
    let type_parameter = Suffix::TypeParameter.value();
    if levels
        .first()
        .is_some_and(|l| l.suffix == namespace && l.name == "crate")
    {
        levels.remove(0);
    }
    let mut alias = false;
    let mut trait_name = None;
    if let Some(i) = levels
        .iter()
        .position(|l| l.suffix == type_ && l.name == "impl")
    {
        let params: Vec<String> = levels[i + 1..]
            .iter()
            .take_while(|l| l.suffix == type_parameter)
            .map(|l| simple_type_name(&l.name))
            .collect();
        if let Some(self_type) = params.first() {
            let rest: Vec<Level> = levels.split_off(i + 1 + params.len());
            levels.truncate(i);
            levels.push(Level {
                name: self_type.clone(),
                suffix: type_,
                disambiguator: String::new(),
            });
            alias = rest.is_empty();
            trait_name = params.get(1).cloned();
            levels.extend(rest);
        }
    }
    (levels, alias, trait_name)
}

/// Parses a SCIP symbol. `document` is the relative path of the document the
/// symbol appears in; it makes `local N` names unique. Returns None for
/// symbols that cannot be parsed or have no descriptors.
pub fn parse(symbol: &str, document: &str) -> Option<Target> {
    if scip::symbol::is_local_symbol(symbol) {
        return Some(Target::Local(format!("{document}#{symbol}")));
    }
    let parsed = scip::symbol::parse_symbol(symbol).ok()?;
    if parsed.descriptors.is_empty() {
        return None;
    }
    let mut levels = Vec::new();
    for d in &parsed.descriptors {
        let suffix = d.suffix.value();
        if suffix == Suffix::Parameter.value() || suffix == Suffix::Local.value() {
            // Parameters are locals of their function in Sourcetrail.
            return Some(Target::Local(symbol.to_string()));
        }
        if !d.name.is_empty() {
            levels.push(Level {
                name: d.name.clone(),
                suffix,
                disambiguator: d.disambiguator.clone(),
            });
        }
    }
    let (levels, alias, trait_impl) = if parsed.scheme == "rust-analyzer" {
        rust_levels(levels)
    } else {
        (levels, false, None)
    };

    let mut name = Vec::new();
    if let Some(package) = parsed.package.as_ref() {
        if !package.name.is_empty() {
            name.push(NameElement {
                name: package.name.clone(),
                ..Default::default()
            });
        }
    }
    let mut kind = if levels.is_empty() {
        Some(SymbolKind::Module)
    } else {
        None
    };
    let mut parent_is_type = false;
    let count = levels.len();
    for (i, l) in levels.into_iter().enumerate() {
        let mut element = NameElement {
            name: l.name,
            ..Default::default()
        };
        if l.suffix == Suffix::Method.value() {
            // The disambiguator tells overloads apart, e.g. `(+1)`; the trait
            // tells `Square::area` for Area from an inherent `Square::area`.
            element.postfix = if l.disambiguator.is_empty() {
                "()".to_string()
            } else {
                format!("({})", l.disambiguator)
            };
            if let Some(t) = &trait_impl {
                element.postfix.push_str(&format!(" ({t})"));
            }
        }
        if i + 1 == count {
            kind = kind_from_suffix(l.suffix, parent_is_type);
        }
        // Type parameters do not change what the member hangs from.
        if l.suffix != Suffix::TypeParameter.value() {
            parent_is_type = l.suffix == Suffix::Type.value();
        }
        name.push(element);
    }
    if name.is_empty() {
        return None;
    }
    let trait_impl = if alias { None } else { trait_impl };
    Some(Target::Global {
        name,
        kind,
        alias,
        trait_impl,
    })
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
    fn rust_analyzer_impls_and_crate_root() {
        const R: &str = "rust-analyzer cargo demo 0.1.0 ";
        let root = parse(&format!("{R}crate/"), "src/main.rs").unwrap();
        assert_eq!(names(&root), ["demo"]);
        assert!(matches!(
            root,
            Target::Global {
                kind: Some(SymbolKind::Module),
                ..
            }
        ));

        let inherent = parse(&format!("{R}shapes/impl#[Square]new()."), "a.rs").unwrap();
        assert_eq!(names(&inherent), ["demo", "shapes", "Square", "new()"]);
        assert!(matches!(
            inherent,
            Target::Global {
                kind: Some(SymbolKind::Method),
                alias: false,
                trait_impl: None,
                ..
            }
        ));

        let trait_impl = parse(&format!("{R}shapes/impl#[Square][Area]area()."), "a.rs").unwrap();
        assert_eq!(
            names(&trait_impl),
            ["demo", "shapes", "Square", "area() (Area)"]
        );
        assert!(matches!(&trait_impl, Target::Global { trait_impl: Some(t), .. } if t == "Area"));

        // `Self` names the type: an alias without a kind of its own.
        let alias = parse(&format!("{R}shapes/impl#[Square]"), "a.rs").unwrap();
        assert_eq!(names(&alias), ["demo", "shapes", "Square"]);
        assert!(matches!(
            alias,
            Target::Global {
                alias: true,
                trait_impl: None,
                ..
            }
        ));

        // Generic traits lose their arguments and backticks.
        let core = "rust-analyzer cargo core https://github.com/rust-lang/rust/library/core \
                    ops/arith/impl#[f64][`Add<Self>`]add().";
        assert_eq!(
            names(&parse(core, "a.rs").unwrap()),
            ["core", "ops", "arith", "f64", "add() (Add)"]
        );

        // Other schemes keep `impl` and `crate` as plain names.
        let other = parse("scip-other cargo demo 0.1.0 crate/impl#", "a.rs").unwrap();
        assert_eq!(names(&other), ["demo", "crate", "impl"]);
    }

    #[test]
    fn delimiters() {
        assert_eq!(delimiter_for("rust", ""), "::");
        assert_eq!(delimiter_for("", "rust-analyzer"), "::");
        assert_eq!(delimiter_for("CSharp", "scip-dotnet"), ".");
    }
}
