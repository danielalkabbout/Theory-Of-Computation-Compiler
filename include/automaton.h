#ifndef AUTOMATON_H
#define AUTOMATON_H
#include <string>
#define MAX_AUTOMATA 8
#define MAX_STATES 16
#define NUM_INPUT_CLASSES 8

// Input character classes for compact DFA
enum InputClass {
	IC_LETTER = 0,
	IC_DIGIT = 1,
	IC_UNDERSCORE = 2,
	IC_WHITESPACE = 3,
	IC_OPERATOR = 4,
	IC_DELIM = 5,
	IC_DOT = 6,
	IC_OTHER = 7
};

// Basic DFA structure
struct Automaton {
	int id;
	char name[64];
	int numStates;
	bool acceptingStates[MAX_STATES];
	int transitionTable[MAX_STATES][NUM_INPUT_CLASSES]; // -1 == no transition
};

// Array-based registry for automatons
struct AutomatonManager {
	Automaton automatons[MAX_AUTOMATA];
	bool used[MAX_AUTOMATA];

	AutomatonManager();

	bool insert(const Automaton& a);
	bool removeById(int id);
	Automaton* findById(int id);
	int listIds(int* outIds, int maxCount) const; // returns count
};

// Helpers
int classifyChar(char c);
void initLexerAutomaton(Automaton& a);
int nextState(const Automaton& a, int currentState, int inputClass);
bool isAccepting(const Automaton& a, int state);
std::string automatonToString(const Automaton& a);

#endif // AUTOMATON_H


