#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "automaton.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "translator.h"
using namespace std;

static string readAllStdin() {
	ostringstream oss;
	string line;
	while (getline(cin, line)) {
		oss << line << "\n";
	}
	return oss.str();
}

static bool readFile(const string& path, string& out) {
	ifstream ifs(path.c_str(), ios::in | ios::binary);
	if (!ifs) return false;
	ostringstream ss;
	ss << ifs.rdbuf();
	out = ss.str();
	return true;
}

// ------------ Interactive Session Support ------------
struct SessionState {
	AutomatonManager automatonManager;
	Automaton* selectedAutomaton = nullptr;

	string sourceCode;

	TokenList tokens;
	bool tokensReady = false;
	string lexerError;

	Parser* parserPtr = nullptr;
	ASTNode* astRoot = nullptr;
	bool parsedOk = false;

	SymbolTable symbolTable;
	SemanticResult semResult{};
	bool semanticsDone = false;

	string translatedPython;
	bool translationDone = false;
};

static void invalidateFromLex(SessionState& s) {
	s.tokensReady = false;
	s.lexerError.clear();
	if (s.parserPtr) { delete s.parserPtr; s.parserPtr = nullptr; }
	if (s.astRoot) { Parser::freeAST(s.astRoot); s.astRoot = nullptr; }
	s.parsedOk = false;
	s.symbolTable = SymbolTable{};
	s.semResult = SemanticResult{};
	s.semanticsDone = false;
	s.translatedPython.clear();
	s.translationDone = false;
}

static void invalidateFromParse(SessionState& s) {
	if (s.parserPtr) { delete s.parserPtr; s.parserPtr = nullptr; }
	if (s.astRoot) { Parser::freeAST(s.astRoot); s.astRoot = nullptr; }
	s.parsedOk = false;
	s.symbolTable = SymbolTable{};
	s.semResult = SemanticResult{};
	s.semanticsDone = false;
	s.translatedPython.clear();
	s.translationDone = false;
}

static int readIntChoice(const string& prompt, int minVal, int maxVal) {
	while (true) {
		cout << prompt;
		string line;
		if (!getline(cin, line)) return minVal;
		if (line.size() == 0) continue;
		istringstream iss(line);
		int v = 0;
		if (iss >> v && v >= minVal && v <= maxVal) return v;
		cout << "Please enter a number in [" << minVal << "-" << maxVal << "].\n";
	}
}

static string readLinePrompt(const string& prompt) {
	cout << prompt;
	string line;
	getline(cin, line);
	return line;
}

static string readUntilSentinel(const string& sentinel) {
	cout << "(Finish input by typing " << sentinel << " on its own line)\n";
	string acc;
	string line;
	while (true) {
		if (!getline(cin, line)) {
			cout << "Input ended (EOF/Ctrl+Z). Finishing manual entry.\n";
			cin.clear();
			break;
		}
		if (line == sentinel) break;
		acc += line;
		acc += "\n";
	}
	return acc;
}

static void printMenu() {
	cout << "\n========== Main Menu ==========\n";
	cout << "1) Run full pipeline\n";
	cout << "2) Load source from file\n";
	cout << "3) Enter source via console\n";
	cout << "4) Lexical analysis\n";
	cout << "5) Parse program (build AST)\n";
	cout << "6) Semantic analysis\n";
	cout << "7) Translate to Python\n";
	cout << "8) Show tokens\n";
	cout << "9) Show parse tree\n";
	cout << "10) Show translated Python\n";
	cout << "11) Save translated Python\n";
	cout << "0) Exit\n";
}

static void loadSourceFromFile(SessionState& s) {
	string path = readLinePrompt("Enter file path: ");
	string data;
	if (!readFile(path, data)) {
		cout << "Failed to read file: " << path << "\n";
		return;
	}
	s.sourceCode = data;
	cout << "Loaded " << s.sourceCode.size() << " bytes.\n";
	invalidateFromLex(s);
}

static void enterSourceConsole(SessionState& s) {
	cout << "Enter program. Type END on a line by itself to finish:\n";
	s.sourceCode = readUntilSentinel("END");
	cout << "Captured " << s.sourceCode.size() << " bytes.\n";
	invalidateFromLex(s);
}

static bool doLex(SessionState& s) {
	if (!s.selectedAutomaton) {
		cout << "Select an automaton first.\n";
		return false;
	}
	if (s.sourceCode.empty()) {
		cout << "Load or enter source first.\n";
		return false;
	}
	s.tokens = TokenList{};
	s.lexerError.clear();
	bool ok = lex(s.sourceCode, *s.selectedAutomaton, s.tokens, s.lexerError);
	if (!ok) {
		cout << "Lexical analysis failed: " << s.lexerError << "\n";
		s.tokensReady = false;
		return false;
	}
	s.tokensReady = true;
	// downstream invalidation
	invalidateFromParse(s);
	cout << "Lexical analysis OK.\n";
	cout << "\n[Tokens]\n";
	printTokens(s.tokens);
	return true;
}

static void showTokens(SessionState& s) {
	if (!s.tokensReady) {
		cout << "(no tokens; run lexical analysis first)\n";
		return;
	}
	cout << "\n[Tokens]\n";
	printTokens(s.tokens);
}

static bool doParse(SessionState& s) {
	if (!s.tokensReady) {
		cout << "No tokens. Run lexical analysis first.\n";
		return false;
	}
	if (s.parserPtr) { delete s.parserPtr; s.parserPtr = nullptr; }
	if (s.astRoot) { Parser::freeAST(s.astRoot); s.astRoot = nullptr; }
	s.parserPtr = new Parser(s.tokens);
	s.astRoot = s.parserPtr->parseProgram();
	if (!s.astRoot) {
		cout << "Parsing failed.\n";
		s.parsedOk = false;
		return false;
	}
	s.parsedOk = true;
	cout << "Parsing OK.\n";
	cout << "\n[Derivation/Parse Tree]\n";
	s.parserPtr->printAST(s.astRoot);
	// downstream invalidation
	s.symbolTable = SymbolTable{};
	s.semResult = SemanticResult{};
	s.semanticsDone = false;
	s.translatedPython.clear();
	s.translationDone = false;
	return true;
}

static void showAST(SessionState& s) {
	if (!s.parsedOk || !s.parserPtr || !s.astRoot) {
		cout << "(no AST; run parsing first)\n";
		return;
	}
	cout << "\n[Derivation/Parse Tree]\n";
	s.parserPtr->printAST(s.astRoot);
}

static bool doSemantics(SessionState& s) {
	if (!s.parsedOk || !s.astRoot) {
		cout << "No AST. Run parsing first.\n";
		return false;
	}
	s.symbolTable = SymbolTable{};
	s.semResult = analyzeSemantics(s.astRoot, s.symbolTable);
	s.semanticsDone = true;
	cout << "\n[Semantic Analysis]\n";
	if (!s.semResult.ok) {
		cout << "Errors: " << s.semResult.errorCount << "\n";
	} else {
		cout << "OK (no errors)\n";
	}
	// downstream invalidation
	s.translatedPython.clear();
	s.translationDone = false;
	return s.semResult.ok;
}

static bool doTranslate(SessionState& s) {
	if (!s.parsedOk || !s.astRoot) {
		cout << "No AST. Run parsing first.\n";
		return false;
	}
	if (s.semanticsDone && !s.semResult.ok) {
		cout << "Semantic errors present. Fix them before translation.\n";
		return false;
	}
	s.translatedPython.clear();
	bool ok = translateToPython(s.astRoot, s.translatedPython);
	if (!ok) {
		cout << "Translation failed.\n";
		s.translationDone = false;
		return false;
	}
	s.translationDone = true;
	cout << "Translation OK.\n";
	cout << "\n[Translation Output - Python]\n";
	cout << s.translatedPython << "\n";
	return true;
}

static void showTranslation(SessionState& s) {
	if (!s.translationDone) {
		cout << "(no translation; run translation first)\n";
		return;
	}
	cout << "\n[Translation Output - Python]\n";
	cout << s.translatedPython << "\n";
}

static void runFullPipeline(SessionState& s) {
	if (!s.selectedAutomaton) {
		cout << "No automaton selected. Selecting default (1) if available.\n";
		s.selectedAutomaton = s.automatonManager.findById(1);
	}
	if (!s.selectedAutomaton) {
		cout << "Fatal: no automaton available.\n";
		return;
	}
	if (s.sourceCode.empty()) {
		cout << "No source loaded. Please load or enter source first.\n";
		return;
	}
	if (!doLex(s)) return;
	if (!doParse(s)) return;
	if (!doSemantics(s)) return;
	(void)doTranslate(s);
}

static void saveTranslation(SessionState& s) {
	if (!s.translationDone) {
		cout << "No translation to save. Run translation first.\n";
		return;
	}
	string path = readLinePrompt("Enter output file path: ");
	ofstream ofs(path.c_str(), ios::out | ios::binary);
	if (!ofs) {
		cout << "Failed to open for write: " << path << "\n";
		return;
	}
	ofs << s.translatedPython;
	cout << "Saved translation to " << path << "\n";
}

int main(int argc, char** argv) {
	cout << "Theory of Computation Project - C++ Backend\n";

	// 1) Automaton setup and manager
	SessionState session;
	Automaton lexDFA;
	initLexerAutomaton(lexDFA);
	session.automatonManager.insert(lexDFA);
	session.selectedAutomaton = session.automatonManager.findById(1);

	// Optional: preload source from argv[1]
	if (argc >= 2) {
		string tmp;
		if (readFile(argv[1], tmp)) {
			session.sourceCode = tmp;
			cout << "Preloaded source from file: " << argv[1] << " (" << session.sourceCode.size() << " bytes)\n";
		} else {
			cout << "Warning: Failed to read file: " << argv[1] << "\n";
		}
	}

	// Interactive loop
	while (true) {
		printMenu();
		int choice = readIntChoice("Select option: ", 0, 11);
		switch (choice) {
		case 0:
			if (session.parserPtr) { delete session.parserPtr; session.parserPtr = nullptr; }
			if (session.astRoot) { Parser::freeAST(session.astRoot); session.astRoot = nullptr; }
			cout << "Bye.\n";
			return 0;
		case 1: runFullPipeline(session); break;
		case 2: loadSourceFromFile(session); break;
		case 3: enterSourceConsole(session); break;
		case 4: doLex(session); break;
		case 5: doParse(session); break;
		case 6: doSemantics(session); break;
		case 7: doTranslate(session); break;
		case 8: showTokens(session); break;
		case 9: showAST(session); break;
		case 10: showTranslation(session); break;
		case 11: saveTranslation(session); break;
		default:
			break;
		}
	}
}


