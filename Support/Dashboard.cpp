#include <Windows.h>
#include <tlhelp32.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#include <array>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

namespace
{
constexpr int kStartScan = 1001;
constexpr int kHardware = 1002;
constexpr int kQuickActions = 1003;
constexpr int kSettings = 1004;
constexpr int kClose = 1005;
constexpr UINT kDashboardFont = 1;

HWND g_systemInfo = nullptr;
HWND g_securityStatus = nullptr;
HWND g_antiCheatStatus = nullptr;
HWND g_scanStatus = nullptr;
HWND g_onlineStatus = nullptr;
HFONT g_titleFont = nullptr;
HFONT g_sectionFont = nullptr;
HFONT g_bodyFont = nullptr;
HBRUSH g_buttonBrush = nullptr;

std::wstring ReadRegistryStringValue(const wchar_t* valueName)
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS)
    {
        return L"Unknown Windows";
    }

    wchar_t value[256]{};
    DWORD valueSize = sizeof(value);
    DWORD valueType = 0;
    LONG result = RegQueryValueExW(key, valueName, nullptr, &valueType,
        reinterpret_cast<LPBYTE>(value), &valueSize);
    RegCloseKey(key);

    if (result != ERROR_SUCCESS || valueType != REG_SZ)
    {
        return L"Unknown Windows";
    }
    return value;
}

std::string TrimAscii(const std::string& value)
{
    size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])))
    {
        ++begin;
    }

    size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])))
    {
        --end;
    }
    return value.substr(begin, end - begin);
}

std::wstring GetProcessorName()
{
    std::array<int, 4> data{};
    std::array<char, 49> brand{};
    for (int index = 0; index < 3; ++index)
    {
        __cpuid(data.data(), 0x80000002 + index);
        std::memcpy(brand.data() + index * sizeof(data), data.data(), sizeof(data));
    }
    std::string processor = TrimAscii(std::string(brand.data(), 48));
    if (processor.empty())
    {
        return L"Unknown processor";
    }

    return std::wstring(processor.begin(), processor.end());
}

std::wstring GetMemoryText()
{
    MEMORYSTATUSEX status{ sizeof(status) };
    if (!GlobalMemoryStatusEx(&status))
    {
        return L"Unavailable";
    }

    std::wstringstream output;
    output << std::fixed << std::setprecision(1)
        << static_cast<double>(status.ullTotalPhys) / (1024.0 * 1024.0 * 1024.0) << L" GB";
    return output.str();
}

std::wstring GetArchitectureText()
{
    SYSTEM_INFO systemInfo{};
    GetNativeSystemInfo(&systemInfo);
    return systemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ? L"x64" : L"x86";
}

std::wstring GetStorageText()
{
    std::wstringstream output;
    DWORD drives = GetLogicalDrives();
    bool addedDrive = false;

    for (wchar_t drive = L'A'; drive <= L'Z'; ++drive)
    {
        if ((drives & (1u << (drive - L'A'))) == 0)
        {
            continue;
        }

        wchar_t root[] = { drive, L':', L'\\', L'\0' };
        ULARGE_INTEGER freeBytes{}, totalBytes{};
        if (!GetDiskFreeSpaceExW(root, &freeBytes, &totalBytes, nullptr) || totalBytes.QuadPart == 0)
        {
            continue;
        }

        if (addedDrive)
        {
            output << L"\r\n";
        }
        output << drive << L":  "
            << std::fixed << std::setprecision(1)
            << static_cast<double>(freeBytes.QuadPart) / (1024.0 * 1024.0 * 1024.0)
            << L" GB free of "
            << static_cast<double>(totalBytes.QuadPart) / (1024.0 * 1024.0 * 1024.0) << L" GB";
        addedDrive = true;
    }

    return addedDrive ? output.str() : L"Unavailable";
}

std::wstring GetSystemInfoText()
{
    std::wstringstream output;
    output << L"OS VERSION\r\n" << ReadRegistryStringValue(L"ProductName") << L"\r\n\r\n"
        << L"PROCESSOR\r\n" << GetProcessorName() << L"\r\n\r\n"
        << L"ARCHITECTURE\r\n" << GetArchitectureText() << L"\r\n\r\n"
        << L"MEMORY\r\n" << GetMemoryText() << L"\r\n\r\n"
        << L"STORAGE\r\n" << GetStorageText();
    return output.str();
}

bool IsAdministrator()
{
    BOOL isMember = FALSE;
    SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_AUTHORITY;
    PSID administrators = nullptr;
    if (AllocateAndInitializeSid(&authority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &administrators))
    {
        CheckTokenMembership(nullptr, administrators, &isMember);
        FreeSid(administrators);
    }
    return isMember == TRUE;
}

std::wstring GetServiceStatus(const wchar_t* serviceName)
{
    SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (manager == nullptr)
    {
        return L"Unavailable";
    }

    SC_HANDLE service = OpenServiceW(manager, serviceName, SERVICE_QUERY_STATUS);
    if (service == nullptr)
    {
        CloseServiceHandle(manager);
        return L"Not installed";
    }

    SERVICE_STATUS_PROCESS status{};
    DWORD bytes = 0;
    bool running = QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO,
        reinterpret_cast<LPBYTE>(&status), sizeof(status), &bytes) != FALSE &&
        status.dwCurrentState == SERVICE_RUNNING;
    CloseServiceHandle(service);
    CloseServiceHandle(manager);
    return running ? L"Running" : L"Stopped";
}

bool IsProcessRunning(const wchar_t* processName)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    PROCESSENTRY32W entry{ sizeof(entry) };
    bool found = false;
    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            if (_wcsicmp(entry.szExeFile, processName) == 0)
            {
                found = true;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return found;
}

std::wstring GetUacStatus()
{
    HKEY key = nullptr;
    DWORD enabled = 0;
    DWORD size = sizeof(enabled);
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
        0, KEY_READ, &key) == ERROR_SUCCESS)
    {
        RegQueryValueExW(key, L"EnableLUA", nullptr, nullptr,
            reinterpret_cast<LPBYTE>(&enabled), &size);
        RegCloseKey(key);
    }
    return enabled != 0 ? L"Enabled" : L"Disabled or unavailable";
}

std::wstring GetSecurityStatusText()
{
    std::wstringstream output;
    output << L"Windows Defender     " << GetServiceStatus(L"WinDefend") << L"\r\n"
        << L"Windows Firewall     " << GetServiceStatus(L"MpsSvc") << L"\r\n"
        << L"User Account Control " << GetUacStatus();
    return output.str();
}

std::wstring GetAntiCheatStatusText()
{
    constexpr std::array<const wchar_t*, 6> knownProcesses = {
        L"vgc.exe", L"vgtray.exe", L"EasyAntiCheat.exe", L"BEService.exe", L"vgk.sys", L"FaceItClient.exe"
    };
    for (const wchar_t* process : knownProcesses)
    {
        if (IsProcessRunning(process))
        {
            std::wstring result = L"Detected running process: ";
            result += process;
            return result;
        }
    }
    return L"No known anti-cheat process is currently running.";
}

std::wstring RunOnlineDiagnostics()
{
    HINTERNET session = WinHttpOpen(L"NugzSupportTool/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == nullptr)
    {
        return L"Offline: could not initialize HTTPS.";
    }

    HINTERNET connection = WinHttpConnect(session, L"www.msftconnecttest.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (connection == nullptr)
    {
        WinHttpCloseHandle(session);
        return L"Offline: DNS or network connection failed.";
    }

    HINTERNET request = WinHttpOpenRequest(connection, L"GET", L"/connecttest.txt", nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (request == nullptr)
    {
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return L"Offline: could not create HTTPS request.";
    }

    bool sent = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0) != FALSE;
    bool received = sent && WinHttpReceiveResponse(request, nullptr) != FALSE;
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (received)
    {
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);

    if (!received)
    {
        return L"Offline: HTTPS request failed.";
    }
    return statusCode >= 200 && statusCode < 400
        ? L"Online: HTTPS connectivity is available."
        : L"Limited connection: HTTPS returned an unexpected status.";
}

void SetControlFont(HWND control, HFONT font)
{
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

HWND AddLabel(HWND parent, const wchar_t* text, int x, int y, int width, int height, HFONT font, DWORD style = 0)
{
    HWND label = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | style,
        x, y, width, height, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
    SetControlFont(label, font);
    return label;
}

HWND AddButton(HWND parent, const wchar_t* text, int id, int x, int y, int width, int height)
{
    HWND button = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, width, height, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
    SetControlFont(button, g_bodyFont);
    return button;
}

void DrawPanel(HDC dc, const RECT& panel, COLORREF fill, COLORREF border)
{
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    RoundRect(dc, panel.left, panel.top, panel.right, panel.bottom, 16, 16);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void PaintDashboard(HWND window, HDC dc)
{
    RECT client{};
    GetClientRect(window, &client);

    for (int y = 0; y < client.bottom; y += 4)
    {
        double ratio = static_cast<double>(y) / max(1, client.bottom);
        int red = static_cast<int>(16 + ratio * 170);
        int green = static_cast<int>(12 + ratio * 2);
        int blue = static_cast<int>(45 + (1.0 - ratio) * 45);
        HBRUSH band = CreateSolidBrush(RGB(red, green, blue));
        RECT bandRect{ 0, y, client.right, min(y + 4, client.bottom) };
        FillRect(dc, &bandRect, band);
        DeleteObject(band);
    }

    RECT leftPanel{ 18, 76, 790, 316 };
    RECT securityPanel{ 18, 330, 790, 406 };
    RECT antiCheatPanel{ 18, 420, 790, 498 };
    RECT actionsPanel{ 808, 76, client.right - 18, 196 };
    RECT antivirusPanel{ 808, 216, client.right - 18, 294 };

    DrawPanel(dc, leftPanel, RGB(37, 25, 76), RGB(108, 22, 116));
    DrawPanel(dc, securityPanel, RGB(31, 27, 67), RGB(91, 23, 106));
    DrawPanel(dc, antiCheatPanel, RGB(31, 27, 67), RGB(91, 23, 106));
    DrawPanel(dc, actionsPanel, RGB(80, 20, 91), RGB(112, 14, 100));
    DrawPanel(dc, antivirusPanel, RGB(80, 20, 91), RGB(112, 14, 100));
}

void RefreshDashboard()
{
    SetWindowTextW(g_systemInfo, GetSystemInfoText().c_str());
    SetWindowTextW(g_securityStatus, GetSecurityStatusText().c_str());
    SetWindowTextW(g_antiCheatStatus, GetAntiCheatStatusText().c_str());
    SetWindowTextW(g_scanStatus, L"SCAN COMPLETE");
}

LRESULT CALLBACK DashboardProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        g_titleFont = CreateFontW(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        g_sectionFont = CreateFontW(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        g_bodyFont = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        g_buttonBrush = CreateSolidBrush(RGB(57, 17, 76));

        AddLabel(window, L"NUGZ", 20, 16, 180, 28, g_titleFont);
        AddLabel(window, L"SUPPORT TOOL", 22, 43, 180, 20, g_bodyFont);
        AddLabel(window, IsAdministrator() ? L"RUNNING AS ADMINISTRATOR" : L"STANDARD USER TOKEN", 858, 19, 180, 24, g_bodyFont);
        AddButton(window, L"START SCAN", kStartScan, 1048, 14, 112, 34);
        AddButton(window, L"HARDWARE", kHardware, 1168, 14, 100, 34);
        AddButton(window, L"SETTINGS", kSettings, 1276, 14, 56, 34);
        AddButton(window, L"X", kClose, 1340, 14, 38, 34);

        AddLabel(window, L"SYSTEM INFORMATION", 38, 94, 300, 30, g_sectionFont);
        g_systemInfo = AddLabel(window, L"Collecting system information...", 38, 132, 730, 166, g_bodyFont, SS_LEFT);
        AddLabel(window, L"SECURITY STATUS", 38, 346, 300, 28, g_sectionFont);
        g_securityStatus = AddLabel(window, L"Waiting for scan...", 38, 380, 730, 64, g_bodyFont);
        AddLabel(window, L"ANTI-CHEAT SOFTWARE", 38, 436, 350, 28, g_sectionFont);
        g_antiCheatStatus = AddLabel(window, L"Waiting for scan...", 38, 469, 730, 36, g_bodyFont);
        AddLabel(window, L"QUICK ACTIONS", 828, 94, 300, 30, g_sectionFont);
        AddButton(window, L"CHECK ONLINE CONNECTION", kQuickActions, 826, 132, 450, 40);
        AddLabel(window, L"THIRD-PARTY ANTIVIRUS", 828, 234, 350, 30, g_sectionFont);
        g_onlineStatus = AddLabel(window, L"Online check has not been run", 828, 270, 420, 42, g_bodyFont);
        g_scanStatus = AddLabel(window, L"READY", 1120, 43, 120, 20, g_bodyFont);
        RefreshDashboard();
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case kStartScan:
            SetWindowTextW(g_scanStatus, L"SCANNING...");
            RefreshDashboard();
            break;
        case kHardware:
            MessageBoxW(window, L"Hardware details are available in the existing HWID Checker report.\n\nUse the console report export for full identifiers.", L"Hardware", MB_OK | MB_ICONINFORMATION);
            break;
        case kQuickActions:
            SetWindowTextW(g_onlineStatus, L"Checking HTTPS connectivity...");
            SetWindowTextW(g_scanStatus, L"ONLINE CHECK...");
            UpdateWindow(window);
            SetWindowTextW(g_onlineStatus, RunOnlineDiagnostics().c_str());
            SetWindowTextW(g_scanStatus, L"SCAN COMPLETE");
            break;
        case kSettings:
            MessageBoxW(window, L"Dashboard settings are not required for this scan.", L"Settings", MB_OK | MB_ICONINFORMATION);
            break;
        case kClose:
            DestroyWindow(window);
            break;
        default:
            break;
        }
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        PaintDashboard(window, dc);
        EndPaint(window, &paint);
        return 0;
    }

    case WM_CTLCOLORSTATIC:
    {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, RGB(245, 241, 255));
        SetBkMode(dc, TRANSPARENT);
        return reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH));
    }

    case WM_CTLCOLORBTN:
    {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, RGB(255, 255, 255));
        SetBkColor(dc, RGB(57, 17, 76));
        return reinterpret_cast<LRESULT>(g_buttonBrush);
    }

    case WM_DESTROY:
        DeleteObject(g_titleFont);
        DeleteObject(g_sectionFont);
        DeleteObject(g_bodyFont);
        DeleteObject(g_buttonBrush);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}
}

int RunDashboard()
{
    HWND console = GetConsoleWindow();
    if (console != nullptr)
    {
        ShowWindow(console, SW_HIDE);
    }

    HINSTANCE instance = GetModuleHandleW(nullptr);
    const wchar_t className[] = L"SupportDashboardWindow";
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = DashboardProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = className;
    RegisterClassW(&windowClass);

    HWND window = CreateWindowExW(0, className, L"Nugz Support Tool",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 1400, 800,
        nullptr, nullptr, instance, nullptr);
    if (window == nullptr)
    {
        return 1;
    }

    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
