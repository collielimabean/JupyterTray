#include <windows.h>
#include <shellapi.h>
#include <string>

#ifndef UNICODE
#define UNICODE
#endif

constexpr const wchar_t *CLASS_NAME = L"_jupyter_tray";
constexpr const wchar_t *TOOLTIP_TEXT = L"Jupyter Tray";
constexpr const int WM_TRAYICON = WM_USER + 1;
constexpr const wchar_t *JULIAPRO_HOME_ENV_VAR = L"JULIAPRO_HOME";
constexpr const int CONTEXT_MENU_EXIT_CMD = 1;
constexpr const int CONTEXT_MENU_SHOW_CONSOLE = 2;

constexpr const wchar_t *RELATIVE_ICON_PATH = L"\\Icons\\Jupyter.ico";
constexpr const wchar_t *RELATIVE_EXEC_DIR = L"\\Python\\Scripts\\";
constexpr const wchar_t *EXEC_NAME = L"jupyter.exe";
constexpr const wchar_t *CMD_ARGS = L" notebook --notebook-dir=";


NOTIFYICONDATA g_notifyIconData = {};
HMENU g_hPopupMenu = {};
BOOL g_jupyterStarted = FALSE;
PROCESS_INFORMATION g_jupyterServer = {};
std::wstring g_juliaPath;


BOOL GetEnvVarWstring(const std::wstring& variable, std::wstring& value)
{
    DWORD bufsz;
    wchar_t *buffer = nullptr;

    // first, get the size of the env variable
    bufsz = GetEnvironmentVariable(
        variable.c_str(),
        nullptr,
        0
    );

    if ((bufsz == 0) && GetLastError() == ERROR_ENVVAR_NOT_FOUND)
        return FALSE;

    buffer = new wchar_t[bufsz];

    // actually retrieve the variable
    GetEnvironmentVariable(
        variable.c_str(),
        buffer,
        bufsz
    );

    value = buffer;
    if (buffer)
        delete[] buffer;

    return TRUE;
}

BOOL StartJupyter()
{
    STARTUPINFO si;
    std::wstring jupyterArgv;
    std::wstring jupyterStartDir;
    std::wstring userProfileDir;

    memset(&si, 0, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);

    GetEnvVarWstring(L"USERPROFILE", userProfileDir);
    jupyterStartDir = g_juliaPath + RELATIVE_EXEC_DIR;
    jupyterArgv = jupyterStartDir + EXEC_NAME + CMD_ARGS + userProfileDir;

    return CreateProcess(
        nullptr,
        &jupyterArgv[0],
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        jupyterStartDir.c_str(),
        &si,
        &g_jupyterServer
    );
}

void JupyterTrayExit()
{
    if (g_jupyterStarted)
        system("taskkill /F /T /IM jupyter.exe /IM jupyter-notebook.exe");

    CloseHandle(g_jupyterServer.hProcess);
    CloseHandle(g_jupyterServer.hThread);
    DestroyMenu(g_hPopupMenu);
    Shell_NotifyIcon(NIM_DELETE, &g_notifyIconData);
    PostQuitMessage(0);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        g_hPopupMenu = CreatePopupMenu();
        InsertMenu(g_hPopupMenu, 0, MF_BYPOSITION | MF_STRING, CONTEXT_MENU_EXIT_CMD, L"Exit");
        break;

    case WM_TRAYICON:
        switch (lParam)
        {
        case WM_RBUTTONDOWN:
        case WM_CONTEXTMENU:
        {
            POINT pt = {};

            SetForegroundWindow(hwnd);
            GetCursorPos(&pt);
            TrackPopupMenu(g_hPopupMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
        }
        break;
        }

        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case CONTEXT_MENU_EXIT_CMD:
            JupyterTrayExit();
            break;
        }
        break;

    case WM_CLOSE:
        break;

    case WM_DESTROY:
        JupyterTrayExit();
        break;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow)
{
    WNDCLASSEX wnd = {};
    HWND hWnd = {};
    HICON hIcon = {};
    GUID notifyIconGuid = {};
    std::wstring iconPath;

    // get julia information
    if (!GetEnvVarWstring(JULIAPRO_HOME_ENV_VAR, g_juliaPath))
        FatalAppExit(0, L"Failed to find JuliaPro - is JULIAPRO_HOME set?");

    // initialize window 
    wnd.hInstance = hInstance;
    wnd.lpszClassName = CLASS_NAME;
    wnd.lpfnWndProc = WindowProc;
    wnd.style = CS_HREDRAW | CS_VREDRAW;
    wnd.cbSize = sizeof(WNDCLASSEX);
    wnd.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wnd.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    wnd.hCursor = LoadCursor(NULL, IDC_ARROW);
    wnd.hbrBackground = (HBRUSH)COLOR_APPWORKSPACE;

    if (!RegisterClassEx(&wnd))
        FatalAppExit(0, L"Failed to start Jupyter Tray!");

    hWnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Jupyter Tray",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        0,
        0,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    // load jupyter icon
    iconPath = g_juliaPath + RELATIVE_ICON_PATH;

    hIcon = (HICON)LoadImage(
        nullptr,
        iconPath.c_str(),
        IMAGE_ICON,
        0,
        0,
        LR_LOADFROMFILE
    );

    if (!hIcon)
        FatalAppExit(0, L"Failed to get icon!");

    // get guid for our app
    if (FAILED(CoCreateGuid(&notifyIconGuid)))
        FatalAppExit(0, L"Failed to generate guid!");

    // initialize notify icon
    g_notifyIconData.cbSize = sizeof(NOTIFYICONDATA_V2_SIZE);
    g_notifyIconData.hWnd = hWnd;
    g_notifyIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_GUID | NIF_TIP;
    g_notifyIconData.uCallbackMessage = WM_TRAYICON;
    g_notifyIconData.hIcon = hIcon;
    g_notifyIconData.guidItem = notifyIconGuid;
    lstrcpyn(g_notifyIconData.szTip, TOOLTIP_TEXT, 64);

    // show the icon
    Shell_NotifyIcon(NIM_ADD, &g_notifyIconData);

    // start jupyter
    g_jupyterStarted = StartJupyter();

    // start message pump
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
