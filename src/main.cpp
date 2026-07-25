#include "Application.h"

#ifdef _WIN32
#include <windows.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    Application app;
    return app.run();
}
#else

int main(int argc, char **argv) {
    Application app;
    if (argc > 1) app.setInitialFile(argv[1]);
    return app.run();
}
#endif
