#include "semantic.h"
#include <cstdio>
using namespace std;

static void addError(SemanticResult& res, int line, int col, const string& msg) {
	if (res.errorCount >= MAX_SEM_ERRORS) return;
	res.ok = false;
	res.errors[res.errorCount].line = line;
	res.errors[res.errorCount].col = col;
	res.errors[res.errorCount].message = msg;
	res.errorCount++;
}

static void addWarning(SemanticResult& res, int line, int col, const string& msg) {
	if (res.warningCount >= MAX_SEM_WARNINGS) return;
	res.warnings[res.warningCount].line = line;
	res.warnings[res.warningCount].col = col;
	res.warnings[res.warningCount].message = msg;
	res.warningCount++;
}

static void analyzeExpr(ASTNode* node, SymbolTable& symbols, SemanticResult& res) {
	if (!node) return;
	switch (node->type) {
	case N_NUMBER:
		return;
	case N_IDENT:
		if (!symbols.isDefined(node->text)) {
			addError(res, node->line, node->col, "use of undefined variable '" + node->text + "'");
		} else {
			// mark as used
			symbols.markUsed(node->text);
		}
		return;
	case N_BIN_OP:
		analyzeExpr(node->left, symbols, res);
		analyzeExpr(node->right, symbols, res);
		return;
	default:
		// nested expr
		analyzeExpr(node->left, symbols, res);
		analyzeExpr(node->right, symbols, res);
		return;
	}
}

static void analyzeStmt(ASTNode* node, SymbolTable& symbols, SemanticResult& res) {
	if (!node) return;
	if (node->type == N_LET_STMT) {
		if (node->left && node->left->type == N_IDENT) {
			// define var
			(void)symbols.define(node->left->text, node->left->line, node->left->col);
		}
		analyzeExpr(node->right, symbols, res);
	} else if (node->type == N_PRINT_STMT) {
		analyzeExpr(node->left, symbols, res);
	}
}

SemanticResult analyzeSemantics(ASTNode* root, SymbolTable& symbols) {
	SemanticResult res;
	if (!root) { res.ok = false; return res; }
	// Traverse statement list
	if (root->type != N_PROGRAM || !root->left) {
		addError(res, 0, 0, "malformed AST");
		return res;
	}
	ASTNode* stmtList = root->left;
	ASTNode* cur = stmtList->left;
	while (cur) {
		analyzeStmt(cur, symbols, res);
		cur = cur->next;
	}
	// Unused variable warnings
	for (int i = 0; i < symbols.count; ++i) {
		if (!symbols.used[i]) {
			addWarning(res, symbols.declLine[i], symbols.declCol[i], "unused variable '" + symbols.names[i] + "'");
		}
	}
	return res;
}


