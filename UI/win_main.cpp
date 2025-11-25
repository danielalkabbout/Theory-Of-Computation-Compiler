#include "ui_app.h"

#ifdef _WIN32
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
	UiApp app;
	if (!app.run(hInstance, nCmdShow)) {
		return 1;
	}
	return 0;
}
#endif


