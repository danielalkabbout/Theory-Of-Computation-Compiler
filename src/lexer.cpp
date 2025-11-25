#include "lexer.h"
#include <cstdio>
using namespace std;

static bool isKeyword(const string& s, TokenType& out) {
	if (s == "let") { out = T_KW_LET; return true; }
	if (s == "print") { out = T_KW_PRINT; return true; }
	return false;
}

static TokenType singleCharTokenType(char c) {
	switch (c) {
	case '+': return T_PLUS;
	case '-': return T_MINUS;
	case '*': return T_STAR;
	case '/': return T_SLASH;
	case '=': return T_EQUAL;
	case '(': return T_LPAREN;
	case ')': return T_RPAREN;
	case ';': return T_SEMI;
	default: return T_UNKNOWN;
	}
}

bool lex(const string& source, const Automaton& a, TokenList& outTokens, string& errorMsg) {
	int i = 0;
	int n = (int)source.size();
	int line = 1;
	int col = 1;

	while (i < n) {
		int startI = i;
		int startLine = line;
		int startCol = col;

		int state = 0; // start state
		int lastAccept = -1;
		int lastAcceptIndex = -1;

		// Try to consume as much as DFA allows and track last accepting position
		while (i < n) {
			int ic = classifyChar(source[i]);
			int ns = nextState(a, state, ic);
			if (ns == -1) break;
			state = ns;
			if (isAccepting(a, state)) {
				lastAccept = state;
				lastAcceptIndex = i;
			}
			// advance tentative
			if (source[i] == '\n') { line++; col = 1; } else { col++; }
			i++;
		}

		if (lastAccept == -1) {
			char buf[128];
			snprintf(buf, sizeof(buf), "Lex error at line %d, col %d near '%c'", startLine, startCol, (startI < n ? source[startI] : '?'));
			errorMsg = buf;
			return false;
		}

		// Roll back to last accepting position
		int consumeEnd = lastAcceptIndex + 1;
		// Fix line/col to reflect rollback
		// Recompute line/col from startI to consumeEnd
		line = startLine;
		col = startCol;
		for (int k = startI; k < consumeEnd; ++k) {
			if (source[k] == '\n') { line++; col = 1; } else { col++; }
		}

		string lexeme = source.substr(startI, consumeEnd - startI);
		// Move i to consumeEnd
		i = consumeEnd;

		// Map accepting state to token type
		Token tok;
		tok.line = startLine;
		tok.col = startCol;
		tok.lexeme = lexeme;

		// Accepting states are: IDENT, NUMBER, WS, OP, DELIM
		// Decide token by inspecting characters
		int firstClass = classifyChar(lexeme[0]);
		if (firstClass == IC_WHITESPACE) {
			// Skip whitespace
			continue;
		}
		if (firstClass == IC_LETTER || firstClass == IC_UNDERSCORE) {
			TokenType kw;
			if (isKeyword(lexeme, kw)) tok.tokType = kw;
			else tok.tokType = T_IDENT;
		} else if (firstClass == IC_DIGIT) {
			tok.tokType = T_NUMBER;
		} else if (firstClass == IC_OPERATOR) {
			tok.tokType = singleCharTokenType(lexeme[0]);
		} else if (firstClass == IC_DELIM) {
			tok.tokType = singleCharTokenType(lexeme[0]);
		} else {
			tok.tokType = T_UNKNOWN;
		}

		if (!outTokens.push(tok)) {
			errorMsg = "Too many tokens (capacity exceeded).";
			return false;
		}
	}

	Token eofTok;
	eofTok.tokType = T_EOF_TOK;
	eofTok.lexeme = "";
	eofTok.line = line;
	eofTok.col = col;
	outTokens.push(eofTok);
	return true;
}

string tokenTypeToString(TokenType t) {
	switch (t) {
	case T_EOF_TOK: return "EOF";
	case T_IDENT: return "IDENT";
	case T_NUMBER: return "NUMBER";
	case T_KW_LET: return "KW_LET";
	case T_KW_PRINT: return "KW_PRINT";
	case T_PLUS: return "PLUS";
	case T_MINUS: return "MINUS";
	case T_STAR: return "STAR";
	case T_SLASH: return "SLASH";
	case T_EQUAL: return "EQUAL";
	case T_LPAREN: return "LPAREN";
	case T_RPAREN: return "RPAREN";
	case T_SEMI: return "SEMI";
	default: return "UNKNOWN";
	}
}

void printTokens(const TokenList& list) {
	for (int i = 0; i < list.count; ++i) {
		const Token& t = list.at(i);
		printf("%-10s  '%s'  (line %d, col %d)\n", tokenTypeToString(t.tokType).c_str(), t.lexeme.c_str(), t.line, t.col);
	}
}


