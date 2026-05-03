#include "D3DApp.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    D3DApp app(hInstance);
    if (!app.Init())
        return -1;
    return app.Run();
}
