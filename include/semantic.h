#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "parser.h"
#include <string>

#define MAX_SYMBOLS 256
#define MAX_SEM_ERRORS 256
#define MAX_SEM_WARNINGS 256

struct SemanticError {
	int line;
	int col;
	std::string message;
};

struct SymbolTable {
	std::string names[MAX_SYMBOLS];
	int declLine[MAX_SYMBOLS];
	int declCol[MAX_SYMBOLS];
	bool used[MAX_SYMBOLS];
	int count;

	SymbolTable() : count(0) {}

	bool define(const std::string& name, int line = 0, int col = 0) {
		if (count >= MAX_SYMBOLS) return false;
		if (isDefined(name)) return true;
		names[count] = name;
		declLine[count] = line;
		declCol[count] = col;
		used[count] = false;
		count++;
		return true;
	}

	bool isDefined(const std::string& name) const {
		for (int i = 0; i < count; ++i) {
			if (names[i] == name) return true;
		}
		return false;
	}

	void markUsed(const std::string& name) {
		for (int i = 0; i < count; ++i) {
			if (names[i] == name) { used[i] = true; return; }
		}
	}
};

struct SemanticResult {
	bool ok;
	int errorCount;
	SemanticError errors[MAX_SEM_ERRORS];
	int warningCount;
	SemanticError warnings[MAX_SEM_WARNINGS];

	SemanticResult() : ok(true), errorCount(0), warningCount(0) {}
};

SemanticResult analyzeSemantics(ASTNode* root, SymbolTable& symbols);

#endif // SEMANTIC_H


