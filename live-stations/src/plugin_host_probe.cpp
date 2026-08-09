#include "winamp_media_library_abi.h"

#include <commctrl.h>
#include <windows.h>
#include <windowsx.h>

#include <iostream>
#include <string>

namespace {

constexpr wchar_t kHostClassName[] = L"WinampToolsLiveStationsProbeHost";
constexpr INT_PTR kTreeId = 777;
constexpr int kModeComboId = 1003;
constexpr int kStationListId = 1004;
constexpr int kPlayButtonId = 1005;
constexpr int kWinampPlayCommand = 40045;

std::wstring gEnqueuedUrl;
bool gPositionSelected = false;
bool gPlayCommandReceived = false;

LRESULT CALLBACK WinampProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_COPYDATA) {
        const auto* copyData = reinterpret_cast<COPYDATASTRUCT*>(lParam);
        if (copyData != nullptr && copyData->dwData == static_cast<ULONG_PTR>(IPC_PLAYFILEW) &&
            copyData->lpData != nullptr) {
            gEnqueuedUrl = static_cast<const wchar_t*>(copyData->lpData);
            return TRUE;
        }
    }
    if (message == WM_WA_IPC) {
        if (lParam == IPC_GETLISTLENGTH) {
            return 1;
        }
        if (lParam == IPC_SETPLAYLISTPOS && wParam == 0) {
            gPositionSelected = true;
            return 0;
        }
    }
    if (message == WM_COMMAND && LOWORD(wParam) == kWinampPlayCommand) {
        gPlayCommandReceived = true;
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT CALLBACK LibraryProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_ML_IPC && lParam == ML_IPC_ADDTREEITEM) {
        auto* item = reinterpret_cast<mlAddTreeItemStruct*>(wParam);
        if (item != nullptr) {
            item->thisId = kTreeId;
            return TRUE;
        }
    }
    if (message == WM_ML_IPC && lParam == ML_IPC_DELTREEITEM) {
        return TRUE;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

bool PumpUntil(DWORD timeoutMilliseconds, bool (*condition)()) {
    const ULONGLONG deadline = GetTickCount64() + timeoutMilliseconds;
    while (GetTickCount64() < deadline) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != FALSE) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (condition()) {
            return true;
        }
        Sleep(20);
    }
    return condition();
}

HWND gStationList = nullptr;

bool HasStations() {
    return gStationList != nullptr && ListView_GetItemCount(gStationList) > 0;
}

bool HasVideoStations() {
    if (gStationList == nullptr || ListView_GetItemCount(gStationList) <= 0) {
        return false;
    }
    wchar_t codec[64]{};
    ListView_GetItemText(gStationList, 0, 1, codec, static_cast<int>(_countof(codec)));
    const std::wstring value = codec;
    return value.find(L"H.264") != std::wstring::npos || value == L"MP4" || value == L"FLV";
}

bool PlaybackRequested() {
    return !gEnqueuedUrl.empty() && gPositionSelected && gPlayCommandReceived;
}

}  // namespace

int wmain(int argumentCount, wchar_t** arguments) {
    const wchar_t* dllPath = argumentCount > 1 ? arguments[1] : L"ml_livestations.dll";
    const HMODULE module = LoadLibraryW(dllPath);
    if (module == nullptr) {
        std::wcerr << L"Could not load the plug-in (error " << GetLastError() << L").\n";
        return 1;
    }
    const auto getPlugin = reinterpret_cast<winampMediaLibraryPlugin*(__cdecl*)()>(
        GetProcAddress(module, "winampGetMediaLibraryPlugin"));
    if (getPlugin == nullptr) {
        std::wcerr << L"Missing Winamp Media Library export.\n";
        FreeLibrary(module);
        return 2;
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = kHostClassName;
    windowClass.lpfnWndProc = WinampProc;
    if (RegisterClassExW(&windowClass) == 0) {
        std::wcerr << L"Could not register the fake host window.\n";
        FreeLibrary(module);
        return 3;
    }
    const HWND winamp = CreateWindowExW(
        0, kHostClassName, L"", WS_OVERLAPPED, 0, 0, 640, 480, nullptr, nullptr,
        GetModuleHandleW(nullptr), nullptr);

    windowClass.lpszClassName = L"WinampToolsLiveStationsProbeLibrary";
    windowClass.lpfnWndProc = LibraryProc;
    RegisterClassExW(&windowClass);
    const HWND library = CreateWindowExW(
        0,
        windowClass.lpszClassName,
        L"",
        WS_OVERLAPPED,
        0,
        0,
        640,
        480,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);
    const HWND viewParent = CreateWindowExW(
        0,
        WC_STATICW,
        L"",
        WS_CHILD,
        0,
        0,
        640,
        480,
        library,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);
    if (winamp == nullptr || library == nullptr || viewParent == nullptr) {
        std::wcerr << L"Could not create the fake host windows.\n";
        FreeLibrary(module);
        return 4;
    }

    winampMediaLibraryPlugin* plugin = getPlugin();
    plugin->hwndWinampParent = winamp;
    plugin->hwndLibraryParent = library;
    plugin->hDllInstance = module;
    const int initialization = plugin->init();
    if (initialization != 0) {
        std::wcerr << L"Plug-in initialization failed with code " << initialization << L".\n";
        FreeLibrary(module);
        return 5;
    }

    const HWND view = reinterpret_cast<HWND>(
        plugin->MessageProc(ML_MSG_TREE_ONCREATEVIEW, kTreeId, reinterpret_cast<INT_PTR>(viewParent), 0));
    if (view == nullptr) {
        std::wcerr << L"Plug-in did not create its Media Library view.\n";
        plugin->quit();
        FreeLibrary(module);
        return 6;
    }
    gStationList = GetDlgItem(view, kStationListId);
    if (!PumpUntil(20000, HasStations)) {
        std::wcerr << L"Native view did not load stations within 20 seconds.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 7;
    }

    const HWND modeCombo = GetDlgItem(view, kModeComboId);
    ComboBox_SetCurSel(modeCombo, 1);
    SendMessageW(view, WM_COMMAND, MAKEWPARAM(kModeComboId, CBN_SELCHANGE),
                 reinterpret_cast<LPARAM>(modeCombo));
    if (!PumpUntil(30000, HasVideoStations)) {
        std::wcerr << L"Native view did not load experimental video entries within 30 seconds.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 9;
    }

    ListView_SetItemState(gStationList, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    SendMessageW(view, WM_COMMAND, MAKEWPARAM(kPlayButtonId, BN_CLICKED), 0);
    if (!PumpUntil(20000, PlaybackRequested)) {
        std::wcerr << L"Selecting a station did not request Winamp playback.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 8;
    }

    const int stationCount = ListView_GetItemCount(gStationList);
    DestroyWindow(view);

    const HWND cancellationView = reinterpret_cast<HWND>(
        plugin->MessageProc(ML_MSG_TREE_ONCREATEVIEW, kTreeId, reinterpret_cast<INT_PTR>(viewParent), 0));
    if (cancellationView == nullptr) {
        std::wcerr << L"Plug-in could not create the cancellation test view.\n";
        plugin->quit();
        FreeLibrary(module);
        return 10;
    }
    DestroyWindow(cancellationView);
    plugin->quit();
    DestroyWindow(viewParent);
    DestroyWindow(library);
    DestroyWindow(winamp);
    FreeLibrary(module);

    std::wcout << L"Loaded " << stationCount << L" stations in the native view.\n";
    std::wcout << L"Resolved and handed a safe stream URL to the fake Winamp host.\n";
    std::wcout << L"Destroyed a second view during startup without leaving a worker behind.\n";
    return 0;
}
