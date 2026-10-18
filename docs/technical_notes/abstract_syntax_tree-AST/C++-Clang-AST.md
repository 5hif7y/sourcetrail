# Viewing the Clang AST

* Command line:
    * [Introduction to the Clang AST](https://clang.llvm.org/docs/IntroductionToTheClangAST.html#examining-the-ast)  
    i.e.: `clang -Xclang -ast-dump -fsyntax-only <input-file.cpp>`

* [Compiler Explorer](https://compiler-explorer.com/):
    * Select a compiler... : **x86-64 clang (trunk)**
    * Add new... **AST**

* [Compiler Explorer link](https://compiler-explorer.com/#g:!((g:!((g:!((h:codeEditor,i:(filename:'1',fontScale:14,fontUsePx:'0',j:1,lang:c%2B%2B,source:'//+Type+your+code+here,+or+load+an+example.%0Aint+square(int+num)+%7B%0A++++return+num+*+num%3B%0A%7D'),l:'5',n:'0',o:'C%2B%2B+source+%231',t:'0')),k:42.447552447552454,l:'4',n:'0',o:'',s:0,t:'0'),(g:!((h:compiler,i:(compiler:clang_trunk,filters:(b:'0',binary:'1',binaryObject:'1',commentOnly:'0',debugCalls:'1',demangle:'0',directives:'0',execute:'1',intel:'0',libraryCode:'0',trim:'1',verboseDemangling:'0'),flagsViewOpen:'1',fontScale:14,fontUsePx:'0',j:1,lang:c%2B%2B,libs:!(),options:'',overrides:!(),selection:(endColumn:1,endLineNumber:1,positionColumn:1,positionLineNumber:1,selectionStartColumn:1,selectionStartLineNumber:1,startColumn:1,startLineNumber:1),source:1),l:'5',n:'0',o:'+x86-64+clang+(trunk)+(Editor+%231)',t:'0'),(h:ast,i:(compilerName:'x86-64+clang+(trunk)',editorid:1,fontScale:14,fontUsePx:'0',j:1,selection:(endColumn:1,endLineNumber:1,positionColumn:1,positionLineNumber:1,selectionStartColumn:1,selectionStartLineNumber:1,startColumn:1,startLineNumber:1),treeid:0),l:'5',n:'0',o:'Ast+Viewer+x86-64+clang+(trunk)+(Editor+%231,+Compiler+%231)',t:'0')),k:57.552447552447546,l:'4',n:'0',o:'',s:1,t:'0')),l:'2',n:'0',o:'',t:'0')),version:4).