#include "nfa.h"
#include <cstring>
#include <queue>

void buildNFAFromDFA(const Automaton& dfa, NFA& out) {
	std::memset(&out, 0, sizeof(NFA));
	out.numStates = dfa.numStates;
	for (int s = 0; s < dfa.numStates; ++s) {
		out.acceptingStates[s] = dfa.acceptingStates[s];
		for (int ic = 0; ic < NUM_INPUT_CLASSES; ++ic) {
			int ns = dfa.transitionTable[s][ic];
			if (ns >= 0) {
				out.trans[s][ic].add(ns);
			}
		}
		// no epsilon edges by default
	}
}

// --- NFA -> DFA (subset construction) ---
static void epsilonClosure(const NFA& nfa, unsigned int startSet, unsigned int& outSet) {
	// startSet: bitset of states
	outSet = startSet;
	std::queue<int> q;
	for (int s = 0; s < nfa.numStates; ++s) {
		if (startSet & (1u << s)) q.push(s);
	}
	while (!q.empty()) {
		int s = q.front(); q.pop();
		const NFATransitions& eps = nfa.trans[s][NUM_INPUT_CLASSES];
		for (int i = 0; i < eps.count; ++i) {
			int ns = eps.dest[i];
			if (!(outSet & (1u << ns))) {
				outSet |= (1u << ns);
				q.push(ns);
			}
		}
	}
}

static unsigned int moveOn(const NFA& nfa, unsigned int set, int inputClass) {
	unsigned int result = 0;
	for (int s = 0; s < nfa.numStates; ++s) {
		if (set & (1u << s)) {
			const NFATransitions& tr = nfa.trans[s][inputClass];
			for (int i = 0; i < tr.count; ++i) {
				int ns = tr.dest[i];
				if (ns >= 0 && ns < nfa.numStates) result |= (1u << ns);
			}
		}
	}
	return result;
}

bool determinizeNFA(const NFA& nfa, Automaton& outDfa) {
	std::memset(&outDfa, 0, sizeof(Automaton));
	outDfa.id = 2;
	// Safe copy for name to avoid C4996 on MSVC
#if defined(_MSC_VER)
	strncpy_s(outDfa.name, sizeof(outDfa.name), "DeterminizedDFA", _TRUNCATE);
#else
	std::strncpy(outDfa.name, "DeterminizedDFA", sizeof(outDfa.name) - 1);
	outDfa.name[sizeof(outDfa.name) - 1] = '\0';
#endif

	// Map from subset bitset to DFA state index
	unsigned int subsetOfState[MAX_STATES] = { 0 };
	unsigned int stateSubset[MAX_STATES] = { 0 };
	int subsetCount = 0;

	// Start state is epsilon-closure of {0}
	unsigned int start = 1u << 0;
	unsigned int startCl;
	epsilonClosure(nfa, start, startCl);
	stateSubset[0] = startCl;
	subsetOfState[0] = startCl;
	subsetCount = 1;
	outDfa.numStates = 0;
	for (int s = 0; s < MAX_STATES; ++s) {
		for (int ic = 0; ic < NUM_INPUT_CLASSES; ++ic) {
			outDfa.transitionTable[s][ic] = -1;
		}
		outDfa.acceptingStates[s] = false;
	}

	// BFS over subsets
	for (int i = 0; i < subsetCount && subsetCount <= MAX_STATES; ++i) {
		unsigned int subset = stateSubset[i];
		// mark accepting if any NFA accepting in subset
		bool acc = false;
		for (int s = 0; s < nfa.numStates; ++s) {
			if ((subset & (1u << s)) && nfa.acceptingStates[s]) { acc = true; break; }
		}
		outDfa.acceptingStates[i] = acc;

		for (int ic = 0; ic < NUM_INPUT_CLASSES; ++ic) {
			unsigned int m = moveOn(nfa, subset, ic);
			if (m == 0) { outDfa.transitionTable[i][ic] = -1; continue; }
			unsigned int cl; epsilonClosure(nfa, m, cl);
			// find or create DFA state for cl
			int found = -1;
			for (int j = 0; j < subsetCount; ++j) {
				if (stateSubset[j] == cl) { found = j; break; }
			}
			if (found == -1) {
				if (subsetCount >= MAX_STATES) continue; // out of capacity
				found = subsetCount;
				stateSubset[found] = cl;
				subsetCount++;
			}
			outDfa.transitionTable[i][ic] = found;
		}
	}
	outDfa.numStates = subsetCount;
	return subsetCount > 0;
}

void buildLexerNFA(NFA& out) {
	std::memset(&out, 0, sizeof(NFA));
	// States
	const int S0 = 0;
	const int S_IDENT = 1;
	const int S_NUMBER = 2;
	const int S_WS = 3;
	const int S_OP_S = 4;      // non-accepting starter for OP
	const int S_DELIM_S = 5;   // non-accepting starter for DELIM
	const int S_OP = 6;        // accepting OP (single char consumed)
	const int S_DELIM = 7;     // accepting DELIM (single char consumed)
	out.numStates = 8;

	// Accepting states
	out.acceptingStates[S_IDENT] = true;
	out.acceptingStates[S_NUMBER] = true;
	out.acceptingStates[S_WS] = true;
	out.acceptingStates[S_OP] = true;
	out.acceptingStates[S_DELIM] = true;

	// Epsilon from start to submachines (NFA hallmark)
	out.trans[S0][NUM_INPUT_CLASSES].add(S_IDENT);
	out.trans[S0][NUM_INPUT_CLASSES].add(S_NUMBER);
	out.trans[S0][NUM_INPUT_CLASSES].add(S_WS);
	out.trans[S0][NUM_INPUT_CLASSES].add(S_OP_S);
	out.trans[S0][NUM_INPUT_CLASSES].add(S_DELIM_S);

	// IDENT: letter/underscore start, then loops on letter/digit/underscore
	out.trans[S_IDENT][IC_LETTER].add(S_IDENT);
	out.trans[S_IDENT][IC_UNDERSCORE].add(S_IDENT);
	out.trans[S_IDENT][IC_DIGIT].add(S_IDENT);

	// NUMBER: loop on digits
	out.trans[S_NUMBER][IC_DIGIT].add(S_NUMBER);

	// WS: loop on whitespace
	out.trans[S_WS][IC_WHITESPACE].add(S_WS);

	// OP: single-character operators (no loop; one char only)
	out.trans[S_OP_S][IC_OPERATOR].add(S_OP);

	// DELIM: single-character delimiters (no loop; one char only)
	out.trans[S_DELIM_S][IC_DELIM].add(S_DELIM);
}


