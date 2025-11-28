#include "ui_app.h"
#include "lexer.h"
#include <cstdio>
#include <cmath>
#include <functional>
#include <memory>

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


static const char* kMainWndClass = "TC_GUI_MAIN";
static const char* kParseGraphClass = "TC_GUI_PARSEGRAPH";

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
static std::string toCRLF(const std::string& s) {
	std::string out;
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

	hTab = CreateWindowExA(0, WC_TABCONTROLA, "",
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
		0, 0, 100, 100, hMainWnd, (HMENU)IDC_TAB, hInst, nullptr);

	TCITEMA item = {}; item.mask = TCIF_TEXT;
	item.pszText = (LPSTR)"Tokens"; TabCtrl_InsertItem(hTab, 0, &item);
	item.pszText = (LPSTR)"Parse Tree"; TabCtrl_InsertItem(hTab, 1, &item);
	item.pszText = (LPSTR)"Semantics"; TabCtrl_InsertItem(hTab, 2, &item);
	item.pszText = (LPSTR)"Translation"; TabCtrl_InsertItem(hTab, 3, &item);

	hEditSource = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
		WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL | WS_HSCROLL,
		0, 0, 100, 100, hMainWnd, (HMENU)IDC_EDIT_SOURCE, hInst, nullptr);
	hEditTokens = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
		WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL | WS_HSCROLL,
		0, 0, 100, 100, hMainWnd, (HMENU)IDC_EDIT_TOKENS, hInst, nullptr);
	hEditParse = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
		WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL | WS_HSCROLL,
		0, 0, 100, 100, hMainWnd, (HMENU)IDC_EDIT_PARSE, hInst, nullptr);
	hEditSemantics = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
		WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL | WS_HSCROLL,
		0, 0, 100, 100, hMainWnd, nullptr, hInst, nullptr);
	hEditTrans = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
		WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL | WS_HSCROLL,
		0, 0, 100, 100, hMainWnd, (HMENU)IDC_EDIT_TRANS, hInst, nullptr);

	hParseGraph = CreateWindowExA(0, kParseGraphClass, "",
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
	MoveWindow(hBtnSave, x, pad, 80, topH, TRUE);

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

	int sel = TabCtrl_GetCurSel(hTab);
	ShowWindow(hEditTokens, (sel == 0) ? SW_SHOW : SW_HIDE);
	ShowWindow(hEditParse, SW_HIDE);
	ShowWindow(hParseGraph, (sel == 1) ? SW_SHOW : SW_HIDE);
	ShowWindow(hEditSemantics, (sel == 2) ? SW_SHOW : SW_HIDE);
	ShowWindow(hEditTrans, (sel == 3) ? SW_SHOW : SW_HIDE);

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
		return;
	}
	tokensReady = true;
	invalidateFromParseOnly();

	std::string tokText;
	appendTokensTo(tokText);
	setEditText(hEditTokens, tokText);
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

	std::string res = "[Semantic Analysis]\r\n";
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
	} else {
		res += "OK (no errors)\r\n";
	}
	std::string crlf = toCRLF(res);
	setEditText(hEditSemantics, crlf);
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
	std::string crlf = toCRLF(translatedPython);
	setEditText(hEditTrans, crlf);
}

void UiApp::onRunAll() {
	onLex();
	if (!tokensReady) return;
	onParse();
	if (!parsedOk) return;
	onSemantics();
	if (semanticsDone && !semResult.ok) return;
	onTranslate();
}

void UiApp::onSaveTranslation() {
	if (!translationDone) return;
	FILE* f = nullptr;
	fopen_s(&f, "translation.py", "wb");
	if (!f) return;
	fwrite(translatedPython.data(), 1, translatedPython.size(), f);
	fclose(f);
	MessageBoxA(hMainWnd, "Saved translation to translation.py", "Save", MB_OK | MB_ICONINFORMATION);
}

void UiApp::onAutomatonChanged() {
	int sel = (int)SendMessageA(hComboAutomata, CB_GETCURSEL, 0, 0);
	if (sel < 0) return;
	int ids[MAX_AUTOMATA]; int cnt = automatonManager.listIds(ids, MAX_AUTOMATA);
	if (sel >= cnt) return;
	selectedAutomaton = automatonManager.findById(ids[sel]);
	// nothing to redraw on the right
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
	HBRUSH bg = CreateSolidBrush(RGB(245, 248, 255));
	FillRect(hdc, &rcClient, bg);
	DeleteObject(bg);

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
		drawCenteredText(hdc, cx, cy, lab);
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
}


