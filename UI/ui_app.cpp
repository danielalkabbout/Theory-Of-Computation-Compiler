#include "ui_app.h"
#include "lexer.h"
#include <cstdio>
#include <cmath>
#include <functional>
#include <memory>
#include <cstring>
#include <richedit.h>

using namespace std;

// Forward declaration for colored rich edit appender
static void richAppend(HWND hRich, const string& text, COLORREF color);

#ifdef type
#undef type
#endif
#ifdef Token
#undef Token
#endif
#ifdef TokenType
#undef TokenType
#endif

#define IDC_COMBO_AUTOMATA 1001
#define IDC_BTN_LEX 1002
#define IDC_BTN_PARSE 1003
#define IDC_BTN_SEM 1004
#define IDC_BTN_TRANS 1005
#define IDC_BTN_RUNALL 1006
#define IDC_BTN_SAVE 1007
#define IDC_TAB 1008
#define IDC_EDIT_SOURCE 1009
#define IDC_EDIT_TOKENS 1010
#define IDC_EDIT_PARSE 1011
#define IDC_EDIT_TRANS 1012
#define IDC_AUTOGRAPH 1013
#define IDC_CHK_TRACE 1014
#define IDC_BTN_STEP 1015
#define IDC_BTN_RESET 1016
#define IDC_BTN_THEME 1017


static const char* kMainWndClass = "TC_GUI_MAIN";
static const char* kParseGraphClass = "TC_GUI_PARSEGRAPH";
static const char* kNfaGraphClass = "TC_GUI_NFAGRAPH";
static const char* kDfaGraphClass = "TC_GUI_DFAGRAPH";

static const char* inputClassName(int ic) {
	switch (ic) {
	case IC_LETTER: return "LETTER";
	case IC_DIGIT: return "DIGIT";
	case IC_UNDERSCORE: return "UNDERSCORE";
	case IC_WHITESPACE: return "WS";
	case IC_OPERATOR: return "OP";
	case IC_DELIM: return "DELIM";
	case IC_DOT: return "DOT";
	default: return "OTHER";
	}
}

static const char* tokenTypeNameInt(int tok) {
	switch (tok) {
	case T_EOF_TOK: return "EOF";
	case T_IDENT: return "IDENT";
	case T_NUMBER: return "NUMBER";
	case T_KW_LET: return "KW_LET";
	case T_KW_PRINT: return "KW_PRINT";
	case T_PLUS: return "PLUS";
	case T_MINUS: return "MINUS";
	case T_STAR: return "STAR";
	case T_SLASH: return "SLASH";
	case T_EQUAL: return "EQUAL";
	case T_LPAREN: return "LPAREN";
	case T_RPAREN: return "RPAREN";
	case T_SEMI: return "SEMI";
	default: return "UNKNOWN";
	}
}

// Normalize newlines for Windows EDIT controls (convert lone \\n to \\r\\n)
static string toCRLF(const string& s) {
	string out;
	out.reserve(s.size() + 16);
	char prev = 0;
	for (char ch : s) {
		if (ch == '\n') {
			if (prev != '\r') out.push_back('\r');
			out.push_back('\n');
		} else {
			out.push_back(ch);
		}
		prev = ch;
	}
	return out;
}

// Simple file writer for text outputs
static bool writeTextFile(const char* path, const string& data) {
	FILE* f = nullptr;
#if defined(_MSC_VER)
	fopen_s(&f, path, "wb");
#else
	f = fopen(path, "wb");
#endif
	if (!f) return false;
	fwrite(data.data(), 1, data.size(), f);
	fclose(f);
	return true;
}

// Render a drawer into an in-memory 32-bit DIB and save as BMP
static bool saveDrawnImageToBMP(const char* path, int width, int height, const function<void(HDC, RECT)>& drawer) {
	if (width <= 0 || height <= 0) return false;
	HDC screenDC = GetDC(nullptr);
	HDC memDC = CreateCompatibleDC(screenDC);

	BITMAPINFO bmi{};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = width;
	// Use TOP-DOWN DIB so the saved image is not vertically flipped
	bmi.bmiHeader.biHeight = -height; // negative -> top-down
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biSizeImage = width * height * 4;

	void* bits = nullptr;
	HBITMAP hbmp = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
	if (!hbmp) { DeleteDC(memDC); ReleaseDC(nullptr, screenDC); return false; }
	HGDIOBJ old = SelectObject(memDC, hbmp);

	RECT rc = { 0, 0, width, height };
	HBRUSH bg = CreateSolidBrush(RGB(255, 255, 255));
	FillRect(memDC, &rc, bg);
	DeleteObject(bg);

	// Let the caller draw into our off-screen surface
	drawer(memDC, rc);

	// Write BMP file
	BITMAPFILEHEADER bfh{};
	BITMAPINFOHEADER bih = bmi.bmiHeader;
	const DWORD rowStride = width * 4;
	const DWORD dataSize = rowStride * height;
	bfh.bfType = 0x4D42; // 'BM'
	bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	bfh.bfSize = bfh.bfOffBits + dataSize;

	FILE* f = nullptr;
#if defined(_MSC_VER)
	fopen_s(&f, path, "wb");
#else
	f = fopen(path, "wb");
#endif
	if (!f) {
		SelectObject(memDC, old);
		DeleteObject(hbmp);
		DeleteDC(memDC);
		ReleaseDC(nullptr, screenDC);
		return false;
	}
	fwrite(&bfh, 1, sizeof(bfh), f);
	fwrite(&bih, 1, sizeof(bih), f);

	// Top-down DIB: can write all pixels in memory order
	fwrite(bits, 1, dataSize, f);
	fclose(f);

	SelectObject(memDC, old);
	DeleteObject(hbmp);
	DeleteDC(memDC);
	ReleaseDC(nullptr, screenDC);
	return true;
}

// Dump NFA in a readable textual form
static string nfaToString(const NFA& nfa) {
	string out;
	char line[256];
	snprintf(line, sizeof(line), "NFA: states=%d\r\n", nfa.numStates);
	out += line;
	out += "Accepting:";
	for (int s = 0; s < nfa.numStates; ++s) {
		if (nfa.acceptingStates[s]) { snprintf(line, sizeof(line), " S%d", s); out += line; }
	}
	out += "\r\nTransitions:\r\n";
	for (int s = 0; s < nfa.numStates; ++s) {
		for (int ic = 0; ic <= NUM_INPUT_CLASSES; ++ic) {
			const NFATransitions& tr = nfa.trans[s][ic];
			for (int i = 0; i < tr.count; ++i) {
				int ns = tr.dest[i];
				const char* lbl = (ic == NUM_INPUT_CLASSES) ? "eps" : inputClassName(ic);
				snprintf(line, sizeof(line), "  S%d --[%s]--> S%d\r\n", s, lbl, ns);
				out += line;
			}
		}
	}
	return out;
}

bool UiApp::run(HINSTANCE hInstance, int nCmdShow) {
	hInst = hInstance;
	INITCOMMONCONTROLSEX icc = { sizeof(INITCOMMONCONTROLSEX), ICC_TAB_CLASSES };
	InitCommonControlsEx(&icc);

	if (!initBackend()) return false;
	if (!createMainWindow(nCmdShow)) return false;

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return true;
}

bool UiApp::initBackend() {
	Automaton lexDFA;
	initLexerAutomaton(lexDFA);
	if (!automatonManager.insert(lexDFA)) return false;
	selectedAutomaton = automatonManager.findById(1);
	if (selectedAutomaton) { buildLexerNFA(nfa); determinizeNFA(nfa, dfaFromNfa); }
	traceEnabled = true;
	traceIndex = -1;
	std::memset(usedEdge, 0, sizeof(usedEdge));
	return selectedAutomaton != nullptr;
}

bool UiApp::createMainWindow(int nCmdShow) {
	WNDCLASSA wc = {};
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = &UiApp::MainWndProc;
	wc.hInstance = hInst;
	wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszClassName = kMainWndClass;
	if (!RegisterClassA(&wc)) return false;

	WNDCLASSA wcParse = {};
	wcParse.style = CS_HREDRAW | CS_VREDRAW;
	wcParse.lpfnWndProc = &UiApp::ParseGraphProc;
	wcParse.hInstance = hInst;
	wcParse.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcParse.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcParse.lpszClassName = kParseGraphClass;
	if (!RegisterClassA(&wcParse)) return false;

	WNDCLASSA wcNfa = {};
	wcNfa.style = CS_HREDRAW | CS_VREDRAW;
	wcNfa.lpfnWndProc = &UiApp::NfaGraphProc;
	wcNfa.hInstance = hInst;
	wcNfa.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcNfa.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcNfa.lpszClassName = kNfaGraphClass;
	if (!RegisterClassA(&wcNfa)) return false;

	// Register DFA canvas window
	WNDCLASSA wcDfa = {};
	wcDfa.style = CS_HREDRAW | CS_VREDRAW;
	wcDfa.lpfnWndProc = &UiApp::DfaGraphProc;
	wcDfa.hInstance = hInst;
	wcDfa.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcDfa.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcDfa.lpszClassName = kDfaGraphClass;
	if (!RegisterClassA(&wcDfa)) return false;

	hMainWnd = CreateWindowExA(
		0, kMainWndClass, "Theory of Computation - GUI",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, 1200, 700,
		nullptr, nullptr, hInst, this);
	if (!hMainWnd) return false;

	ShowWindow(hMainWnd, nCmdShow);
	UpdateWindow(hMainWnd);
	return true;
}

void UiApp::createChildControls() {
<<<<<<< HEAD
	// Ensure RichEdit library is available (for colored semantics)
	LoadLibraryA("Riched20.dll"); // OK if already loaded
	// Ensure background brush exists
	if (!hBgBrush) hBgBrush = CreateSolidBrush(bgColor);
=======



>>>>>>> 3624fb48deab9420b55360ad80d7b69a6ba44d48
	hComboAutomata = CreateWindowExA(0, "COMBOBOX", "",
		CBS_DROPDOWNLIST | WS_CHILD | WS_VISIBLE,
		0, 0, 200, 200, hMainWnd, (HMENU)IDC_COMBO_AUTOMATA, hInst, nullptr);

	hBtnLex = CreateWindowExA(0, "BUTTON", "Lex",
		WS_CHILD | WS_VISIBLE, 0, 0, 80, 24, hMainWnd, (HMENU)IDC_BTN_LEX, hInst, nullptr);
	hBtnParse = CreateWindowExA(0, "BUTTON", "Parse",
		WS_CHILD | WS_VISIBLE, 0, 0, 80, 24, hMainWnd, (HMENU)IDC_BTN_PARSE, hInst, nullptr);
	hBtnSem = CreateWindowExA(0, "BUTTON", "Semantics",
		WS_CHILD | WS_VISIBLE, 0, 0, 90, 24, hMainWnd, (HMENU)IDC_BTN_SEM, hInst, nullptr);
	hBtnTrans = CreateWindowExA(0, "BUTTON", "Translate",
		WS_CHILD | WS_VISIBLE, 0, 0, 90, 24, hMainWnd, (HMENU)IDC_BTN_TRANS, hInst, nullptr);
	hBtnRunAll = CreateWindowExA(0, "BUTTON", "Run All",
		WS_CHILD | WS_VISIBLE, 0, 0, 90, 24, hMainWnd, (HMENU)IDC_BTN_RUNALL, hInst, nullptr);
	hBtnSave = CreateWindowExA(0, "BUTTON", "Save",
		WS_CHILD | WS_VISIBLE, 0, 0, 80, 24, hMainWnd, (HMENU)IDC_BTN_SAVE, hInst, nullptr);
	// Disable translate by default until semantics passes
	EnableWindow(hBtnTrans, FALSE);
	hChkTrace = CreateWindowExA(0, "BUTTON", "Trace",
		WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 80, 24, hMainWnd, (HMENU)IDC_CHK_TRACE, hInst, nullptr);
	SendMessageA(hChkTrace, BM_SETCHECK, BST_CHECKED, 0);
	hBtnStep = CreateWindowExA(0, "BUTTON", "Step",
		WS_CHILD | WS_VISIBLE, 0, 0, 70, 24, hMainWnd, (HMENU)IDC_BTN_STEP, hInst, nullptr);
	hBtnReset = CreateWindowExA(0, "BUTTON", "Reset",
		WS_CHILD | WS_VISIBLE, 0, 0, 70, 24, hMainWnd, (HMENU)IDC_BTN_RESET, hInst, nullptr);
	// Theme toggle
	hBtnTheme = CreateWindowExA(0, "BUTTON", "Dark Mode",
		WS_CHILD | WS_VISIBLE, 0, 0, 90, 24, hMainWnd, (HMENU)IDC_BTN_THEME, hInst, nullptr);

	hTab = CreateWindowExA(0, WC_TABCONTROLA, "",
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
		0, 0, 100, 100, hMainWnd, (HMENU)IDC_TAB, hInst, nullptr);

	TCITEMA item = {}; item.mask = TCIF_TEXT;
	item.pszText = (LPSTR)"Tokens"; TabCtrl_InsertItem(hTab, 0, &item);
	item.pszText = (LPSTR)"Parse Tree"; TabCtrl_InsertItem(hTab, 1, &item);
	item.pszText = (LPSTR)"Semantics"; TabCtrl_InsertItem(hTab, 2, &item);
	item.pszText = (LPSTR)"Translation"; TabCtrl_InsertItem(hTab, 3, &item);
	item.pszText = (LPSTR)"DFA"; TabCtrl_InsertItem(hTab, 4, &item);
	item.pszText = (LPSTR)"NFA"; TabCtrl_InsertItem(hTab, 5, &item);

	hEditSource = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
		WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL | WS_HSCROLL,
		0, 0, 100, 100, hMainWnd, (HMENU)IDC_EDIT_SOURCE, hInst, nullptr);
	hEditTokens = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
		WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL | WS_HSCROLL,
		0, 0, 100, 100, hMainWnd, (HMENU)IDC_EDIT_TOKENS, hInst, nullptr);
	hEditParse = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
		WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL | WS_HSCROLL,
		0, 0, 100, 100, hMainWnd, (HMENU)IDC_EDIT_PARSE, hInst, nullptr);
	// Use RichEdit for colored semantics output
	const char* kRichClass = "RichEdit20A";
	hEditSemantics = CreateWindowExA(WS_EX_CLIENTEDGE, kRichClass, "",
		WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL | WS_HSCROLL,
		0, 0, 100, 100, hMainWnd, nullptr, hInst, nullptr);
	hEditTrans = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
		WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL | WS_HSCROLL,
		0, 0, 100, 100, hMainWnd, (HMENU)IDC_EDIT_TRANS, hInst, nullptr);

	hParseGraph = CreateWindowExA(0, kParseGraphClass, "",
		WS_CHILD | WS_VISIBLE, 0, 0, 100, 100, hMainWnd, nullptr, hInst, this);
	hNfaGraph = CreateWindowExA(0, kNfaGraphClass, "",
		WS_CHILD | WS_VISIBLE, 0, 0, 100, 100, hMainWnd, nullptr, hInst, this);
	hDfaGraph = CreateWindowExA(0, kDfaGraphClass, "",
		WS_CHILD | WS_VISIBLE, 0, 0, 100, 100, hMainWnd, nullptr, hInst, this);

	// Create a modern font
	hUIFont = CreateFontA(
		18, 0, 0, 0, FW_NORMAL,
		FALSE, FALSE, FALSE,
		ANSI_CHARSET, OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
		VARIABLE_PITCH, "Segoe UI"
	);

	// Apply it to all controls
	HWND ctrls[] = { hComboAutomata, hBtnLex, hBtnParse, hBtnSem, hBtnTrans,
					 hBtnRunAll, hBtnSave, hEditSource, hEditTokens,
					 hEditParse, hEditSemantics, hEditTrans };

	for (HWND c : ctrls) {
		SendMessageA(c, WM_SETFONT, (WPARAM)hUIFont, TRUE);
	}


	// Populate automaton combo
	SendMessageA(hComboAutomata, CB_RESETCONTENT, 0, 0);
	int ids[MAX_AUTOMATA]; int cnt = automatonManager.listIds(ids, MAX_AUTOMATA);
	for (int i = 0; i < cnt; ++i) {
		char buf[64]; snprintf(buf, sizeof(buf), "Automaton %d", ids[i]);
		SendMessageA(hComboAutomata, CB_ADDSTRING, 0, (LPARAM)buf);
	}
	SendMessageA(hComboAutomata, CB_SETCURSEL, 0, 0);
}

void UiApp::layoutControls(int clientW, int clientH) {
	int pad = 8;
	int topH = 32;
	int leftW = 260;

	MoveWindow(hComboAutomata, pad, pad, 200, topH, TRUE);
	int x = pad + 210;
	MoveWindow(hBtnLex, x, pad, 80, topH, TRUE); x += 85;
	MoveWindow(hBtnParse, x, pad, 80, topH, TRUE); x += 85;
	MoveWindow(hBtnSem, x, pad, 90, topH, TRUE); x += 95;
	MoveWindow(hBtnTrans, x, pad, 90, topH, TRUE); x += 95;
	MoveWindow(hBtnRunAll, x, pad, 90, topH, TRUE); x += 95;
	MoveWindow(hBtnSave, x, pad, 80, topH, TRUE); x += 85;
	MoveWindow(hChkTrace, x, pad, 80, topH, TRUE); x += 85;
	MoveWindow(hBtnStep, x, pad, 70, topH, TRUE); x += 75;
	MoveWindow(hBtnReset, x, pad, 70, topH, TRUE);
	x += 75;
	MoveWindow(hBtnTheme, x, pad, 90, topH, TRUE);

	int y = pad + topH + pad;
	int rightW = clientW - (pad + leftW + pad);
	if (rightW < 300) rightW = 300;

	MoveWindow(hEditSource, pad, y, leftW, clientH - y - pad, TRUE);

	MoveWindow(hTab, pad + leftW + pad, y, rightW, clientH - y - pad, TRUE);
	RECT rc; GetClientRect(hTab, &rc);
	TabCtrl_AdjustRect(hTab, FALSE, &rc);
	int tabX = rc.left, tabY = rc.top, tabW = rc.right - rc.left, tabH = rc.bottom - rc.top;
	MoveWindow(hEditTokens, pad + leftW + pad + tabX, y + tabY, tabW, tabH, TRUE);
	MoveWindow(hEditParse, pad + leftW + pad + tabX, y + tabY, tabW, tabH, TRUE);
	MoveWindow(hEditTrans, pad + leftW + pad + tabX, y + tabY, tabW, tabH, TRUE);
	MoveWindow(hEditSemantics, pad + leftW + pad + tabX, y + tabY, tabW, tabH, TRUE);
	MoveWindow(hParseGraph, pad + leftW + pad + tabX, y + tabY, tabW, tabH, TRUE);
	MoveWindow(hNfaGraph, pad + leftW + pad + tabX, y + tabY, tabW, tabH, TRUE);
	MoveWindow(hDfaGraph, pad + leftW + pad + tabX, y + tabY, tabW, tabH, TRUE);

	int sel = TabCtrl_GetCurSel(hTab);
	int tabCount = TabCtrl_GetItemCount(hTab);
	// Backward compatibility: if only 5 tabs exist, index 4 is the legacy "Automaton" (NFA) tab.
	bool showDfa = (tabCount >= 6 && sel == 4);
	bool showNfa = (tabCount >= 6 ? (sel == 5) : (sel == 4));
	ShowWindow(hEditTokens, (sel == 0) ? SW_SHOW : SW_HIDE);
	ShowWindow(hEditParse, SW_HIDE);
	ShowWindow(hParseGraph, (sel == 1) ? SW_SHOW : SW_HIDE);
	ShowWindow(hEditSemantics, (sel == 2) ? SW_SHOW : SW_HIDE);
	ShowWindow(hEditTrans, (sel == 3) ? SW_SHOW : SW_HIDE);
	ShowWindow(hDfaGraph, showDfa ? SW_SHOW : SW_HIDE);
	ShowWindow(hNfaGraph, showNfa ? SW_SHOW : SW_HIDE);

	// no right-side graph
}

void UiApp::onLex() {
	int len = GetWindowTextLengthA(hEditSource);
	std::unique_ptr<char[]> buf(new char[len + 1]);
	GetWindowTextA(hEditSource, buf.get(), len + 1);
	sourceCode.assign(buf.get(), len);

	if (!selectedAutomaton) return;
	tokens = TokenList{};
	lexerError.clear();
	bool ok = lex(sourceCode, *selectedAutomaton, tokens, lexerError);
	if (!ok) {
		std::string out = "Lexical analysis failed: " + lexerError + "\r\n";
		setEditText(hEditTokens, out);
		tokensReady = false;
		// Disable translate when lexing fails
		if (hBtnTrans) EnableWindow(hBtnTrans, FALSE);
		return;
	}
	tokensReady = true;
	invalidateFromParseOnly();

	std::string tokText;
	appendTokensTo(tokText);
	setEditText(hEditTokens, tokText);
	recomputeNfaTrace();
	InvalidateRect(hNfaGraph, nullptr, TRUE);
	InvalidateRect(hDfaGraph, nullptr, TRUE);
}

void UiApp::onParse() {
	if (!tokensReady) return;
	if (parserPtr) { delete parserPtr; parserPtr = nullptr; }
	if (astRoot) { Parser::freeAST(astRoot); astRoot = nullptr; }
	parserPtr = new Parser(tokens);
	astRoot = parserPtr->parseProgram();
	if (!astRoot) {
		setEditText(hEditParse, "Parsing failed.\r\n");
		parsedOk = false;
		// Disable translate when parsing fails
		if (hBtnTrans) EnableWindow(hBtnTrans, FALSE);
		return;
	}
	parsedOk = true;

	std::string tree;
	formatASTTo(tree);
	setEditText(hEditParse, tree);

	symbolTable = SymbolTable{};
	semResult = SemanticResult{};
	semanticsDone = false;
	translatedPython.clear();
	translationDone = false;
	// Still disable translate until semantics passes
	if (hBtnTrans) EnableWindow(hBtnTrans, FALSE);
}

void UiApp::onSemantics() {
	if (!parsedOk || !astRoot) {
		setEditText(hEditSemantics, "No AST. Run parsing first or fix parse errors.\r\n");
		SendMessageA(hTab, TCM_SETCURSEL, 2, 0);
		RECT r; GetClientRect(hMainWnd, &r); layoutControls(r.right - r.left, r.bottom - r.top);
		return;
	}
	symbolTable = SymbolTable{};
	semResult = analyzeSemantics(astRoot, symbolTable);
	semanticsDone = true;

	// Build colored output in RichEdit: header (black), errors (red), warnings (yellow)
	string res = "[Semantic Analysis]\r\n";
	if (!semResult.ok) {
		char buf[64]; snprintf(buf, sizeof(buf), "Errors: %d\r\n", semResult.errorCount);
		res += buf;
		for (int i = 0; i < semResult.errorCount; ++i) {
			const SemanticError& e = semResult.errors[i];
			char line[256];
			if (e.line > 0)
				snprintf(line, sizeof(line), "  line %d, col %d: %s\r\n", e.line, e.col, e.message.c_str());
			else
				snprintf(line, sizeof(line), "  %s\r\n", e.message.c_str());
			res += line;
		}
		// Keep translation view in sync: clear any stale translation
		setEditText(hEditTrans, "Semantic errors present. Fix them before translation.\r\n");
		translatedPython.clear();
		translationDone = false;
		// Disable translate button when there are semantic errors
		if (hBtnTrans) EnableWindow(hBtnTrans, FALSE);
	} else {
		res += "OK (no errors)\r\n";
		// Enable translate when semantics passes
		if (hBtnTrans) EnableWindow(hBtnTrans, TRUE);
	}
	// Warnings are shown regardless, and do not block translation
	if (semResult.warningCount > 0) {
		char wbuf[64]; snprintf(wbuf, sizeof(wbuf), "\r\n[Warnings] %d\r\n", semResult.warningCount);
		res += wbuf;
		for (int i = 0; i < semResult.warningCount; ++i) {
			const SemanticError& w = semResult.warnings[i];
			char wline[256];
			if (w.line > 0)
				snprintf(wline, sizeof(wline), "  line %d, col %d: %s\r\n", w.line, w.col, w.message.c_str());
			else
				snprintf(wline, sizeof(wline), "  %s\r\n", w.message.c_str());
			res += wline;
		}
	}
	// Write with colors: header and OK text (black), errors (red), warnings (yellow)
	SetWindowTextA(hEditSemantics, "");
	{
		// header & OK/summary
		size_t posErrHdr = res.find("Errors:");
		size_t posWarnHdr = res.find("[Warnings]");
		string headerPart = res.substr(0, posErrHdr != string::npos ? posErrHdr : (posWarnHdr != string::npos ? posWarnHdr : res.size()));
		richAppend(hEditSemantics, toCRLF(headerPart), RGB(0,0,0));

		// errors section
		if (posErrHdr != string::npos) {
			size_t endErr = (posWarnHdr != string::npos && posWarnHdr > posErrHdr) ? posWarnHdr : res.size();
			string errPart = res.substr(posErrHdr, endErr - posErrHdr);
			richAppend(hEditSemantics, toCRLF(errPart), RGB(200,30,30)); // red
		}
		// warnings section
		if (posWarnHdr != string::npos) {
			string warnPart = res.substr(posWarnHdr);
			richAppend(hEditSemantics, toCRLF(warnPart), RGB(180,140,0)); // yellow-ish
		}
	}
	// switch to Semantics tab
	SendMessageA(hTab, TCM_SETCURSEL, 2, 0);
	RECT r; GetClientRect(hMainWnd, &r); layoutControls(r.right - r.left, r.bottom - r.top);

	translatedPython.clear();
	translationDone = false;
}

void UiApp::onTranslate() {
	if (!parsedOk || !astRoot) return;
	if (semanticsDone && !semResult.ok) {
		setEditText(hEditTrans, "Semantic errors present. Fix them before translation.\r\n");
		return;
	}
	translatedPython.clear();
	bool ok = translateToPython(astRoot, translatedPython);
	if (!ok) {
		setEditText(hEditTrans, "Translation failed.\r\n");
		translationDone = false;
		return;
	}
	translationDone = true;
	string crlf = toCRLF(translatedPython);
	setEditText(hEditTrans, crlf);
}

void UiApp::onRunAll() {
	// Always re-run the full pipeline from a clean state
	invalidateFromLex();
	traceIndex = -1; // reset trace stepping

	onLex();
	if (!tokensReady) return;

	onParse();
	if (!parsedOk) return;

	onSemantics();
	if (semanticsDone && !semResult.ok) return;

	onTranslate();
	// After a successful run, show the Translation tab so the new output is visible
	SendMessageA(hTab, TCM_SETCURSEL, 3, 0);
	RECT r; GetClientRect(hMainWnd, &r); layoutControls(r.right - r.left, r.bottom - r.top);
}

void UiApp::onSaveTranslation() {
	// Save multiple artifacts to separate files in the working directory
	// Create/ensure target folder
	const char* outDirParent = "UI";
	const char* outDir = "UI\\Saved Files";
	CreateDirectoryA(outDirParent, nullptr); // ensure UI exists (OK if already exists)
	CreateDirectoryA(outDir, nullptr);       // ensure Saved Files exists
	auto pathInOutDir = [&](const char* name) -> std::string {
		std::string p(outDir);
		p += "\\";
		p += name;
		return p;
	};

	// 1) translation.py
	if (translationDone) {
		(void)writeTextFile(pathInOutDir("translation.py").c_str(), translatedPython);
	}

	// 2) tokens.txt
	string tokensText;
	appendTokensTo(tokensText);
	writeTextFile(pathInOutDir("tokens.txt").c_str(), toCRLF(tokensText));

	// 3) ast.txt
	string astText;
	if (parsedOk && parserPtr && astRoot) {
		formatASTTo(astText);
	} else {
		astText = "AST not available. Run Parse first.\r\n";
	}
	writeTextFile(pathInOutDir("ast.txt").c_str(), toCRLF(astText));

	// 4) semantics.txt (includes errors and warnings)
	string semText;
	if (semanticsDone) {
		semText = "[Semantic Analysis]\r\n";
		if (!semResult.ok) {
			char buf[64]; snprintf(buf, sizeof(buf), "Errors: %d\r\n", semResult.errorCount);
			semText += buf;
			for (int i = 0; i < semResult.errorCount; ++i) {
				const SemanticError& e = semResult.errors[i];
				char line[256];
				if (e.line > 0)
					snprintf(line, sizeof(line), "  line %d, col %d: %s\r\n", e.line, e.col, e.message.c_str());
				else
					snprintf(line, sizeof(line), "  %s\r\n", e.message.c_str());
				semText += line;
			}
		} else {
			semText += "OK (no errors)\r\n";
		}
		if (semResult.warningCount > 0) {
			char wbuf[64]; snprintf(wbuf, sizeof(wbuf), "\r\n[Warnings] %d\r\n", semResult.warningCount);
			semText += wbuf;
			for (int i = 0; i < semResult.warningCount; ++i) {
				const SemanticError& w = semResult.warnings[i];
				char wline[256];
				if (w.line > 0)
					snprintf(wline, sizeof(wline), "  line %d, col %d: %s\r\n", w.line, w.col, w.message.c_str());
				else
					snprintf(wline, sizeof(wline), "  %s\r\n", w.message.c_str());
				semText += wline;
			}
		}
	} else {
		semText = "Semantics not run. Run Semantics (or Run All) first.\r\n";
	}
	writeTextFile(pathInOutDir("semantics.txt").c_str(), toCRLF(semText));

	// 5) dfa.txt (determinized DFA or selected DFA as fallback)
	string dfaText;
	if (dfaFromNfa.numStates > 0) dfaText = automatonToString(dfaFromNfa);
	else if (selectedAutomaton) dfaText = automatonToString(*selectedAutomaton);
	else dfaText = "No DFA available.\r\n";
	writeTextFile(pathInOutDir("dfa.txt").c_str(), toCRLF(dfaText));

	// 6) nfa.txt (textual listing with eps)
	string nfaText = nfaToString(nfa);
	writeTextFile(pathInOutDir("nfa.txt").c_str(), toCRLF(nfaText));

	// 7) Images: nfa.bmp, dfa.bmp, parse_tree.bmp
	RECT tabRc; GetClientRect(hTab, &tabRc); TabCtrl_AdjustRect(hTab, FALSE, &tabRc);
	int imgW = (tabRc.right - tabRc.left); if (imgW < 640) imgW = 640;
	int imgH = (tabRc.bottom - tabRc.top); if (imgH < 360) imgH = 360;

	saveDrawnImageToBMP(pathInOutDir("nfa.bmp").c_str(), imgW, imgH, [this](HDC hdc, RECT rc) { this->drawNfa(hdc, rc); });
	saveDrawnImageToBMP(pathInOutDir("dfa.bmp").c_str(), imgW, imgH, [this](HDC hdc, RECT rc) { this->drawDfa(hdc, rc); });
	saveDrawnImageToBMP(pathInOutDir("parse_tree.bmp").c_str(), imgW, imgH, [this](HDC hdc, RECT rc) {
		if (this->parsedOk && this->parserPtr && this->astRoot) {
			// Reuse the existing printer by drawing text into the image
			string s; this->formatASTTo(s);
			const std::string crlf = toCRLF(s);
			// Simple text dump onto image
			RECT textRc = rc;
			DrawTextA(hdc, crlf.c_str(), (int)crlf.size(), &textRc, DT_LEFT | DT_TOP | DT_NOPREFIX);
		} else {
			const char* msg = "AST not available. Run Parse first.";
			TextOutA(hdc, rc.left + 8, rc.top + 8, msg, (int)strlen(msg));
		}
	});

	MessageBoxA(hMainWnd,
		"Saved files in folder 'UI\\Saved Files':\r\n"
		"  translation.py (if available)\r\n"
		"  tokens.txt\r\n"
		"  ast.txt\r\n"
		"  semantics.txt\r\n"
		"  dfa.txt\r\n"
		"  nfa.txt\r\n"
		"Images:\r\n"
		"  nfa.bmp\r\n"
		"  dfa.bmp\r\n"
		"  parse_tree.bmp",
		"Save", MB_OK | MB_ICONINFORMATION);
}

void UiApp::onAutomatonChanged() {
	int sel = (int)SendMessageA(hComboAutomata, CB_GETCURSEL, 0, 0);
	if (sel < 0) return;
	int ids[MAX_AUTOMATA]; int cnt = automatonManager.listIds(ids, MAX_AUTOMATA);
	if (sel >= cnt) return;
	selectedAutomaton = automatonManager.findById(ids[sel]);
	if (selectedAutomaton) { buildLexerNFA(nfa); determinizeNFA(nfa, dfaFromNfa); }
	recomputeNfaTrace();
	InvalidateRect(hNfaGraph, nullptr, TRUE);
	InvalidateRect(hDfaGraph, nullptr, TRUE);
}
void UiApp::recomputeNfaTrace() {
	std::memset(usedEdge, 0, sizeof(usedEdge));
	if (!selectedAutomaton) return;
	const Automaton& a = dfaFromNfa.numStates > 0 ? dfaFromNfa : *selectedAutomaton;
	int limit = (traceIndex < 0 || !traceEnabled) ? (int)sourceCode.size() : (traceIndex + 1);
	int state = 0;
	for (int i = 0; i < limit; ++i) {
		int ic = classifyChar(sourceCode[i]);
		int ns = nextState(a, state, ic);
		if (ns == -1) { state = 0; continue; }
		if (traceEnabled && state >= 0 && state < MAX_STATES && ic >= 0 && ic <= NUM_INPUT_CLASSES) {
			usedEdge[state][ic] = true;
		}
		state = ns;
	}
}

void UiApp::recomputeTrace() {
	if (!traceEnabled) {
		std::memset(usedEdge, 0, sizeof(usedEdge));
		return;
	}
	recomputeNfaTrace();
}

void UiApp::setEditText(HWND hEdit, const std::string& text) {
	SetWindowTextA(hEdit, text.c_str());
}

void UiApp::appendTokensTo(std::string& out) {
	for (int i = 0; i < tokens.count; ++i) {
		const ::Token& t = tokens.at(i);
		char line[256];
		snprintf(line, sizeof(line), "%-10s  '%s'  (line %d, col %d)\r\n",
			tokenTypeNameInt((int)t.tokType), t.lexeme.c_str(), t.line, t.col);
		out += line;
	}
}

static void appendIndent(std::string& out, const std::string& pref, bool isTail) {
	out += pref;
	out += (isTail ? "`-- " : "|-- ");
}

void UiApp::formatASTTo(std::string& out) {
	if (!astRoot) return;

	std::function<void(ASTNode*, const std::string&, bool, const std::string&)> printWithLabel;
	printWithLabel = [&](ASTNode* n, const std::string& pref, bool tail, const std::string& edgeLabel) {
		if (!n) return;
		std::string label;
		switch (n->type) {
		case N_PROGRAM: label = "PROGRAM"; break;
		case N_STMT_LIST: label = "STMT_LIST"; break;
		case N_LET_STMT: label = "LET"; break;
		case N_PRINT_STMT: label = "PRINT"; break;
		case N_BIN_OP: label = std::string("BIN_OP(") + n->text + ")"; break;
		case N_IDENT: label = std::string("IDENT(") + n->text + ")"; break;
		case N_NUMBER: label = std::string("NUMBER(") + n->text + ")"; break;
		}
		appendIndent(out, pref, tail);
		if (!edgeLabel.empty()) {
			out += edgeLabel; out += ": "; out += label; out += "\r\n";
		} else {
			out += label; out += "\r\n";
		}
		std::string childPrefix = pref + (tail ? "    " : "|   ");

		if (n->type == N_PROGRAM) {
			if (n->left) printWithLabel(n->left, childPrefix, true, "body");
			return;
		}
		if (n->type == N_LET_STMT) {
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
		if (n->left && n->right) {
			printWithLabel(n->left, childPrefix, false, "left");
			printWithLabel(n->right, childPrefix, true, "right");
		} else if (n->left) {
			printWithLabel(n->left, childPrefix, true, "left");
		} else if (n->right) {
			printWithLabel(n->right, childPrefix, true, "right");
		}
	};

	printWithLabel(astRoot, "", true, "root");
}

// --- Helpers for graphical parse rendering ---
static void nodeLabel(ASTNode* n, std::string& out) {
	out.clear();
	if (!n) return;
	switch (n->type) {
	case N_PROGRAM: out = "PROG"; return;
	case N_STMT_LIST: out = "LIST"; return;
	case N_LET_STMT: out = "LET"; return;
	case N_PRINT_STMT: out = "PRINT"; return;
	case N_BIN_OP: out = n->text.empty() ? "op" : n->text; return;
	case N_IDENT: out = n->text; return;
	case N_NUMBER: out = n->text; return;
	}
	out = "?";
}

static void collectChildren(ASTNode* n, ASTNode** outArr, int& outCount) {
	outCount = 0;
	if (!n) return;
	if (n->type == N_STMT_LIST) {
		ASTNode* cur = n->left;
		while (cur && outCount < 64) {
			outArr[outCount++] = cur;
			cur = cur->next;
		}
		return;
	}
	if (n->type == N_PROGRAM) {
		if (n->left) outArr[outCount++] = n->left;
		return;
	}
	if (n->type == N_LET_STMT) {
		if (n->left) outArr[outCount++] = n->left;
		if (n->right) outArr[outCount++] = n->right;
		return;
	}
	if (n->type == N_PRINT_STMT) {
		if (n->left) outArr[outCount++] = n->left;
		return;
	}
	if (n->left) outArr[outCount++] = n->left;
	if (n->right) outArr[outCount++] = n->right;
}

static int subtreeWidthPx(ASTNode* n, int nodeW, int hGap) {
	if (!n) return 0;
	ASTNode* kids[64]; int kc = 0;
	collectChildren(n, kids, kc);
	if (kc == 0) return nodeW;
	int sum = 0;
	for (int i = 0; i < kc; ++i) {
		int cw = subtreeWidthPx(kids[i], nodeW, hGap);
		sum += cw;
		if (i + 1 < kc) sum += hGap;
	}
	if (sum < nodeW) sum = nodeW;
	return sum;
}

static void drawCenteredText(HDC hdc, int cx, int cy, const std::string& s) {
	SIZE sz{};
	GetTextExtentPoint32A(hdc, s.c_str(), (int)s.size(), &sz);
	int x = cx - sz.cx / 2;
	int y = cy - sz.cy / 2;
	TextOutA(hdc, x, y, s.c_str(), (int)s.size());
}

// Draw a curved self-loop arc to the right of a node and label it
static void drawSelfLoop(HDC hdc, int cx, int cy, int nodeRadius, const char* label) {
	const int loopR = nodeRadius + 6; // loop size
	RECT arcRc = { cx + 8, cy - loopR, cx + 8 + 2 * loopR, cy + loopR };
	POINT start = { (arcRc.left + arcRc.right) / 2, arcRc.top };     // top
	POINT end   = { (arcRc.left + arcRc.right) / 2, arcRc.bottom };  // bottom
	Arc(hdc, arcRc.left, arcRc.top, arcRc.right, arcRc.bottom, start.x, start.y, end.x, end.y);
	if (label && *label) {
		TextOutA(hdc, arcRc.right + 4, cy - 6, label, (int)strlen(label));
	}
}

static void drawLegend(HDC hdc, int x, int y) {
	const char* lines[] = {
		"Legend:",
		"LETTER: a-z, A-Z",
		"UNDERSCORE: _",
		"DIGIT: 0-9",
		"WS: space, tab, newline, CR",
		"OP: + - * / =",
		"DELIM: ( ) { } , ; [ ]",
		"eps: epsilon transition (NFA only, dashed)"
	};
	for (int i = 0; i < (int)(sizeof(lines)/sizeof(lines[0])); ++i) {
		TextOutA(hdc, x, y + i * 16, lines[i], (int)strlen(lines[i]));
	}
}

static void drawStartArrow(HDC hdc, int x, int y) {
	// Draw an arrow pointing to (x,y) from the left with label "start"
	int ax = x - 80;
	int ay = y;
	MoveToEx(hdc, ax, ay, nullptr);
	LineTo(hdc, x - 24, y);
	// simple arrow head
	LineTo(hdc, x - 30, y - 6);
	MoveToEx(hdc, x - 24, y, nullptr);
	LineTo(hdc, x - 30, y + 6);
	const char* s = "start";
	TextOutA(hdc, ax - 40, ay - 8, s, (int)strlen(s));
}

// Append colored text to a RichEdit control
static void richAppend(HWND hRich, const string& text, COLORREF color) {
	// Temporarily make it writable
	SendMessageA(hRich, EM_SETREADONLY, FALSE, 0);
	LONG len = GetWindowTextLengthA(hRich);
	SendMessageA(hRich, EM_SETSEL, (WPARAM)len, (LPARAM)len);
	CHARFORMAT2A cf{};
	cf.cbSize = sizeof(cf);
	cf.dwMask = CFM_COLOR;
	cf.crTextColor = color;
	SendMessageA(hRich, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
	SendMessageA(hRich, EM_REPLACESEL, TRUE, (LPARAM)text.c_str());
	// Restore read-only
	SendMessageA(hRich, EM_SETREADONLY, TRUE, 0);
}


void UiApp::invalidateFromLex() {
	tokensReady = false;
	lexerError.clear();
	if (parserPtr) { delete parserPtr; parserPtr = nullptr; }
	if (astRoot) { Parser::freeAST(astRoot); astRoot = nullptr; }
	parsedOk = false;
	symbolTable = SymbolTable{};
	semResult = SemanticResult{};
	semanticsDone = false;
	translatedPython.clear();
	translationDone = false;
	// Disable translate until semantics passes again
	if (hBtnTrans) EnableWindow(hBtnTrans, FALSE);
}

void UiApp::invalidateFromParseOnly() {
	if (parserPtr) { delete parserPtr; parserPtr = nullptr; }
	if (astRoot) { Parser::freeAST(astRoot); astRoot = nullptr; }
	parsedOk = false;
	symbolTable = SymbolTable{};
	semResult = SemanticResult{};
	semanticsDone = false;
	translatedPython.clear();
	translationDone = false;
	// Disable translate until semantics passes again
	if (hBtnTrans) EnableWindow(hBtnTrans, FALSE);
}

LRESULT CALLBACK UiApp::MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	UiApp* self = (UiApp*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (msg == WM_NCCREATE) {
		CREATESTRUCTA* cs = (CREATESTRUCTA*)lParam;
		self = (UiApp*)cs->lpCreateParams;
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)self);
		self->hMainWnd = hWnd;
		return TRUE;
	}
	if (!self) return DefWindowProcA(hWnd, msg, wParam, lParam);

	switch (msg) {
	case WM_CREATE: {
		self->createChildControls();
	} break;
	case WM_CTLCOLORSTATIC:
	case WM_CTLCOLOREDIT: {
		HDC hdc = (HDC)wParam;
		SetBkColor(hdc, self->bgColor);
		SetTextColor(hdc, self->fgColor);
		return (LRESULT)self->hBgBrush;
	}
	case WM_CTLCOLORBTN: {
		// Best-effort text color for buttons (may be ignored by themed buttons)
		HDC hdc = (HDC)wParam;
		SetBkColor(hdc, self->bgColor);
		SetTextColor(hdc, self->fgColor);
		return (LRESULT)self->hBgBrush;
	}
	case WM_CTLCOLORLISTBOX: {
		HDC hdc = (HDC)wParam;
		SetBkColor(hdc, self->bgColor);
		SetTextColor(hdc, self->fgColor);
		return (LRESULT)self->hBgBrush;
	}
	case WM_SIZE: {
		int cw = LOWORD(lParam);
		int ch = HIWORD(lParam);
		self->layoutControls(cw, ch);
	} break;
	case WM_COMMAND: {
		int id = LOWORD(wParam);
		switch (id) {
		case IDC_COMBO_AUTOMATA:
			if (HIWORD(wParam) == CBN_SELCHANGE) self->onAutomatonChanged();
			break;
		case IDC_BTN_LEX: self->onLex(); break;
		case IDC_BTN_PARSE: self->onParse(); break;
		case IDC_BTN_SEM: self->onSemantics(); break;
		case IDC_BTN_TRANS: self->onTranslate(); break;
		case IDC_BTN_RUNALL: self->onRunAll(); break;
		case IDC_BTN_SAVE: self->onSaveTranslation(); break;
		case IDC_BTN_THEME: {
			// Toggle theme
			self->darkMode = !self->darkMode;
			self->bgColor = self->darkMode ? RGB(24, 26, 32) : RGB(245, 248, 255);
			self->fgColor = self->darkMode ? RGB(235, 235, 235) : RGB(0, 0, 0);
			if (self->hBgBrush) DeleteObject(self->hBgBrush);
			self->hBgBrush = CreateSolidBrush(self->bgColor);
			// Update RichEdit background
			SendMessageA(self->hEditSemantics, EM_SETBKGNDCOLOR, 0, (LPARAM)self->bgColor);
			// Invalidate controls
			InvalidateRect(self->hEditSource, nullptr, TRUE);
			InvalidateRect(self->hEditTokens, nullptr, TRUE);
			InvalidateRect(self->hEditParse, nullptr, TRUE);
			InvalidateRect(self->hEditSemantics, nullptr, TRUE);
			InvalidateRect(self->hEditTrans, nullptr, TRUE);
			InvalidateRect(self->hTab, nullptr, TRUE);
			InvalidateRect(self->hNfaGraph, nullptr, TRUE);
			InvalidateRect(self->hDfaGraph, nullptr, TRUE);
			// Update button caption
			SetWindowTextA(self->hBtnTheme, self->darkMode ? "Light Mode" : "Dark Mode");
		} break;
		case IDC_CHK_TRACE: {
			self->traceEnabled = (SendMessageA(self->hChkTrace, BM_GETCHECK, 0, 0) == BST_CHECKED);
			self->recomputeTrace();
			InvalidateRect(self->hNfaGraph, nullptr, TRUE);
			InvalidateRect(self->hDfaGraph, nullptr, TRUE);
		} break;
		case IDC_BTN_STEP: {
			if (!self->traceEnabled) {
				self->traceEnabled = true;
				SendMessageA(self->hChkTrace, BM_SETCHECK, BST_CHECKED, 0);
			}
			if (self->traceIndex < 0) self->traceIndex = 0;
			else if (self->traceIndex < (int)self->sourceCode.size() - 1) self->traceIndex++;
			else self->traceIndex = -1; // full
			self->recomputeTrace();
			InvalidateRect(self->hNfaGraph, nullptr, TRUE);
			InvalidateRect(self->hDfaGraph, nullptr, TRUE);
		} break;
		case IDC_BTN_RESET: {
			self->traceIndex = 0;
			if (!self->traceEnabled) {
				self->traceEnabled = true;
				SendMessageA(self->hChkTrace, BM_SETCHECK, BST_CHECKED, 0);
			}
			self->recomputeTrace();
			InvalidateRect(self->hNfaGraph, nullptr, TRUE);
			InvalidateRect(self->hDfaGraph, nullptr, TRUE);
		} break;
		default: break;
		}
	} break;
	case WM_NOTIFY: {
		NMHDR* hdr = (NMHDR*)lParam;
		if (hdr->hwndFrom == self->hTab && hdr->code == TCN_SELCHANGE) {
			int cw, ch; RECT r; GetClientRect(hWnd, &r); cw = r.right - r.left; ch = r.bottom - r.top;
			self->layoutControls(cw, ch);
		}
	} break;
	case WM_DESTROY: {
		if (self->parserPtr) { delete self->parserPtr; self->parserPtr = nullptr; }
		if (self->astRoot) { Parser::freeAST(self->astRoot); self->astRoot = nullptr; }
		PostQuitMessage(0);
	} break;
	default:
		return DefWindowProcA(hWnd, msg, wParam, lParam);
	}
	return 0;
}

LRESULT CALLBACK UiApp::ParseGraphProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	UiApp* self = (UiApp*)GetWindowLongPtr(GetParent(hWnd), GWLP_USERDATA);
	switch (msg) {
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		RECT rc; GetClientRect(hWnd, &rc);
		if (self) self->drawParseTree(hdc, rc);
		EndPaint(hWnd, &ps);
		return 0;
	}
	default:
		return DefWindowProcA(hWnd, msg, wParam, lParam);
	}
}

void UiApp::drawParseTree(HDC hdc, RECT rcClient) {
	HBRUSH bg = CreateSolidBrush(bgColor);
	FillRect(hdc, &rcClient, bg);
	DeleteObject(bg);
	COLORREF oldTxt = SetTextColor(hdc, fgColor);

	if (!astRoot) {
		const char* hint = "Run Parse to visualize the AST";
		TextOutA(hdc, rcClient.left + 8, rcClient.top + 8, hint, (int)strlen(hint));
		return;
	}
	const int nodeR = 20;
	const int nodeW = nodeR * 2 + 4;
	const int hGap = 30;
	const int vGap = 80;

	auto colorFor = [](NodeType t) -> COLORREF {
		switch (t) {
		case N_PROGRAM: return RGB(99, 141, 255);
		case N_STMT_LIST: return RGB(170, 120, 255);
		case N_LET_STMT: return RGB(92, 201, 150);
		case N_PRINT_STMT: return RGB(52, 199, 204);
		case N_BIN_OP: return RGB(255, 170, 50);
		case N_IDENT: return RGB(80, 180, 255);
		case N_NUMBER: return RGB(255, 210, 70);
		default: return RGB(200, 200, 200);
		}
	};
	HPEN edgePen = CreatePen(PS_SOLID, 2, RGB(160, 180, 200));
	HGDIOBJ oldPen = SelectObject(hdc, edgePen);

	std::function<void(ASTNode*, int, int, int)> drawRec;
	drawRec = [&](ASTNode* n, int leftX, int topY, int availableW) {
		if (!n) return;
		ASTNode* kids[64]; int kc = 0;
		collectChildren(n, kids, kc);
		int cx = leftX + availableW / 2;
		int cy = topY + nodeR + 4;
		// node fill
		COLORREF c = colorFor(n->type);
		HBRUSH br = CreateSolidBrush(c);
		HGDIOBJ oldBr = SelectObject(hdc, br);
		Ellipse(hdc, cx - nodeR, cy - nodeR, cx + nodeR, cy + nodeR);
		SelectObject(hdc, oldBr);
		DeleteObject(br);
		// label
		std::string lab; nodeLabel(n, lab);
		COLORREF prevText = SetTextColor(hdc, RGB(0, 0, 0));
		drawCenteredText(hdc, cx, cy, lab);
		SetTextColor(hdc, prevText);
		if (kc == 0) return;
		int childW[64];
		int total = 0;
		for (int i = 0; i < kc; ++i) {
			childW[i] = subtreeWidthPx(kids[i], nodeW, hGap);
			total += childW[i];
			if (i + 1 < kc) total += hGap;
		}
		int startX = cx - total / 2;
		for (int i = 0; i < kc; ++i) {
			int cw = childW[i];
			int ccx = startX + cw / 2;
			int ccy = cy + vGap;
			MoveToEx(hdc, cx, cy + nodeR, nullptr);
			LineTo(hdc, ccx, ccy - nodeR);
			drawRec(kids[i], startX, cy + vGap - nodeR, cw);
			startX += cw + hGap;
		}
	};

	int totalW = subtreeWidthPx(astRoot, nodeW, hGap);
	if (totalW > (rcClient.right - rcClient.left - 16)) totalW = rcClient.right - rcClient.left - 16;
	int left = rcClient.left + (rcClient.right - rcClient.left - totalW) / 2;
	int top = rcClient.top + 16;
	drawRec(astRoot, left, top, totalW);

	SelectObject(hdc, oldPen);
	DeleteObject(edgePen);
	SetTextColor(hdc, oldTxt);
}

LRESULT CALLBACK UiApp::NfaGraphProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	UiApp* self = (UiApp*)GetWindowLongPtr(GetParent(hWnd), GWLP_USERDATA);
	switch (msg) {
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		RECT rc; GetClientRect(hWnd, &rc);
		if (self) self->drawNfa(hdc, rc);
		EndPaint(hWnd, &ps);
		return 0;
	}
	default:
		return DefWindowProcA(hWnd, msg, wParam, lParam);
	}
}

void UiApp::drawNfa(HDC hdc, RECT rcClient) {
	HBRUSH bg = CreateSolidBrush(bgColor);
	FillRect(hdc, &rcClient, bg);
	DeleteObject(bg);
	COLORREF oldTxt = SetTextColor(hdc, fgColor);

	int w = rcClient.right - rcClient.left;
	int h = rcClient.bottom - rcClient.top;
	if (nfa.numStates <= 0 || w <= 0 || h <= 0) return;

	int cx = rcClient.left + w / 2;
	int cy = rcClient.top + h / 2;
	int radius = (w < h ? w : h) / 2 - 40;
	if (radius < 40) radius = 40;

	POINT pos[MAX_STATES];
	for (int i = 0; i < nfa.numStates; ++i) {
		double ang = (2.0 * 3.1415926535 * i) / (double)nfa.numStates;
		pos[i].x = cx + (LONG)(radius * cos(ang));
		pos[i].y = cy + (LONG)(radius * sin(ang));
	}

	// Solid for normal transitions, dotted for epsilon.
	HPEN penEdge = CreatePen(PS_SOLID, 1, RGB(160, 160, 200));
	HPEN penTrace = CreatePen(PS_SOLID, 2, RGB(220, 80, 80));
	HPEN penEps = CreatePen(PS_DOT, 1, RGB(120, 120, 200));
	HPEN penEpsTrace = CreatePen(PS_DOT, 1, RGB(220, 80, 80));
	HGDIOBJ oldPen = SelectObject(hdc, penEdge);

	for (int s = 0; s < nfa.numStates; ++s) {
		for (int ic = 0; ic <= NUM_INPUT_CLASSES; ++ic) {
			const NFATransitions& tr = nfa.trans[s][ic];
			for (int k = 0; k < tr.count; ++k) {
				int ns = tr.dest[k];
				bool isEps = (ic == NUM_INPUT_CLASSES);
				bool traced = (ic >= 0 && ic <= NUM_INPUT_CLASSES) ? usedEdge[s][ic] : false;
				SelectObject(hdc, isEps ? (traced ? penEpsTrace : penEps) : (traced ? penTrace : penEdge));
				if (ns == s) {
					const char* lbl = isEps ? "eps" : inputClassName(ic);
					drawSelfLoop(hdc, pos[s].x, pos[s].y, 20, lbl);
				} else {
					MoveToEx(hdc, pos[s].x, pos[s].y, nullptr);
					LineTo(hdc, pos[ns].x, pos[ns].y);
					int mx = (pos[s].x + pos[ns].x) / 2;
					int my = (pos[s].y + pos[ns].y) / 2;
					const char* lbl = isEps ? "eps" : inputClassName(ic);
					TextOutA(hdc, mx, my, lbl, (int)strlen(lbl));
				}
			}
		}
	}
	SelectObject(hdc, oldPen);
	DeleteObject(penEdge);
	DeleteObject(penTrace);
	DeleteObject(penEps);
	DeleteObject(penEpsTrace);

	for (int s = 0; s < nfa.numStates; ++s) {
		RECT r = { pos[s].x - 20, pos[s].y - 20, pos[s].x + 20, pos[s].y + 20 };
		Ellipse(hdc, r.left, r.top, r.right, r.bottom);
		if (nfa.acceptingStates[s]) {
			RECT r2 = { pos[s].x - 16, pos[s].y - 16, pos[s].x + 16, pos[s].y + 16 };
			Ellipse(hdc, r2.left, r2.top, r2.right, r2.bottom);
		}
		char name[16]; snprintf(name, sizeof(name), "S%d", s);
		TextOutA(hdc, r.left + 6, r.top + 6, name, (int)strlen(name));
	}

	// legend
	drawLegend(hdc, rcClient.left + 8, rcClient.top + 8);
	// start arrow to S0
	drawStartArrow(hdc, pos[0].x, pos[0].y);
	SetTextColor(hdc, oldTxt);
}

LRESULT CALLBACK UiApp::DfaGraphProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	UiApp* self = (UiApp*)GetWindowLongPtr(GetParent(hWnd), GWLP_USERDATA);
	switch (msg) {
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		RECT rc; GetClientRect(hWnd, &rc);
		if (self) self->drawDfa(hdc, rc);
		EndPaint(hWnd, &ps);
		return 0;
	}
	default:
		return DefWindowProcA(hWnd, msg, wParam, lParam);
	}
}

void UiApp::drawDfa(HDC hdc, RECT rcClient) {
	HBRUSH bg = CreateSolidBrush(bgColor);
	FillRect(hdc, &rcClient, bg);
	DeleteObject(bg);
	COLORREF oldTxt = SetTextColor(hdc, fgColor);

	if (!selectedAutomaton) return;
	const Automaton& a = dfaFromNfa.numStates > 0 ? dfaFromNfa : *selectedAutomaton;
	int w = rcClient.right - rcClient.left;
	int h = rcClient.bottom - rcClient.top;
	if (a.numStates <= 0 || w <= 0 || h <= 0) return;

	int cx = rcClient.left + w / 2;
	int cy = rcClient.top + h / 2;
	int radius = (w < h ? w : h) / 2 - 40;
	if (radius < 40) radius = 40;

	POINT pos[MAX_STATES];
	for (int i = 0; i < a.numStates; ++i) {
		double ang = (2.0 * 3.1415926535 * i) / (double)a.numStates;
		pos[i].x = cx + (LONG)(radius * cos(ang));
		pos[i].y = cy + (LONG)(radius * sin(ang));
	}

	HPEN penEdge = CreatePen(PS_SOLID, 1, RGB(150, 180, 160));
	HPEN penTrace = CreatePen(PS_SOLID, 2, RGB(220, 80, 80));
	HGDIOBJ oldPen = SelectObject(hdc, penEdge);

	for (int s = 0; s < a.numStates; ++s) {
		for (int ic = 0; ic < NUM_INPUT_CLASSES; ++ic) {
			int ns = a.transitionTable[s][ic];
			if (ns < 0) continue;
			bool traced = usedEdge[s][ic];
			SelectObject(hdc, traced ? penTrace : penEdge);
			if (ns == s) {
				const char* lbl = inputClassName(ic);
				drawSelfLoop(hdc, pos[s].x, pos[s].y, 20, lbl);
			} else {
				MoveToEx(hdc, pos[s].x, pos[s].y, nullptr);
				LineTo(hdc, pos[ns].x, pos[ns].y);
				int mx = (pos[s].x + pos[ns].x) / 2;
				int my = (pos[s].y + pos[ns].y) / 2;
				const char* lbl = inputClassName(ic);
				TextOutA(hdc, mx, my, lbl, (int)strlen(lbl));
			}
		}
	}
	SelectObject(hdc, oldPen);
	DeleteObject(penEdge);
	DeleteObject(penTrace);

	for (int s = 0; s < a.numStates; ++s) {
		RECT r = { pos[s].x - 20, pos[s].y - 20, pos[s].x + 20, pos[s].y + 20 };
		Ellipse(hdc, r.left, r.top, r.right, r.bottom);
		if (a.acceptingStates[s]) {
			RECT r2 = { pos[s].x - 16, pos[s].y - 16, pos[s].x + 16, pos[s].y + 16 };
			Ellipse(hdc, r2.left, r2.top, r2.right, r2.bottom);
		}
		char name[16]; snprintf(name, sizeof(name), "S%d", s);
		TextOutA(hdc, r.left + 6, r.top + 6, name, (int)strlen(name));
	}
	// legend and start marker
	drawLegend(hdc, rcClient.left + 8, rcClient.top + 8);
	drawStartArrow(hdc, pos[0].x, pos[0].y);
	SetTextColor(hdc, oldTxt);
}


