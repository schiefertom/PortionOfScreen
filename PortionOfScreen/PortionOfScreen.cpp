// PortionOfScreen.cpp : Defines the entry point for the application.
//
// Terms (Share Region, Window, Size Preset, Free, Locked, Title Bar) are defined in ../CONTEXT.md.

#include "framework.h"
#include "PortionOfScreen.h"
#include "WinReg.hpp"
#include <windowsx.h>
#include <cstdio>

#define MAX_LOADSTRING 100
#define IDT_REDRAW     101
#define POS_MIN_WIDTH  320
#define POS_MIN_HEIGHT 200
#define POS_MAX_SIZE   16384

// System menu command IDs. The four low-order bits of a WM_SYSCOMMAND wParam are used by the
// system, so application IDs keep them zero and the handler masks wParam with 0xFFF0.
#define IDC_OPTIONS       0x1000
#define IDC_HIDE_CAPTION  0x1010
#define IDC_SIZE_FIRST    0x1100   // Free; preset n is IDC_SIZE_FIRST + n * IDC_SIZE_STEP
#define IDC_SIZE_STEP     0x10

// Window styles with and without the Title Bar.
#define WINDOW_STYLE_CAPTION     (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MAXIMIZEBOX)
#define WINDOW_STYLE_NO_CAPTION  (WS_POPUP)

// Size Presets: the Share Region in physical pixels. Index 0 is Free, the last index is Custom.
struct SizePreset { int width; int height; const wchar_t* label; };
const SizePreset SIZE_PRESETS[] = {
    { 1280,  720, L"1280 × 720" },
    { 1600,  900, L"1600 × 900" },
    { 1920, 1080, L"1920 × 1080" },
    { 1920, 1200, L"1920 × 1200" },
    { 2560, 1440, L"2560 × 1440" },
};
const int SIZE_PRESET_COUNT = sizeof(SIZE_PRESETS) / sizeof(SIZE_PRESETS[0]);
const int SIZE_FREE = 0;
const int SIZE_CUSTOM = SIZE_PRESET_COUNT + 1;
#define IDC_SIZE_CUSTOM   (IDC_SIZE_FIRST + SIZE_CUSTOM * IDC_SIZE_STEP)

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
bool moveToDefaultWindowPos = false;
HWND newFocusHwnd;
unsigned int focusTime;
HMENU hSizeMenu;                                // "Size" submenu of the system menu
int sizeMenuPosition;                           // its position in the system menu
bool captionHidden = false;                     // Title Bar hidden via the system menu

// Global settings
bool focusMode =  false;
RECT defaultWindowPos;
int sizePreset = SIZE_FREE;                     // SIZE_FREE, 1..SIZE_PRESET_COUNT or SIZE_CUSTOM
int customWidth = 1920;
int customHeight = 1080;

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    CustomSize(HWND, UINT, WPARAM, LPARAM);
void                LoadSettings();
void                SaveSettings();
bool                GetPresetSize(int preset, int& width, int& height);
RECT                GetShareRegion(HWND hWnd);
void                SetShareRegion(HWND hWnd, RECT region);
bool                ApplySizePreset(HWND hWnd, int preset);
void                ShowCaption(HWND hWnd, bool show);
void                UpdateTitle(HWND hWnd);
void                UpdateSystemMenu(HWND hWnd);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // DPI awareness (Per-Monitor V2) is declared in PortionOfScreen.manifest, so that all
    // coordinates in this program are physical pixels on every monitor.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_PORTIONOFSCREEN, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    HICON hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PORTIONOFSCREEN));

    WNDCLASSEXW wcex;
    wcex.cbSize         = sizeof(WNDCLASSEX);
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = hIcon;
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = nullptr;
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = hIcon;

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   LoadSettings();

   // Create the window Free; the saved preset is applied below, once the window knows its monitor.
   int startupPreset = sizePreset;
   sizePreset = SIZE_FREE;

   HWND hWnd = CreateWindowExW(
       WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TRANSPARENT,
       szWindowClass,
       szTitle,
       WINDOW_STYLE_CAPTION,
       defaultWindowPos.left,
       defaultWindowPos.top,
       defaultWindowPos.right - defaultWindowPos.left,
       defaultWindowPos.bottom - defaultWindowPos.top,
       nullptr,
       nullptr,
       hInstance,
       nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   hSizeMenu = CreatePopupMenu();
   AppendMenu(hSizeMenu, MF_STRING, IDC_SIZE_FIRST, L"Free");
   for (int i = 0; i < SIZE_PRESET_COUNT; ++i)
       AppendMenu(hSizeMenu, MF_STRING, IDC_SIZE_FIRST + (i + 1) * IDC_SIZE_STEP, SIZE_PRESETS[i].label);
   AppendMenu(hSizeMenu, MF_STRING, IDC_SIZE_CUSTOM, L"Custom...");

   HMENU hSysMenu = GetSystemMenu(hWnd, FALSE);
   AppendMenu(hSysMenu, MF_SEPARATOR, 0, NULL);
   sizeMenuPosition = GetMenuItemCount(hSysMenu);
   AppendMenu(hSysMenu, MF_POPUP, (UINT_PTR) hSizeMenu, L"Size");
   AppendMenu(hSysMenu, MF_STRING, IDC_HIDE_CAPTION, L"Hide title bar");
   AppendMenu(hSysMenu, MF_STRING, IDC_OPTIONS, L"Options");

   ShowWindow(hWnd, nCmdShow);
   SetLayeredWindowAttributes(hWnd, RGB(255, 255, 255), 128, LWA_ALPHA);
   UpdateWindow(hWnd);

   if (!focusMode)
       ApplySizePreset(hWnd, startupPreset);   // stays Free, with a message, if it does not fit
   else
       sizePreset = startupPreset;             // kept for when the user returns to Fixed Mode

   SetTimer(hWnd, IDT_REDRAW, 200, (TIMERPROC)NULL);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_ACTIVATE:
        // The Title Bar returns whenever the Window is activated (taskbar, Alt+Tab).
        if (captionHidden && LOWORD(wParam) != WA_INACTIVE)
            ShowCaption(hWnd, true);
        return DefWindowProc(hWnd, message, wParam, lParam);

    case WM_LBUTTONUP:
        SetWindowPos(hWnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
        break;

    case WM_SETFOCUS:
        if (focusMode)
        {
            // The window could be anywhere in Focus Mode, even over the taskbar. Quickly move window to it's original position.
            SetWindowPos(hWnd, HWND_TOPMOST, defaultWindowPos.left, defaultWindowPos.top, defaultWindowPos.right - defaultWindowPos.left, 0, 0);
        }
        else if (moveToDefaultWindowPos)
        {
            SetWindowPos(hWnd, HWND_TOPMOST, defaultWindowPos.left, defaultWindowPos.top, defaultWindowPos.right - defaultWindowPos.left, defaultWindowPos.bottom - defaultWindowPos.top, 0);
            moveToDefaultWindowPos = false;
            // Back in Fixed Mode: re-apply the preset, which Focus Mode ignored.
            ApplySizePreset(hWnd, sizePreset);
        }

        SetLayeredWindowAttributes(hWnd, RGB(255, 255, 255), 128, LWA_ALPHA);
        SetWindowLong(hWnd, GWL_EXSTYLE, WS_EX_LAYERED | WS_EX_TOPMOST);
        return DefWindowProc(hWnd, message, wParam, lParam);

    case WM_KILLFOCUS:
        SetLayeredWindowAttributes(hWnd, RGB(255, 255, 255), 0, LWA_ALPHA);
        SetWindowLong(hWnd, GWL_EXSTYLE, WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TRANSPARENT);
        return DefWindowProc(hWnd, message, wParam, lParam);

    case WM_SIZE:
    case WM_MOVE:
    {
        LRESULT result = DefWindowProc(hWnd, message, wParam, lParam);

        // While the Title Bar is hidden the window rect equals the Share Region; do not remember
        // it as the default position, which is always a rect with the Title Bar.
        if (GetForegroundWindow() == hWnd && !captionHidden)
        {
            int prevBottom = defaultWindowPos.bottom;
            GetWindowRect(hWnd, &defaultWindowPos);
            if (focusMode) defaultWindowPos.bottom = prevBottom;
        }

        if (message == WM_SIZE)
            UpdateTitle(hWnd);

        return result;
    }

    case WM_DPICHANGED:
        // Keep the Share Region at its physical size; only the frame is recomputed for the new DPI.
        SetShareRegion(hWnd, GetShareRegion(hWnd));
        return 0;

    case WM_INITMENUPOPUP:
        UpdateSystemMenu(hWnd);
        return DefWindowProc(hWnd, message, wParam, lParam);

    case WM_CONTEXTMENU:
    {
        // Right-click anywhere in the Window (or the menu key) opens the system menu, so that
        // the whole Share Region is a click target and not only the small title bar icon.
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (lParam == -1)
        {
            RECT region = GetShareRegion(hWnd);
            pt.x = (region.left + region.right) / 2;
            pt.y = (region.top + region.bottom) / 2;
        }
        UINT command = TrackPopupMenu(GetSystemMenu(hWnd, FALSE), TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, nullptr);
        if (command)
            SendMessage(hWnd, WM_SYSCOMMAND, command, MAKELPARAM(pt.x, pt.y));
        return 0;
    }

    case WM_TIMER:
        switch (wParam)
        {
        case IDT_REDRAW:
            if (focusMode)
            {
                HWND hwndForeground = GetForegroundWindow();
                if (hwndForeground != hWnd)
                {
                    // Add a slight delay between activating a window and moving the PoS window.
                    // This makes changing the focused window smoother, less jumpy.
                    if (newFocusHwnd != hwndForeground)
                    {
                        newFocusHwnd = hwndForeground;
                        focusTime = 0;
                    }
                    else
                        ++focusTime;

                    if (focusTime < 3)
                        break;

                    RECT rectForeground;
                    GetWindowRect(hwndForeground, &rectForeground);

                    RECT rectPoS;
                    GetWindowRect(hWnd, &rectPoS);

                    if (rectPoS.left != rectForeground.left ||
                        rectPoS.top != rectForeground.top ||
                        rectPoS.right != rectForeground.right ||
                        rectPoS.bottom != rectForeground.bottom)
                    {
                        SetWindowPos(hWnd, HWND_TOPMOST, rectForeground.left, rectForeground.top, rectForeground.right - rectForeground.left, rectForeground.bottom - rectForeground.top, SWP_NOACTIVATE);
                    }
                }
            }

            InvalidateRect(hWnd, NULL, TRUE);
        }
        break;

    case WM_ERASEBKGND:
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rect;
        GetClientRect(hWnd, &rect);

        HWND hwndDesktop = GetDesktopWindow();
        HDC hdcDesktop = GetWindowDC(hwndDesktop);
        POINT clientPoint = { 0, 0 };
        ClientToScreen(hWnd, &clientPoint);

        BitBlt(hdc, 0, 0, rect.right, rect.bottom, hdcDesktop, clientPoint.x, clientPoint.y, SRCCOPY);

        ReleaseDC(hwndDesktop, hdcDesktop);
        EndPaint(hWnd, &ps);
    }
    break;

    case WM_GETMINMAXINFO:
    {
        MINMAXINFO* lpMMI = (MINMAXINFO*)lParam;
        UINT dpi = GetDpiForWindow(hWnd);

        int width, height;
        if (!focusMode && GetPresetSize(sizePreset, width, height))
        {
            // Locked: the only allowed size is the one that gives the preset Share Region.
            RECT outer = { 0, 0, width, height };
            AdjustWindowRectExForDpi(&outer, captionHidden ? WINDOW_STYLE_NO_CAPTION : WINDOW_STYLE_CAPTION, FALSE, (DWORD) GetWindowLong(hWnd, GWL_EXSTYLE), dpi);
            POINT size = { outer.right - outer.left, outer.bottom - outer.top };
            lpMMI->ptMinTrackSize = size;
            lpMMI->ptMaxTrackSize = size;
            lpMMI->ptMaxSize = size;
            break;
        }

        lpMMI->ptMinTrackSize.x = POS_MIN_WIDTH;
        if (focusMode)
        {
            lpMMI->ptMinTrackSize.y = GetSystemMetricsForDpi(SM_CYCAPTION, dpi);
            // Prevent vertical sizing if the PoS window has the focus
            if (hWnd == GetForegroundWindow())
                lpMMI->ptMaxTrackSize.y = lpMMI->ptMinTrackSize.y;
        }
        else
            lpMMI->ptMinTrackSize.y = POS_MIN_HEIGHT;
    }
    break;

    case WM_DESTROY:
        SaveSettings();
        PostQuitMessage(0);
        break;

    case WM_SYSCOMMAND:
    {
        UINT command = (UINT) (wParam & 0xFFF0);
        if (command == IDC_OPTIONS)
        {
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        }
        if (command == IDC_HIDE_CAPTION)
        {
            ShowCaption(hWnd, false);
            break;
        }
        if (focusMode && command > IDC_SIZE_FIRST && command <= IDC_SIZE_CUSTOM)
        {
            // Presets only exist in Fixed Mode: choosing one leaves Focus Mode, like Options does,
            // and puts the Window back where the user last placed it.
            focusMode = false;
            SetWindowPos(hWnd, HWND_TOPMOST, defaultWindowPos.left, defaultWindowPos.top, defaultWindowPos.right - defaultWindowPos.left, defaultWindowPos.bottom - defaultWindowPos.top, 0);
        }
        if (command == IDC_SIZE_CUSTOM)
        {
            int prevWidth = customWidth, prevHeight = customHeight;
            if (DialogBox(hInst, MAKEINTRESOURCE(IDD_CUSTOM_SIZE), hWnd, CustomSize) == IDOK)
            {
                if (!ApplySizePreset(hWnd, SIZE_CUSTOM))
                {
                    customWidth = prevWidth;
                    customHeight = prevHeight;
                }
            }
            break;
        }
        if (command >= IDC_SIZE_FIRST && command < IDC_SIZE_CUSTOM)
        {
            ApplySizePreset(hWnd, (command - IDC_SIZE_FIRST) / IDC_SIZE_STEP);
            break;
        }
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

// Width and height of a preset; false for Free.
bool GetPresetSize(int preset, int& width, int& height)
{
    if (preset >= 1 && preset <= SIZE_PRESET_COUNT)
    {
        width = SIZE_PRESETS[preset - 1].width;
        height = SIZE_PRESETS[preset - 1].height;
        return true;
    }
    if (preset == SIZE_CUSTOM)
    {
        width = customWidth;
        height = customHeight;
        return true;
    }
    return false;
}

// The Share Region in screen coordinates (physical pixels).
RECT GetShareRegion(HWND hWnd)
{
    RECT region;
    GetClientRect(hWnd, &region);
    MapWindowPoints(hWnd, nullptr, (LPPOINT) &region, 2);
    return region;
}

// Moves and resizes the Window so that its Share Region matches the given screen rectangle.
// If the Window would extend past the monitor it is pushed inwards, keeping the Title Bar
// reachable at the top.
void SetShareRegion(HWND hWnd, RECT region)
{
    RECT outer = region;
    AdjustWindowRectExForDpi(&outer, captionHidden ? WINDOW_STYLE_NO_CAPTION : WINDOW_STYLE_CAPTION, FALSE, (DWORD) GetWindowLong(hWnd, GWL_EXSTYLE), GetDpiForWindow(hWnd));
    int width = outer.right - outer.left;
    int height = outer.bottom - outer.top;

    MONITORINFO monitor = { sizeof(monitor) };
    GetMonitorInfo(MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST), &monitor);

    int x = outer.left, y = outer.top;
    if (x + width > monitor.rcMonitor.right) x = monitor.rcMonitor.right - width;
    if (y + height > monitor.rcMonitor.bottom) y = monitor.rcMonitor.bottom - height;
    if (x < monitor.rcMonitor.left) x = monitor.rcMonitor.left;
    if (y < monitor.rcMonitor.top) y = monitor.rcMonitor.top;

    SetWindowPos(hWnd, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

// Activates a preset (or Free). A Share Region larger than the monitor is refused with a message
// and the previous preset stays active. If only the Title Bar does not fit, the preset is applied
// and the user is told to hide the Title Bar once sharing runs.
bool ApplySizePreset(HWND hWnd, int preset)
{
    int width, height;
    if (!GetPresetSize(preset, width, height))
    {
        sizePreset = SIZE_FREE;
        return true;
    }

    MONITORINFO monitor = { sizeof(monitor) };
    GetMonitorInfo(MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST), &monitor);
    int monitorWidth = monitor.rcMonitor.right - monitor.rcMonitor.left;
    int monitorHeight = monitor.rcMonitor.bottom - monitor.rcMonitor.top;

    WCHAR text[512];
    if (width > monitorWidth || height > monitorHeight)
    {
        swprintf_s(text, L"A %d × %d shared area does not fit on this monitor (%d × %d).\n\nChoose a smaller size.",
            width, height, monitorWidth, monitorHeight);
        MessageBoxW(hWnd, text, szTitle, MB_OK | MB_ICONINFORMATION);
        return false;
    }

    sizePreset = preset;
    RECT region = GetShareRegion(hWnd);
    region.right = region.left + width;
    region.bottom = region.top + height;
    SetShareRegion(hWnd, region);

    RECT window;
    GetWindowRect(hWnd, &window);
    if (!captionHidden && window.bottom > monitor.rcMonitor.bottom)
    {
        swprintf_s(text, L"The %d × %d shared area only fits this monitor (%d × %d) without the title bar.\n\nStart sharing, then choose \"Hide title bar\" from the system menu.",
            width, height, monitorWidth, monitorHeight);
        MessageBoxW(hWnd, text, szTitle, MB_OK | MB_ICONINFORMATION);
    }
    return true;
}

// Shows or hides the Title Bar, keeping the Share Region where it is.
void ShowCaption(HWND hWnd, bool show)
{
    if (captionHidden == !show)
        return;

    RECT region = GetShareRegion(hWnd);
    captionHidden = !show;
    DWORD style = (DWORD) GetWindowLong(hWnd, GWL_STYLE) & WS_VISIBLE;
    style |= captionHidden ? WINDOW_STYLE_NO_CAPTION : WINDOW_STYLE_CAPTION;
    SetWindowLong(hWnd, GWL_STYLE, (LONG) style);
    SetShareRegion(hWnd, region);
}

// Window title: "Portion of Screen 1920 x 1080", the current Share Region size.
void UpdateTitle(HWND hWnd)
{
    RECT client;
    GetClientRect(hWnd, &client);
    WCHAR title[MAX_LOADSTRING + 32];
    swprintf_s(title, L"%s %d × %d", szTitle, client.right, client.bottom);
    SetWindowTextW(hWnd, title);
}

// Check mark on the active preset. In Focus Mode no preset is active and the submenu says so.
void UpdateSystemMenu(HWND hWnd)
{
    HMENU hSysMenu = GetSystemMenu(hWnd, FALSE);
    ModifyMenu(hSysMenu, sizeMenuPosition, MF_BYPOSITION | MF_POPUP, (UINT_PTR) hSizeMenu, focusMode ? L"Size (leaves Focus Mode)" : L"Size");
    if (focusMode)
    {
        for (UINT id = IDC_SIZE_FIRST; id <= IDC_SIZE_CUSTOM; id += IDC_SIZE_STEP)
            CheckMenuItem(hSizeMenu, id, MF_BYCOMMAND | MF_UNCHECKED);
    }
    else
        CheckMenuRadioItem(hSizeMenu, IDC_SIZE_FIRST, IDC_SIZE_CUSTOM, IDC_SIZE_FIRST + sizePreset * IDC_SIZE_STEP, MF_BYCOMMAND);
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        SendMessage(GetDlgItem(hDlg, IDC_FOCUS_MODE), BM_SETCHECK, focusMode ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessage(GetDlgItem(hDlg, IDC_FIXED_MODE), BM_SETCHECK, !focusMode ? BST_CHECKED : BST_UNCHECKED, 0);
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            if (!focusMode) moveToDefaultWindowPos = true;
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }

        if (LOWORD(wParam) == IDC_FOCUS_MODE || LOWORD(wParam) == IDC_FIXED_MODE)
        {
            UINT checkState = (UINT) SendMessage(GetDlgItem(hDlg, IDC_FOCUS_MODE), BM_GETCHECK, 0, 0);
            focusMode = checkState == BST_CHECKED;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

// Message handler for the Custom Size dialog. On OK the values are stored in customWidth/Height.
INT_PTR CALLBACK CustomSize(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        SetDlgItemInt(hDlg, IDC_CUSTOM_WIDTH, customWidth, FALSE);
        SetDlgItemInt(hDlg, IDC_CUSTOM_HEIGHT, customHeight, FALSE);
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK)
        {
            BOOL widthOk, heightOk;
            int width = (int) GetDlgItemInt(hDlg, IDC_CUSTOM_WIDTH, &widthOk, FALSE);
            int height = (int) GetDlgItemInt(hDlg, IDC_CUSTOM_HEIGHT, &heightOk, FALSE);
            if (!widthOk || !heightOk || width < POS_MIN_WIDTH || height < POS_MIN_HEIGHT || width > POS_MAX_SIZE || height > POS_MAX_SIZE)
            {
                WCHAR text[160];
                swprintf_s(text, L"Enter a width of at least %d and a height of at least %d pixels.", POS_MIN_WIDTH, POS_MIN_HEIGHT);
                MessageBoxW(hDlg, text, szTitle, MB_OK | MB_ICONINFORMATION);
                return (INT_PTR)TRUE;
            }
            customWidth = width;
            customHeight = height;
            EndDialog(hDlg, IDOK);
            return (INT_PTR)TRUE;
        }
        if (LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, IDCANCEL);
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

void LoadSettings()
{
    try
    {
        winreg::RegKey key{ HKEY_CURRENT_USER, L"SOFTWARE\\PortionOfScreen" };
        defaultWindowPos.left = key.GetDwordValue(L"Left");
        defaultWindowPos.top = key.GetDwordValue(L"Top");
        defaultWindowPos.right = key.GetDwordValue(L"Right");
        defaultWindowPos.bottom = key.GetDwordValue(L"Bottom");
        winreg::RegExpected<DWORD> focusExpected = key.TryGetDwordValue(L"FocusMode");
        focusMode = focusExpected.IsValid() ? (bool) focusExpected.GetValue() : true;

        winreg::RegExpected<DWORD> presetExpected = key.TryGetDwordValue(L"SizePreset");
        if (presetExpected.IsValid() && presetExpected.GetValue() <= (DWORD) SIZE_CUSTOM)
            sizePreset = (int) presetExpected.GetValue();
        winreg::RegExpected<DWORD> widthExpected = key.TryGetDwordValue(L"CustomWidth");
        winreg::RegExpected<DWORD> heightExpected = key.TryGetDwordValue(L"CustomHeight");
        if (widthExpected.IsValid() && heightExpected.IsValid() &&
            widthExpected.GetValue() >= POS_MIN_WIDTH && widthExpected.GetValue() <= POS_MAX_SIZE &&
            heightExpected.GetValue() >= POS_MIN_HEIGHT && heightExpected.GetValue() <= POS_MAX_SIZE)
        {
            customWidth = (int) widthExpected.GetValue();
            customHeight = (int) heightExpected.GetValue();
        }
    }
    catch(...)
    {
        defaultWindowPos.left = 100;
        defaultWindowPos.top = 100;
        defaultWindowPos.right = 900;
        defaultWindowPos.bottom = 700;
        focusMode = true;
    }
}

void SaveSettings()
{
    winreg::RegKey key{ HKEY_CURRENT_USER, L"SOFTWARE\\PortionOfScreen" };
    key.SetDwordValue(L"Left", defaultWindowPos.left);
    key.SetDwordValue(L"Top", defaultWindowPos.top);
    key.SetDwordValue(L"Right", defaultWindowPos.right);
    key.SetDwordValue(L"Bottom", defaultWindowPos.bottom);
    key.SetDwordValue(L"FocusMode", (DWORD) focusMode);
    key.SetDwordValue(L"SizePreset", (DWORD) sizePreset);
    key.SetDwordValue(L"CustomWidth", (DWORD) customWidth);
    key.SetDwordValue(L"CustomHeight", (DWORD) customHeight);
}
