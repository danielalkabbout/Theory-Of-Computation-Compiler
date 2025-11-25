# TOC

Theory of Computation Project - C++ Backend

Overview
This is a C++ backend that demonstrates:
- Automaton implementation (DFA) stored in an array-based registry
- Lexical analysis driven by the DFA
- Grammar-based parsing with a recursive descent parser
- Derivation tree display (ASCII)
- Semantic analysis (simple symbol table and checks)
- Translation of the source language to Python

No std::vector is used; only fixed-size arrays and std::string.

Simple Source Language
- let x = 1 + 2 * 3;
- print x;
- Parentheses in expressions are supported.

File Structure
- include/automaton.h, src/automaton.cpp: DFA and automaton registry
- include/lexer.h, src/lexer.cpp: Tokenizer using DFA
- include/parser.h, src/parser.cpp: Recursive descent parser and AST
- include/semantic.h, src/semantic.cpp: Semantic checks (defined identifiers)
- include/translator.h, src/translator.cpp: Python translation
- src/main.cpp: Entry point, ties together all stages

Build (CMake)
1) Create a build folder and generate project files:
   - Windows (PowerShell):
     mkdir build
     cd build
     cmake -G "MinGW Makefiles" ..
     cmake --build . --config Release

   If you use Visual Studio:
     mkdir build
     cd build
     cmake -G "Visual Studio 17 2022" ..
     cmake --build . --config Release

2) The executable tc_backend will be created in the build tree.

Run
- With file input:
  ./tc_backend ../example.src

- Or type program via standard input and end with Ctrl+Z then Enter on Windows.

Example Source (example.src)
let x = 10;
let y = x + 2 * (3 + 4);
print y;

Notes
- This is a console backend (no GUI), but prints the automaton, tokens, parse tree, and translation.
- Automaton registry supports insert/delete/search by ID with a fixed-size array.

## Fixing "Permission denied" when adding files (Visual Studio .vs files)

If you see errors like:
fatal: adding files failed
error: open(".vs/TOC/... .vsidx"): Permission denied

Do the following from the repository root:

1) Add the new ignore files:
   git add .gitignore .gitattributes

2) Remove any already-indexed .vs files from the index (they remain tracked until removed):
   git rm --cached -r .vs
   or, for a single path:
   git rm --cached ".vs/TOC/FileContentIndex/724188a1-097b-4f80-b120-15afeaad3a8c.vsidx"

   - If git rm reports "Permission denied", close Visual Studio (or any process that may hold the file), then retry.

3) Commit the change:
   git commit -m "Ignore .vs and remove Visual Studio index files"

4) Push as needed:
   git push

This will stop Git from attempting to add or index the Visual Studio workspace/index files that are often locked by the IDE.

Customize the .gitignore to match your project's technologies (e.g. Node, Python, .NET, IDEs).


