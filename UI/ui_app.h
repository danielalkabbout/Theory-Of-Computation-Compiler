#pragma once

#include <string>
#include "automaton.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "translator.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#ifdef type
#undef type
#endif
#ifdef Token
#undef Token
#endif
#ifdef TokenType
#undef TokenType
#endif
#endif

class UiApp {
public:
	bool run(HINSTANCE hInstance, int nCmdShow);

private:
	// Windows
	HINSTANCE hInst = nullptr;
	HWND hMainWnd = nullptr;
	HWND hComboAutomata = nullptr;
	HWND hBtnLex = nullptr;
	HWND hBtnParse = nullptr;
	HWND hBtnSem = nullptr;
	HWND hBtnTrans = nullptr;
	HWND hBtnRunAll = nullptr;
	HWND hBtnSave = nullptr;
	HWND hTab = nullptr;
	HWND hEditSource = nullptr;
	HWND hEditTokens = nullptr;
	HWND hEditParse = nullptr;
	HWND hEditSemantics = nullptr;
	HWND hEditTrans = nullptr;
	HWND hParseGraph = nullptr;
	HWND hAutoGraph = nullptr;

	// Styling
	HFONT hUIFont = nullptr;
	HBRUSH hBgBrush = nullptr;
	COLORREF bgColor = RGB(245, 248, 255);

	// Backend session (local to UI)
	AutomatonManager automatonManager;
	Automaton* selectedAutomaton = nullptr;

	std::string sourceCode;
	TokenList tokens;
	bool tokensReady = false;
	std::string lexerError;

	Parser* parserPtr = nullptr;
	ASTNode* astRoot = nullptr;
	bool parsedOk = false;

	SymbolTable symbolTable;
	SemanticResult semResult{};
	bool semanticsDone = false;

	std::string translatedPython;
	bool translationDone = false;
	static LRESULT CALLBACK AutoGraphProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
private:
	// Lifecycle
	bool initBackend();
	bool createMainWindow(int nCmdShow);
	void createChildControls();
	void layoutControls(int clientW, int clientH);

	// Actions
	void onLex();
	void onParse();
	void onSemantics();
	void onTranslate();
	void onRunAll();
	void onSaveTranslation();
	void onAutomatonChanged();

	// Helpers
	void setEditText(HWND hEdit, const std::string& text);
	void appendTokensTo(std::string& out);
	void formatASTTo(std::string& out);
	void invalidateFromLex();
	void invalidateFromParseOnly();
	void drawAutomaton(HDC hdc, RECT rcClient);
	// Graph
	void drawParseTree(HDC hdc, RECT rcClient);

	// Window procs
	static LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK ParseGraphProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
};



