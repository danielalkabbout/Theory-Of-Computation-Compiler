#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "automaton.h"
#include "nfa.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "translator.h"

using std::string;

static int g_pass = 0;
static int g_fail = 0;

static void report(bool ok, const char* name) {
	if (ok) { ++g_pass; std::printf("[PASS] %s\n", name); }
	else    { ++g_fail; std::printf("[FAIL] %s\n", name); }
}

// Helpers
static ASTNode* parseSource(const string& src, TokenList& outTokens) {
	Automaton dfa; initLexerAutomaton(dfa);
	string err;
	if (!lex(src, dfa, outTokens, err)) {
		std::printf("Lex error: %s\n", err.c_str());
		return nullptr;
	}
	Parser p(outTokens);
	return p.parseProgram();
}

int main() {
	// --- automaton.h ---
	{
		report(classifyChar('a') == IC_LETTER, "classifyChar: letter");
		report(classifyChar(']') == IC_DELIM,  "classifyChar: delim bracket");
	}
	{
		Automaton a; initLexerAutomaton(a);
		report(a.numStates == 6, "initLexerAutomaton: numStates==6");
		report(a.acceptingStates[1] == true, "initLexerAutomaton: S_IDENT accepting");
	}
	{
		Automaton a; initLexerAutomaton(a);
		report(nextState(a, 0, IC_LETTER) == 1, "nextState: start + LETTER -> IDENT");
		report(nextState(a, 2, IC_DIGIT)  == 2, "nextState: NUMBER + DIGIT -> NUMBER");
	}
	{
		Automaton a; initLexerAutomaton(a);
		report(isAccepting(a, 1), "isAccepting: true on IDENT");
		report(!isAccepting(a, 0), "isAccepting: false on START");
	}
	{
		Automaton a; initLexerAutomaton(a);
		string s = automatonToString(a);
		report(s.find("Automaton ID:") != string::npos, "automatonToString: header");
		report(s.find("States: 6") != string::npos, "automatonToString: states");
	}

	// --- nfa.h ---
	{
		Automaton a; initLexerAutomaton(a);
		NFA nfa; buildNFAFromDFA(a, nfa);
		report(nfa.numStates == a.numStates, "buildNFAFromDFA: state count");
		report(nfa.trans[0][IC_LETTER].count == 1, "buildNFAFromDFA: has 0--LETTER-->1");
	}
	{
		NFA nfa; buildLexerNFA(nfa);
		int accepting = 0; for (int s = 0; s < nfa.numStates; ++s) if (nfa.acceptingStates[s]) ++accepting;
		report(nfa.numStates == 8, "buildLexerNFA: numStates==8");
		report(accepting == 5, "buildLexerNFA: five accepting states");
	}
	{
		NFA nfa; buildLexerNFA(nfa);
		Automaton dfa;
		bool ok = determinizeNFA(nfa, dfa);
		report(ok, "determinizeNFA: returns true");
		report(dfa.numStates > 0, "determinizeNFA: produced states");
	}

	// --- lexer.h ---
	{
		Automaton a; initLexerAutomaton(a);
		TokenList toks; string err;
		bool ok = lex("let x=10;", a, toks, err);
		bool kw = ok && toks.count >= 1 && toks.at(0).tokType == T_KW_LET;
		bool num = ok && toks.count >= 4 && toks.at(3).tokType == T_NUMBER;
		report(ok,  "lex: simple program ok");
		report(kw && num, "lex: KW_LET and NUMBER present");
	}
	{
		report(tokenTypeToString(T_PLUS) == "PLUS", "tokenTypeToString: PLUS");
		report(tokenTypeToString(T_RPAREN) == "RPAREN", "tokenTypeToString: RPAREN");
	}

	// --- parser.h (Parser::parseProgram) ---
	{
		TokenList toks; ASTNode* root = parseSource("let x=1; print(x);", toks);
		bool ok = (root != nullptr);
		report(ok, "parseProgram: valid program returns AST");
		if (ok) Parser::freeAST(root);
	}
	{
		TokenList toks; ASTNode* root = parseSource("let = 1;", toks);
		report(root == nullptr, "parseProgram: invalid program fails");
		if (root) { Parser::freeAST(root); }
	}

	// --- semantic.h ---
	{
		TokenList toks; ASTNode* root = parseSource("let x=1; print(x);", toks);
		SymbolTable sym; auto res = analyzeSemantics(root, sym);
		report(res.ok, "analyzeSemantics: valid program ok");
		report(res.errorCount == 0, "analyzeSemantics: no errors");
		if (root) Parser::freeAST(root);
	}
	{
		TokenList toks; ASTNode* root = parseSource("print(y);", toks);
		SymbolTable sym; auto res = analyzeSemantics(root, sym);
		report(!res.ok, "analyzeSemantics: undefined ident fails");
		report(res.errorCount > 0, "analyzeSemantics: has errors");
		if (root) Parser::freeAST(root);
	}

	// --- translator.h ---
	{
		TokenList toks; ASTNode* root = parseSource("let x=1; print(x);", toks);
		string out; bool ok = translateToPython(root, out);
		bool a = out.find("x = 1") != string::npos;
		bool b = out.find("print(x)") != string::npos;
		report(ok, "translateToPython: returns true");
		report(a && b, "translateToPython: contains assignments and print");
		if (root) Parser::freeAST(root);
	}
	{
		TokenList toks; ASTNode* root = parseSource("let c=2; let y = (c + 10) * 2; print(y);", toks);
		string out; bool ok = translateToPython(root, out);
		bool a = out.find("c = 2") != string::npos;
		bool b = out.find("print(y)") != string::npos;
		report(ok, "translateToPython: returns true (arithmetic)");
		report(a && b, "translateToPython: contains c=2 and print(y)");
		if (root) Parser::freeAST(root);
	}

	std::printf("\nSummary: %d passed, %d failed\n", g_pass, g_fail);
	return g_fail == 0 ? 0 : 1;
}


