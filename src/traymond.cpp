#include <Windows.h>
#include <windowsx.h>

#include <string>
#include <vector>

// Mouse jiggle timer
#define IDT_TIMER       1
#define TIMER_INTERVAL  59000

// These keys are used to send windows to tray
// Alt + `
#define TRAY_KEY        0xC0
#define MOD_KEY         MOD_ALT

#define WM_ICON         0x1C0A
#define WM_OURICON      0x1C0B
#define EXIT_ID         0x99
#define SHOW_ALL_ID     0x98
#define MAXIMUM_WINDOWS 100

// Stores hidden window record.
typedef struct HIDDEN_WINDOW {
    NOTIFYICONDATA icon;
    HWND window;
} HIDDEN_WINDOW;

// Current execution context
typedef struct TRCONTEXT {
    HWND mainWindow;
    HIDDEN_WINDOW icons[MAXIMUM_WINDOWS];
    HMENU trayMenu;
    int iconIndex;  // How many windows are currently hidden
    INPUT input;    // For mouse jiggle
} TRCONTEXT;

HANDLE saveFile;

// Saves our hidden windows so they can be restored in case
// of crashing.
void save(const TRCONTEXT *context) {
    DWORD numbytes;
    // Truncate file
    SetFilePointer(saveFile, 0, NULL, FILE_BEGIN);
    SetEndOfFile(saveFile);
    if (!context->iconIndex) {
        return;
    }
    for (int i = 0; i < context->iconIndex; i++) {
        if (context->icons[i].window) {
            std::string str;
            str = std::to_string((long)context->icons[i].window);
            str += ',';
            const char *handleString = str.c_str();
            WriteFile(saveFile, handleString, strlen(handleString), &numbytes, NULL);
        }
    }
}

// Restores a window
void showWindow(TRCONTEXT *context, LPARAM lParam) {
    for (int i = 0; i < context->iconIndex; i++) {
        if (context->icons[i].icon.uID == HIWORD(lParam)) {
            ShowWindow(context->icons[i].window, SW_SHOW);
            Shell_NotifyIcon(NIM_DELETE, &context->icons[i].icon);
            SetForegroundWindow(context->icons[i].window);
            context->icons[i] = {};
            std::vector<HIDDEN_WINDOW> temp = std::vector<HIDDEN_WINDOW>(context->iconIndex);
            // Restructure array so there are no holes
            for (int j = 0, x = 0; j < context->iconIndex; j++) {
                if (context->icons[j].window) {
                    temp[x] = context->icons[j];
                    x++;
                }
            }
            memcpy_s(context->icons, sizeof(context->icons), &temp.front(), sizeof(HIDDEN_WINDOW) * context->iconIndex);
            context->iconIndex--;
            save(context);
            break;
        }
    }
}

// Minimizes the current window to tray.
// Uses currently focused window unless supplied a handle as the argument.
void minimizeToTray(TRCONTEXT *context, long restoreWindow) {
    // Taskbar and desktop windows are restricted from hiding.
    const char restrictWins[][14] = {{"WorkerW"}, {"Shell_TrayWnd"}};

    HWND currWin = 0;
    if (!restoreWindow) {
        currWin = GetForegroundWindow();
    } else {
        currWin = reinterpret_cast<HWND>(restoreWindow);
    }

    if (!currWin) {
        return;
    }

    char className[256];
    if (!GetClassName(currWin, className, 256)) {
        return;
    } else {
        for (int i = 0; i < sizeof(restrictWins) / sizeof(*restrictWins); i++) {
            if (strcmp(restrictWins[i], className) == 0) {
                return;
            }
        }
    }
    if (context->iconIndex == MAXIMUM_WINDOWS) {
        MessageBox(NULL, "Error! Too many hidden windows. Please unhide some.", "Traymond", MB_OK | MB_ICONERROR);
        return;
    }
    ULONG_PTR icon = GetClassLongPtr(currWin, GCLP_HICONSM);
    if (!icon) {
        icon = SendMessage(currWin, WM_GETICON, 2, NULL);
        if (!icon) {
            return;
        }
    }

    NOTIFYICONDATA nid;
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = context->mainWindow;
    nid.hIcon = (HICON)icon;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    nid.uVersion = NOTIFYICON_VERSION_4;
    nid.uID = LOWORD(reinterpret_cast<UINT>(currWin));
    nid.uCallbackMessage = WM_ICON;
    GetWindowText(currWin, nid.szTip, 128);
    context->icons[context->iconIndex].icon = nid;
    context->icons[context->iconIndex].window = currWin;
    context->iconIndex++;
    Shell_NotifyIcon(NIM_ADD, &nid);
    Shell_NotifyIcon(NIM_SETVERSION, &nid);
    ShowWindow(currWin, SW_HIDE);
    if (!restoreWindow) {
        save(context);
    }
}

// Adds our own icon to tray
void createTrayIcon(HWND mainWindow, HINSTANCE hInstance, NOTIFYICONDATA *icon) {
    icon->cbSize = sizeof(NOTIFYICONDATA);
    icon->hWnd = mainWindow;
    icon->hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(101));
    icon->uFlags = NIF_ICON | NIF_TIP | NIF_SHOWTIP | NIF_MESSAGE;
    icon->uVersion = NOTIFYICON_VERSION_4;
    icon->uID = reinterpret_cast<UINT>(mainWindow);
    icon->uCallbackMessage = WM_OURICON;
    strcpy_s(icon->szTip, "ToTray");
    Shell_NotifyIcon(NIM_ADD, icon);
    Shell_NotifyIcon(NIM_SETVERSION, icon);
}

// Creates our tray icon menu
void createTrayMenu(HMENU *trayMenu) {
    *trayMenu = CreatePopupMenu();

    MENUITEMINFO showAllMenuItem;
    MENUITEMINFO exitMenuItem;

    exitMenuItem.cbSize = sizeof(MENUITEMINFO);
    exitMenuItem.fMask = MIIM_STRING | MIIM_ID;
    exitMenuItem.fType = MFT_STRING;
    exitMenuItem.dwTypeData = "Exit";
    exitMenuItem.cch = 5;
    exitMenuItem.wID = EXIT_ID;

    showAllMenuItem.cbSize = sizeof(MENUITEMINFO);
    showAllMenuItem.fMask = MIIM_STRING | MIIM_ID;
    showAllMenuItem.fType = MFT_STRING;
    showAllMenuItem.dwTypeData = "Restore all windows";
    showAllMenuItem.cch = 20;
    showAllMenuItem.wID = SHOW_ALL_ID;

    InsertMenuItem(*trayMenu, 0, FALSE, &showAllMenuItem);
    InsertMenuItem(*trayMenu, 0, FALSE, &exitMenuItem);
}
// Shows all hidden windows;
void showAllWindows(TRCONTEXT *context) {
    for (int i = 0; i < context->iconIndex; i++) {
        ShowWindow(context->icons[i].window, SW_SHOW);
        Shell_NotifyIcon(NIM_DELETE, &context->icons[i].icon);
        context->icons[i] = {};
    }
    save(context);
    context->iconIndex = 0;
}

void exitApp() {
    PostQuitMessage(0);
}

// Creates and reads the save file to restore hidden windows in case of unexpected termination
void startup(TRCONTEXT *context) {
    if ((saveFile = CreateFile("traymond.dat", GENERIC_READ | GENERIC_WRITE,
                               0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
        MessageBox(NULL, "Error! Traymond could not create a save file.", "Traymond", MB_OK | MB_ICONERROR);
        exitApp();
    }
    // Check if we've crashed (i. e. there is a save file) during current uptime and
    // if there are windows to restore, in which case restore them and
    // display a reassuring message.
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        DWORD numbytes;
        DWORD fileSize = GetFileSize(saveFile, NULL);

        if (!fileSize) {
            return;
        };

        FILETIME saveFileWriteTime;
        GetFileTime(saveFile, NULL, NULL, &saveFileWriteTime);
        uint64_t writeTime = ((uint64_t)saveFileWriteTime.dwHighDateTime << 32 | (uint64_t)saveFileWriteTime.dwLowDateTime) / 10000;
        GetSystemTimeAsFileTime(&saveFileWriteTime);
        writeTime = (((uint64_t)saveFileWriteTime.dwHighDateTime << 32 | (uint64_t)saveFileWriteTime.dwLowDateTime) / 10000) - writeTime;

        if (GetTickCount64() < writeTime) {
            return;
        }

        std::vector<char> contents = std::vector<char>(fileSize);
        ReadFile(saveFile, &contents.front(), fileSize, &numbytes, NULL);
        char handle[10];
        int index = 0;
        for (size_t i = 0; i < fileSize; i++) {
            if (contents[i] != ',') {
                handle[index] = contents[i];
                index++;
            } else {
                index = 0;
                minimizeToTray(context, std::stoi(std::string(handle)));
                memset(handle, 0, sizeof(handle));
            }
        }
        std::string restore_message = "Traymond had previously been terminated unexpectedly.\n\nRestored " +
                                      std::to_string(context->iconIndex) + (context->iconIndex > 1 ? " icons." : " icon.");
        MessageBox(NULL, restore_message.c_str(), "Traymond", MB_OK);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    TRCONTEXT *context = reinterpret_cast<TRCONTEXT *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    POINT pt;
    switch (uMsg) {
        case WM_ICON:
            if (LOWORD(lParam) == WM_LBUTTONDBLCLK) {
                showWindow(context, lParam);
            }
            break;
        case WM_OURICON:
            if (LOWORD(lParam) == WM_RBUTTONUP) {
                SetForegroundWindow(hwnd);
                GetCursorPos(&pt);
                TrackPopupMenuEx(context->trayMenu,
                                 (GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN : TPM_LEFTALIGN) | TPM_BOTTOMALIGN,
                                 pt.x, pt.y, hwnd, NULL);
            }
            break;
        case WM_COMMAND:
            if (HIWORD(wParam) == 0) {
                switch
                    LOWORD(wParam) {
                        case SHOW_ALL_ID:
                            showAllWindows(context);
                            break;
                        case EXIT_ID:
                            exitApp();
                            break;
                    }
            }
            break;
        case WM_HOTKEY:  // We only have one hotkey, so no need to check the message
            minimizeToTray(context, NULL);
            break;
        case WM_TIMER:
            if (wParam == IDT_TIMER) {
                SendInput(1, &context->input, sizeof(INPUT));
            }
            break;
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

#pragma warning(push)
#pragma warning(disable : 4100)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
#pragma warning(pop)

    TRCONTEXT context = {};

    NOTIFYICONDATA icon = {};

    // Mutex to allow only one instance
    const char szUniqueNamedMutex[] = "traymond_mutex";
    HANDLE mutex = CreateMutex(NULL, TRUE, szUniqueNamedMutex);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBox(NULL, "Error! Another instance of Traymond is already running.", "Traymond", MB_OK | MB_ICONERROR);
        return 1;
    }

    BOOL bRet;
    MSG msg;

    const char CLASS_NAME[] = "Traymond";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClass(&wc)) {
        return 1;
    }

    context.mainWindow = CreateWindow(CLASS_NAME, NULL, NULL, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);

    if (!context.mainWindow) {
        return 1;
    }

    // Store our context in main window for retrieval by WindowProc
    SetWindowLongPtr(context.mainWindow, GWLP_USERDATA, reinterpret_cast<LONG>(&context));

    if (!RegisterHotKey(context.mainWindow, 0, MOD_KEY | MOD_NOREPEAT, TRAY_KEY)) {
        MessageBox(NULL, "Error! Could not register the hotkey.", "Traymond", MB_OK | MB_ICONERROR);
        return 1;
    }

    createTrayIcon(context.mainWindow, hInstance, &icon);
    createTrayMenu(&context.trayMenu);
    startup(&context);

    // preventing the system from entering sleep or turning off the display while the application is running
    //SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);

    context.input.type = INPUT_MOUSE;
    context.input.mi.mouseData = 0;
    context.input.mi.dx = 0;
    context.input.mi.dy = 0;
    context.input.mi.dwFlags = MOUSEEVENTF_MOVE;

    SetTimer(context.mainWindow, IDT_TIMER, TIMER_INTERVAL, (TIMERPROC)NULL);

    while ((bRet = GetMessage(&msg, 0, 0, 0)) != 0) {
        if (bRet != -1) {
            DispatchMessage(&msg);
        }
    }

    KillTimer(context.mainWindow, IDT_TIMER);

    //SetThreadExecutionState(ES_CONTINUOUS);

    // Clean up on exit;
    showAllWindows(&context);
    Shell_NotifyIcon(NIM_DELETE, &icon);
    ReleaseMutex(mutex);
    CloseHandle(mutex);
    CloseHandle(saveFile);
    DestroyMenu(context.trayMenu);
    DestroyWindow(context.mainWindow);
    DeleteFile("traymond.dat");  // No save file means we have exited gracefully
    UnregisterHotKey(context.mainWindow, 0);
    return msg.wParam;
}
