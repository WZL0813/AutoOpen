// 定义Windows版本宏以确保所有API可用
#define _WIN32_WINNT 0x0600  // Windows Vista或更高版本
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <commctrl.h>

#define ID_TRAY_ICON 1001
#define WM_TRAYICON (WM_USER + 1)
#define IDM_EXIT 1002
#define IDM_SHOW 1003
#define IDM_AUTOSTART 1004
#define IDM_FILEPATH 1008
#define IDM_OPENTIME 1009
#define IDM_SAVE 1010
#define IDM_TIMELABEL 1007
#define IDM_SELECT_FILE 1011

const TCHAR* WINDOW_TITLE = _T("AutoOpen");
const TCHAR* CLASS_NAME = _T("AutoOpenWindowClass");
HICON hIcon = NULL;
HWND hWnd = NULL;
NOTIFYICONDATA nid = {};
bool autoStartEnabled = false;
bool fileOpened = false;  // 标记文件是否已打开

// 函数声明
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void AddTrayIcon(HWND);
void RemoveTrayIcon();
void ShowContextMenu(HWND);
void ToggleAutoStart(bool enable);
bool IsAutoStartEnabled();
void MinimizeToTray(HWND);
void RestoreFromTray();
void UpdateConfigDisplay(HWND hWnd);

// 主函数
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    (void)hPrevInstance;
    (void)pCmdLine;

    // 防多开检测 - 使用互斥体
    HANDLE hMutex = CreateMutex(NULL, TRUE, _T("AutoOpen_Mutex"));
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBox(NULL, _T("程序已经在运行中"), _T("AutoOpen"), MB_ICONINFORMATION | MB_OK);
                    UpdateConfigDisplay(hWnd);
                    return 0;
    }
    // 程序退出时释放互斥体
    static HANDLE hMutexStatic = hMutex;
    std::atexit([]() {
        if (hMutexStatic) {
            ReleaseMutex(hMutexStatic);
            CloseHandle(hMutexStatic);
        }
    });

    // 初始化Common Controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LINK_CLASS;
    InitCommonControlsEx(&icex);
    
    // 初始化NOTIFYICONDATA结构体
    NOTIFYICONDATA nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = NULL;
    nid.uID = 0;
    nid.uFlags = 0;
    nid.uCallbackMessage = 0;
    nid.hIcon = NULL;
    memset(nid.szTip, 0, sizeof(nid.szTip));
    nid.dwState = 0;
    nid.dwStateMask = 0;
    memset(nid.szInfo, 0, sizeof(nid.szInfo));
    nid.uVersion = 0;
    memset(nid.szInfoTitle, 0, sizeof(nid.szInfoTitle));
    nid.dwInfoFlags = 0;
    memset(&nid.guidItem, 0, sizeof(GUID));
    nid.hBalloonIcon = NULL;
    // 加载图标
    hIcon = (HICON)LoadImage(NULL, _T("ico.ico"), IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
    if (!hIcon) {
        hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }

    // 注册窗口类
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = 0;
    wc.lpfnWndProc = NULL;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = NULL;
    wc.hIcon = NULL;
    wc.hCursor = NULL;
    wc.hbrBackground = NULL;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = NULL;
    wc.hIconSm = NULL;
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = hIcon;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS_NAME;
    wc.hIconSm = hIcon;

    if (!RegisterClassEx(&wc)) {
        return 1;
    }

    // 创建窗口
    hWnd = CreateWindowEx(
        0, CLASS_NAME, WINDOW_TITLE,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 400,
        NULL, NULL, hInstance, NULL);

    if (!hWnd) {
        return 1;
    }

    // 添加托盘图标
    AddTrayIcon(hWnd);

    // 检查开机自启状态
    autoStartEnabled = IsAutoStartEnabled();

    // 显示窗口
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // 主消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 移除托盘图标
    RemoveTrayIcon();

    return (int)msg.wParam;
}

// 窗口过程
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        {
            // 创建时间显示控件
            HWND hTimeLabel = CreateWindowW(L"STATIC", L"",
                WS_VISIBLE | WS_CHILD | SS_RIGHT,
                350, 20, 140, 25, hWnd, (HMENU)1007, NULL, NULL);

            // 创建文件路径输入框
            CreateWindowW(L"STATIC", L"文件路径:", 
                WS_VISIBLE | WS_CHILD,
                30, 70, 80, 25, hWnd, NULL, NULL, NULL);
            CreateWindowW(L"EDIT", L"D:\\Auto\\Video.mp4", 
                WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
                120, 70, 250, 25, hWnd, (HMENU)1008, NULL, NULL);

            // 创建选择文件按钮
            CreateWindowW(L"BUTTON", L"选择文件", 
                WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                380, 70, 80, 25, hWnd, (HMENU)1011, NULL, NULL);

            // 创建时间设置控件
            CreateWindowW(L"STATIC", L"打开时间:", 
                WS_VISIBLE | WS_CHILD,
                30, 110, 80, 25, hWnd, NULL, NULL, NULL);
            CreateWindowW(L"EDIT", L"12:34:00", 
                WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
                120, 110, 250, 25, hWnd, (HMENU)1009, NULL, NULL);

            // 创建保存按钮
            CreateWindowW(L"BUTTON", L"保存配置", 
                WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                30, 150, 100, 30, hWnd, (HMENU)1010, NULL, NULL);

            // 创建配置显示框
            CreateWindowW(L"STATIC", L"当前配置:", 
                WS_VISIBLE | WS_CHILD,
                30, 190, 100, 25, hWnd, NULL, NULL, NULL);
            CreateWindowW(L"EDIT", L"", 
                WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL,
                30, 215, 340, 100, hWnd, (HMENU)1012, NULL, NULL);

            // 创建关于菜单
            HMENU hMenu = CreateMenu();
            HMENU hAboutMenu = CreatePopupMenu();
            AppendMenuW(hAboutMenu, MF_STRING, 1100, L"GitHub");
            AppendMenuW(hAboutMenu, MF_STRING, 1101, L"Ryokuryuneko");
            AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hAboutMenu, L"关于");
            SetMenu(hWnd, hMenu);

            // 创建复选框和按钮
            CreateWindowW(L"BUTTON", L"开机自启动", 
                WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
                20, 20, 200, 25, hWnd, (HMENU)IDM_AUTOSTART, NULL, NULL);
            
            // 设置初始状态
            CheckDlgButton(hWnd, IDM_AUTOSTART, autoStartEnabled ? BST_CHECKED : BST_UNCHECKED);

            // 加载配置文件
            WCHAR configDir[MAX_PATH];
            SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, configDir);
            PathAppendW(configDir, L"Ryokuryuneko\\AutoOpen");
            WCHAR configPath[MAX_PATH];
            PathCombineW(configPath, configDir, L"Config.ini");

            if (PathFileExistsW(configPath)) {
                // 读取UTF-8格式的配置文件
                HANDLE hFile = CreateFileW(configPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                if (hFile != INVALID_HANDLE_VALUE) {
                    DWORD fileSize = GetFileSize(hFile, NULL);
                    char* buffer = new char[fileSize + 1];
                    DWORD bytesRead;
                    if (ReadFile(hFile, buffer, fileSize, &bytesRead, NULL)) {
                        buffer[bytesRead] = '\0';
                        
                        // 转换为宽字符
                        int wideSize = MultiByteToWideChar(CP_UTF8, 0, buffer, -1, NULL, 0);
                        WCHAR* wideBuffer = new WCHAR[wideSize];
                        MultiByteToWideChar(CP_UTF8, 0, buffer, -1, wideBuffer, wideSize);
                        
                        // 解析配置文件内容
                        WCHAR filePath[MAX_PATH] = L"";
                        WCHAR openTime[64] = L"";
                        int savedAutoStart = 0;
                        
                        // 简单解析INI格式
                        WCHAR* section = wcsstr(wideBuffer, L"[Settings]");
                        if (section) {
                            WCHAR* fileKey = wcsstr(section, L"FilePath=");
                            if (fileKey) {
                                WCHAR* end = wcschr(fileKey + 9, L'\n');
                                if (end) {
                                    wcsncpy(filePath, fileKey + 9, end - (fileKey + 9));
                                    filePath[end - (fileKey + 9)] = L'\0';
                                }
                            }
                            
                            WCHAR* timeKey = wcsstr(section, L"OpenTime=");
                            if (timeKey) {
                                WCHAR* end = wcschr(timeKey + 9, L'\n');
                                if (end) {
                                    wcsncpy(openTime, timeKey + 9, end - (timeKey + 9));
                                    openTime[end - (timeKey + 9)] = L'\0';
                                }
                            }
                            
                            WCHAR* autoKey = wcsstr(section, L"AutoStart=");
                            if (autoKey) {
                                savedAutoStart = _wtoi(autoKey + 10);
                            }
                        }
                        
                        // 更新UI控件
                        SetDlgItemTextW(hWnd, 1008, filePath);
                        SetDlgItemTextW(hWnd, 1009, openTime);
                        if (savedAutoStart != autoStartEnabled) {
                            autoStartEnabled = savedAutoStart;
                            CheckDlgButton(hWnd, IDM_AUTOSTART, autoStartEnabled ? BST_CHECKED : BST_UNCHECKED);
                            ToggleAutoStart(autoStartEnabled);
                        }
                        
                        delete[] wideBuffer;
                    }
                    delete[] buffer;
                    CloseHandle(hFile);
                }
            }

            // 创建Github超链接
            HWND hLinkGithub = CreateWindowW(L"SysLink", 
                L"<a href=\"https://github.com/WZL0813/AutoOpen\">Github</a>",
                WS_VISIBLE | WS_CHILD | WS_TABSTOP,
                20, 220, 100, 25, hWnd, (HMENU)1005, NULL, NULL);

            // 创建个人网站链接
            HWND hLinkWebsite = CreateWindowW(L"SysLink", 
                L"<a href=\"https://wzl0813.github.io\">Ryokuryuneko</a>",
                WS_VISIBLE | WS_CHILD | WS_TABSTOP,
                20, 250, 150, 25, hWnd, (HMENU)1006, NULL, NULL);

            // 设置定时器，每秒更新一次时间
            SetTimer(hWnd, 1, 1000, NULL);

            // 调试输出
            OutputDebugStringW(hTimeLabel ? L"时间控件创建成功\n" : L"时间控件创建失败\n");
            OutputDebugStringW(hLinkGithub ? L"Github链接创建成功\n" : L"Github链接创建失败\n");
            OutputDebugStringW(hLinkWebsite ? L"Ryokuryuneko链接创建成功\n" : L"Ryokuryuneko链接创建失败\n");
        }
        break;

    case WM_NOTIFY:
        {
            NMLINK* pNMLink = (NMLINK*)lParam;
            if (pNMLink->hdr.code == NM_CLICK || pNMLink->hdr.code == NM_RETURN)
            {
                ShellExecuteW(NULL, L"open", pNMLink->item.szUrl, NULL, NULL, SW_SHOW);
            }
            return TRUE;  // 确保消息被正确处理
        }
        break;

    case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;

            case IDM_SHOW:
                RestoreFromTray();
                break;

            case IDM_AUTOSTART:
                autoStartEnabled = IsDlgButtonChecked(hWnd, IDM_AUTOSTART) == BST_CHECKED;
                ToggleAutoStart(autoStartEnabled);
                break;

            case 1010: // 保存配置按钮
                {
                    // 获取配置数据
                    WCHAR filePath[MAX_PATH];
                    WCHAR openTime[64];
                    GetDlgItemTextW(hWnd, 1008, filePath, MAX_PATH);
                    GetDlgItemTextW(hWnd, 1009, openTime, 64);

                    // 创建配置目录
                    WCHAR configDir[MAX_PATH];
                    SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, configDir);
                    PathAppendW(configDir, L"Ryokuryuneko");
                    
                    // 先创建Ryokuryuneko目录
                    if (!CreateDirectoryW(configDir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                        MessageBoxW(hWnd, L"无法创建配置目录", L"错误", MB_OK | MB_ICONERROR);
                        break;
                    }
                    
                    // 再创建AutoOpen子目录
                    PathAppendW(configDir, L"AutoOpen");
                    if (!CreateDirectoryW(configDir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                        MessageBoxW(hWnd, L"无法创建配置目录", L"错误", MB_OK | MB_ICONERROR);
                        break;
                    }

                    // 保存配置到文件
                    WCHAR configPath[MAX_PATH];
                    PathCombineW(configPath, configDir, L"Config.ini");
                    
                    // 调试输出路径信息
                    OutputDebugStringW(L"配置文件路径: ");
                    OutputDebugStringW(configPath);
                    OutputDebugStringW(L"\n");
                    
                    // 准备UTF-8编码的配置数据
                    WCHAR configDataW[1024];
                    wsprintfW(configDataW, L"[Settings]\nFilePath=%s\nOpenTime=%s\nAutoStart=%d",
                             filePath, openTime, autoStartEnabled);
                    
                    // 转换为UTF-8
                    int utf8Size = WideCharToMultiByte(CP_UTF8, 0, configDataW, -1, NULL, 0, NULL, NULL);
                    char* configData = new char[utf8Size];
                    WideCharToMultiByte(CP_UTF8, 0, configDataW, -1, configData, utf8Size, NULL, NULL);
                    
                    HANDLE hFile = CreateFileW(configPath, GENERIC_WRITE, 0, NULL, 
                                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                    if (hFile != INVALID_HANDLE_VALUE) {
                        DWORD bytesWritten;
                        if (WriteFile(hFile, configData, utf8Size - 1, &bytesWritten, NULL)) {
                            MessageBoxW(hWnd, L"配置保存成功", L"提示", MB_OK);
                            OutputDebugStringW(L"配置文件写入成功\n");
                        } else {
                            WCHAR errorMsg[256];
                            wsprintfW(errorMsg, L"文件写入失败，错误代码: %d", GetLastError());
                            MessageBoxW(hWnd, errorMsg, L"错误", MB_OK | MB_ICONERROR);
                            OutputDebugStringW(errorMsg);
                            OutputDebugStringW(L"\n");
                        }
                        CloseHandle(hFile);
                    } else {
                        WCHAR errorMsg[256];
                        wsprintfW(errorMsg, L"无法创建文件，错误代码: %d", GetLastError());
                        MessageBoxW(hWnd, errorMsg, L"错误", MB_OK | MB_ICONERROR);
                        OutputDebugStringW(errorMsg);
                        OutputDebugStringW(L"\n");
                    }
                    delete[] configData;
                }
                break;

            case 1100: // GitHub菜单项
                ShellExecuteW(NULL, L"open", L"https://github.com/WZL0813/AutoOpen", NULL, NULL, SW_SHOW);
                break;
                
            case 1101: // Ryokuryuneko菜单项
                ShellExecuteW(NULL, L"open", L"https://wzl0813.github.io", NULL, NULL, SW_SHOW);
                break;
                
            case IDM_SELECT_FILE: // 选择文件按钮
                {
                    OPENFILENAMEW ofn;
                    WCHAR szFile[MAX_PATH] = L"";

                    ZeroMemory(&ofn, sizeof(ofn));
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hWnd;
                    ofn.lpstrFile = szFile;
                    ofn.nMaxFile = sizeof(szFile)/sizeof(szFile[0]);
                    ofn.lpstrFilter = L"All Files\0*.*\0\0";
                    ofn.nFilterIndex = 1;
                    ofn.lpstrFileTitle = NULL;
                    ofn.nMaxFileTitle = 0;
                    ofn.lpstrInitialDir = L"D:\\Auto";
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                    if (GetOpenFileNameW(&ofn)) {
                        SetDlgItemTextW(hWnd, 1008, szFile);
                    }
                }
                break;
            }
        }
        break;

    case WM_TRAYICON:
        {
            if (lParam == WM_RBUTTONUP)
            {
                ShowContextMenu(hWnd);
            }
            else if (lParam == WM_LBUTTONDBLCLK)
            {
                RestoreFromTray();
            }
        }
        break;

    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
        {
            MinimizeToTray(hWnd);
        }
        break;

    case WM_CLOSE:
        MinimizeToTray(hWnd);
        return 0;

    case WM_TIMER:
        if (wParam == 1) {
            // 获取当前时间
            SYSTEMTIME st;
            GetLocalTime(&st);
            
            // 格式化时间字符串
            WCHAR timeStr[64];
            wsprintfW(timeStr, L"%04d-%02d-%02d %02d:%02d:%02d", 
                     st.wYear, st.wMonth, st.wDay,
                     st.wHour, st.wMinute, st.wSecond);
            
            // 更新时间显示
            SetDlgItemTextW(hWnd, 1007, timeStr);

            // 检查是否到达设置时间
            WCHAR openTime[64];
            GetDlgItemTextW(hWnd, 1009, openTime, 64);
            if (wcslen(openTime) > 0) {
                // 根据输入的时间格式决定比较精度
                WCHAR currentTime[64];
                if (wcslen(openTime) >= 8) { // 格式为"HH:MM:SS"
                    wsprintfW(currentTime, L"%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
                } else { // 格式为"HH:MM"
                    wsprintfW(currentTime, L"%02d:%02d", st.wHour, st.wMinute);
                }
                
                if (wcscmp(currentTime, openTime) == 0) {
                    if (!fileOpened) {
                        // 获取文件路径并打开文件
                        WCHAR filePath[MAX_PATH];
                        GetDlgItemTextW(hWnd, 1008, filePath, MAX_PATH);
                        if (PathFileExistsW(filePath)) {
                            ShellExecuteW(NULL, L"open", filePath, NULL, NULL, SW_SHOW);
                            fileOpened = true;  // 标记文件已打开
                        }
                    }
                } else {
                    fileOpened = false;  // 时间不匹配时重置标志
                }
            }
        }
        break;

    case WM_DESTROY:
        KillTimer(hWnd, 1);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// 更新配置显示
void UpdateConfigDisplay(HWND hWnd) {
    WCHAR configPath[MAX_PATH];
    GetModuleFileNameW(NULL, configPath, MAX_PATH);
    PathRemoveFileSpecW(configPath);
    PathAppendW(configPath, L"Config.ini");

    WCHAR configContent[1024] = L"";
    HANDLE hFile = CreateFileW(configPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD bytesRead;
        ReadFile(hFile, configContent, sizeof(configContent) - sizeof(WCHAR), &bytesRead, NULL);
        configContent[bytesRead / sizeof(WCHAR)] = L'\0';
        CloseHandle(hFile);
    }

    SetDlgItemTextW(hWnd, 1012, configContent);
}

// 添加托盘图标
void AddTrayIcon(HWND hWnd)
{
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = ID_TRAY_ICON;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = hIcon;
    StringCchCopy(nid.szTip, ARRAYSIZE(nid.szTip), WINDOW_TITLE);

    Shell_NotifyIcon(NIM_ADD, &nid);
}

// 移除托盘图标
void RemoveTrayIcon()
{
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

// 显示上下文菜单
void ShowContextMenu(HWND hWnd)
{
    HMENU hMenu = CreatePopupMenu();
    if (hMenu)
    {
        InsertMenu(hMenu, 0, MF_BYPOSITION | MF_STRING, IDM_SHOW, _T("显示窗口"));
        InsertMenu(hMenu, 1, MF_BYPOSITION | MF_STRING, IDM_EXIT, _T("退出"));
        
        POINT pt;
        GetCursorPos(&pt);
        SetForegroundWindow(hWnd);
        TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, NULL);
        DestroyMenu(hMenu);
    }
}

// 最小化到托盘
void MinimizeToTray(HWND hWnd)
{
    ShowWindow(hWnd, SW_HIDE);
}

// 从托盘恢复
void RestoreFromTray()
{
    ShowWindow(hWnd, SW_SHOW);
    SetForegroundWindow(hWnd);
}

// 设置开机自启
void ToggleAutoStart(bool enable)
{
    HKEY hKey;
    TCHAR szPath[MAX_PATH];
    
    GetModuleFileName(NULL, szPath, MAX_PATH);

    if (RegOpenKeyEx(HKEY_CURRENT_USER, 
        _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 
        0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
    {
        if (enable) {
            RegSetValueEx(hKey, WINDOW_TITLE, 0, REG_SZ, 
                (BYTE*)szPath, (DWORD)(_tcslen(szPath) + 1) * sizeof(TCHAR));
        } else {
            RegDeleteValue(hKey, WINDOW_TITLE);
        }
        RegCloseKey(hKey);
    }
}

// 检查开机自启状态
bool IsAutoStartEnabled()
{
    HKEY hKey;
    TCHAR szPath[MAX_PATH];
    DWORD dwSize = MAX_PATH;
    
    if (RegOpenKeyEx(HKEY_CURRENT_USER, 
        _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 
        0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS)
    {
        if (RegQueryValueEx(hKey, WINDOW_TITLE, NULL, NULL, 
            (LPBYTE)szPath, &dwSize) == ERROR_SUCCESS)
        {
            TCHAR szCurrentPath[MAX_PATH];
            GetModuleFileName(NULL, szCurrentPath, MAX_PATH);
            
            RegCloseKey(hKey);
            return _tcsicmp(szPath, szCurrentPath) == 0;
        }
        RegCloseKey(hKey);
    }
    return false;
}