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
static const char* kAutoGraphClass = "TC_GUI_AUTOGRAPH";

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
	wc.lpfnWndProc = UiApp::MainWndProc;
	wc.hInstance = hInst;
	wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszClassName = kMainWndClass;
	if (!RegisterClassA(&wc)) return false;

	WNDCLASSA wcAuto = {};
	wcAuto.style = CS_HREDRAW | CS_VREDRAW;
	wcAuto.lpfnWndProc = UiApp::AutoGraphProc;
	wcAuto.hInstance = hInst;
	wcAuto.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcAuto.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcAuto.lpszClassName = kAutoGraphClass;
	if (!RegisterClassA(&wcAuto)) return false;

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
	item.pszText = (LPSTR)"Translation"; TabCtrl_InsertItem(hTab, 2, &item);

	hEditSource = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
		WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL | WS_HSCROLL,
		0, 0, 100, 100, hMainWnd, (HMENU)IDC_EDIT_SOURCE, hInst, nullptr);
	hEditTokens = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
		WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL | WS_HSCROLL,
		0, 0, 100, 100, hMainWnd, (HMENU)IDC_EDIT_TOKENS, hInst, nullptr);
	hEditParse = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
		WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL | WS_HSCROLL,
		0, 0, 100, 100, hMainWnd, (HMENU)IDC_EDIT_PARSE, hInst, nullptr);
	hEditTrans = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
		WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL | WS_HSCROLL,
		0, 0, 100, 100, hMainWnd, (HMENU)IDC_EDIT_TRANS, hInst, nullptr);

	hAutoGraph = CreateWindowExA(0, kAutoGraphClass, "",
		WS_CHILD | WS_VISIBLE, 0, 0, 100, 100, hMainWnd, (HMENU)IDC_AUTOGRAPH, hInst, this);

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
	int graphW = 360;
	int rightW = clientW - (pad + leftW + pad + graphW + pad);
	if (rightW < 300) rightW = 300;

	MoveWindow(hEditSource, pad, y, leftW, clientH - y - pad, TRUE);

	MoveWindow(hTab, pad + leftW + pad, y, rightW, clientH - y - pad, TRUE);
	RECT rc; GetClientRect(hTab, &rc);
	TabCtrl_AdjustRect(hTab, FALSE, &rc);
	int tabX = rc.left, tabY = rc.top, tabW = rc.right - rc.left, tabH = rc.bottom - rc.top;
	MoveWindow(hEditTokens, pad + leftW + pad + tabX, y + tabY, tabW, tabH, TRUE);
	MoveWindow(hEditParse, pad + leftW + pad + tabX, y + tabY, tabW, tabH, TRUE);
	MoveWindow(hEditTrans, pad + leftW + pad + tabX, y + tabY, tabW, tabH, TRUE);

	int sel = TabCtrl_GetCurSel(hTab);
	ShowWindow(hEditTokens, (sel == 0) ? SW_SHOW : SW_HIDE);
	ShowWindow(hEditParse, (sel == 1) ? SW_SHOW : SW_HIDE);
	ShowWindow(hEditTrans, (sel == 2) ? SW_SHOW : SW_HIDE);

	int autoX = pad + leftW + pad + rightW + pad;
	MoveWindow(hAutoGraph, autoX, y, graphW, clientH - y - pad, TRUE);
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
	if (!parsedOk || !astRoot) return;
	symbolTable = SymbolTable{};
	semResult = analyzeSemantics(astRoot, symbolTable);
	semanticsDone = true;

	std::string res = "[Semantic Analysis]\r\n";
	if (!semResult.ok) {
		char buf[64]; snprintf(buf, sizeof(buf), "Errors: %d\r\n", semResult.errorCount);
		res += buf;
	} else {
		res += "OK (no errors)\r\n";
	}
	int curLen = GetWindowTextLengthA(hEditParse);
	std::unique_ptr<char[]> curbuf(new char[curLen + 1]);
	GetWindowTextA(hEditParse, curbuf.get(), curLen + 1);
	std::string currentText(curbuf.get(), curLen);
	currentText += "\r\n";
	currentText += res;
	setEditText(hEditParse, currentText);

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
	setEditText(hEditTrans, translatedPython);
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
	InvalidateRect(hAutoGraph, nullptr, TRUE);
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

void UiApp::drawAutomaton(HDC hdc, RECT rcClient) {
	if (!selectedAutomaton) return;
	int w = rcClient.right - rcClient.left;
	int h = rcClient.bottom - rcClient.top;
	int cx = rcClient.left + w / 2;
	int cy = rcClient.top + h / 2;
	int radius = (w < h ? w : h) / 2 - 40;
	if (radius < 40) radius = 40;

	const Automaton& a = *selectedAutomaton;
	POINT pos[MAX_STATES];
	int n = a.numStates;
	for (int i = 0; i < n; ++i) {
		double ang = (2.0 * 3.1415926535 * i) / (double)n;
		pos[i].x = cx + (LONG)(radius * cos(ang));
		pos[i].y = cy + (LONG)(radius * sin(ang));
	}

	HPEN penEdge = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
	HGDIOBJ oldPen = SelectObject(hdc, penEdge);
	for (int s = 0; s < n; ++s) {
		for (int ic = 0; ic < NUM_INPUT_CLASSES; ++ic) {
			int ns = a.transitionTable[s][ic];
			if (ns >= 0 && ns < n) {
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

	for (int s = 0; s < n; ++s) {
		RECT r = { pos[s].x - 20, pos[s].y - 20, pos[s].x + 20, pos[s].y + 20 };
		Ellipse(hdc, r.left, r.top, r.right, r.bottom);
		if (isAccepting(a, s)) {
			RECT r2 = { pos[s].x - 16, pos[s].y - 16, pos[s].x + 16, pos[s].y + 16 };
			Ellipse(hdc, r2.left, r2.top, r2.right, r2.bottom);
		}
		char name[16]; snprintf(name, sizeof(name), "S%d", s);
		TextOutA(hdc, r.left + 6, r.top + 6, name, (int)strlen(name));
	}
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

LRESULT CALLBACK UiApp::AutoGraphProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	UiApp* self = (UiApp*)GetWindowLongPtr(GetParent(hWnd), GWLP_USERDATA);
	switch (msg) {
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		RECT rc; GetClientRect(hWnd, &rc);
		if (self) self->drawAutomaton(hdc, rc);
		EndPaint(hWnd, &ps);
		return 0;
	}
	default:
		return DefWindowProcA(hWnd, msg, wParam, lParam);
	}
}


