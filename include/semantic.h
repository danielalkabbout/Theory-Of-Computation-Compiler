#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "parser.h"
#include <string>

#define MAX_SYMBOLS 256
#define MAX_SEM_ERRORS 256

struct SemanticError {
	int line;
	int col;
	std::string message;
};

struct SymbolTable {
	std::string names[MAX_SYMBOLS];
	int count;

	SymbolTable() : count(0) {}

	bool define(const std::string& name) {
		if (count >= MAX_SYMBOLS) return false;
		if (isDefined(name)) return true;
		names[count++] = name;
		return true;
	}

	bool isDefined(const std::string& name) const {
		for (int i = 0; i < count; ++i) {
			if (names[i] == name) return true;
		}
		return false;
	}
};

struct SemanticResult {
	bool ok;
	int errorCount;
	SemanticError errors[MAX_SEM_ERRORS];

	SemanticResult() : ok(true), errorCount(0) {}
};

SemanticResult analyzeSemantics(ASTNode* root, SymbolTable& symbols);

#endif // SEMANTIC_H


