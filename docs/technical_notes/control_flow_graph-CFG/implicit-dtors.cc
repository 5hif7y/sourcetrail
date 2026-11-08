// implicit-dtors.cc
// Report implicit destructor calls.

#include "clang/AST/RecursiveASTVisitor.h"				   // clang::RecursiveASTVisitor
#include "clang/Analysis/CFG.h"							   // clang::CFG
#include "clang/Basic/Diagnostic.h"						   // clang::DiagnosticsEngine
#include "clang/Basic/DiagnosticOptions.h"				   // clang::DiagnosticOptions
#include "clang/Frontend/ASTUnit.h"						   // clang::ASTUnit
#include "clang/Frontend/CompilerInstance.h"			   // clang::CompilerInstance
#include "clang/Serialization/PCHContainerOperations.h"	   // clang::PCHContainerOperations

#include "llvm/Support/raw_os_ostream.h"	// llvm::raw_os_ostream

#include <cstdlib>	   // std::getenv
#include <iostream>	   // std::cout
#include <sstream>	   // std::ostringstream
#include <string>	   // std::string

#include <assert.h>	   // assert

// Convenient string construction.
#define stringb(stuff) (static_cast<std::ostringstream&>(std::ostringstream() << stuff).str())

using clang::dyn_cast;

using std::cout;
using std::endl;
using std::string;


class Visitor: public clang::RecursiveASTVisitor<Visitor>
{
public:	   // data
	clang::ASTUnit* m_astUnit;
	clang::ASTContext& m_astContext;

	bool m_printCFG;

public:	   // methods
	Visitor(clang::ASTUnit* astUnit)
		: m_astUnit(astUnit), m_astContext(astUnit->getASTContext()), m_printCFG(false)
	{
	}

	// Convenience methods to stringify some things.
	string locStr(clang::SourceLocation loc) const;
	string declLocStr(clang::Decl const* decl) const;
	string declNameLocStr(clang::NamedDecl const* decl) const;

	// Visitor methods.
	bool VisitFunctionDecl(clang::FunctionDecl* functionDecl);

	// Print destructors that 'functionDecl' implicitly calls.
	void printImplicitDtors(clang::FunctionDecl* functionDecl);

	// Kick off the traversal.
	void traverseTU();
};

string Visitor::locStr(clang::SourceLocation loc) const
{
	return loc.printToString(m_astContext.getSourceManager());
}

string Visitor::declLocStr(clang::Decl const* decl) const
{
	return locStr(decl->getLocation());
}

string Visitor::declNameLocStr(clang::NamedDecl const* decl) const
{
	return stringb(decl->getQualifiedNameAsString() << " declared at " << declLocStr(decl));
}

bool Visitor::VisitFunctionDecl(clang::FunctionDecl* functionDecl)
{
	if (functionDecl->isThisDeclarationADefinition())
	{
		printImplicitDtors(functionDecl);
	}
	return true;
}


void Visitor::printImplicitDtors(clang::FunctionDecl* functionDecl)
{
	clang::CFG::BuildOptions buildOptions;
	buildOptions.AddImplicitDtors = true;	  // Want CFGImplicitDtors.
	buildOptions.AddTemporaryDtors = true;	  // Want CFGTemporaryDtors.

	std::unique_ptr<clang::CFG> cfg(
		clang::CFG::buildCFG(functionDecl, functionDecl->getBody(), &m_astContext, buildOptions));

	// For debugging, print the entire CFG.
	if (m_printCFG)
	{
		cout << "CFG for " << declNameLocStr(functionDecl) << "\n";
		llvm::raw_os_ostream roo(cout);
		cfg->print(roo, m_astContext.getLangOpts(), false /*showColors*/);
	}

	for (clang::CFGBlock const* block: cfg->const_nodes())
	{
		for (auto elementRef: block->refs())
		{
			// According to the API documentation, it should not be necessary
			// to special-case 'CFGBaseDtor'.  But as of Clang+LLVM 16.0.0,
			// 'CFGImplicitDtor::getDestructorDecl' is simply missing the
			// implementation of that case, and has a comment that says, "Not
			// yet supported."
			if (auto baseDtorElt = elementRef->getAs<clang::CFGBaseDtor>())
			{
				clang::CXXBaseSpecifier const* baseSpec = baseDtorElt->getBaseSpecifier();

				auto baseType = dyn_cast<clang::RecordType>(
					baseSpec->getType().getDesugaredType(m_astContext).getTypePtr());
				assert(baseType);

				auto baseClass = dyn_cast<clang::CXXRecordDecl>(baseType->getDecl());
				assert(baseClass);

				clang::CXXDestructorDecl const* dtor = baseClass->getDestructor();
				assert(dtor);

				cout << declLocStr(functionDecl)
					 << ": Implicit base class dtor call: " << declNameLocStr(dtor) << "\n";
			}

			// If it were not for the above unimplemented functionality, we
			// would only need this block.
			else if (auto idtorElt = elementRef->getAs<clang::CFGImplicitDtor>())
			{
				clang::CXXDestructorDecl const* dtor = idtorElt->getDestructorDecl(m_astContext);
				assert(dtor);

				cout << declLocStr(functionDecl) << ": Implicit dtor call: " << declNameLocStr(dtor)
					 << "\n";
			}
		}
	}
}


void Visitor::traverseTU()
{
	this->TraverseDecl(m_astContext.getTranslationUnitDecl());
}


int main(int argc, char const** argv)
{
	// Copy the arguments into a vector of char pointers since that is
	// what 'createInvocationFromCommandLine' wants.
	std::vector<char const*> commandLine;
	{
		// Path to the 'clang' binary that I am behaving like.  This path is
		// used to compute the location of compiler headers like stddef.h.
		commandLine.push_back(CLANG_LLVM_INSTALL_DIR "/bin/clang");

		for (int i = 1; i < argc; ++i)
		{
			commandLine.push_back(argv[i]);
		}
	}

	// Parse the command line options.
	std::shared_ptr<clang::CompilerInvocation> compilerInvocation(
		clang::createInvocation(llvm::ArrayRef(commandLine)));
	if (!compilerInvocation)
	{
		// Command line parsing errors have already been printed.
		return 2;
	}

	// Boilerplate.
	std::shared_ptr<clang::PCHContainerOperations> pchContainerOps(
		new clang::PCHContainerOperations());
	clang::IntrusiveRefCntPtr<clang::DiagnosticsEngine> diagnosticsEngine(
		clang::CompilerInstance::createDiagnostics(&(compilerInvocation->getDiagnosticOpts())));

	// Run the Clang parser to produce an AST.
	std::unique_ptr<clang::ASTUnit> ast(clang::ASTUnit::LoadFromCompilerInvocationAction(
		compilerInvocation, pchContainerOps, diagnosticsEngine));

	if (ast == nullptr || diagnosticsEngine->getNumErrors() > 0)
	{
		// Error messages have already been printed.
		return 2;
	}

	Visitor visitor(ast.get());
	if (std::getenv("PRINT_CFG"))
	{
		visitor.m_printCFG = true;
	}

	visitor.traverseTU();

	return 0;
}


// EOF
