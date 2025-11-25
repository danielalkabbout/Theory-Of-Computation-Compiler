#include "lexer.h"
#ifndef PARSER_H
#define PARSER_H

// Neutralize possible macro collisions from platform headers
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

enum NodeType {
	N_PROGRAM = 0,
	N_STMT_LIST,
	N_LET_STMT,
	N_PRINT_STMT,
	N_BIN_OP,
	N_IDENT,
	N_NUMBER
};

struct ASTNode {
	NodeType type;
	std::string text; // for IDENT/NUMBER or operator symbol
	int line;
	int col;
	ASTNode* left;
	ASTNode* right;
	ASTNode* next; // for lists (e.g., statement list)

	ASTNode(NodeType t) : type(t), text(""), line(0), col(0), left(nullptr), right(nullptr), next(nullptr) {}
};

class Parser {
public:
	Parser(const TokenList& tokens);
	ASTNode* parseProgram();

	void printAST(ASTNode* node);
	static void freeAST(ASTNode* node);

private:
	const TokenList& tokens;
	int pos;

	const Token& peek();
	const Token& consume();
	bool match(TokenType t);
	bool expect(TokenType t, const char* what, std::string& err);

	ASTNode* parseStmtList(std::string& err);
	ASTNode* parseStmt(std::string& err);
	ASTNode* parseExpr(std::string& err);
	ASTNode* parseTerm(std::string& err);
	ASTNode* parseFactor(std::string& err);

	void printASTRec(ASTNode* node, const std::string& prefix, bool isTail);
};

#endif // PARSER_H

