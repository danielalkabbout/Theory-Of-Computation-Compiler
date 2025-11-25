#ifndef LEXER_H
#define LEXER_H

// Protect against Windows headers or other headers defining conflicting macros
#ifdef type
#undef type
#endif
#ifdef Token
#undef Token
#endif
#ifdef TokenType
#undef TokenType
#endif

#include <string>
#include "automaton.h"

#define MAX_TOKENS 2048

enum TokenType {
	T_EOF_TOK = 0,
	T_IDENT,
	T_NUMBER,
	T_KW_LET,
	T_KW_PRINT,
	T_PLUS,
	T_MINUS,
	T_STAR,
	T_SLASH,
	T_EQUAL,
	T_LPAREN,
	T_RPAREN,
	T_SEMI,
	T_UNKNOWN
};

struct Token {
	TokenType tokType;
	std::string lexeme;
	int line;
	int col;
};

struct TokenList {
	Token data[MAX_TOKENS];
	int count;

	TokenList() : count(0) {}

	bool push(const Token& t) {
		if (count >= MAX_TOKENS) return false;
		data[count++] = t;
		return true;
	}

	const Token& at(int i) const {
		return data[i];
	}
};

bool lex(const std::string& source, const Automaton& a, TokenList& outTokens, std::string& errorMsg);
std::string tokenTypeToString(TokenType t);
void printTokens(const TokenList& list);

#endif // LEXER_H


