#include "parser.h"
#include <cstdio>
#include <functional>
using namespace std;

Parser::Parser(const TokenList& tokensRef) : tokens(tokensRef), pos(0) {}

const Token& Parser::peek() {
	static Token sEOF = { T_EOF_TOK, "", 0, 0 };
	if (tokens.count <= 0) return sEOF;
	if (pos >= tokens.count) return tokens.data[tokens.count - 1];
	return tokens.data[pos];
}

const Token& Parser::consume() {
	static Token sEOF = { T_EOF_TOK, "", 0, 0 };
	if (tokens.count <= 0) return sEOF;
	if (pos >= tokens.count) return tokens.data[tokens.count - 1];
	return tokens.data[pos++];
}

bool Parser::match(TokenType t) {
	if (peek().tokType == t) { consume(); return true; }
	return false;
}

bool Parser::expect(TokenType t, const char* what, string& err) {
	if (!match(t)) {
		char buf[256];
		snprintf(buf, sizeof(buf), "Expected %s at line %d, col %d", what, peek().line, peek().col);
		err = buf;
		return false;
	}
	return true;
}

ASTNode* Parser::parseProgram() {
	string err;
	ASTNode* list = parseStmtList(err);
	if (list == nullptr) {
		printf("Parse error: %s\n", err.c_str());
		return nullptr;
	}
	// Expect EOF
	if (peek().tokType != T_EOF_TOK) {
		printf("Parse error: Unexpected tokens after program end at line %d, col %d\n", peek().line, peek().col);
		return nullptr;
	}
	ASTNode* prog = new ASTNode(N_PROGRAM);
	prog->left = list;
	return prog;
}

ASTNode* Parser::parseStmtList(string& err) {
	ASTNode* head = nullptr;
	ASTNode* tail = nullptr;

	while (true) {
		TokenType tt = peek().tokType;
		if (tt == T_KW_LET || tt == T_KW_PRINT) {
			ASTNode* stmt = parseStmt(err);
			if (!stmt) {
				// propagate error
				// clean partial list
				while (head) { ASTNode* n = head->next; freeAST(head); head = n; }
				return nullptr;
			}
			if (!head) { head = tail = stmt; }
			else { tail->next = stmt; tail = stmt; }
		} else {
			break; // epsilon (empty) allowed
		}
	}

	ASTNode* list = new ASTNode(N_STMT_LIST);
	list->left = head; // chain via next
	return list;
}

ASTNode* Parser::parseStmt(string& err) {
	if (match(T_KW_LET)) {
		if (peek().tokType != T_IDENT) {
			err = "Expected identifier after 'let'";
			return nullptr;
		}
		ASTNode* ident = new ASTNode(N_IDENT);
		{
			const Token tok = consume();
			ident->text = tok.lexeme;
			ident->line = tok.line;
			ident->col = tok.col;
		}
		if (!expect(T_EQUAL, "'='", err)) { freeAST(ident); return nullptr; }
		ASTNode* expr = parseExpr(err);
		if (!expr) { freeAST(ident); return nullptr; }
		if (!expect(T_SEMI, "';'", err)) { freeAST(ident); freeAST(expr); return nullptr; }
		ASTNode* letNode = new ASTNode(N_LET_STMT);
		letNode->left = ident;
		letNode->right = expr;
		return letNode;
	}

	if (match(T_KW_PRINT)) {
		ASTNode* expr = parseExpr(err);
		if (!expr) return nullptr;
		if (!expect(T_SEMI, "';'", err)) { freeAST(expr); return nullptr; }
		ASTNode* prNode = new ASTNode(N_PRINT_STMT);
		prNode->left = expr;
		return prNode;
	}

	err = "Unknown statement";
	return nullptr;
}

ASTNode* Parser::parseExpr(string& err) {
	ASTNode* left = parseTerm(err);
	if (!left) return nullptr;
	while (true) {
		TokenType tt = peek().tokType;
		if (tt == T_PLUS || tt == T_MINUS) {
			Token opTok = consume();
			string op = opTok.lexeme;
			ASTNode* right = parseTerm(err);
			if (!right) { freeAST(left); return nullptr; }
			ASTNode* bin = new ASTNode(N_BIN_OP);
			bin->text = op;
			bin->line = opTok.line;
			bin->col = opTok.col;
			bin->left = left;
			bin->right = right;
			left = bin;
		} else {
			break;
		}
	}
	return left;
}

ASTNode* Parser::parseTerm(string& err) {
	ASTNode* left = parseFactor(err);
	if (!left) return nullptr;
	while (true) {
		TokenType tt = peek().tokType;
		if (tt == T_STAR || tt == T_SLASH) {
			Token opTok = consume();
			string op = opTok.lexeme;
			ASTNode* right = parseFactor(err);
			if (!right) { freeAST(left); return nullptr; }
			ASTNode* bin = new ASTNode(N_BIN_OP);
			bin->text = op;
			bin->line = opTok.line;
			bin->col = opTok.col;
			bin->left = left;
			bin->right = right;
			left = bin;
		} else {
			break;
		}
	}
	return left;
}

ASTNode* Parser::parseFactor(string& err) {
	if (peek().tokType == T_NUMBER) {
		ASTNode* num = new ASTNode(N_NUMBER);
		const Token tok = consume();
		num->text = tok.lexeme;
		num->line = tok.line;
		num->col = tok.col;
		return num;
	}
	if (peek().tokType == T_IDENT) {
		ASTNode* id = new ASTNode(N_IDENT);
		const Token tok = consume();
		id->text = tok.lexeme;
		id->line = tok.line;
		id->col = tok.col;
		return id;
	}
	if (match(T_LPAREN)) {
		ASTNode* e = parseExpr(err);
		if (!e) return nullptr;
		if (!expect(T_RPAREN, "')'", err)) { freeAST(e); return nullptr; }
		return e;
	}
	err = "Expected number, identifier, or '('";
	return nullptr;
}

void Parser::printAST(ASTNode* node) {
	printASTRec(node, "", true);
}

void Parser::printASTRec(ASTNode* node, const string& prefix, bool isTail) {
	if (!node) return;

	// helper to print with an edge label
	std::function<void(ASTNode*, const string&, bool, const string&)> printWithLabel;
	printWithLabel = [&](ASTNode* n, const string& pref, bool tail, const string& edgeLabel) {
		if (!n) return;
		string label;
		switch (n->type) {
		case N_PROGRAM: label = "PROGRAM"; break;
		case N_STMT_LIST: label = "STMT_LIST"; break;
		case N_LET_STMT: label = "LET"; break;
		case N_PRINT_STMT: label = "PRINT"; break;
		case N_BIN_OP: label = "BIN_OP(" + n->text + ")"; break;
		case N_IDENT: label = "IDENT(" + n->text + ")"; break;
		case N_NUMBER: label = "NUMBER(" + n->text + ")"; break;
		}
		const char* branch = tail ? "`-- " : "|-- ";
		if (!edgeLabel.empty()) {
			printf("%s%s%s: %s\n", pref.c_str(), branch, edgeLabel.c_str(), label.c_str());
		} else {
			printf("%s%s%s\n", pref.c_str(), branch, label.c_str());
		}

		string childPrefix = pref + (tail ? "    " : "|   ");

		// Structured children with clearer edge labels
		if (n->type == N_PROGRAM) {
			if (n->left) printWithLabel(n->left, childPrefix, true, "body");
			return;
		}
		if (n->type == N_LET_STMT) {
			// left: ident, right: expr
			if (n->left && n->right) {
				printWithLabel(n->left, childPrefix, false, "ident");
				printWithLabel(n->right, childPrefix, true, "expr");
			} else if (n->left) {
				printWithLabel(n->left, childPrefix, true, "ident");
			} else if (n->right) {
				printWithLabel(n->right, childPrefix, true, "expr");
			}
			return;
		}
		if (n->type == N_PRINT_STMT) {
			if (n->left) printWithLabel(n->left, childPrefix, true, "expr");
			return;
		}
		if (n->type == N_BIN_OP) {
			if (n->left && n->right) {
				printWithLabel(n->left, childPrefix, false, "left");
				printWithLabel(n->right, childPrefix, true, "right");
			} else if (n->left) {
				printWithLabel(n->left, childPrefix, true, "left");
			} else if (n->right) {
				printWithLabel(n->right, childPrefix, true, "right");
			}
			return;
		}
		if (n->type == N_STMT_LIST) {
			ASTNode* cur = n->left;
			int idx = 0;
			while (cur) {
				bool last = (cur->next == nullptr);
				char edge[32]; snprintf(edge, sizeof(edge), "stmt[%d]", idx++);
				printWithLabel(cur, childPrefix, last, edge);
				cur = cur->next;
			}
			return;
		}
		// generic binary fallback
		if (n->left && n->right) {
			printWithLabel(n->left, childPrefix, false, "left");
			printWithLabel(n->right, childPrefix, true, "right");
		} else if (n->left) {
			printWithLabel(n->left, childPrefix, true, "left");
		} else if (n->right) {
			printWithLabel(n->right, childPrefix, true, "right");
		}
	};

	// Root call with explicit 'root' label for clarity
	printWithLabel(node, prefix, isTail, "root");
}

void Parser::freeAST(ASTNode* node) {
	if (!node) return;
	// Free list via next for stmt_list
	if (node->type == N_STMT_LIST) {
		ASTNode* cur = node->left;
		while (cur) {
			ASTNode* nxt = cur->next;
			freeAST(cur);
			cur = nxt;
		}
		delete node;
		return;
	}
	freeAST(node->left);
	freeAST(node->right);
	delete node;
}


