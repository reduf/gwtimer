#ifndef WINDOWS_LEAN_AND_MEAN
#define WINDOWS_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <Windowsx.h>
#include <psapi.h>

#include <wchar.h>

#include <d3d9.h>
#include <d3dx9core.h>

#include <MinHook.h>

#include "main.h"
#include "Scanner.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

static POINT GetMousePos();
static HRESULT WriteText(LPCWSTR text);
static HRESULT WriteTextF(LPCWSTR frmt, ...);
static bool FindDirectXFunctions();

typedef HRESULT (WINAPI *Reset_pt)(LPDIRECT3DDEVICE9, D3DPRESENT_PARAMETERS *);
typedef HRESULT (WINAPI *EndScene_pt)(LPDIRECT3DDEVICE9);

static Reset_pt     Reset = nullptr;
static EndScene_pt  EndScene = nullptr;
static Reset_pt     fpReset;
static EndScene_pt  fpEndScene;
static LONG_PTR     oldWndProc;
static LPD3DXFONT   textFont;

static WCHAR        fontStyle[256] = L"Arial";
static D3DCOLOR     textColor = D3DCOLOR_ARGB(255, 0, 255, 0);
static DWORD        textSize = 50;

static RECT         textPos; // current text pos that can be updated in EndScene detour & WriteText (the last just ajust size)
static POINT        mOff; // offset when mouse click in timer rect
static bool         draggable; // tell if the timer can be dragged (happend after a click in the rect)
static bool         show = true;

static LPVOID basePtr;
static LPVOID gwWindowHandle;

static GameContext *GetGameCtx()
{
    static uintptr_t base = *reinterpret_cast<uintptr_t *>(basePtr);
    return *reinterpret_cast<GameContext **>(base + 0x18);
}

static DWORD GetMapTimer()
{
    return GetGameCtx()->agent->timer;
}

static HWND GetWindowHandle()
{
    return *reinterpret_cast<HWND *>(gwWindowHandle);
}

static HRESULT WINAPI DetourReset(LPDIRECT3DDEVICE9 device, D3DPRESENT_PARAMETERS *pPresentationParameters)
{
    if (textFont != nullptr)
    {
        textFont->Release();
        textFont = nullptr;
    }

    return fpReset(device, pPresentationParameters);
}

static HRESULT WINAPI DetourEndScene(LPDIRECT3DDEVICE9 device)
{
    if (draggable)
    {
        POINT pos = GetMousePos();
        textPos.left = pos.x - mOff.x;
        textPos.top = pos.y - mOff.y;
    }

    if (textFont == nullptr)
    {
        HRESULT result = D3DXCreateFontW(
            device,
            textSize,
            0,
            0,
            0,
            FALSE,
            DEFAULT_CHARSET,
            OUT_TT_ONLY_PRECIS,
            DEFAULT_QUALITY,
            DEFAULT_PITCH,
            fontStyle,
            &textFont);

        if (!SUCCEEDED(result))
            textFont = nullptr;
    }

    if (show && textFont)
    {
        ULONG time = GetMapTimer() / 1000; // We don't care about milliseconds
        ULONG secs = time % 60;
        ULONG mins = (time / 60) % 60;
        ULONG hours = (time / 3600);
        WriteTextF(L"%02d:%02d:%02d", hours, mins, secs);
    }

    return fpEndScene(device);
}

static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_LBUTTONDOWN:
        {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            if (textPos.left < x && x < textPos.right &&
                textPos.top < y && y < textPos.bottom)
            {
                mOff.x = x - textPos.left;
                mOff.y = y - textPos.top;

                draggable = true;
                return TRUE; // we don't want to let the message go to gw o/w we could click through the timer
            }
        } break;
        case WM_LBUTTONUP:
            draggable = false;
            break;
        case WM_KEYUP:
        {
            if ((ULONG)wParam == VK_HOME)
            {
                // show = !show;
            }
        } break;
    }

    return CallWindowProc((WNDPROC)oldWndProc, hWnd, message, wParam, lParam);
}

static void LoadConfiguration()
{
    HKEY hKey;
    DWORD cbData;
    LSTATUS status;
    WCHAR defaultFont[] = L"Arial";
    static_assert(sizeof(defaultFont) <= sizeof(fontStyle),
        "Size of the default font should fit in the buffer holding the font");

    DWORD disposition;
    status = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\GwTimer",
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE | KEY_READ,
        nullptr,
        &hKey,
        &disposition);

    if (status != ERROR_SUCCESS)
    {
        OutputDebugStringW(L"Failed to open the reg hive 'Software\\GwTimer'");
        return;
    }

    if (disposition == REG_CREATED_NEW_KEY)
    {
        status = RegSetValueExW(
            hKey,
            L"FontStyle",
            0,
            REG_SZ,
            reinterpret_cast<const BYTE *>(defaultFont),
            sizeof(defaultFont));

        if (status != ERROR_SUCCESS)
        {
            OutputDebugStringW(L"RegSetValueExW failed for FontStyle");
        }

        status = RegSetValueExW(
            hKey,
            L"TextColor",
            0,
            REG_DWORD,
            reinterpret_cast<const BYTE *>(&textColor),
            sizeof(textColor));

        if (status != ERROR_SUCCESS)
        {
            OutputDebugStringW(L"RegSetValueExW failed for TextColor");
        }

        status = RegSetValueExW(
            hKey,
            L"TextSize",
            0,
            REG_DWORD,
            reinterpret_cast<const BYTE *>(&textSize),
            sizeof(textSize));

        if (status != ERROR_SUCCESS)
        {
            OutputDebugStringW(L"RegSetValueExW failed for TextSize");
        }

        textSize = 50;
        textColor = D3DCOLOR_ARGB(255, 0, 255, 0);
        memcpy(fontStyle, defaultFont, sizeof(defaultFont));
    }
    else if (disposition == REG_OPENED_EXISTING_KEY)
    {
        cbData = sizeof(fontStyle) - 2;
        status = RegGetValueW(
            hKey,
            L"",
            L"FontStyle",
            RRF_RT_REG_SZ,
            nullptr,
            fontStyle,
            &cbData);

        if (status != ERROR_SUCCESS)
        {
            cbData = sizeof(defaultFont) - 2;
            memcpy(fontStyle, defaultFont, sizeof(defaultFont));
        }

        fontStyle[cbData / 2] = 0;

        cbData = sizeof(textColor);
        status = RegGetValueW(
            hKey,
            L"",
            L"TextColor",
            RRF_RT_REG_DWORD,
            nullptr,
            &textColor,
            &cbData);

        if (status != ERROR_SUCCESS)
        {
            textColor = D3DCOLOR_ARGB(255, 0, 255, 0);
        }

        cbData = sizeof(textSize);
        status = RegGetValueW(
            hKey,
            L"",
            L"TextSize",
            RRF_RT_REG_DWORD,
            nullptr,
            &textSize,
            &cbData);

        if (status != ERROR_SUCCESS)
        {
            textSize = 50;
        }
    }

    RegCloseKey(hKey);
}

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        LoadConfiguration();

        Scanner scanner;

        {
            LPVOID found = scanner.FindPattern("\x50\x6A\x0F\x6A\x00\xFF\x35", "xxxxxxx", +7);
            if (found == nullptr)
            {
                MessageBoxA(0, "Can't find 'BasePtr' pattern.", "Error", MB_OK | MB_ICONERROR);
                return FALSE;
            }
            basePtr = *(LPVOID *)found;
        }

        {
            LPVOID found = scanner.FindPattern("\x83\xC4\x04\x83\x3D\x00\x00\x00\x00\x00\x75\x31", "xxxxx????xxx", -0xC);
            if (found == nullptr)
            {
                MessageBoxA(0, "Can't find 'GwHWND' pattern.", "Error", MB_OK | MB_ICONERROR);
                return FALSE;
            }
            gwWindowHandle = *(LPVOID *)found;
        }

        if (MH_Initialize() != MH_OK)
        {
            MessageBoxA(0, "Error at Initialization.", "Error", MB_OK | MB_ICONERROR);
            return FALSE;
        }

        if (!FindDirectXFunctions())
        {
            MessageBoxA(0, "Error at Finding EndScene and Reset", "Error", MB_OK | MB_ICONERROR);
            return FALSE;
        }

        if (MH_CreateHook(Reset, &DetourReset, (LPVOID*)&fpReset) != MH_OK)
        {
            MessageBoxA(0, "Error creating hook on Reset.", "Error", MB_OK | MB_ICONERROR);
            return FALSE;
        }

        if (MH_CreateHook(EndScene, &DetourEndScene, (LPVOID*)&fpEndScene) != MH_OK)
        {
            MessageBoxA(0, "Error creating hook on EndScene.", "Error", MB_OK | MB_ICONERROR);
            return FALSE;
        }
        
        oldWndProc = SetWindowLongPtrW(GetWindowHandle(), GWLP_WNDPROC, (LONG_PTR)WindowProc);
        if (oldWndProc == 0)
        {
            MessageBoxA(0, "Error SetWindowLongPtr", "Error", MB_OK | MB_ICONERROR);
            return FALSE;
        }

        MH_EnableHook(MH_ALL_HOOKS);
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        SetWindowLongPtr(GetWindowHandle(), GWLP_WNDPROC, oldWndProc);
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
    }

    return TRUE;
}

static bool FindDirectXFunctions()
{
    static LPCSTR sign = "\xC7\x06\x00\x00\x00\x00\x89\x86\x00\x00\x00\x00\x89\x86";
    static LPCSTR mask = "xx????xx????xx";

    Scanner scanner("d3d9.dll", 0x128000);
    DWORD **finded = (DWORD**)scanner.FindPattern(sign, mask, 2);
    if (finded == nullptr)
        return nullptr;

    DWORD *vTable = *finded;
    if (vTable == nullptr)
        return false;

    Reset = (Reset_pt)vTable[16];
    EndScene = (EndScene_pt)vTable[42];
    return (Reset != nullptr) && (EndScene != nullptr);
}

static POINT GetMousePos()
{
    POINT pos = {};
    GetCursorPos(&pos);
    ScreenToClient(GetWindowHandle(), &pos);

    return pos;
}

static HRESULT WriteTextF(LPCWSTR frmt, ...)
{
    va_list args;
    WCHAR buffer[256]; // Already overkill, no need to check with _vscwprintf

    va_start(args, frmt);
    vswprintf_s(buffer, 256, frmt, args);
    va_end(args);

    return WriteText(buffer);
}

static HRESULT WriteText(LPCWSTR text)
{
    textFont->DrawTextW(nullptr, text, -1, &textPos, DT_CALCRECT, 0);
    textFont->DrawTextW(nullptr, text, -1, &textPos, DT_CENTER, textColor);
    return S_OK;
}
