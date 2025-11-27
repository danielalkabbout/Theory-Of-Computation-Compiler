#include "translator.h"
using namespace std;

static void emitExpr(ASTNode* node, string& out) {
	if (!node) return;
	switch (node->type) {
	case N_NUMBER:
	case N_IDENT:
		out += node->text;
		return;
	case N_BIN_OP:
		out += "(";
		emitExpr(node->left, out);
		out += " ";
		out += node->text;
		out += " ";
		emitExpr(node->right, out);
		out += ")";
		return;
	default:
		emitExpr(node->left, out);
		emitExpr(node->right, out);
		return;
	}
}

static void emitStmt(ASTNode* node, string& out) {
	if (!node) return;
	if (node->type == N_LET_STMT) {
		// let x = expr;  -> x = expr
		if (node->left && node->left->type == N_IDENT) {
			out += node->left->text;
			out += " = ";
			emitExpr(node->right, out);
			out += "\n";
		}
	} else if (node->type == N_PRINT_STMT) {
		out += "print(";
		emitExpr(node->left, out);
		out += ")\n";
	}
}

bool translateToPython(ASTNode* root, string& outCode) {
	outCode.clear();
	if (!root || root->type != N_PROGRAM || !root->left) return false;
	ASTNode* stmtList = root->left;
	ASTNode* cur = stmtList->left;
	while (cur) {
		emitStmt(cur, outCode);
		// Add a blank line between statements for readability
		outCode += "\n";
		cur = cur->next;
	}
	return true;
}


