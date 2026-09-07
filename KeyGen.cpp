#include <Windows.h>
#include <bcrypt.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <string>

#pragma comment(lib, "bcrypt.lib")

namespace
{
constexpr int kGenerateButton = 1001;
constexpr int kCopyButton = 1002;
constexpr int kCloseButton = 1003;
constexpr int kKeyField = 1004;
constexpr int kStatusText = 1005;
constexpr size_t kKeyBytes = 16;
constexpr char kAlphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";

HWND g_keyField = nullptr;
HWND g_statusText = nullptr;
HFONT g_titleFont = nullptr;
HFONT g_bodyFont = nullptr;
HBRUSH g_backgroundBrush = nullptr;
COLORREF g_statusColor = RGB(42, 30, 58);

std::filesystem::path GetKeyPath()
{
    char executablePath[MAX_PATH]{};
    DWORD length = GetModuleFileNameA(nullptr, executablePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
    {
        return "support.key";
    }
    return std::filesystem::path(executablePath).parent_path() / "support.key";
}

std::string GenerateKey()
{
    std::array<unsigned char, kKeyBytes> bytes{};
    if (BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
    {
        return {};
    }

    std::string key;
    key.reserve(kKeyBytes * 2);
    for (unsigned char byte : bytes)
    {
        key.push_back(kAlphabet[byte >> 3]);
        key.push_back(kAlphabet[byte & 0x1F]);
    }
    return key;
}

bool SaveKey(const std::string& key)
{
    std::ofstream output(GetKeyPath(), std::ios::trunc);
    if (!output)
    {
        return false;
    }
    output << key << '\n';
    return output.good();
}

void SetStatus(const wchar_t* text, COLORREF color)
{
    SetWindowTextW(g_statusText, text);
    InvalidateRect(g_statusText, nullptr, TRUE);
    g_statusColor = color;
}

void CopyKeyToClipboard(HWND window)
{
    int length = GetWindowTextLengthW(g_keyField);
    if (length == 0)
    {
        SetStatus(L"Generate a key first.", RGB(190, 60, 70));
        return;
    }

    std::wstring value(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(g_keyField, value.data(), length + 1);
    value.resize(length);

    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (value.size() + 1) * sizeof(wchar_t));
    if (memory == nullptr)
    {
        SetStatus(L"Unable to access the clipboard.", RGB(190, 60, 70));
        return;
    }

    void* target = GlobalLock(memory);
    memcpy(target, value.c_str(), (value.size() + 1) * sizeof(wchar_t));
    GlobalUnlock(memory);

    if (!OpenClipboard(window))
    {
        GlobalFree(memory);
        SetStatus(L"Unable to access the clipboard.", RGB(190, 60, 70));
        return;
    }
    EmptyClipboard();
    SetClipboardData(CF_UNICODETEXT, memory);
    CloseClipboard();
    SetStatus(L"Key copied to clipboard.", RGB(40, 150, 95));
}

HWND AddControl(HWND parent, const wchar_t* className, const wchar_t* text, DWORD style,
    int x, int y, int width, int height, int id = 0)
{
    HWND control = CreateWindowExW(0, className, text, WS_CHILD | WS_VISIBLE | style,
        x, y, width, height, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_bodyFont), TRUE);
    return control;
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        g_titleFont = CreateFontW(25, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        g_bodyFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        g_backgroundBrush = CreateSolidBrush(RGB(242, 237, 250));

        HWND title = AddControl(window, L"STATIC", L"NUGZ KEY GENERATOR", 0, 28, 24, 400, 34);
        SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(g_titleFont), TRUE);
        AddControl(window, L"STATIC", L"Generate an access key for Support.exe", 0, 30, 64, 380, 24);
        AddControl(window, L"STATIC", L"ACCESS KEY", 0, 30, 108, 160, 22);
        g_keyField = AddControl(window, L"EDIT", L"", WS_BORDER | ES_CENTER | ES_READONLY, 30, 134, 440, 40, kKeyField);
        AddControl(window, L"BUTTON", L"GENERATE KEY", BS_PUSHBUTTON, 30, 198, 136, 38, kGenerateButton);
        AddControl(window, L"BUTTON", L"COPY KEY", BS_PUSHBUTTON, 178, 198, 112, 38, kCopyButton);
        AddControl(window, L"BUTTON", L"CLOSE", BS_PUSHBUTTON, 302, 198, 112, 38, kCloseButton);
        g_statusText = AddControl(window, L"STATIC", L"Ready.", 0, 30, 258, 440, 24, kStatusText);
        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case kGenerateButton:
        {
            std::string key = GenerateKey();
            if (key.empty() || !SaveKey(key))
            {
                SetStatus(L"Could not generate or save the key.", RGB(190, 60, 70));
                break;
            }
            std::wstring wideKey(key.begin(), key.end());
            SetWindowTextW(g_keyField, wideKey.c_str());
            SetStatus(L"Key saved beside KeyGen.exe.", RGB(40, 150, 95));
            break;
        }
        case kCopyButton:
            CopyKeyToClipboard(window);
            break;
        case kCloseButton:
            DestroyWindow(window);
            break;
        default:
            break;
        }
        return 0;

    case WM_CTLCOLORSTATIC:
    {
        HDC dc = reinterpret_cast<HDC>(wParam);
        HWND control = reinterpret_cast<HWND>(lParam);
        SetTextColor(dc, control == g_statusText ? g_statusColor : RGB(42, 30, 58));
        SetBkMode(dc, TRANSPARENT);
        return reinterpret_cast<LRESULT>(g_backgroundBrush);
    }

    case WM_CTLCOLOREDIT:
    {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, RGB(42, 30, 58));
        SetBkColor(dc, RGB(255, 255, 255));
        return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
    }

    case WM_CTLCOLORBTN:
    {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, RGB(255, 255, 255));
        SetBkColor(dc, RGB(93, 35, 125));
        static HBRUSH buttonBrush = CreateSolidBrush(RGB(93, 35, 125));
        return reinterpret_cast<LRESULT>(buttonBrush);
    }

    case WM_DESTROY:
        DeleteObject(g_titleFont);
        DeleteObject(g_bodyFont);
        DeleteObject(g_backgroundBrush);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    const wchar_t className[] = L"NugzKeyGeneratorWindow";
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = className;
    RegisterClassW(&windowClass);

    HWND window = CreateWindowExW(0, className, L"Nugz Key Generator",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 520, 340,
        nullptr, nullptr, instance, nullptr);
    if (window == nullptr)
    {
        return 1;
    }

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
