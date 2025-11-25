## Design Document — Theory of Computation Compiler Backend

This document describes the architecture and detailed design for the interactive compiler backend that implements a simple language using automata, grammar-based parsing, semantic analysis, and translation to Python. It is optimized for clarity, extensibility, and functional completeness.

### 1. Goals
- Build a clear, didactic implementation tying automata, grammars, parsing, semantics, and translation.
- Provide an interactive CLI to run each compilation phase and visualize outputs.
- Keep the codebase simple, readable, and easy to extend for new language features.

### 2. Non‑Goals
- Not a production compiler. No complex error recovery, control-flow analysis, or optimization passes.
- Not a full language; intentionally minimal to focus on core concepts.

### 3. High‑Level Architecture

```
┌──────────────┐   source code   ┌──────────────┐   tokens   ┌────────────┐   AST    ┌───────────────┐   checked AST   ┌──────────────┐
│  CLI / Main  ├────────────────►│    Lexer     ├───────────►│   Parser   ├─────────►│   Semantics   ├─────────────────►│ Translator   │
└──────┬───────┘                 └──────┬──────┘            └─────┬──────┘          └──────┬────────┘                   └──────┬───────┘
       │  select DFA / load input       │                         │                         │                                │
       │                                 ▼                         ▼                         ▼                                ▼
       │                        Automaton (DFA)           Grammar (BNF)                SymbolTable                   Python source code
       │                                                                                  (scoped)
       ▼
 Interactive outputs at each stage (tokens, parse tree, semantic result, translation)
```

### 4. Modules and Responsibilities
- `automaton.*`:
  - Defines DFA representation, input classes, transition table, and helpers (`nextState`, `isAccepting`, `automatonToString`).
  - Provides `initLexerAutomaton` to create a default lexer DFA.
- `lexer.*`:
  - Tokenizes source using the selected DFA.
  - Produces a bounded `TokenList` with line/column tracking and prints tokens in a tabular format.
- `parser.*`:
  - Recursive‑descent parser that builds the AST.
  - Pretty-prints the AST with labeled edges to improve readability.
- `semantic.*`:
  - Performs basic checks: defined identifiers usage, type‑like sanity (as applicable).
  - Maintains a simple `SymbolTable` shared across the compilation unit.
- `translator.*`:
  - Translates AST into Python (straightforward mapping).
- `main.cpp` (CLI):
  - Menu-driven interface with simple options to run each step independently or the full pipeline.
  - Input supports console entry with the sentinel `END` and clears EOF properly.
  - Prints outputs immediately for each step (tokens, tree, translation).

### 5. Data Structures
- Tokens
  - `Token { type, lexeme, line, col }`
  - `TokenList { Token data[MAX_TOKENS]; int count; }` (bounded for simplicity)
- AST
  - Node types: `PROGRAM`, `STMT_LIST`, `LET`, `PRINT`, `BIN_OP(+,-,*,/)`, `IDENT`, `NUMBER`
  - Node shape: `ASTNode { NodeType type; string text; ASTNode* left; ASTNode* right; ASTNode* next; }`
  - Lists link via `next`, while structured children use `left`/`right` as per node contract.
- Automaton (DFA)
  - Fixed‐size tables for states and input classes.
  - Encodes transitions for identifiers, numbers, whitespace, operators, and delimiters.
- Symbol Table
  - Minimal symbol set for declared identifiers (from `let` statements). Extendable to track types/constness/scopes.

### 6. Source Language Definition

Informal syntax:
- Declarations: `let <ident> = <expr>;`
- Printing: `print <expr>;`
- Expressions: `expr (+|-) expr | term`, `term (*|/) term | factor`, `factor = number | ident | '(' expr ')'`

BNF (simplified):
```
program      ::= stmt_list EOF
stmt_list    ::= (stmt)*
stmt         ::= "let" IDENT "=" expr ";" | "print" expr ";"
expr         ::= term (("+" | "-") term)*
term         ::= factor (("*" | "/") factor)*
factor       ::= NUMBER | IDENT | "(" expr ")"
```

### 7. Lexer Design (Automata)
Input classes:
- Letters, digits, underscore, whitespace, operators (+ - * / =), delimiters (( ) ; { } ,), dot, other.

DFA overview:
- Start transitions to IDENT, NUMBER, WS, OP, DELIM states based on the first character class.
- `IDENT` loops on letters/digits/underscore.
- `NUMBER` loops on digits (integers for now).
- Whitespace accumulates and is skipped.
- Operators and delimiters are single‑character tokens.

Keyword recognition:
- After accepting an identifier lexeme, remap to keywords `let`, `print` if matched.

### 8. Parsing Strategy
- Hand‑written recursive‑descent parser.
- Operator precedence is encoded by the call structure (`expr` > `term` > `factor`).
- On error, the parser fails fast with a precise message (line/column from the current token).
- Robust token access: `peek()` and `consume()` gracefully handle out‑of‑range and empty token lists by using a static EOF token.

### 9. Parse Tree Rendering
- ASCII tree with consistent prefixes and labeled edges:
  - `root`, `body` for `PROGRAM`
  - `stmt[i]` entries for `STMT_LIST`
  - `ident` and `expr` under `LET`
  - `expr` under `PRINT`
  - `left` and `right` under `BIN_OP(op)`
- Example (abbreviated):
```
└── root: PROGRAM
    └── body: STMT_LIST
        ├── stmt[0]: LET
        │   ├── ident: IDENT(x)
        │   └── expr: NUMBER(10)
        └── stmt[1]: PRINT
            └── expr: IDENT(x)
```

### 10. Semantic Analysis
- Rules:
  - Identifiers used in expressions must be declared via `let` beforehand.
  - Duplicates may either be allowed to reassign (current behavior) or be flagged (optional enhancement).
- Data:
  - A flat `SymbolTable`. Extend to scoped tables if blocks/functions are added later.
- Outcome:
  - `SemanticResult { ok, errorCount, ... }`
  - On success, translation proceeds; on failure, translation is blocked with a clear message.

### 11. Translation to Python
- Mappings:
  - `let x = E;` → `x = E`
  - `print E;` → `print(E)`
  - Expressions and operator precedence preserved via recursive emission.
- Output:
  - Concise Python code, one statement per line.
  - No imports or runtime dependencies required.

### 12. CLI/UX Design
- Menu keeps actions simple and predictable:
  - Load/enter source, then Lex → Parse → Semantics → Translate.
  - Each step prints its full output immediately (tokens, tree, translation).
  - `Enter source via console` uses a sentinel `END` line to finish input, and gracefully clears EOF if Ctrl+Z is used.
  - Save translated Python to a file via menu option.
- Full pipeline option runs all steps in order with early abort on failure.

### 12A. GUI Design (Visualization and Interaction)
This section specifies a C++ GUI that visualizes the automaton, tokens, derivation tree, and translation output while providing automaton CRUD operations. It complements (does not replace) the CLI.

- Technology choice:
  - Windows-first: Win32 + GDI+ (no external deps) or Dear ImGui + GLFW (simpler UI authoring). Either option is acceptable; the design below is library-agnostic.
- Process model:
  - A single `UiApp` hosts views. Backend pipeline remains as-is and is invoked via UI events. No shared mutable state across threads; UI calls backend on the UI thread.
- UI layout (3-pane with toolbar):
  - Left pane: Automaton Manager
    - List of automaton IDs (`AutomatonManager::listIds`)
    - Buttons: Add, Delete, Select
    - Details: accepting states and transition table (scrollable grid)
  - Center pane: Source + Outputs
    - Tabs: Source Editor | Tokens | Parse Tree | Translation
    - Buttons (toolbar): Lex, Parse, Semantics, Translate, Run All, Save Translation
  - Right pane: Automaton Visualization
    - DFA graph view: states as circles (accepting states double-ring), directed edges with input class labels

- Key views and responsibilities:
  - `AutomatonView`
    - Shows automaton list and selected automaton details
    - CRUD:
      - Insert: capture `id`, `name`, `numStates`, accepting set; open a transition editor; on OK → `AutomatonManager::insert`
      - Delete by ID → `AutomatonManager::removeById`
      - Select by ID → binds `SessionState::selectedAutomaton`
  - `AutomatonGraphView`
    - Renders DFA: simple force layout or circular layout
    - Edge labels: input classes (0..7) mapped to readable names (e.g., `LETTER`, `DIGIT`, ...)
  - `SourceEditorView`
    - Multiline text; on change invalidates downstream stages (tokens/AST/etc.)
  - `TokenTableView`
    - Columns: Index, Type, Lexeme, Line, Col
  - `ParseTreeView`
    - Interactive tree; expand/collapse; mirrors the ASCII output of `Parser::printAST`
  - `TranslationView`
    - Read-only text area with copy/save
  - `Toolbar`
    - Buttons dispatch to pipeline functions:
      - Lex → `lex(source, *selectedAutomaton, ...)`
      - Parse → `Parser(tokens).parseProgram()`
      - Semantics → `analyzeSemantics(ast, symbols)`
      - Translate → `translateToPython(ast, out)`
      - Run All → sequentially triggers all the above

- Session invalidation rules (mirrors CLI):
  - On source change: invalidate tokens, AST, semantic result, translation
  - On re-lex: invalidate AST, semantic result, translation
  - On re-parse: invalidate semantic result, translation

- Minimal UI class skeletons (names only):
  - `class UiApp;`
  - `class AutomatonView;`
  - `class AutomatonGraphView;`
  - `class SourceEditorView;`
  - `class TokenTableView;`
  - `class ParseTreeView;`
  - `class TranslationView;`
  - `struct UiState { SessionState session; /* selection indices, expanded nodes, etc. */ };`

- Integration points:
  - UI reads/writes `SessionState` already defined in `main.cpp` (or moved into a shared header if needed).
  - No backend changes required for functionality; only expose lightweight helper functions for UI (e.g., `automatonToString`, token iteration).

### 12B. Visualization Details
- DFA layout:
  - Circle nodes around a radius; node index = state id.
  - Double ring for accepting states (`isAccepting`).
  - Directed edges: `transitionTable[s][ic] = s2` drawn with arrow and label `InputClassToText(ic)`.
- Parse tree:
  - Tree control whose nodes are `NodeType` labels with selected `text` for identifiers, numbers, and `BIN_OP(op)`.
  - Edge labels: `left`, `right`, `ident`, `expr`, `stmt[i]` (matching `printAST`).

### 13. Error Handling
- Lexer:
  - Reports location and offending character near lex errors.
  - Bounded `TokenList`; reports overflow explicitly.
- Parser:
  - Clear “expected X at line Y, col Z” messages via `expect`.
- Semantics:
  - Reports number of errors; blocks translation if any exist.
- CLI:
  - Prints concise, actionable messages; never crashes on invalid order (e.g., parse before lex).

### 14. Extensibility
- Adding tokens:
  - Update input classes/DFA if new lexical categories are needed.
  - Add token types, single-char handlers or multi-char logic if necessary.
  - Update keyword mapping as needed.
- Adding grammar constructs:
  - Extend AST node types.
  - Update recursive‑descent functions and precedence rules.
  - Extend semantics and translator for new nodes.
- Multiple automatons:
  - `AutomatonManager` already supports multiple DFAs; add new initializers and selection UI if desired.
- New target languages:
  - Create a new translator module mapping AST to the target language.

### 15. Performance Considerations
- Intended for teaching-scale inputs.
- Lexer: linear in input length with constant‑time transitions.
- Parser: linear in token count for this grammar.
- Memory: bounded tokens; AST allocations per node.

### 16. Build & Run
- Build with MSVC or MinGW (see README).
- Run:
  - `.\tc_backend.exe example.src` preloads the sample.
  - Use the menu to run individual steps or the full pipeline.
  - For console entry, paste code and type `END` to finish.

### 17. Testing Notes
- Happy path: sample in `example.src` should lex/parse/semantically validate and translate.
- Negative tests:
  - Lex: insert invalid char → lex fails with message.
  - Parse: delete a `;` or `)` → parse fails.
  - Semantics: reference an undefined variable → semantic errors > 0.
- Regression: after changes to DFA/grammar, re-run the above to confirm no regressions.

### 18. Future Work
- Typed variables and basic type checking.
- Better error recovery in parser (synchronization on `;` or `}`).
- Scope and blocks; functions; constants.
- Multi-character operators, comments, and strings.
- Additional target backends (e.g., C#, JavaScript) via modular translators.

### 19. Requirements Traceability (from Project Brief)
- Automaton Implementation (4.1):
  - Data: `Automaton`, `AutomatonManager` (array of structs, insert/delete/search by ID).
  - Functions: `insert`, `removeById`, `findById`, `listIds`.
  - Visualization: `AutomatonGraphView` (GUI) + `automatonToString` (text).
- Lexical Analysis (4.2):
  - Input program → `lex(...)` with DFA; outputs `TokenList` with positions.
  - Tokens display: CLI `printTokens` and GUI `TokenTableView`.
- Grammar Definition and Parsing (4.3):
  - Grammar encoded in `Parser` functions; builds AST; `printAST` for derivation tree.
  - GUI `ParseTreeView` renders interactive tree.
- Semantic Analysis and Translation (4.4):
  - `analyzeSemantics` validates usage; `translateToPython` generates target code.
  - GUI `TranslationView` presents output with save.
- Visualization and GUI (4.5):
  - DFA graph, tokens table, parse tree, translation tab; toolbar to drive pipeline.
  - C++ GUI app (`UiApp`) hosts views; no backend changes required.

This design keeps the system modular, transparent, and extendable while making every stage demonstrable and functional in the interactive interface.