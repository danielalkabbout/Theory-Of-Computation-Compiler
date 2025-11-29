#ifndef NFA_H
#define NFA_H

#include "automaton.h"

#define MAX_NFA_EDGES 8
#define EPSILON_IC (NUM_INPUT_CLASSES) // extra slot for epsilon transitions

struct NFATransitions {
	int count;
	int dest[MAX_NFA_EDGES];
	NFATransitions() : count(0) {}

	bool add(int d) {
		if (count >= MAX_NFA_EDGES) return false;
		dest[count++] = d;
		return true;
	}
};

struct NFA {
	int numStates;
	bool acceptingStates[MAX_STATES];
	NFATransitions trans[MAX_STATES][NUM_INPUT_CLASSES + 1]; // last index is epsilon
};

// Initialize out from DFA: each DFA edge becomes a single-edge NFA transition.
void buildNFAFromDFA(const Automaton& dfa, NFA& out);

// Subset construction (with epsilon-closure). Returns true on success.
// Produces a DFA in the Automaton format (bounded by MAX_STATES).
bool determinizeNFA(const NFA& nfa, Automaton& outDfa);

// Build a didactic lexer NFA with epsilon branches from start to submachines.
// States (example):
//  0: start
//  1: IDENT*, 2: NUMBER*, 3: WS*, 4: OP, 5: DELIM
// Accepting: 1,2,3,4,5
void buildLexerNFA(NFA& out);

#endif // NFA_H


