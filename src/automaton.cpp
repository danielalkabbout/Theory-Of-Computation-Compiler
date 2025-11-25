#include "automaton.h"
#include <cstring>
#include <sstream>
using namespace std;

AutomatonManager::AutomatonManager() {
	for (int i = 0; i < MAX_AUTOMATA; ++i) {
		used[i] = false;
	}
}

bool AutomatonManager::insert(const Automaton& a) {
	// ensure unique id
	for (int i = 0; i < MAX_AUTOMATA; ++i) {
		if (used[i] && automatons[i].id == a.id) {
			return false;
		}
	}
	for (int i = 0; i < MAX_AUTOMATA; ++i) {
		if (!used[i]) {
			automatons[i] = a;
			used[i] = true;
			return true;
		}
	}
	return false;
}

bool AutomatonManager::removeById(int id) {
	for (int i = 0; i < MAX_AUTOMATA; ++i) {
		if (used[i] && automatons[i].id == id) {
			used[i] = false;
			return true;
		}
	}
	return false;
}

Automaton* AutomatonManager::findById(int id) {
	for (int i = 0; i < MAX_AUTOMATA; ++i) {
		if (used[i] && automatons[i].id == id) {
			return &automatons[i];
		}
	}
	return nullptr;
}

int AutomatonManager::listIds(int* outIds, int maxCount) const {
	int count = 0;
	for (int i = 0; i < MAX_AUTOMATA && count < maxCount; ++i) {
		if (used[i]) {
			outIds[count++] = automatons[i].id;
		}
	}
	return count;
}

int classifyChar(char c) {
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) return IC_LETTER;
	if (c >= '0' && c <= '9') return IC_DIGIT;
	if (c == '_') return IC_UNDERSCORE;
	if (c == ' ' || c == '\t' || c == '\n' || c == '\r') return IC_WHITESPACE;
	if (c == '+' || c == '-' || c == '*' || c == '/' || c == '=') return IC_OPERATOR;
	if (c == '(' || c == ')' || c == ';' || c == '{' || c == '}' || c == ',') return IC_DELIM;
	if (c == '.') return IC_DOT;
	return IC_OTHER;
}

void initLexerAutomaton(Automaton& a) {
	memset(&a, 0, sizeof(Automaton));
	a.id = 1;
	strncpy(a.name, "DefaultLexerDFA", sizeof(a.name) - 1);
	a.numStates = 6;
	// initialize transitions to -1
	for (int s = 0; s < MAX_STATES; ++s) {
		a.acceptingStates[s] = false;
		for (int ic = 0; ic < NUM_INPUT_CLASSES; ++ic) {
			a.transitionTable[s][ic] = -1;
		}
	}

	const int S_START = 0;
	const int S_IDENT = 1;
	const int S_NUMBER = 2;
	const int S_WS = 3;
	const int S_OP = 4;
	const int S_DELIM = 5;

	// Accepting states
	a.acceptingStates[S_IDENT] = true;
	a.acceptingStates[S_NUMBER] = true;
	a.acceptingStates[S_WS] = true;
	a.acceptingStates[S_OP] = true;
	a.acceptingStates[S_DELIM] = true;

	// From START
	a.transitionTable[S_START][IC_LETTER] = S_IDENT;
	a.transitionTable[S_START][IC_UNDERSCORE] = S_IDENT;
	a.transitionTable[S_START][IC_DIGIT] = S_NUMBER;
	a.transitionTable[S_START][IC_WHITESPACE] = S_WS;
	a.transitionTable[S_START][IC_OPERATOR] = S_OP;
	a.transitionTable[S_START][IC_DELIM] = S_DELIM;

	// IDENT continues on letters/digits/underscore
	a.transitionTable[S_IDENT][IC_LETTER] = S_IDENT;
	a.transitionTable[S_IDENT][IC_DIGIT] = S_IDENT;
	a.transitionTable[S_IDENT][IC_UNDERSCORE] = S_IDENT;

	// NUMBER continues on digits only (integers for simplicity)
	a.transitionTable[S_NUMBER][IC_DIGIT] = S_NUMBER;

	// WHITESPACE continues on whitespace
	a.transitionTable[S_WS][IC_WHITESPACE] = S_WS;

	// OP/DELIM are single-char tokens (no continuing transitions)
	// everything else from other states leads to -1 (forcing emission)
}

int nextState(const Automaton& a, int currentState, int inputClass) {
	if (currentState < 0 || currentState >= a.numStates) return -1;
	if (inputClass < 0 || inputClass >= NUM_INPUT_CLASSES) return -1;
	return a.transitionTable[currentState][inputClass];
}

bool isAccepting(const Automaton& a, int state) {
	if (state < 0 || state >= a.numStates) return false;
	return a.acceptingStates[state];
}

string automatonToString(const Automaton& a) {
	ostringstream oss;
	oss << "Automaton ID: " << a.id << " Name: " << a.name << "\n";
	oss << "States: " << a.numStates << " Accepting: ";
	for (int s = 0; s < a.numStates; ++s) {
		if (a.acceptingStates[s]) {
			oss << s << " ";
		}
	}
	oss << "\nTransitions (state,inputClass -> next):\n";
	for (int s = 0; s < a.numStates; ++s) {
		for (int ic = 0; ic < NUM_INPUT_CLASSES; ++ic) {
			int ns = a.transitionTable[s][ic];
			if (ns != -1) {
				oss << "  " << s << "," << ic << " -> " << ns << "\n";
			}
		}
	}
	return oss.str();
}


