#include "winamp_media_library_abi.h"

#include <commctrl.h>
#include <windows.h>
#include <windowsx.h>
#include <uxtheme.h>

#pragma comment(lib, "uxtheme.lib")

#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr wchar_t kHostClassName[] = L"WinampToolsLiveStationsProbeHost";
constexpr wchar_t kViewClassName[] = L"WinampToolsLiveStationsView";
constexpr INT_PTR kTreeId = 777;
constexpr int kSearchEditId = 1001;
constexpr int kSearchButtonId = 1002;
constexpr int kModeComboId = 1003;
constexpr int kStationListId = 1004;
constexpr int kTunerLabelId = 1009;
constexpr int kGenreComboId = 1012;
constexpr int kCountryComboId = 1013;
constexpr int kLanguageComboId = 1014;
constexpr int kBrowseLabelId = 1015;
constexpr int kResetButtonId = 1016;
constexpr int kTunerToggleButtonId = 1017;
constexpr int kResultCountId = 1018;
constexpr int kNowPlayingId = 1019;
constexpr int kBitrateComboId = 1020;
constexpr int kStationsViewButtonId = 1021;
constexpr int kBrowseViewButtonId = 1022;
constexpr int kFavoritesViewButtonId = 1034;
constexpr int kLocationListId = 1023;
constexpr int kGenreListId = 1024;
constexpr int kProviderTotalId = 1027;
constexpr int kPageSizeComboId = 1028;
constexpr int kPreviousPageButtonId = 1029;
constexpr int kNextPageButtonId = 1030;
constexpr int kStatusId = 1031;
constexpr int kFilterToggleButtonId = 1032;
constexpr int kCompatibilityComboId = 1033;
constexpr int kSelectionDetailsId = 1011;
constexpr int kAddFavoriteCommandId = 1040;
constexpr int kAddWinampPlaylistCommandId = 1041;
constexpr int kBodyViewportId = 1042;
constexpr int kToolbarViewportId = 1043;
constexpr int kWinampPlayCommand = 40045;
constexpr UINT kProbeWorkState = WM_APP + 47; // Offline fixture only; production has no handler.
constexpr UINT kDeferredSearchMessage = WM_APP + 42;

constexpr int kToolbarControlIds[] = {
    kBrowseViewButtonId, kFavoritesViewButtonId, kFilterToggleButtonId, kTunerToggleButtonId};

HWND FindControl(HWND root, int id) {
    if (const HWND control = GetDlgItem(root, id)) return control;
    for (int containerId : {kToolbarViewportId, kBodyViewportId}) {
        const HWND container = GetDlgItem(root, containerId);
        if (container != nullptr) {
            if (const HWND control = GetDlgItem(container, id)) return control;
        }
    }
    return nullptr;
}

std::wstring gEnqueuedUrl;
std::wstring gPlayingTitle;
bool gPositionSelected = false;
bool gPlayCommandReceived = false;
int gCurrentPlaylistPosition = -1;
int gPlaybackState = 0;
int gListSkinCount = 0;
int gComboSkinCount = 0;
int gListUnskinCount = 0;
int gComboUnskinCount = 0;
int gSearchStartCount = 0;
int gBusyListClearCount = 0;
int gViewEraseCount = 0;
int gViewPaintCount = 0;
int gPaletteLookups = 0;
int gPaletteCalls = 0;
int gPaletteMode = 0; // dark, light, absent, unsupported sentinel, invalid RGB
HWND gPaletteDestroyOnLookup = nullptr;
HWND gPaletteDestroyOnColor = nullptr;
HWND gPaletteReenterOnLookup = nullptr;
HWND gGenreTraceRoot = nullptr;
bool gGenreTraceFixture = false;
int gGenreTraceDeferredSearches = 0;
int gGenreTraceNotificationCount = 0;

struct GenreSelectionTrace {
    int row = -1;
    int selected = -1;
    UINT changed = 0;
    UINT oldState = 0;
    UINT newState = 0;
    LPARAM rowData = 0;
    LRESULT workBefore = -1;
    LRESULT workAfter = -1;
};

std::vector<GenreSelectionTrace> gGenreSelectionTrace;

struct SkinWrapperCounts {
    int listSkins = 0;
    int comboSkins = 0;
    int listUnskins = 0;
    int comboUnskins = 0;
};

SkinWrapperCounts CurrentSkinWrapperCounts() {
    return {
        gListSkinCount,
        gComboSkinCount,
        gListUnskinCount,
        gComboUnskinCount,
    };
}

bool HasSkinWrapperCounts(
    const SkinWrapperCounts& counts,
    int listSkins,
    int comboSkins,
    int listUnskins,
    int comboUnskins) {
    return counts.listSkins == listSkins && counts.comboSkins == comboSkins &&
           counts.listUnskins == listUnskins && counts.comboUnskins == comboUnskins;
}

void ReportSkinWrapperCounts(const wchar_t* stage, const SkinWrapperCounts& counts) {
    std::wcerr << L"Skin-only " << stage << L" wrappers: list skins=" << counts.listSkins
               << L", combo skins=" << counts.comboSkins
               << L", list unskins=" << counts.listUnskins
               << L", combo unskins=" << counts.comboUnskins << L".\n";
}

COLORREF ExpectedSkinColor(int index) {
    if (gPaletteMode == 1) {
        switch (index) {
            case WADLG_ITEMBG: return RGB(250, 248, 235);
            case WADLG_WNDBG: return RGB(239, 234, 211);
            case WADLG_ITEMFG:
            case WADLG_WNDFG: return RGB(0, 0, 0); // Zero is a valid color.
            default: return RGB(90, 80, 60);
        }
    }
    switch (index) {
        case WADLG_ITEMBG:
            return RGB(18, 24, 30);
        case WADLG_ITEMFG:
            return RGB(222, 232, 238);
        case WADLG_WNDBG:
            return RGB(30, 38, 48);
        case WADLG_WNDFG:
            return RGB(232, 239, 244);
        case WADLG_HILITE:
            return RGB(95, 140, 180);
        default:
            return RGB(255, 255, 255);
    }
}

int __cdecl FakeSkinColor(int index) {
    ++gPaletteCalls;
    if (gPaletteDestroyOnColor != nullptr) {
        const HWND doomed = gPaletteDestroyOnColor;
        gPaletteDestroyOnColor = nullptr;
        DestroyWindow(doomed);
    }
    return gPaletteMode == 4 ? -1 : static_cast<int>(ExpectedSkinColor(index));
}

LRESULT __cdecl FakeHandleDialogMessage(HWND, UINT, WPARAM, LPARAM) {
    return 0;
}

void __cdecl FakeDrawBorders(HWND window, int*, int) {
    PAINTSTRUCT paint{};
    BeginPaint(window, &paint);
    EndPaint(window, &paint);
}

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
        if (lParam == IPC_ISPLAYING) {
            return gPlaybackState;
        }
        if (lParam == IPC_GETLISTLENGTH) {
            return gEnqueuedUrl.empty() ? 0 : 1;
        }
        if (lParam == IPC_SETPLAYLISTPOS && wParam == 0) {
            gPositionSelected = true;
            gCurrentPlaylistPosition = 0;
            return 0;
        }
        if (lParam == IPC_GETLISTPOS) {
            return gCurrentPlaylistPosition;
        }
        if (lParam == IPC_GETPLAYLISTFILEW && wParam == 0 && !gEnqueuedUrl.empty()) {
            return reinterpret_cast<LRESULT>(gEnqueuedUrl.c_str());
        }
        if (lParam == IPC_GET_PLAYING_TITLE && !gPlayingTitle.empty()) {
            return reinterpret_cast<LRESULT>(gPlayingTitle.c_str());
        }
    }
    if (message == WM_COMMAND && LOWORD(wParam) == kWinampPlayCommand) {
        gPlayCommandReceived = true;
        gPlaybackState = 1;
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT CALLBACK LibraryProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_ML_IPC && lParam == ML_IPC_TREEITEM_ADDW) {
        auto* item = reinterpret_cast<mlTreeItemW*>(wParam);
        if (item != nullptr) {
            item->id = kTreeId;
            return TRUE;
        }
    }
    if (message == WM_ML_IPC && lParam == ML_IPC_DELTREEITEM) {
        return TRUE;
    }
    if (message == WM_ML_IPC && lParam == ML_IPC_SKIN_WADLG_GETFUNC) {
        switch (wParam) {
            case ML_WADLG_GETCOLOR:
                ++gPaletteLookups;
                if (gPaletteDestroyOnLookup != nullptr) {
                    const HWND doomed = gPaletteDestroyOnLookup;
                    gPaletteDestroyOnLookup = nullptr;
                    DestroyWindow(doomed);
                }
                if (gPaletteReenterOnLookup != nullptr) {
                    const HWND nested = gPaletteReenterOnLookup;
                    gPaletteReenterOnLookup = nullptr;
                    SendMessageW(nested, WM_THEMECHANGED, 0, 0);
                }
                if (gPaletteMode == 2) return 0;
                if (gPaletteMode == 3) return 1;
                return reinterpret_cast<LRESULT>(&FakeSkinColor);
            case ML_WADLG_HANDLEDIALOGMSGS:
                return reinterpret_cast<LRESULT>(&FakeHandleDialogMessage);
            case ML_WADLG_DRAWCHILDWINDOWBORDERS:
                return reinterpret_cast<LRESULT>(&FakeDrawBorders);
            case ML_WADLG_SKINFONT:
                return reinterpret_cast<LRESULT>(GetStockObject(DEFAULT_GUI_FONT));
            default:
                return 0;
        }
    }
    if (message == WM_ML_IPC && lParam == ML_IPC_SKIN_LISTVIEW) {
        if (IsWindow(reinterpret_cast<HWND>(wParam)) != FALSE) {
            ++gListSkinCount;
        }
        return 1000 + gListSkinCount;
    }
    if (message == WM_ML_IPC && lParam == ML_IPC_SKIN_COMBOBOX) {
        if (IsWindow(reinterpret_cast<HWND>(wParam)) != FALSE) {
            ++gComboSkinCount;
        }
        return 2000 + gComboSkinCount;
    }
    if (message == WM_ML_IPC && lParam == ML_IPC_UNSKIN_LISTVIEW && wParam > 1000 &&
        wParam < 2000) {
        ++gListUnskinCount;
        return TRUE;
    }
    if (message == WM_ML_IPC && lParam == ML_IPC_UNSKIN_COMBOBOX && wParam > 2000) {
        ++gComboUnskinCount;
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

void PumpFor(DWORD durationMilliseconds) {
    const ULONGLONG deadline = GetTickCount64() + durationMilliseconds;
    while (GetTickCount64() < deadline) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != FALSE) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        Sleep(20);
    }
}

HWND gStationList = nullptr;
HWND gGenreCombo = nullptr;
HWND gCountryCombo = nullptr;
HWND gLanguageCombo = nullptr;
HWND gNowPlaying = nullptr;
HWND gLocationList = nullptr;
HWND gBrowseGenreList = nullptr;
HWND gProviderTotal = nullptr;
std::wstring gExpectedNowPlaying;

std::wstring ControlText(HWND control);
bool VerifyResultCount(HWND resultCount, bool matching);

bool HasStations() {
    return gStationList != nullptr && ListView_GetItemCount(gStationList) > 0;
}

bool HasSearchReplacement() {
    return gSearchStartCount > 0;
}

bool HasBrowseCategories() {
    return gGenreCombo != nullptr && gCountryCombo != nullptr && gLanguageCombo != nullptr &&
           ComboBox_GetCount(gGenreCombo) > 1 && ComboBox_GetCount(gCountryCombo) > 1 &&
           ComboBox_GetCount(gLanguageCombo) > 1 && IsWindowEnabled(gGenreCombo) != FALSE;
}

std::wstring ListCellText(HWND list, int row, int column) {
    wchar_t text[512]{};
    ListView_GetItemText(list, row, column, text, static_cast<int>(_countof(text)));
    return text;
}

bool HasBrowsePanes() {
    return gLocationList != nullptr && gBrowseGenreList != nullptr &&
           ListView_GetItemCount(gLocationList) > 1 &&
           ListView_GetItemCount(gBrowseGenreList) > 1 &&
           gProviderTotal != nullptr &&
           ControlText(gProviderTotal).rfind(L"Radio Browser total: ", 0) == 0;
}

bool HasPreviousGenreCategoryPage() {
    return gBrowseGenreList != nullptr && ListView_GetItemCount(gBrowseGenreList) > 2 &&
           ListCellText(gBrowseGenreList, 1, 0) == L"Previous categories...";
}

bool HasFirstGenreCategoryPage() {
    return gBrowseGenreList != nullptr && ListView_GetItemCount(gBrowseGenreList) > 1 &&
           ListCellText(gBrowseGenreList, 1, 0) != L"Previous categories..." &&
           IsWindowEnabled(gBrowseGenreList) != FALSE;
}

bool ResultPageStartsAfterFirst() {
    const HWND resultCount = gStationList == nullptr
                                 ? nullptr
                                 : GetDlgItem(GetParent(gStationList), kResultCountId);
    return resultCount != nullptr &&
           ControlText(resultCount).find(L"rows 51-") != std::wstring::npos;
}

bool StationHeaderHasSortDirection(int column, bool descending) {
    if (gStationList == nullptr) {
        return false;
    }
    const HWND header = ListView_GetHeader(gStationList);
    if (header == nullptr) {
        return false;
    }
    HDITEMW item{};
    item.mask = HDI_FORMAT;
    if (Header_GetItem(header, column, &item) == FALSE) {
        return false;
    }
    return descending ? (item.fmt & HDF_SORTDOWN) != 0 : (item.fmt & HDF_SORTUP) != 0;
}

void ClickStationColumn(HWND view, int column) {
    NMLISTVIEW click{};
    click.hdr.hwndFrom = ListView_GetHeader(gStationList);
    click.hdr.idFrom = kStationListId;
    click.hdr.code = LVN_COLUMNCLICK;
    click.iSubItem = column;
    SendMessageW(FindControl(view, kBodyViewportId), WM_NOTIFY, kStationListId,
                 reinterpret_cast<LPARAM>(&click));
}

bool VerifyStationColumnSorting(HWND view, HWND resultCount) {
    if (view == nullptr || gStationList == nullptr || ListView_GetItemCount(gStationList) < 2) {
        return false;
    }
    // Fixture station 1 is 64 kbps and every other initial row is 128 kbps.
    // The first bitrate click must be highest-first, then reverse to lowest-first.
    ClickStationColumn(view, 2);
    if (ListCellText(gStationList, 0, 2) != L"128" ||
        !StationHeaderHasSortDirection(2, true) || !VerifyResultCount(resultCount, false)) {
        return false;
    }
    ClickStationColumn(view, 2);
    if (ListCellText(gStationList, 0, 2) != L"64" ||
        !StationHeaderHasSortDirection(2, false) || !VerifyResultCount(resultCount, false)) {
        return false;
    }
    ClickStationColumn(view, 0);
    const std::wstring ascendingFirst = ListCellText(gStationList, 0, 0);
    const std::wstring ascendingLast =
        ListCellText(gStationList, ListView_GetItemCount(gStationList) - 1, 0);
    if (CompareStringOrdinal(
            ascendingFirst.c_str(), -1, ascendingLast.c_str(), -1, TRUE) != CSTR_LESS_THAN ||
        !StationHeaderHasSortDirection(0, false)) {
        return false;
    }
    ClickStationColumn(view, 0);
    const std::wstring descendingFirst = ListCellText(gStationList, 0, 0);
    const std::wstring descendingLast =
        ListCellText(gStationList, ListView_GetItemCount(gStationList) - 1, 0);
    return CompareStringOrdinal(
               descendingFirst.c_str(), -1, descendingLast.c_str(), -1, TRUE) == CSTR_GREATER_THAN &&
           StationHeaderHasSortDirection(0, true) && VerifyResultCount(resultCount, false);
}

bool PlaybackRequested() {
    return !gEnqueuedUrl.empty() && gPositionSelected && gPlayCommandReceived;
}

int WindowWidth(HWND window) {
    RECT bounds{};
    return GetWindowRect(window, &bounds) != FALSE ? bounds.right - bounds.left : 0;
}

bool HasVisibleStyle(HWND window) {
    return window != nullptr && (GetWindowLongPtrW(window, GWL_STYLE) & WS_VISIBLE) != 0;
}

std::wstring ControlText(HWND control) {
    const int length = GetWindowTextLengthW(control);
    std::wstring text(static_cast<size_t>((std::max)(0, length)) + 1U, L'\0');
    if (length > 0) {
        GetWindowTextW(control, text.data(), length + 1);
    }
    text.resize(static_cast<size_t>((std::max)(0, length)));
    return text;
}

bool VerifyResultCount(HWND resultCount, bool matching) {
    const int count = ListView_GetItemCount(gStationList);
    std::wstring expected = std::to_wstring(count);
    if (matching) {
        expected += count == 1 ? L" matching station shown on this page"
                               : L" matching stations shown on this page";
    } else {
        expected += count == 1 ? L" station shown" : L" stations shown";
    }
    const std::wstring actual = ControlText(resultCount);
    return HasVisibleStyle(resultCount) && actual.rfind(expected, 0) == 0;
}

bool ControlStripIsInteractive(HWND view) {
    constexpr int kInteractiveControlIds[] = {
        kSearchEditId,
        kSearchButtonId,
        kResetButtonId,
        kModeComboId,
        kBitrateComboId,
        kCompatibilityComboId,
        kStationsViewButtonId,
        kBrowseViewButtonId,
        kFavoritesViewButtonId,
        kPageSizeComboId,
        kPreviousPageButtonId,
        kNextPageButtonId,
        kFilterToggleButtonId,
    };
    for (const int controlId : kInteractiveControlIds) {
        const HWND control = FindControl(view, controlId);
        if (control == nullptr || IsWindowEnabled(control) == FALSE) {
            return false;
        }
    }
    return true;
}

bool NowPlayingMatches() {
    return gNowPlaying != nullptr && ControlText(gNowPlaying) == gExpectedNowPlaying;
}

bool NowPlayingIsClear() {
    return gNowPlaying != nullptr && ControlText(gNowPlaying).empty();
}

RECT BoundsInView(HWND control, HWND view) {
    RECT bounds{};
    GetWindowRect(control, &bounds);
    MapWindowPoints(nullptr, view, reinterpret_cast<POINT*>(&bounds), 2);
    return bounds;
}

BOOL CALLBACK SetProbeFont(HWND control, LPARAM font) {
    SendMessageW(control, WM_SETFONT, static_cast<WPARAM>(font), TRUE);
    return TRUE;
}

bool PaletteMatches(HWND view, bool fallback) {
    const COLORREF background = fallback ? GetSysColor(COLOR_WINDOW) : ExpectedSkinColor(WADLG_WNDBG);
    const COLORREF foreground = fallback ? GetSysColor(COLOR_WINDOWTEXT) : ExpectedSkinColor(WADLG_WNDFG);
    const COLORREF itemBackground = fallback ? GetSysColor(COLOR_WINDOW) : ExpectedSkinColor(WADLG_ITEMBG);
    const COLORREF itemForeground = fallback ? GetSysColor(COLOR_WINDOWTEXT) : ExpectedSkinColor(WADLG_ITEMFG);
    for (int id : {kStationListId, kLocationListId, kGenreListId}) {
        const HWND list = FindControl(view, id);
        if (ListView_GetBkColor(list) != itemBackground ||
            ListView_GetTextBkColor(list) != itemBackground ||
            ListView_GetTextColor(list) != itemForeground) return false;
        const HWND tooltip = ListView_GetToolTips(list);
        if (tooltip == nullptr || GetWindowTheme(tooltip) != nullptr ||
            static_cast<COLORREF>(SendMessageW(tooltip, TTM_GETTIPBKCOLOR, 0, 0)) != itemBackground ||
            static_cast<COLORREF>(SendMessageW(tooltip, TTM_GETTIPTEXTCOLOR, 0, 0)) != itemForeground ||
            SendMessageW(tooltip, WM_GETFONT, 0, 0) != SendMessageW(list, WM_GETFONT, 0, 0)) return false;
    }
    const HWND body = FindControl(view, kBodyViewportId);
    if (body == nullptr) return false;
    const HDC dc = GetDC(body);
    if (dc == nullptr) return false;
    const auto brush = reinterpret_cast<HBRUSH>(SendMessageW(body, WM_CTLCOLORSTATIC,
        reinterpret_cast<WPARAM>(dc), reinterpret_cast<LPARAM>(FindControl(view, kTunerLabelId))));
    LOGBRUSH definition{};
    const bool correct = GetObjectW(brush, sizeof(definition), &definition) != 0 &&
        definition.lbColor == background && GetBkColor(dc) == background && GetTextColor(dc) == foreground;
    const auto detailsBrush = reinterpret_cast<HBRUSH>(SendMessageW(body, WM_CTLCOLORSTATIC,
        reinterpret_cast<WPARAM>(dc), reinterpret_cast<LPARAM>(FindControl(view, kSelectionDetailsId))));
    const bool details = GetObjectW(detailsBrush, sizeof(definition), &definition) != 0 &&
        definition.lbColor == itemBackground && GetTextColor(dc) == itemForeground;
    ReleaseDC(body, dc);
    return correct && details;
}

bool VerifyPaletteRefresh(HWND view) {
    if (gPaletteLookups < 1 || !PaletteMatches(view, false)) return false;
    for (int mode : {1, 2, 3, 4, 0}) {
        gPaletteMode = mode;
        const int before = gPaletteLookups;
        // Exercise both native theme/system notifications and ML's display path.
        const UINT notification = mode == 1 ? WM_THEMECHANGED :
                                  mode == 2 ? WM_SYSCOLORCHANGE : WM_DISPLAYCHANGE;
        SendMessageW(view, notification, 0, 0);
        if (gPaletteLookups != before + 1 || !PaletteMatches(view, mode >= 2)) {
            std::wcerr << L"Palette refresh failed in mode " << mode << L".\n";
            return false;
        }
    }
    const int before = gPaletteLookups;
    gPaletteReenterOnLookup = view;
    SendMessageW(view, WM_THEMECHANGED, 0, 0);
    if (gPaletteLookups != before + 1 || !PaletteMatches(view, false)) return false;
    const int calls = gPaletteCalls;
    RedrawWindow(view, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    if (gPaletteLookups != before + 1 || gPaletteCalls != calls) return false;
    std::wcout << L"Skin palette: dark/light, valid black, missing/sentinel/invalid fallback, "
                  L"nested notification and cached RGB-only painting passed.\n";
    return true;
}

// Check each complete document independently: body scrolling must never move
// the toolbar, and tiny toolbar overflow must remain reachable in its own pane.
bool VerifyDocumentGeometry(HWND container) {
    SCROLLINFO horizontal{sizeof(horizontal), SIF_ALL};
    SCROLLINFO vertical{sizeof(vertical), SIF_ALL};
    GetScrollInfo(container, SB_HORZ, &horizontal);
    GetScrollInfo(container, SB_VERT, &vertical);
    RECT client{};
    GetClientRect(container, &client);
    const int documentWidth = (std::max)(static_cast<int>(client.right), horizontal.nMax + 1);
    const int documentHeight = (std::max)(static_cast<int>(client.bottom), vertical.nMax + 1);
    std::vector<RECT> occupied;
    for (HWND child = GetWindow(container, GW_CHILD); child != nullptr;
         child = GetWindow(child, GW_HWNDNEXT)) {
        if (!HasVisibleStyle(child)) continue;
        RECT bounds = BoundsInView(child, container);
        OffsetRect(&bounds, horizontal.nPos, vertical.nPos);
        if (bounds.left < 0 || bounds.top < 0 || bounds.right > documentWidth ||
            bounds.bottom > documentHeight || bounds.right <= bounds.left ||
            bounds.bottom <= bounds.top) {
            std::wcerr << L"Control outside scrollable document: " << GetDlgCtrlID(child)
                       << L" [" << bounds.left << L"," << bounds.top << L","
                       << bounds.right << L"," << bounds.bottom << L"] extent "
                       << documentWidth << L"x" << documentHeight << L".\n";
            return false;
        }
        for (const RECT& other : occupied) {
            RECT intersection{};
            if (IntersectRect(&intersection, &bounds, &other)) {
                std::wcerr << L"Overlapping control: " << GetDlgCtrlID(child) << L".\n";
                return false;
            }
        }
        occupied.push_back(bounds);
    }
    return true;
}

bool VerifyResponsiveGeometry(HWND view) {
    const HWND body = FindControl(view, kBodyViewportId);
    const HWND toolbar = FindControl(view, kToolbarViewportId);
    if (body == nullptr || toolbar == nullptr) return false;
    RECT client{};
    GetClientRect(view, &client);
    const RECT bodyBounds = BoundsInView(body, view);
    const RECT toolbarBounds = BoundsInView(toolbar, view);
    for (HWND container : {toolbar, body}) {
        const RECT bounds = BoundsInView(container, view);
        if (GetParent(container) != view || !HasVisibleStyle(container) ||
            (GetWindowLongPtrW(container, GWL_EXSTYLE) & WS_EX_CONTROLPARENT) == 0 ||
            (GetWindowLongPtrW(container, GWL_STYLE) & WS_CLIPCHILDREN) == 0 ||
            bounds.left < 0 || bounds.top < 0 || bounds.right > client.right ||
            bounds.bottom > client.bottom || bounds.right <= bounds.left ||
            bounds.bottom <= bounds.top || !VerifyDocumentGeometry(container)) {
            std::wcerr << L"Invalid nested viewport: " << GetDlgCtrlID(container) << L".\n";
            return false;
        }
    }
    if (toolbarBounds.top != 0 || toolbarBounds.bottom > bodyBounds.top ||
        (GetWindowLongPtrW(view, GWL_STYLE) & (WS_HSCROLL | WS_VSCROLL)) != 0) return false;
    for (HWND child = GetWindow(view, GW_CHILD); child != nullptr;
         child = GetWindow(child, GW_HWNDNEXT)) {
        if (child != body && child != toolbar) return false;
    }
    for (int id : kToolbarControlIds) {
        const HWND button = FindControl(view, id);
        if (GetParent(button) != toolbar || !HasVisibleStyle(button) || WindowWidth(button) < 90 ||
            (GetWindowLongPtrW(button, GWL_STYLE) & WS_TABSTOP) == 0) {
            std::wcerr << L"Toolbar control is unavailable or too narrow: " << id << L".\n";
            return false;
        }
    }
    return !HasVisibleStyle(FindControl(view, kStationsViewButtonId));
}

std::vector<RECT> ToolbarBounds(HWND view) {
    std::vector<RECT> result;
    for (int id : kToolbarControlIds) result.push_back(BoundsInView(FindControl(view, id), view));
    return result;
}

bool ToolbarStayedPut(HWND view, const std::vector<RECT>& expected) {
    const std::vector<RECT> actual = ToolbarBounds(view);
    if (actual.size() != expected.size()) return false;
    for (size_t index = 0; index != actual.size(); ++index) {
        if (!EqualRect(&actual[index], &expected[index])) return false;
    }
    return true;
}

bool VerifyPinnedToolbar(HWND view, bool exerciseFocus) {
    if (!VerifyResponsiveGeometry(view)) return false;
    const HWND body = FindControl(view, kBodyViewportId);
    const HWND toolbar = FindControl(view, kToolbarViewportId);
    const auto pinned = ToolbarBounds(view);
    bool stable = true;
    for (UINT message : {WM_VSCROLL, WM_HSCROLL}) {
        SendMessageW(body, message, SB_BOTTOM, 0);
        stable = ToolbarStayedPut(view, pinned) && VerifyResponsiveGeometry(view) && stable;
        SendMessageW(body, message, SB_TOP, 0);
        stable = ToolbarStayedPut(view, pinned) && VerifyResponsiveGeometry(view) && stable;
    }
    if (exerciseFocus) {
        for (int id : {kSearchEditId, kLocationListId, kGenreListId, kSelectionDetailsId}) {
            const HWND control = FindControl(view, id);
            if (!HasVisibleStyle(control) || IsWindowEnabled(control) == FALSE) continue;
            SetFocus(control);
            stable = GetFocus() == control && ToolbarStayedPut(view, pinned) && stable;
        }
        SendMessageW(body, WM_VSCROLL, SB_BOTTOM, 0);
        const int bodyOffset = GetScrollPos(body, SB_VERT);
        for (int id : kToolbarControlIds) {
            const HWND button = FindControl(view, id);
            SetFocus(button);
            stable = GetFocus() == button && GetScrollPos(body, SB_VERT) == bodyOffset &&
                     ToolbarStayedPut(view, pinned) && stable;
        }
        // IsDialogMessage's tab traversal relies on both nested control parents.
        const HWND lastToolbar = FindControl(view, kTunerToggleButtonId);
        const HWND next = GetNextDlgTabItem(view, lastToolbar, FALSE);
        const HWND previous = next == nullptr ? nullptr : GetNextDlgTabItem(view, next, TRUE);
        stable = next != nullptr && IsChild(body, next) && previous == lastToolbar && stable;
        if (next != nullptr) {
            SetFocus(next);
            stable = GetFocus() == next && ToolbarStayedPut(view, pinned) && stable;
        }
        SetFocus(FindControl(view, kBrowseViewButtonId));
    }
    SendMessageW(body, WM_VSCROLL, SB_TOP, 0);
    SendMessageW(body, WM_HSCROLL, SB_TOP, 0);
    RECT rootClient{};
    GetClientRect(view, &rootClient);
    // These sizes have room for the measured toolbar, even when it wraps.
    if (rootClient.right >= 1000 && rootClient.bottom >= 700) {
        RECT toolbarClient{};
        GetClientRect(toolbar, &toolbarClient);
        if ((GetWindowLongPtrW(toolbar, GWL_STYLE) & (WS_HSCROLL | WS_VSCROLL)) != 0) stable = false;
        for (int id : kToolbarControlIds) {
            const RECT bounds = BoundsInView(FindControl(view, id), toolbar);
            if (bounds.left < 0 || bounds.top < 0 || bounds.right > toolbarClient.right ||
                bounds.bottom > toolbarClient.bottom) stable = false;
        }
    }
    if ((GetWindowLongPtrW(toolbar, GWL_STYLE) & (WS_HSCROLL | WS_VSCROLL)) != 0) {
        const int bodyX = GetScrollPos(body, SB_HORZ);
        const int bodyY = GetScrollPos(body, SB_VERT);
        for (int id : kToolbarControlIds) {
            const HWND button = FindControl(view, id);
            SetFocus(toolbar);
            SetFocus(button);
            RECT client{};
            GetClientRect(toolbar, &client);
            const RECT bounds = BoundsInView(button, toolbar);
            RECT intersection{};
            if (GetFocus() != button || !IntersectRect(&intersection, &client, &bounds) ||
                (bounds.right - bounds.left <= client.right &&
                 (bounds.left < 0 || bounds.right > client.right)) ||
                (bounds.bottom - bounds.top <= client.bottom &&
                 (bounds.top < 0 || bounds.bottom > client.bottom))) stable = false;
        }
        // Header scrolling is independent too; every button remains inside its
        // full document even when only one part can fit at an extreme size.
        for (UINT message : {WM_VSCROLL, WM_HSCROLL}) {
            SendMessageW(toolbar, message, SB_BOTTOM, 0);
            stable = VerifyResponsiveGeometry(view) && stable;
            SendMessageW(toolbar, message, SB_TOP, 0);
        }
        stable = GetScrollPos(body, SB_HORZ) == bodyX &&
                 GetScrollPos(body, SB_VERT) == bodyY && stable;
    }
    if (!stable) std::wcerr << L"Toolbar moved, clipped, or changed body scroll during focus/navigation.\n";
    return stable;
}

bool VerifyToolbarFontMatrix(HWND view) {
    const HFONT original = reinterpret_cast<HFONT>(
        SendMessageW(FindControl(view, kBrowseViewButtonId), WM_GETFONT, 0, 0));
    bool valid = original != nullptr;
    for (int pixels : {12, 22, 36}) {
        const HFONT font = CreateFontW(-pixels, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        if (font == nullptr) { valid = false; break; }
        EnumChildWindows(view, SetProbeFont, reinterpret_cast<LPARAM>(font));
        MoveWindow(view, 0, 0, 1000, 700, TRUE);
        RECT client{};
        GetClientRect(view, &client);
        SendMessageW(view, WM_SIZE, SIZE_RESTORED, MAKELPARAM(client.right, client.bottom));
        std::vector<int> widths;
        for (int id : kToolbarControlIds) widths.push_back(WindowWidth(FindControl(view, id)));
        for (const SIZE size : {SIZE{480, 420}, SIZE{320, 240}, SIZE{260, 90},
                               SIZE{1600, 900}, SIZE{480, 420}, SIZE{1000, 700}}) {
            MoveWindow(view, 0, 0, size.cx, size.cy, TRUE);
            valid = VerifyPinnedToolbar(view, false) && valid;
            size_t index = 0;
            for (int id : kToolbarControlIds) {
                valid = WindowWidth(FindControl(view, id)) == widths[index++] && valid;
            }
        }
        // A borrowed font must be detached from every nested control before deletion.
        EnumChildWindows(view, SetProbeFont, reinterpret_cast<LPARAM>(original));
        DeleteObject(font);
    }
    MoveWindow(view, 0, 0, 1000, 700, TRUE);
    RECT client{};
    GetClientRect(view, &client);
    SendMessageW(view, WM_SIZE, SIZE_RESTORED, MAKELPARAM(client.right, client.bottom));
    if (!valid) std::wcerr << L"Pinned toolbar fixed-width font/viewport matrix failed.\n";
    return valid && VerifyPinnedToolbar(view, true);
}

LRESULT CALLBACK CategoryMutationProbeProc(HWND window, UINT message, WPARAM wParam,
                                         LPARAM lParam, UINT_PTR, DWORD_PTR data) {
    if (message == LVM_DELETEALLITEMS || message == LVM_INSERTITEMW || message == LVM_INSERTITEMA) {
        ++*reinterpret_cast<int*>(data);
    }
    return DefSubclassProc(window, message, wParam, lParam);
}

bool VerifyCategoryFocusIsPassive(HWND view, HWND locationList, HWND genreList) {
    const HWND body = FindControl(view, kBodyViewportId);
    const int selectedLocation = ListView_GetNextItem(locationList, -1, LVNI_SELECTED);
    const int selectedGenre = ListView_GetNextItem(genreList, -1, LVNI_SELECTED);
    if (body == nullptr || selectedLocation < 0 || selectedGenre < 0) return false;
    const std::wstring locationText = ListCellText(locationList, selectedLocation, 0);
    const std::wstring genreText = ListCellText(genreList, selectedGenre, 0);
    const auto toolbar = ToolbarBounds(view);
    const int stationReplacements = gSearchStartCount;
    const int stationRows = ListView_GetItemCount(gStationList);
    int categoryMutations = 0;
    if (!SetWindowSubclass(locationList, CategoryMutationProbeProc, 0x4204U,
                           reinterpret_cast<DWORD_PTR>(&categoryMutations))) return false;
    if (!SetWindowSubclass(genreList, CategoryMutationProbeProc, 0x4204U,
                           reinterpret_cast<DWORD_PTR>(&categoryMutations))) {
        RemoveWindowSubclass(locationList, CategoryMutationProbeProc, 0x4204U);
        return false;
    }
    bool valid = true;
    for (HWND list : {locationList, genreList}) {
        NMLISTVIEW changed{};
        changed.hdr.hwndFrom = list;
        changed.hdr.idFrom = static_cast<UINT_PTR>(GetDlgCtrlID(list));
        changed.hdr.code = LVN_ITEMCHANGED;
        changed.iItem = ListView_GetNextItem(list, -1, LVNI_SELECTED);
        changed.uChanged = LVIF_STATE;
        changed.uOldState = LVIS_SELECTED;
        changed.uNewState = LVIS_SELECTED | LVIS_FOCUSED;
        SendMessageW(body, WM_NOTIFY, changed.hdr.idFrom, reinterpret_cast<LPARAM>(&changed));
        changed.uOldState = LVIS_SELECTED | LVIS_FOCUSED;
        changed.uNewState = LVIS_SELECTED;
        SendMessageW(body, WM_NOTIFY, changed.hdr.idFrom, reinterpret_cast<LPARAM>(&changed));
        SetFocus(list);
        SetFocus(FindControl(view, kBrowseViewButtonId));
        valid = ToolbarStayedPut(view, toolbar) && valid;
    }
    for (int id : {kFilterToggleButtonId, kTunerToggleButtonId}) {
        const HWND button = FindControl(view, id);
        for (int pass = 0; pass != 2; ++pass) {
            SendMessageW(button, BM_CLICK, 0, 0);
            valid = ToolbarStayedPut(view, toolbar) && VerifyResponsiveGeometry(view) && valid;
        }
    }
    // This bounded check is invoked only after an offline fixture search has
    // completed, so a replacement here is attributable to focus/toggle traffic.
    PumpFor(150);
    valid = categoryMutations == 0 && gSearchStartCount == stationReplacements &&
        ListView_GetItemCount(gStationList) == stationRows &&
        ListView_GetNextItem(locationList, -1, LVNI_SELECTED) == selectedLocation &&
        ListView_GetNextItem(genreList, -1, LVNI_SELECTED) == selectedGenre &&
        ListCellText(locationList, selectedLocation, 0) == locationText &&
        ListCellText(genreList, selectedGenre, 0) == genreText &&
        ToolbarStayedPut(view, toolbar) && valid;
    RemoveWindowSubclass(locationList, CategoryMutationProbeProc, 0x4204U);
    RemoveWindowSubclass(genreList, CategoryMutationProbeProc, 0x4204U);
    if (!valid) std::wcerr << L"Category focus/toggles changed selection or rebuilt visible data.\n";
    return valid;
}

bool VerifySelectedRowPainting(HWND view, HWND list) {
    const auto noDeferredSearch = [&](const wchar_t* stage) {
        MSG pending{};
        if (!PeekMessageW(&pending, view, kDeferredSearchMessage,
                kDeferredSearchMessage, PM_NOREMOVE)) return true;
        std::wcerr << L"Selected-row painting found a queued search " << stage
                   << L" for list " << GetDlgCtrlID(list) << L".\n";
        return false; // Observe only; never consume work to make this test pass.
    };
    if (!noDeferredSearch(L"before setup")) return false;
    const int count = ListView_GetItemCount(list);
    if (count <= 0 || GetParent(list) != FindControl(view, kBodyViewportId)) return false;
    const int selected = ListView_GetNextItem(list, -1, LVNI_SELECTED);
    if (selected < 0 && GetDlgCtrlID(list) != kStationListId) {
        std::wcerr << L"Selected-row painting requires an existing category selection: "
                   << GetDlgCtrlID(list) << L".\n";
        return false;
    }
    const int row = selected < 0 ? 0 : selected;
    // Existing category selections are live filters. A paint test must not
    // rewrite their selected bit or queue a filter request during cleanup.
    const UINT changedStateMask = selected < 0 ? LVIS_SELECTED | LVIS_FOCUSED : LVIS_FOCUSED;
    const UINT originalState = ListView_GetItemState(list, row, LVIS_SELECTED | LVIS_FOCUSED);
    const int originalFocused = ListView_GetNextItem(list, -1, LVNI_FOCUSED);
    const HWND originalFocus = GetFocus();
    RECT rowBounds{}, client{};
    GetClientRect(list, &client);
    if (!ListView_GetItemRect(list, row, &rowBounds, LVIR_BOUNDS) ||
        client.right < 8 || rowBounds.bottom - rowBounds.top < 5) return false;
    const int width = client.right;
    const int height = rowBounds.bottom - rowBounds.top;
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    const HDC dc = CreateCompatibleDC(nullptr);
    const HBITMAP bitmap = dc == nullptr ? nullptr : CreateDIBSection(
        dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bitmap == nullptr) {
        if (dc != nullptr) DeleteDC(dc);
        return false;
    }
    const HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
    const HFONT font = reinterpret_cast<HFONT>(SendMessageW(list, WM_GETFONT, 0, 0));
    const HGDIOBJ oldFont = font == nullptr ? nullptr : SelectObject(dc, font);
    SetViewportOrgEx(dc, 0, -rowBounds.top, nullptr);
    const int replacements = gSearchStartCount;
    ListView_SetItemState(list, row, LVIS_SELECTED | LVIS_FOCUSED, changedStateMask);
    const int paletteCalls = gPaletteCalls;
    const COLORREF highlight = GetSysColor(COLOR_HIGHLIGHT);
    const COLORREF textColor = GetSysColor(COLOR_HIGHLIGHTTEXT);
    const DWORD textPixel = (static_cast<DWORD>(GetRValue(textColor)) << 16) |
        (static_cast<DWORD>(GetGValue(textColor)) << 8) | GetBValue(textColor);
    bool valid = true;
    for (bool focused : {true, false}) {
        SetFocus(focused ? list : FindControl(view, kBrowseViewButtonId));
        RECT clear{0, rowBounds.top, width, rowBounds.bottom};
        FillRect(dc, &clear, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        NMLVCUSTOMDRAW draw{};
        draw.nmcd.hdr.hwndFrom = list;
        draw.nmcd.hdr.idFrom = static_cast<UINT_PTR>(GetDlgCtrlID(list));
        draw.nmcd.hdr.code = NM_CUSTOMDRAW;
        draw.nmcd.hdc = dc;
        draw.nmcd.dwDrawStage = CDDS_PREPAINT;
        const LRESULT prepaint = SendMessageW(GetParent(list), WM_NOTIFY,
            draw.nmcd.hdr.idFrom, reinterpret_cast<LPARAM>(&draw));
        draw.nmcd.dwDrawStage = CDDS_ITEMPREPAINT;
        draw.nmcd.dwItemSpec = static_cast<DWORD_PTR>(row);
        draw.nmcd.uItemState = CDIS_SELECTED | (focused ? CDIS_FOCUS : 0);
        draw.nmcd.rc = rowBounds;
        const LRESULT itemPaint = SendMessageW(GetParent(list), WM_NOTIFY,
            draw.nmcd.hdr.idFrom, reinterpret_cast<LPARAM>(&draw));
        GdiFlush();
        bool foregroundDrawn = false;
        const auto* pixels = static_cast<const DWORD*>(bits);
        for (int y = 2; y < height - 2 && !foregroundDrawn; ++y) {
            for (int x = 4; x < width - 4; ++x) {
                if ((pixels[y * width + x] & 0x00ffffffU) == textPixel) {
                    foregroundDrawn = true;
                    break;
                }
            }
        }
        valid = prepaint == CDRF_NOTIFYITEMDRAW && itemPaint == CDRF_SKIPDEFAULT &&
            GetFocus() == (focused ? list : FindControl(view, kBrowseViewButtonId)) &&
            GetPixel(dc, width - 2, rowBounds.top + height / 2) == highlight && foregroundDrawn &&
            ListView_GetItemState(list, row, LVIS_SELECTED | LVIS_FOCUSED) ==
                (LVIS_SELECTED | LVIS_FOCUSED) && ListView_GetItemCount(list) == count &&
            gSearchStartCount == replacements && gPaletteCalls == paletteCalls && valid;
    }
    ListView_SetItemState(list, row, originalState, changedStateMask);
    if (originalFocused >= 0 && originalFocused != row)
        ListView_SetItemState(list, originalFocused, LVIS_FOCUSED, LVIS_FOCUSED);
    if (originalFocus != nullptr) SetFocus(originalFocus);
    if (oldFont != nullptr) SelectObject(dc, oldFont);
    SelectObject(dc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
    valid = noDeferredSearch(L"after cleanup") && gSearchStartCount == replacements && valid;
    if (!valid) std::wcerr << L"Selected-row focused/unfocused bitmap colors or retained state failed: "
                          << GetDlgCtrlID(list) << L".\n";
    return valid;
}

bool VerifyDetailsScaling(
    HWND view,
    HWND stationList,
    HWND selectionDetails,
    HWND status,
    HWND tunerLabel,
    HWND toggleButton) {
    const HWND body = FindControl(view, kBodyViewportId);
    if (body == nullptr) return false;
    if (selectionDetails == nullptr || status == nullptr ||
        (GetWindowLongPtrW(selectionDetails, GWL_STYLE) &
         (WS_VSCROLL | ES_MULTILINE | ES_READONLY)) !=
            (WS_VSCROLL | ES_MULTILINE | ES_READONLY)) {
        return false;
    }
    RECT details = BoundsInView(selectionDetails, view);
    RECT statusBounds = BoundsInView(status, view);
    RECT client{};
    GetClientRect(view, &client);
    if (!HasVisibleStyle(selectionDetails) || details.bottom > statusBounds.top ||
        !VerifyResponsiveGeometry(view)) {
        return false;
    }

    const HFONT largeFont = CreateFontW(
        -22,
        0,
        0,
        0,
        FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI");
    if (largeFont == nullptr) {
        return false;
    }
    EnumChildWindows(view, SetProbeFont, reinterpret_cast<LPARAM>(largeFont));
    MoveWindow(view, 0, 0, 1000, 800, TRUE);
    RECT listBounds = BoundsInView(stationList, view);
    GetClientRect(view, &client);
    const bool largeFontFallback = HasVisibleStyle(tunerLabel) &&
                                   HasVisibleStyle(toggleButton) &&
                                   BoundsInView(tunerLabel, view).top >= listBounds.bottom &&
                                   VerifyResponsiveGeometry(view);
    bool smallWindowsValid = true;
    for (const SIZE size : {SIZE{480, 420}, SIZE{320, 240}, SIZE{260, 90},
                           SIZE{1600, 900}, SIZE{480, 420}, SIZE{1000, 700}}) {
        MoveWindow(view, 0, 0, size.cx, size.cy, TRUE);
        smallWindowsValid = VerifyPinnedToolbar(view, false) && smallWindowsValid;
    }
    SendMessageW(body, WM_VSCROLL, SB_BOTTOM, 0);
    GetClientRect(body, &client);
    const bool footerReachable =
        BoundsInView(status, body).bottom <= client.bottom;
    SendMessageW(body, WM_VSCROLL, SB_TOP, 0);

    const HFONT normalFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    EnumChildWindows(view, SetProbeFont, reinterpret_cast<LPARAM>(normalFont));
    DeleteObject(largeFont);
    MoveWindow(view, 0, 0, 1000, 700, TRUE);
    return largeFontFallback && smallWindowsValid && footerReachable && VerifyToolbarFontMatrix(view) &&
           HasVisibleStyle(tunerLabel) &&
           HasVisibleStyle(toggleButton);
}

LRESULT CALLBACK FramePaintProbeProc(HWND window, UINT message, WPARAM wParam,
                                    LPARAM lParam, UINT_PTR, DWORD_PTR data) {
    if (message == WM_NCPAINT) ++*reinterpret_cast<int*>(data);
    return DefSubclassProc(window, message, wParam, lParam);
}

bool VerifyResizePainting(HWND view, HWND locationList, HWND genreList) {
    const HWND body = FindControl(view, kBodyViewportId);
    if (body == nullptr) return false;
    // The normal fixture host is hidden: empty update regions there would not
    // prove any painting. Briefly show only these test-owned windows, without
    // activation, and observe real paint dispatch before each drag returns.
    const HWND parent = GetParent(view);
    const HWND host = GetParent(parent);
    const bool parentShown = HasVisibleStyle(parent);
    const bool hostShown = HasVisibleStyle(host);
    int framePaints = 0;
    if (!SetWindowSubclass(locationList, FramePaintProbeProc, 0x4203U,
                           reinterpret_cast<DWORD_PTR>(&framePaints))) return false;
    ShowWindow(parent, SW_SHOWNOACTIVATE);
    ShowWindow(host, SW_SHOWNOACTIVATE);
    RedrawWindow(host, nullptr, nullptr,
        RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
    const int initialFrames = framePaints;
    const int initialPaints = gViewPaintCount;
    gViewEraseCount = 0;
    bool serviced = IsWindowVisible(view) != FALSE;
    const auto checkDamage = [&]() {
        if (GetUpdateRect(view, nullptr, FALSE) ||
            GetUpdateRect(body, nullptr, FALSE) ||
            GetUpdateRect(locationList, nullptr, FALSE)) {
            serviced = false;
        }
    };
    const RECT location = BoundsInView(locationList, body);
    const RECT genre = BoundsInView(genreList, body);
    const POINT start{(location.right + genre.left) / 2,
                      (location.top + location.bottom) / 2};
    SendMessageW(body, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(start.x, start.y));
    for (int step = 1; step <= 8; ++step) {
        SendMessageW(body, WM_MOUSEMOVE, MK_LBUTTON,
                     MAKELPARAM(start.x - step * 4, start.y));
        // No PumpFor/UpdateWindow here: production must service its own damage.
        checkDamage();
    }
    SendMessageW(body, WM_LBUTTONUP, 0, MAKELPARAM(start.x - 32, start.y));
    const bool dragPainted = framePaints > initialFrames &&
                             gViewPaintCount > initialPaints && gViewEraseCount == 0;
    for (int step = 1; step <= 8; ++step) {
        MoveWindow(view, 0, 0, 1000 - step * 12, 700 - step * 8, FALSE);
        checkDamage();
    }
    SendMessageW(body, WM_VSCROLL, SB_BOTTOM, 0);
    checkDamage();
    MoveWindow(view, 0, 0, 1000, 700, FALSE);
    checkDamage();
    SendMessageW(body, WM_VSCROLL, SB_TOP, 0);
    const bool geometry = VerifyResponsiveGeometry(view);
    RemoveWindowSubclass(locationList, FramePaintProbeProc, 0x4203U);
    if (!parentShown) ShowWindow(parent, SW_HIDE);
    if (!hostShown) ShowWindow(host, SW_HIDE);
    if (!serviced || !dragPainted || !geometry) {
        std::wcerr << L"Resize paint: serviced=" << serviced << L", dragPainted="
                   << dragPainted << L", frames=" << framePaints - initialFrames
                   << L", paints=" << gViewPaintCount - initialPaints
                   << L", geometry=" << geometry << L".\n";
        return false;
    }
    std::wcout << L"Resize paint: child frames and parent damage serviced during drag; "
                  L"repeated/scrolled resize passed without forced parent erase.\n";
    return true;
}

bool VerifyPaneDragging(HWND view, HWND locationList, HWND genreList, HWND stationList,
                        HWND details, HWND filterLabel) {
    const HWND body = FindControl(view, kBodyViewportId);
    if (body == nullptr) return false;
    const auto drag = [&](POINT from, POINT to) {
        SendMessageW(body, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(from.x, from.y));
        SendMessageW(body, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(to.x, to.y));
        SendMessageW(body, WM_LBUTTONUP, 0, MAKELPARAM(to.x, to.y));
    };
    const RECT location = BoundsInView(locationList, body);
    const RECT genre = BoundsInView(genreList, body);
    const POINT vertical{(location.right + genre.left) / 2, (location.top + location.bottom) / 2};
    drag(vertical, POINT{vertical.x - 60, vertical.y});
    if (WindowWidth(locationList) >= location.right - location.left ||
        !VerifyResponsiveGeometry(view)) {
        std::wcerr << L"Pane drag: Location/Genre divider failed.\n";
        return false;
    }
    const RECT resizedLocation = BoundsInView(locationList, body);
    const RECT filters = BoundsInView(filterLabel, body);
    const POINT bottom{resizedLocation.left + 10, (resizedLocation.bottom + filters.top) / 2};
    drag(bottom, POINT{bottom.x, bottom.y + 35});
    if (BoundsInView(locationList, body).bottom <= resizedLocation.bottom ||
        !VerifyResponsiveGeometry(view)) {
        std::wcerr << L"Pane drag: category height divider failed.\n";
        return false;
    }
    const RECT list = BoundsInView(stationList, body);
    const RECT detail = BoundsInView(details, body);
    const POINT edge{(list.right + detail.left) / 2, list.top + 30};
    drag(edge, POINT{edge.x - 35, edge.y});
    if (WindowWidth(stationList) >= list.right - list.left ||
        !VerifyResponsiveGeometry(view)) {
        std::wcerr << L"Pane drag: details divider failed.\n";
        return false;
    }
    const int customWidth = ListView_GetColumnWidth(locationList, 0) - 20;
    ListView_SetColumnWidth(locationList, 0, customWidth);
    RECT client{};
    GetClientRect(view, &client);
    SendMessageW(view, WM_SIZE, SIZE_RESTORED, MAKELPARAM(client.right, client.bottom));
    if (ListView_GetColumnWidth(locationList, 0) != customWidth ||
        !VerifyResponsiveGeometry(view)) {
        std::wcerr << L"Pane drag: category column width was not preserved.\n";
        return false;
    }
    return true;
}

bool VerifyBrowseSwitchAndGeometry(
    HWND view,
    HWND stationsButton,
    HWND browseButton,
    HWND favoritesButton,
    HWND locationList,
    HWND genreList,
    HWND providerTotal,
    HWND filterLabel,
    HWND filterToggleButton,
    HWND languageCombo,
    HWND codecCombo,
    HWND bitrateCombo,
    HWND compatibilityCombo,
    HWND resetButton,
    HWND stationList,
    HWND pageSizeCombo,
    HWND previousButton,
    HWND nextButton,
    HWND tunerLabel,
    HWND tunerToggleButton) {
    if (stationsButton == nullptr || browseButton == nullptr || favoritesButton == nullptr ||
        locationList == nullptr ||
        genreList == nullptr || providerTotal == nullptr || pageSizeCombo == nullptr ||
        filterLabel == nullptr || filterToggleButton == nullptr || languageCombo == nullptr ||
        codecCombo == nullptr || bitrateCombo == nullptr || compatibilityCombo == nullptr ||
        resetButton == nullptr || stationList == nullptr || previousButton == nullptr ||
        nextButton == nullptr ||
        ComboBox_GetCount(pageSizeCombo) != 3 ||
        ComboBox_GetCount(compatibilityCombo) != 5 ||
        (GetWindowLongPtrW(browseButton, GWL_STYLE) & WS_TABSTOP) == 0 ||
        (GetWindowLongPtrW(favoritesButton, GWL_STYLE) & WS_TABSTOP) == 0 ||
        IsWindowEnabled(browseButton) == FALSE || IsWindowEnabled(stationsButton) == FALSE ||
        IsWindowEnabled(favoritesButton) == FALSE) {
        return false;
    }
    if (!HasVisibleStyle(locationList) || !HasVisibleStyle(genreList) ||
        !HasVisibleStyle(tunerLabel) || !HasVisibleStyle(tunerToggleButton) ||
        ListCellText(locationList, 0, 0) != L"All Locations" ||
        ListCellText(genreList, 0, 0) != L"All Genres" ||
        ListCellText(locationList, 0, 1).empty() ||
        ListCellText(genreList, 0, 1).empty() ||
        ControlText(providerTotal).rfind(L"Radio Browser total: ", 0) != 0) {
        return false;
    }
    const HWND locationHeader = ListView_GetHeader(locationList);
    const HWND genreHeader = ListView_GetHeader(genreList);
    RECT locationClient{};
    RECT genreClient{};
    GetClientRect(locationList, &locationClient);
    GetClientRect(genreList, &genreClient);
    if (locationHeader == nullptr || genreHeader == nullptr ||
        Header_GetItemCount(locationHeader) != 2 || Header_GetItemCount(genreHeader) != 2 ||
        ListView_GetColumnWidth(locationList, 0) + ListView_GetColumnWidth(locationList, 1) >
            locationClient.right ||
        ListView_GetColumnWidth(genreList, 0) + ListView_GetColumnWidth(genreList, 1) >
            genreClient.right) {
        return false;
    }
    const RECT labelBounds = BoundsInView(filterLabel, view);
    const RECT languageBounds = BoundsInView(languageCombo, view);
    if (!HasVisibleStyle(filterLabel) || !HasVisibleStyle(languageCombo) ||
        labelBounds.bottom > languageBounds.top ||
        ControlText(filterToggleButton) != L"&Hide filters") {
        return false;
    }
    const auto hasStableResultRow = [&]() {
        return VerifyResponsiveGeometry(view) && HasVisibleStyle(pageSizeCombo) &&
               HasVisibleStyle(previousButton) && HasVisibleStyle(nextButton) &&
               HasVisibleStyle(tunerToggleButton);
    };
    const RECT stableFilter = BoundsInView(filterToggleButton, view);
    const RECT stableDetails = BoundsInView(tunerToggleButton, view);
    const auto togglesStayedPut = [&]() {
        const RECT filters = BoundsInView(filterToggleButton, view);
        const RECT details = BoundsInView(tunerToggleButton, view);
        return EqualRect(&stableFilter, &filters) && EqualRect(&stableDetails, &details);
    };
    if ((!hasStableResultRow() || !togglesStayedPut())) {
        return false;
    }
    const int expandedListTop = BoundsInView(stationList, view).top;
    const RECT expandedList = BoundsInView(stationList, view);
    const RECT expandedDetails = BoundsInView(tunerLabel, view);
    if (expandedList.right > expandedDetails.left) {
        return false;
    }
    gViewEraseCount = 0;
    SetFocus(tunerToggleButton);
    SendMessageW(tunerToggleButton, WM_KEYDOWN, VK_SPACE, 1);
    SendMessageW(tunerToggleButton, WM_KEYUP, VK_SPACE, 1 | (static_cast<LPARAM>(1) << 30) |
                                                    (static_cast<LPARAM>(1) << 31));
    PumpFor(100);
    if (ControlText(tunerToggleButton) != L"&Show details" || HasVisibleStyle(tunerLabel) ||
        BoundsInView(stationList, view).right <= expandedList.right || GetFocus() != tunerToggleButton ||
        (!hasStableResultRow() || !togglesStayedPut())) {
        return false;
    }
    SendMessageW(tunerToggleButton, WM_KEYDOWN, VK_SPACE, 1);
    SendMessageW(tunerToggleButton, WM_KEYUP, VK_SPACE, 1 | (static_cast<LPARAM>(1) << 30) |
                                                    (static_cast<LPARAM>(1) << 31));
    PumpFor(100);
    if (ControlText(tunerToggleButton) != L"&Hide details" || !HasVisibleStyle(tunerLabel) ||
        BoundsInView(stationList, view).right != expandedList.right || gViewEraseCount != 0 ||
        (!hasStableResultRow() || !togglesStayedPut())) {
        return false;
    }
    SetFocus(filterToggleButton);
    SendMessageW(filterToggleButton, WM_KEYDOWN, VK_SPACE, 1);
    SendMessageW(
        filterToggleButton,
        WM_KEYUP,
        VK_SPACE,
        1 | (static_cast<LPARAM>(1) << 30) | (static_cast<LPARAM>(1) << 31));
    PumpFor(100);
    if (ControlText(filterToggleButton) != L"&Show filters" ||
        HasVisibleStyle(filterLabel) || HasVisibleStyle(languageCombo) ||
        HasVisibleStyle(codecCombo) || HasVisibleStyle(bitrateCombo) ||
        HasVisibleStyle(compatibilityCombo) || HasVisibleStyle(resetButton) ||
        GetFocus() != filterToggleButton || BoundsInView(stationList, view).top > expandedListTop ||
        (!hasStableResultRow() || !togglesStayedPut())) {
        return false;
    }
    SendMessageW(filterToggleButton, WM_KEYDOWN, VK_SPACE, 1);
    SendMessageW(
        filterToggleButton,
        WM_KEYUP,
        VK_SPACE,
        1 | (static_cast<LPARAM>(1) << 30) | (static_cast<LPARAM>(1) << 31));
    PumpFor(100);
    if (ControlText(filterToggleButton) != L"&Hide filters" ||
        !HasVisibleStyle(filterLabel) || !HasVisibleStyle(compatibilityCombo) ||
        !HasVisibleStyle(resetButton) || (!hasStableResultRow() || !togglesStayedPut())) {
        return false;
    }
    const RECT wideLocation = BoundsInView(locationList, view);
    const RECT wideGenre = BoundsInView(genreList, view);
    if (wideLocation.right > wideGenre.left) {
        return false;
    }

    MoveWindow(view, 0, 0, 600, 800, TRUE);
    const RECT narrowLocation = BoundsInView(locationList, view);
    const RECT narrowGenre = BoundsInView(genreList, view);
    const bool stacked = narrowLocation.bottom <= narrowGenre.top &&
                         HasVisibleStyle(locationList) && HasVisibleStyle(genreList);
    MoveWindow(view, 0, 0, 1000, 700, TRUE);

    const RECT beforeMinimize = BoundsInView(stationList, view);
    SendMessageW(view, WM_SIZE, SIZE_MINIMIZED, MAKELPARAM(0, 0));
    const RECT duringMinimize = BoundsInView(stationList, view);
    const bool minimizePreservedLayout =
        EqualRect(&beforeMinimize, &duringMinimize) != FALSE;

    MoveWindow(view, 0, 0, 720, 560, TRUE);
    RECT narrowClient{};
    GetClientRect(view, &narrowClient);
    const RECT narrowStationList = BoundsInView(stationList, view);
    const RECT narrowPageSize = BoundsInView(pageSizeCombo, view);
    const RECT narrowPrevious = BoundsInView(previousButton, view);
    const RECT narrowNext = BoundsInView(nextButton, view);
    const bool narrowResultFrameValid =
        narrowStationList.left >= narrowClient.left &&
        narrowStationList.right <= narrowClient.right &&
        narrowPageSize.right <= narrowPrevious.left &&
        narrowPrevious.right <= narrowNext.left &&
        narrowNext.right <= narrowClient.right &&
        HasVisibleStyle(tunerToggleButton) && HasVisibleStyle(tunerLabel) &&
        BoundsInView(tunerLabel, view).top >= narrowStationList.bottom &&
        VerifyResponsiveGeometry(view);

    MoveWindow(view, 0, 0, 1000, 700, TRUE);
    const bool restoredFrameValid =
        HasVisibleStyle(locationList) && HasVisibleStyle(genreList) &&
        HasVisibleStyle(tunerLabel) && HasVisibleStyle(tunerToggleButton) &&
        hasStableResultRow();

    SetFocus(browseButton);
    SendMessageW(browseButton, WM_KEYDOWN, VK_SPACE, 1);
    SendMessageW(
        browseButton,
        WM_KEYUP,
        VK_SPACE,
        1 | (static_cast<LPARAM>(1) << 30) | (static_cast<LPARAM>(1) << 31));
    PumpFor(100);
    if (ControlText(browseButton) != L"&View: Stations" || !stacked || HasVisibleStyle(locationList) || HasVisibleStyle(genreList) ||
        !HasVisibleStyle(tunerLabel)) {
        return false;
    }
    SetFocus(browseButton);
    SendMessageW(browseButton, WM_KEYDOWN, VK_SPACE, 1);
    SendMessageW(
        browseButton,
        WM_KEYUP,
        VK_SPACE,
        1 | (static_cast<LPARAM>(1) << 30) | (static_cast<LPARAM>(1) << 31));
    PumpFor(100);
    return minimizePreservedLayout && narrowResultFrameValid && restoredFrameValid &&
           ControlText(browseButton) == L"&View: Browse" &&
           HasVisibleStyle(locationList) && HasVisibleStyle(genreList) &&
           HasVisibleStyle(tunerLabel) && IsWindowEnabled(browseButton) != FALSE;
}

LRESULT CALLBACK StationListSubclassProc(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    UINT_PTR subclassId,
    DWORD_PTR) {
    if (message == LVM_DELETEALLITEMS && ListView_GetItemCount(window) > 0) {
        ++gSearchStartCount;
        const HWND searchButton = GetDlgItem(GetParent(window), kSearchButtonId);
        if (searchButton != nullptr && IsWindowEnabled(searchButton) == FALSE) {
            ++gBusyListClearCount;
        }
    }
    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(window, StationListSubclassProc, subclassId);
    }
    return DefSubclassProc(window, message, wParam, lParam);
}

LRESULT CALLBACK ViewRedrawSubclassProc(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    UINT_PTR subclassId,
    DWORD_PTR) {
    if (window == gGenreTraceRoot && message == kDeferredSearchMessage)
        ++gGenreTraceDeferredSearches;
    if (window == gGenreTraceRoot && message == WM_NOTIFY && lParam != 0) {
        const auto* header = reinterpret_cast<NMHDR*>(lParam);
        if (header->idFrom == kGenreListId && header->code == LVN_ITEMCHANGED) {
            ++gGenreTraceNotificationCount;
            if (gGenreSelectionTrace.size() < 32U) {
                const auto* changed = reinterpret_cast<NMLISTVIEW*>(lParam);
                const size_t index = gGenreSelectionTrace.size();
                gGenreSelectionTrace.push_back({
                    changed->iItem,
                    ListView_GetNextItem(header->hwndFrom, -1, LVNI_SELECTED),
                    changed->uChanged, changed->uOldState, changed->uNewState,
                    changed->lParam,
                    gGenreTraceFixture ? SendMessageW(window, kProbeWorkState, 0, 0) : -1,
                    -1});
                const LRESULT result = DefSubclassProc(window, message, wParam, lParam);
                gGenreSelectionTrace[index].workAfter = gGenreTraceFixture
                    ? SendMessageW(window, kProbeWorkState, 0, 0) : -1;
                return result;
            }
        }
    }
    if (message == WM_ERASEBKGND) {
        ++gViewEraseCount;
    }
    if (message == WM_PAINT) {
        ++gViewPaintCount;
    }
    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(window, ViewRedrawSubclassProc, subclassId);
    }
    return DefSubclassProc(window, message, wParam, lParam);
}

bool VerifyResetButton(HWND resetButton) {
    wchar_t label[32]{};
    GetWindowTextW(resetButton, label, static_cast<int>(_countof(label)));
    if (std::wstring(label) != L"RESET") {
        return false;
    }

    const HDC dc = GetDC(resetButton);
    if (dc == nullptr) {
        return false;
    }
    const HFONT font = reinterpret_cast<HFONT>(SendMessageW(resetButton, WM_GETFONT, 0, 0));
    const HGDIOBJ previous = font != nullptr ? SelectObject(dc, font) : nullptr;
    SIZE textSize{};
    const BOOL measured = GetTextExtentPoint32W(dc, label, 5, &textSize);
    if (previous != nullptr) {
        SelectObject(dc, previous);
    }
    ReleaseDC(resetButton, dc);
    return measured != FALSE && WindowWidth(resetButton) >= textSize.cx + 32;
}

bool VerifyTunerToggle(
    HWND view,
    HWND stationList,
    HWND resetButton,
    HWND resultCount,
    HWND tunerLabel,
    HWND toggleButton) {
    auto report = [&](const wchar_t* stage, const wchar_t* label) {
        const RECT listBounds = BoundsInView(stationList, view);
        const RECT tunerBounds = BoundsInView(tunerLabel, view);
        const RECT toggleBounds = BoundsInView(toggleButton, view);
        std::wcerr << L"Tuner toggle " << stage << L": label=\"" << label
                   << L"\", station-list=" << (stationList != nullptr)
                   << L", tab-stop="
                   << (toggleButton != nullptr &&
                               (GetWindowLongPtrW(toggleButton, GWL_STYLE) & WS_TABSTOP) != 0)
                   << L", toggle-visible=" << HasVisibleStyle(toggleButton)
                   << L", tuner-visible=" << HasVisibleStyle(tunerLabel)
                   << L", result-visible=" << HasVisibleStyle(resultCount)
                   << L", list=[" << listBounds.left << L"," << listBounds.top << L","
                   << listBounds.right << L"," << listBounds.bottom << L"]"
                   << L", tuner=[" << tunerBounds.left << L"," << tunerBounds.top << L","
                   << tunerBounds.right << L"," << tunerBounds.bottom << L"]"
                   << L", toggle=[" << toggleBounds.left << L"," << toggleBounds.top << L","
                   << toggleBounds.right << L"," << toggleBounds.bottom << L"].\n";
    };

    wchar_t label[64]{};
    if (toggleButton != nullptr) {
        GetWindowTextW(toggleButton, label, static_cast<int>(_countof(label)));
    }
    if (stationList == nullptr || toggleButton == nullptr || tunerLabel == nullptr ||
        (GetWindowLongPtrW(toggleButton, GWL_STYLE) & WS_TABSTOP) == 0 ||
        !HasVisibleStyle(toggleButton) || !HasVisibleStyle(tunerLabel)) {
        report(L"initial precondition failed", label);
        return false;
    }

    if (std::wstring(label) != L"&Hide details") {
        report(L"expanded label failed", label);
        return false;
    }
    const RECT expandedListBounds = BoundsInView(stationList, view);
    const RECT expandedTunerBounds = BoundsInView(tunerLabel, view);
    const RECT expandedToggleBounds = BoundsInView(toggleButton, view);
    if (expandedListBounds.right > expandedTunerBounds.left ||
        !VerifyResponsiveGeometry(view) ||
        expandedToggleBounds.bottom > expandedListBounds.top ||
        expandedToggleBounds.left >= expandedToggleBounds.right ||
        expandedToggleBounds.top >= expandedToggleBounds.bottom ||
        !HasVisibleStyle(resultCount)) {
        report(L"expanded header geometry failed", label);
        return false;
    }
    const int expandedListWidth = WindowWidth(stationList);
    SetFocus(toggleButton);
    SendMessageW(toggleButton, WM_KEYDOWN, VK_SPACE, 1);
    SendMessageW(
        toggleButton,
        WM_KEYUP,
        VK_SPACE,
        1 | (static_cast<LPARAM>(1) << 30) | (static_cast<LPARAM>(1) << 31));
    GetWindowTextW(toggleButton, label, static_cast<int>(_countof(label)));
    const RECT collapsedListBounds = BoundsInView(stationList, view);
    const RECT collapsedToggleBounds = BoundsInView(toggleButton, view);
    if (std::wstring(label) != L"&Show details" || HasVisibleStyle(tunerLabel) ||
        WindowWidth(stationList) <= expandedListWidth ||
        collapsedToggleBounds.bottom > collapsedListBounds.top ||
        collapsedToggleBounds.top >= collapsedToggleBounds.bottom ||
        collapsedToggleBounds.left < collapsedListBounds.left ||
        !EqualRect(&collapsedToggleBounds, &expandedToggleBounds) ||
        GetFocus() != toggleButton) {
        report(L"collapsed keyboard geometry failed", label);
        return false;
    }

    SendMessageW(toggleButton, WM_KEYDOWN, VK_SPACE, 1);
    SendMessageW(
        toggleButton,
        WM_KEYUP,
        VK_SPACE,
        1 | (static_cast<LPARAM>(1) << 30) | (static_cast<LPARAM>(1) << 31));
    GetWindowTextW(toggleButton, label, static_cast<int>(_countof(label)));
    if (std::wstring(label) != L"&Hide details" || !HasVisibleStyle(tunerLabel) ||
        WindowWidth(stationList) != expandedListWidth) {
        report(L"restored keyboard geometry failed", label);
        return false;
    }

    MoveWindow(view, 0, 0, 700, 600, TRUE);
    RECT listBounds{};
    RECT clientBounds{};
    GetWindowRect(stationList, &listBounds);
    GetClientRect(view, &clientBounds);
    MapWindowPoints(nullptr, view, reinterpret_cast<POINT*>(&listBounds), 2);
    const bool narrowLayoutValid = HasVisibleStyle(toggleButton) &&
                                   HasVisibleStyle(tunerLabel) &&
                                   BoundsInView(tunerLabel, view).top >= listBounds.bottom &&
                                   listBounds.left >= clientBounds.left &&
                                   listBounds.right <= clientBounds.right &&
                                   VerifyResponsiveGeometry(view);
    MoveWindow(view, 0, 0, 1000, 700, TRUE);
    SendMessageW(view, WM_DISPLAYCHANGE, 32, MAKELPARAM(1920, 1080));
    GetWindowTextW(toggleButton, label, static_cast<int>(_countof(label)));
    const RECT restoredListBounds = BoundsInView(stationList, view);
    const RECT restoredToggleBounds = BoundsInView(toggleButton, view);
    const bool restoredLayoutValid =
        narrowLayoutValid && HasVisibleStyle(toggleButton) && HasVisibleStyle(tunerLabel) &&
        std::wstring(label) == L"&Hide details" &&
        restoredToggleBounds.bottom <= restoredListBounds.top &&
        WindowWidth(stationList) == expandedListWidth && VerifyResetButton(resetButton);
    if (!restoredLayoutValid) {
        report(L"narrow or restored geometry failed", label);
    }
    return restoredLayoutValid;
}

}  // namespace

int wmain(int argumentCount, wchar_t** arguments) {
    const wchar_t* dllPath = argumentCount > 1 ? arguments[1] : L"ml_livestations.dll";
    const bool skinOnly =
        argumentCount > 2 && _wcsicmp(arguments[2], L"--skin-only") == 0;
    const bool fullFixture =
        argumentCount > 2 && _wcsicmp(arguments[2], L"--full-fixture") == 0;
    if (argumentCount > 2 && !skinOnly && !fullFixture) {
        std::wcerr << L"Expected --skin-only or --full-fixture.\n";
        return 47;
    }
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
        0, kHostClassName, L"", WS_OVERLAPPED, 0, 0, 1000, 700, nullptr, nullptr,
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
        1000,
        700,
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
        1000,
        700,
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
    if (fullFixture &&
        std::string(plugin->description) !=
            "Winamp Tools Live Stations OFFLINE FIXTURE (NOT FOR INSTALL)") {
        std::wcerr << L"--full-fixture requires ml_livestations_fixture.dll, not an install candidate.\n";
        plugin->quit();
        FreeLibrary(module);
        return 48;
    }

    const HWND view = reinterpret_cast<HWND>(
        plugin->MessageProc(ML_MSG_TREE_ONCREATEVIEW, kTreeId, reinterpret_cast<INT_PTR>(viewParent), 0));
    if (view == nullptr) {
        std::wcerr << L"Plug-in did not create its Media Library view.\n";
        plugin->quit();
        FreeLibrary(module);
        return 6;
    }
    if (SetWindowSubclass(view, ViewRedrawSubclassProc, 0x4202U, 0) == FALSE) {
        std::wcerr << L"Could not observe Live Stations view redraws.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 52;
    }
    const HWND body = FindControl(view, kBodyViewportId);
    const HWND toolbar = FindControl(view, kToolbarViewportId);
    // Inspect the actual first-show geometry before any explicit resize, font
    // refresh, or synthetic WM_SIZE can accidentally repair an empty layout.
    if (!VerifyResponsiveGeometry(view) || !VerifyPinnedToolbar(view, false) ||
        SetWindowSubclass(body, ViewRedrawSubclassProc, 0x4202U, 0) == FALSE ||
        SetWindowSubclass(toolbar, ViewRedrawSubclassProc, 0x4202U, 0) == FALSE) {
        std::wcerr << L"Initial pinned toolbar/body layout was not valid before resize.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 72;
    }
    gStationList = FindControl(view, kStationListId);
    const HWND codecCombo = FindControl(view, kModeComboId);
    const HWND bitrateCombo = FindControl(view, kBitrateComboId);
    const HWND compatibilityCombo = FindControl(view, kCompatibilityComboId);
    const HWND searchEdit = FindControl(view, kSearchEditId);
    const HWND resetButton = FindControl(view, kResetButtonId);
    const HWND resultCount = FindControl(view, kResultCountId);
    const HWND tunerLabel = FindControl(view, kTunerLabelId);
    const HWND tunerToggleButton = FindControl(view, kTunerToggleButtonId);
    const HWND selectionDetails = FindControl(view, kSelectionDetailsId);
    const HWND status = FindControl(view, kStatusId);
    const HWND filterLabel = FindControl(view, kBrowseLabelId);
    const HWND filterToggleButton = FindControl(view, kFilterToggleButtonId);
    const HWND stationsViewButton = FindControl(view, kStationsViewButtonId);
    const HWND browseViewButton = FindControl(view, kBrowseViewButtonId);
    const HWND locationList = FindControl(view, kLocationListId);
    const HWND browseGenreList = FindControl(view, kGenreListId);
    const HWND providerTotal = FindControl(view, kProviderTotalId);
    const HWND pageSizeCombo = FindControl(view, kPageSizeComboId);
    const HWND previousPageButton = FindControl(view, kPreviousPageButtonId);
    const HWND nextPageButton = FindControl(view, kNextPageButtonId);
    gLocationList = locationList;
    gBrowseGenreList = browseGenreList;
    gProviderTotal = providerTotal;
    gNowPlaying = FindControl(view, kNowPlayingId);
    const std::wstring initialProviderText = ControlText(providerTotal);
    if (gStationList == nullptr || searchEdit == nullptr || resetButton == nullptr ||
        resultCount == nullptr || tunerLabel == nullptr || tunerToggleButton == nullptr ||
        gNowPlaying == nullptr || selectionDetails == nullptr || status == nullptr ||
        filterLabel == nullptr || filterToggleButton == nullptr ||
        compatibilityCombo == nullptr ||
        stationsViewButton == nullptr || browseViewButton == nullptr ||
        locationList == nullptr || browseGenreList == nullptr || providerTotal == nullptr ||
        pageSizeCombo == nullptr || previousPageButton == nullptr || nextPageButton == nullptr ||
        !VerifyResetButton(resetButton) ||
        !VerifyResultCount(resultCount, false) ||
        SetWindowSubclass(gStationList, StationListSubclassProc, kStationListId, 0) == FALSE ||
        !HasVisibleStyle(locationList) || !HasVisibleStyle(browseGenreList) ||
        // Browse now intentionally starts with its tab-focusable details drawer open.
        !HasVisibleStyle(tunerLabel) || !HasVisibleStyle(tunerToggleButton) ||
         (initialProviderText.rfind(L"Loading Radio Browser", 0) != 0 &&
          initialProviderText.rfind(L"Radio Browser total: ", 0) != 0 &&
          (!fullFixture ||
           initialProviderText != L"Radio Browser provider total unavailable"))) {
        std::wcerr << L"Default Browse, loading state, result count, or Reset was not valid.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 17;
    }
    if (!VerifyPaletteRefresh(view)) {
        std::wcerr << L"Palette-only skin integration failed.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 67;
    }
    if (skinOnly) {
        // Skin-only is a layout/lifecycle test, so route the production command
        // directly without depending on provider timing.
        SendMessageW(
            view,
            WM_COMMAND,
            MAKEWPARAM(kStationsViewButtonId, BN_CLICKED),
            reinterpret_cast<LPARAM>(stationsViewButton));
        PumpFor(100);
        const bool nativeControlsValid =
            gStationList != nullptr && codecCombo != nullptr && bitrateCombo != nullptr &&
            compatibilityCombo != nullptr && ComboBox_GetCount(compatibilityCombo) == 5 &&
            ComboBox_GetCount(bitrateCombo) == 6;
        const SkinWrapperCounts initialWrappers = CurrentSkinWrapperCounts();
        const bool initialWrappersValid = HasSkinWrapperCounts(initialWrappers, 0, 0, 0, 0);
        if (!nativeControlsValid) {
            std::wcerr << L"Skin-only native control creation was not valid.\n";
            DestroyWindow(view);
            plugin->quit();
            FreeLibrary(module);
            return 42;
        }
        if (!initialWrappersValid) {
            ReportSkinWrapperCounts(L"initial", initialWrappers);
            DestroyWindow(view);
            plugin->quit();
            FreeLibrary(module);
            return 43;
        }

        // VerifyTunerToggle deliberately sends WM_DISPLAYCHANGE after its narrow-layout
        // exercise. The stability mode must remain wrapper-free across that reskin path.
        const bool tunerToggleValid =
            VerifyTunerToggle(view, gStationList, resetButton, resultCount, tunerLabel, tunerToggleButton);
        const bool detailsScalingValid = VerifyDetailsScaling(
            view,
            gStationList,
            selectionDetails,
            status,
            tunerLabel,
            tunerToggleButton);
        const SkinWrapperCounts reskinnedWrappers = CurrentSkinWrapperCounts();
        const bool reskinnedWrappersValid =
            HasSkinWrapperCounts(reskinnedWrappers, 0, 0, 0, 0);
        if (!tunerToggleValid) {
            ReportSkinWrapperCounts(L"after tuner-toggle exercise", reskinnedWrappers);
            DestroyWindow(view);
            plugin->quit();
            FreeLibrary(module);
            return 44;
        }
        if (!detailsScalingValid) {
            ReportSkinWrapperCounts(L"after details-scaling exercise", reskinnedWrappers);
            DestroyWindow(view);
            plugin->quit();
            FreeLibrary(module);
            return 45;
        }
        if (!reskinnedWrappersValid) {
            ReportSkinWrapperCounts(L"after wrapper-free display-change reskin", reskinnedWrappers);
            DestroyWindow(view);
            plugin->quit();
            FreeLibrary(module);
            return 46;
        }
        const int teardownLookups = gPaletteLookups;
        const int teardownCalls = gPaletteCalls;
        DestroyWindow(view);
        if (gPaletteLookups != teardownLookups || gPaletteCalls != teardownCalls) {
            std::wcerr << L"Skin palette provider was queried during destruction.\n";
            plugin->quit();
            FreeLibrary(module);
            return 68;
        }
        if (!HasSkinWrapperCounts(CurrentSkinWrapperCounts(), 0, 0, 0, 0)) {
            std::wcerr << L"Stability mode unexpectedly invoked Media Library skin wrappers.\n";
            plugin->quit();
            FreeLibrary(module);
            return 13;
        }
        plugin->quit();
        DestroyWindow(viewParent);
        DestroyWindow(library);
        DestroyWindow(winamp);
        FreeLibrary(module);
        std::wcout << L"Created and destroyed the wrapper-free radio-console view.\n";
        std::wcout << L"Kept Media Library skin-wrapper IPC absent across reskin and teardown.\n";
        return 0;
    }
    if (!PumpUntil(30000, HasStations)) {
        std::wcerr << L"Native view did not load stations within 30 seconds.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 7;
    }
    if (!VerifyResultCount(resultCount, false)) {
        std::wcerr << L"Initial visible station count was not exact.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 20;
    }
    if (fullFixture && !VerifyStationColumnSorting(view, resultCount)) {
        std::wcerr << L"Station table header sorting did not provide deterministic directions.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 62;
    }

    if (codecCombo == nullptr || ComboBox_GetCount(codecCombo) < 2 ||
        bitrateCombo == nullptr || ComboBox_GetCount(bitrateCombo) != 6) {
        std::wcerr << L"Native view did not expose codec and bitrate filters.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 9;
    }
    const HWND genreCombo = FindControl(view, kGenreComboId);
    const HWND countryCombo = FindControl(view, kCountryComboId);
    const HWND languageCombo = FindControl(view, kLanguageComboId);
    gGenreCombo = genreCombo;
    gCountryCombo = countryCombo;
    gLanguageCombo = languageCombo;
    if (!PumpUntil(60000, HasBrowseCategories)) {
        std::wcerr << L"Native view did not load browse categories.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 14;
    }
    if (!PumpUntil(10000, HasBrowsePanes) ||
        !VerifyBrowseSwitchAndGeometry(
            view,
            stationsViewButton,
            browseViewButton,
            FindControl(view, kFavoritesViewButtonId),
             locationList,
             browseGenreList,
             providerTotal,
             filterLabel,
             filterToggleButton,
             languageCombo,
             codecCombo,
             bitrateCombo,
             compatibilityCombo,
             resetButton,
             gStationList,
             pageSizeCombo,
            previousPageButton,
            nextPageButton,
            tunerLabel,
            tunerToggleButton)) {
        std::wcerr << L"Stations/Browse switching, provider counts, or stacked geometry failed.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 27;
    }
    for (int pass = 0; pass != 3; ++pass) {
        SendMessageW(view, WM_COMMAND, MAKEWPARAM(kStationsViewButtonId, BN_CLICKED), 0);
        PumpFor(20);
        SendMessageW(view, WM_COMMAND, MAKEWPARAM(kBrowseViewButtonId, BN_CLICKED), 0);
        PumpFor(20);
        if (!HasVisibleStyle(locationList) || !HasVisibleStyle(browseGenreList) ||
            !HasVisibleStyle(tunerToggleButton) || !ControlStripIsInteractive(view) ||
            !HasSkinWrapperCounts(CurrentSkinWrapperCounts(), 0, 0, 0, 0)) {
            std::wcerr << L"Repeated Stations/Browse switching changed stable controls or invoked skin wrappers.\n";
            DestroyWindow(view);
            plugin->quit();
            FreeLibrary(module);
            return 61;
        }
    }

    SendMessageW(view, WM_COMMAND, MAKEWPARAM(kStationsViewButtonId, BN_CLICKED), 0);
    PumpFor(100);
    if (!VerifyTunerToggle(
            view, gStationList, resetButton, resultCount, tunerLabel, tunerToggleButton) ||
        !VerifyDetailsScaling(
            view,
            gStationList,
            selectionDetails,
            status,
            tunerLabel,
            tunerToggleButton)) {
        std::wcerr << L"Measured details or tuner behavior was not valid in Stations view.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 17;
    }
    SendMessageW(view, WM_COMMAND, MAKEWPARAM(kBrowseViewButtonId, BN_CLICKED), 0);
    PumpFor(100);
    gBusyListClearCount = 0;
    gSearchStartCount = 0;
    ListView_SetItemState(
        browseGenreList, 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    if (!PumpUntil(30000, HasSearchReplacement) ||
        !VerifyResultCount(resultCount, true)) {
        std::wcerr << L"Browse Genre selection did not refine the station page.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 28;
    }
    if (fullFixture &&
        (!VerifyCategoryFocusIsPassive(view, locationList, browseGenreList) ||
         !VerifySelectedRowPainting(view, locationList) ||
         !VerifySelectedRowPainting(view, browseGenreList) ||
         !VerifySelectedRowPainting(view, gStationList))) {
        std::wcerr << L"Focused/unfocused selection or passive category focus failed.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 73;
    }
    const int lastGenreRow = ListView_GetItemCount(browseGenreList) - 1;
    if (lastGenreRow <= 1 ||
        ListCellText(browseGenreList, lastGenreRow, 0) != L"Next categories...") {
        std::wcerr << L"Browse Genre pane did not expose bounded forward paging.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 29;
    }
    const int selectedBeforeNext = ListView_GetNextItem(browseGenreList, -1, LVNI_SELECTED);
    const UINT nextStateBefore = ListView_GetItemState(
        browseGenreList, lastGenreRow, LVIS_SELECTED | LVIS_FOCUSED);
    const std::wstring nextStatusBefore = ControlText(status);
    const std::wstring nextRowOneBefore = ListCellText(browseGenreList, 1, 0);
    const LRESULT nextWorkBefore = fullFixture ? SendMessageW(view, kProbeWorkState, 0, 0) : -1;
    const int nextStationReplacementsBefore = gSearchStartCount;
    gGenreTraceRoot = fullFixture ? view : nullptr;
    gGenreTraceFixture = fullFixture;
    gGenreTraceDeferredSearches = 0;
    gGenreTraceNotificationCount = 0;
    gGenreSelectionTrace.clear();
    ListView_SetItemState(
        browseGenreList,
        lastGenreRow,
        LVIS_SELECTED | LVIS_FOCUSED,
        LVIS_SELECTED | LVIS_FOCUSED);
    const int selectedImmediatelyAfterNext = ListView_GetNextItem(browseGenreList, -1, LVNI_SELECTED);
    const LRESULT nextWorkImmediate = fullFixture ? SendMessageW(view, kProbeWorkState, 0, 0) : -1;
    const std::wstring nextStatusImmediate = ControlText(status);
    const bool genreNextLoaded = PumpUntil(30000, HasPreviousGenreCategoryPage);
    gGenreTraceRoot = nullptr;
    if (fullFixture) {
        std::wcout << L"Genre Next diagnostic: passed=" << genreNextLoaded
                   << L", target=" << lastGenreRow << L", targetStateBefore=" << nextStateBefore
                   << L", selected=" << selectedBeforeNext << L" -> " << selectedImmediatelyAfterNext
                   << L" -> " << ListView_GetNextItem(browseGenreList, -1, LVNI_SELECTED)
                   << L", workBits=" << nextWorkBefore << L" -> " << nextWorkImmediate
                   << L" -> " << SendMessageW(view, kProbeWorkState, 0, 0)
                   << L" (running=1, queuedSearch=2, rebuilding=4), notifications="
                   << gGenreTraceNotificationCount << L", deferredSearches=" << gGenreTraceDeferredSearches
                   << L", stationReplacements=" << gSearchStartCount - nextStationReplacementsBefore
                   << L", genreRows=" << ListView_GetItemCount(browseGenreList) << L".\n";
        std::wcout << L"Genre Next status: [" << nextStatusBefore << L"] -> ["
                   << nextStatusImmediate << L"] -> [" << ControlText(status)
                   << L"]; row1: [" << nextRowOneBefore << L"] -> ["
                   << ListCellText(browseGenreList, 1, 0) << L"].\n";
        for (const auto& item : gGenreSelectionTrace) {
            std::wcout << L"Genre notification: row=" << item.row << L", selected=" << item.selected
                       << L", changed=" << item.changed << L", state=" << item.oldState
                       << L" -> " << item.newState << L", data=" << item.rowData
                       << L", work=" << item.workBefore << L" -> " << item.workAfter << L".\n";
        }
    }
    if (!genreNextLoaded) {
        std::wcerr << L"Browse Genre Next did not replace the category page.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 30;
    }
    ListView_SetItemState(
        browseGenreList, 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    if (!PumpUntil(30000, HasFirstGenreCategoryPage)) {
        std::wcerr << L"Browse Genre Previous did not restore the first bounded page.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 31;
    }
    if (fullFixture) {
        // fixture-stale deliberately completes after Reset. The visible page
        // must only be the later reset request, not that stale completion.
        SetWindowTextW(searchEdit, L"fixture-stale");
        gSearchStartCount = 0;
        SendMessageW(view, WM_COMMAND, MAKEWPARAM(kSearchButtonId, BN_CLICKED), 0);
        PumpFor(10);
        SendMessageW(view, WM_COMMAND, MAKEWPARAM(kResetButtonId, BN_CLICKED), 0);
        if (!PumpUntil(30000, HasSearchReplacement) ||
            ControlText(searchEdit) != L"" ||
            ListCellText(gStationList, 0, 0) == L"Stale fixture completion sentinel" ||
            !VerifyResultCount(resultCount, false) ||
            ControlText(status).find(L"Station results loaded") == std::wstring::npos) {
            std::wcerr << L"Reset did not replace an in-flight stale search with the All page.\n";
            DestroyWindow(view);
            plugin->quit();
            FreeLibrary(module);
            return 56;
        }
    }
    gSearchStartCount = 0;
    SetWindowTextW(searchEdit, L"reset-me");
    SendMessageW(view, WM_COMMAND, MAKEWPARAM(kResetButtonId, BN_CLICKED), 0);
    PumpFor(1);
    const bool resetStripStayedInteractive = ControlStripIsInteractive(view);
    if (!resetStripStayedInteractive) {
        std::wcerr << L"Reset disabled the Live Stations control strip while loading.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 57;
    }
    if (!PumpUntil(30000, HasSearchReplacement) ||
        ControlText(searchEdit) != L"" ||
        ListView_GetNextItem(locationList, -1, LVNI_SELECTED) != 0 ||
        ListView_GetNextItem(browseGenreList, -1, LVNI_SELECTED) != 0 ||
        ComboBox_GetCurSel(languageCombo) != 0 || ComboBox_GetCurSel(codecCombo) != 0 ||
        ComboBox_GetCurSel(bitrateCombo) != 0 ||
        ComboBox_GetCurSel(compatibilityCombo) != 0 ||
        !VerifyResultCount(resultCount, false)) {
        std::wcerr << L"Browse Reset did not restore every upper and lower filter to All.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 32;
    }
    SendMessageW(view, WM_COMMAND, MAKEWPARAM(kStationsViewButtonId, BN_CLICKED), 0);
    PumpFor(100);

    gSearchStartCount = 0;
    ComboBox_SetCurSel(pageSizeCombo, 0);
    SendMessageW(
        view,
        WM_COMMAND,
        MAKEWPARAM(kPageSizeComboId, CBN_SELCHANGE),
        reinterpret_cast<LPARAM>(pageSizeCombo));
    if (!PumpUntil(30000, HasSearchReplacement) ||
        ListView_GetItemCount(gStationList) > 50 ||
        IsWindowEnabled(nextPageButton) == FALSE || !VerifyResultCount(resultCount, false)) {
        std::wcerr << L"The 50-row station page size was not bounded correctly.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 33;
    }
    gSearchStartCount = 0;
    gBusyListClearCount = 0;
    SendMessageW(nextPageButton, BM_CLICK, 0, 0);
    PumpFor(1);
    const bool pagerStripStayedInteractive = ControlStripIsInteractive(view);
    if (!pagerStripStayedInteractive) {
        std::wcerr << L"Paging disabled the Live Stations control strip while loading.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 58;
    }
    if (!PumpUntil(30000, HasSearchReplacement) ||
        !ResultPageStartsAfterFirst() || IsWindowEnabled(previousPageButton) == FALSE ||
        ControlText(previousPageButton) != L"Previous" || gBusyListClearCount != 0) {
        std::wcerr << L"Station Next did not replace the first 50-row page without clearing the "
                   << L"rendered page while busy.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 34;
    }
    gSearchStartCount = 0;
    SendMessageW(previousPageButton, BM_CLICK, 0, 0);
    if (!PumpUntil(30000, HasSearchReplacement) ||
        ResultPageStartsAfterFirst() || IsWindowEnabled(previousPageButton) == FALSE ||
        ControlText(previousPageButton) != L"First page") {
        std::wcerr << L"Station Previous did not restore the first page.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 35;
    }
    gSearchStartCount = 0;
    ComboBox_SetCurSel(pageSizeCombo, 2);
    SendMessageW(
        view,
        WM_COMMAND,
        MAKEWPARAM(kPageSizeComboId, CBN_SELCHANGE),
        reinterpret_cast<LPARAM>(pageSizeCombo));
    if (!PumpUntil(30000, HasSearchReplacement)) {
        std::wcerr << L"The 200-row station page size did not restore.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 36;
    }
    gSearchStartCount = 0;
    ComboBox_SetCurSel(genreCombo, 1);
    SendMessageW(
        view,
        WM_COMMAND,
        MAKEWPARAM(kGenreComboId, CBN_SELCHANGE),
        reinterpret_cast<LPARAM>(genreCombo));
    if (!PumpUntil(30000, HasSearchReplacement) ||
        !VerifyResultCount(resultCount, true)) {
        std::wcerr << L"Selecting a genre did not complete a filtered station search.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 15;
    }
    gSearchStartCount = 0;
    ComboBox_SetCurSel(genreCombo, 0);
    ComboBox_SetCurSel(compatibilityCombo, 1);
    SendMessageW(
        view,
        WM_COMMAND,
        MAKEWPARAM(kCompatibilityComboId, CBN_SELCHANGE),
        reinterpret_cast<LPARAM>(compatibilityCombo));
    if (!PumpUntil(30000, HasSearchReplacement) ||
        ComboBox_GetCurSel(codecCombo) != 0 ||
        ComboBox_GetCurSel(compatibilityCombo) != 1 ||
        ListCellText(gStationList, 0, 4) != L"Best" ||
        !VerifyResultCount(resultCount, true)) {
        std::wcerr << L"Selecting Best compatibility did not run the typed MP3 filter.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 37;
    }
    gSearchStartCount = 0;
    ComboBox_SetCurSel(compatibilityCombo, 0);
    ComboBox_SetCurSel(bitrateCombo, 1);
    SendMessageW(
        view,
        WM_COMMAND,
        MAKEWPARAM(kBitrateComboId, CBN_SELCHANGE),
        reinterpret_cast<LPARAM>(bitrateCombo));
    PumpFor(1);
    const bool filterStripStayedInteractive = ControlStripIsInteractive(view);
    if (!filterStripStayedInteractive) {
        std::wcerr << L"Filtering disabled the Live Stations control strip while loading.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 59;
    }
    if (!PumpUntil(30000, HasSearchReplacement) ||
        !VerifyResultCount(resultCount, true)) {
        std::wcerr << L"Selecting a bitrate did not complete a filtered station search.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 26;
    }
    gSearchStartCount = 0;
    SendMessageW(view, WM_COMMAND, MAKEWPARAM(kResetButtonId, BN_CLICKED), 0);
    if (!PumpUntil(30000, HasSearchReplacement) ||
        ComboBox_GetCurSel(genreCombo) != 0 || ComboBox_GetCurSel(countryCombo) != 0 ||
        ComboBox_GetCurSel(languageCombo) != 0 || ComboBox_GetCurSel(codecCombo) != 0 ||
        ComboBox_GetCurSel(bitrateCombo) != 0 ||
        ComboBox_GetCurSel(compatibilityCombo) != 0 ||
        !VerifyResultCount(resultCount, false)) {
        std::wcerr << L"Reset did not clear and reload the browse filters.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 16;
    }

    // Paging/filter coverage above deliberately selected Stations. Exercise
    // Browse's dividers through the real View command before measuring them.
    SendMessageW(view, WM_COMMAND, MAKEWPARAM(kBrowseViewButtonId, BN_CLICKED),
                 reinterpret_cast<LPARAM>(browseViewButton));
    PumpFor(100);
    if (ControlText(browseViewButton) != L"&View: Browse" ||
        !HasVisibleStyle(locationList) || !HasVisibleStyle(browseGenreList)) {
        std::wcerr << L"Pane-drag setup did not switch from Stations to Browse.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 65;
    }
    if (!VerifyPaneDragging(view, locationList, browseGenreList, gStationList,
                            selectionDetails, filterLabel)) {
        std::wcerr << L"Resizable panes or preserved category column widths failed.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 64;
    }
    if (!VerifyResizePainting(view, locationList, browseGenreList)) {
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 66;
    }

    SetWindowTextW(searchEdit, L"rock");
    gEnqueuedUrl.clear();
    gPlayingTitle = L"Previous Artist - Previous Track";
    gPositionSelected = false;
    gPlayCommandReceived = false;
    gCurrentPlaylistPosition = -1;
    gPlaybackState = 0;
    gSearchStartCount = 0;
    SendMessageW(searchEdit, WM_KEYDOWN, VK_RETURN, 1);
    SendMessageW(searchEdit, WM_KEYDOWN, VK_RETURN, 1 | (static_cast<LPARAM>(1) << 30));
    SendMessageW(searchEdit, WM_CHAR, L'\r', 1);
    SendMessageW(searchEdit, WM_KEYUP, VK_RETURN, 1 | (static_cast<LPARAM>(1) << 30));
    PumpFor(1);
    const bool searchStripStayedInteractive = ControlStripIsInteractive(view);
    if (!searchStripStayedInteractive) {
        std::wcerr << L"Search disabled the Live Stations control strip while loading.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 60;
    }
    if (!PumpUntil(30000, HasSearchReplacement) ||
        gSearchStartCount != 1 || PlaybackRequested() ||
        !VerifyResultCount(resultCount, true)) {
        std::wcerr << L"Enter in the search edit did not run one bounded search action "
                   << L"(observed starts: " << gSearchStartCount << L").\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 18;
    }
    wchar_t searchText[32]{};
    GetWindowTextW(searchEdit, searchText, static_cast<int>(_countof(searchText)));
    if (std::wstring(searchText) != L"rock") {
        std::wcerr << L"Search text changed unexpectedly after Enter.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 19;
    }

    ListView_SetItemState(gStationList, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    PumpFor(100);
    const std::wstring selectedDetails = ControlText(selectionDetails);
    if (selectedDetails.find(L"Directory: Radio Browser") == std::wstring::npos ||
        selectedDetails.find(L"Stream host: ") == std::wstring::npos ||
        selectedDetails.find(L"Delivery: ") == std::wstring::npos) {
        std::wcerr << L"Selected-station details did not expose bounded source metadata.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 38;
    }
    if (fullFixture) {
        const HWND favoritesViewButton = FindControl(view, kFavoritesViewButtonId);
        if (favoritesViewButton == nullptr || !HasVisibleStyle(favoritesViewButton)) {
            std::wcerr << L"Offline fixture Favorites view control was unavailable.\n";
            DestroyWindow(view);
            plugin->quit();
            FreeLibrary(module);
            return 49;
        }
        // Start from an empty root and make the details text agree with its
        // visible zero-row count before adding the deterministic favorite.
        SendMessageW(
            view,
            WM_COMMAND,
            MAKEWPARAM(kFavoritesViewButtonId, BN_CLICKED),
            reinterpret_cast<LPARAM>(favoritesViewButton));
        PumpFor(100);
        if (ListView_GetItemCount(gStationList) != 0 ||
            ControlText(resultCount).rfind(L"0 favorites shown", 0) != 0 ||
            ControlText(selectionDetails).find(L"No saved Favorites yet") == std::wstring::npos) {
            std::wcerr << L"Empty Favorites count and details were inconsistent.\n";
            DestroyWindow(view);
            plugin->quit();
            FreeLibrary(module);
            return 50;
        }
        SendMessageW(
            view,
            WM_COMMAND,
            MAKEWPARAM(kBrowseViewButtonId, BN_CLICKED),
            reinterpret_cast<LPARAM>(browseViewButton));
        // The retained-row counter intentionally observes only replacement of
        // a nonempty page. Favorites just established a genuine empty list,
        // so this transition instead proves that Browse supplies its bounded
        // matching page and result label from the current search text.
        if (!PumpUntil(30000, HasStations) ||
            !VerifyResultCount(resultCount, true) ||
            ListCellText(gStationList, 0, 0).find(L"Fixture Rock Radio") == std::wstring::npos) {
            std::wcerr << L"Leaving empty Favorites did not restore the bounded Browse page.\n";
            DestroyWindow(view);
            plugin->quit();
            FreeLibrary(module);
            return 51;
        }
        ListView_SetItemState(
            gStationList, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        PumpFor(100);
        const std::wstring favoriteName = ListCellText(gStationList, 0, 0);
        SendMessageW(view, WM_COMMAND, MAKEWPARAM(kAddFavoriteCommandId, 0), 0);
        PumpFor(100);
        if (ControlText(selectionDetails).find(L"Favorite: Live Stations / Favorites") ==
                std::wstring::npos ||
            ControlText(status).find(L"fixture") == std::wstring::npos) {
            std::wcerr << L"Offline fixture could not add the selected station to Favorites.\n";
            DestroyWindow(view);
            plugin->quit();
            FreeLibrary(module);
            return 52;
        }
        SendMessageW(
            view,
            WM_COMMAND,
            MAKEWPARAM(kFavoritesViewButtonId, BN_CLICKED),
            reinterpret_cast<LPARAM>(favoritesViewButton));
        PumpFor(100);
        if (ListView_GetItemCount(gStationList) != 1 ||
            ListCellText(gStationList, 0, 0) != favoriteName ||
            ControlText(resultCount).rfind(L"1 favorite shown", 0) != 0 ||
            ControlText(selectionDetails).find(L"Select a saved favorite") == std::wstring::npos) {
            std::wcerr << L"Offline fixture Favorites root list was not deterministic.\n";
            DestroyWindow(view);
            plugin->quit();
            FreeLibrary(module);
            return 53;
        }
        // A Browse completion in flight must never replace the Favorites list.
        SendMessageW(
            view,
            WM_COMMAND,
            MAKEWPARAM(kBrowseViewButtonId, BN_CLICKED),
            reinterpret_cast<LPARAM>(browseViewButton));
        PumpFor(5);
        SendMessageW(
            view,
            WM_COMMAND,
            MAKEWPARAM(kFavoritesViewButtonId, BN_CLICKED),
            reinterpret_cast<LPARAM>(favoritesViewButton));
        PumpFor(150);
        if (ListView_GetItemCount(gStationList) != 1 ||
            ListCellText(gStationList, 0, 0) != favoriteName ||
            ControlText(resultCount).rfind(L"1 favorite shown", 0) != 0 ||
            ControlText(selectionDetails).find(L"Select a saved favorite") == std::wstring::npos) {
            std::wcerr << L"Pending Browse completion replaced the saved Favorites root.\n";
            DestroyWindow(view);
            plugin->quit();
            FreeLibrary(module);
            return 54;
        }
        ListView_SetItemState(
            gStationList, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        PumpFor(100);
        if (ControlText(selectionDetails).find(L"Favorite: Live Stations / Favorites") ==
            std::wstring::npos) {
            std::wcerr << L"Favorites selection did not reuse station details.\n";
            DestroyWindow(view);
            plugin->quit();
            FreeLibrary(module);
            return 55;
        }
        gEnqueuedUrl.clear();
        gPositionSelected = false;
        gPlayCommandReceived = false;
        SendMessageW(view, WM_COMMAND, MAKEWPARAM(kAddWinampPlaylistCommandId, 0), 0);
        PumpFor(100);
#if defined(LIVE_STATIONS_UI_ONLY)
        if (!gEnqueuedUrl.empty() || gPositionSelected || gPlayCommandReceived) {
            std::wcerr << L"UI-only build exposed the deferred companion command.\n";
            DestroyWindow(view);
            plugin->quit();
            FreeLibrary(module);
            return 56;
        }
#else
        if (gEnqueuedUrl.find(
                L"http://127.0.0.1:48937/v1/radio-browser/fixture-station-") != 0 ||
            gPositionSelected || gPlayCommandReceived ||
            ControlText(status).find(L"refreshable station link") == std::wstring::npos) {
            std::wcerr << L"Refreshable Winamp playlist action did not enqueue one stable identity without playing it.\n";
            DestroyWindow(view);
            plugin->quit();
            FreeLibrary(module);
            return 56;
        }
        gEnqueuedUrl.clear();
#endif
    }
    NMLVKEYDOWN key{};
    key.hdr.hwndFrom = gStationList;
    key.hdr.idFrom = kStationListId;
    key.hdr.code = LVN_KEYDOWN;
    key.wVKey = VK_RETURN;
    SendMessageW(body, WM_NOTIFY, kStationListId, reinterpret_cast<LPARAM>(&key));
    if (!PumpUntil(20000, PlaybackRequested)) {
        std::wcerr << L"Selecting a station did not request Winamp playback.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 8;
    }

    gExpectedNowPlaying =
        L"Now playing\r\nTrack information not provided by this stream.";
    if (!PumpUntil(4000, NowPlayingMatches)) {
        std::wcerr << L"Missing stream metadata did not show the honest fallback.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 21;
    }
    gPlayingTitle = L"Probe Artist - Probe Track";
    gExpectedNowPlaying = L"Now playing\r\nProbe Artist - Probe Track";
    if (!PumpUntil(4000, NowPlayingMatches)) {
        std::wcerr << L"Available stream metadata was not displayed.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 22;
    }
    gPlayingTitle.clear();
    gExpectedNowPlaying =
        L"Now playing\r\nTrack information not provided by this stream.";
    if (!PumpUntil(4000, NowPlayingMatches)) {
        std::wcerr << L"Unavailable stream metadata did not return to the fallback.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 23;
    }
    gCurrentPlaylistPosition = 7;
    if (!PumpUntil(4000, NowPlayingIsClear)) {
        std::wcerr << L"Now-playing metadata was not cleared after playback moved away.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 24;
    }
    gPlayingTitle = L"Stale Artist - Stale Track";
    PumpFor(2000);
    if (!NowPlayingIsClear()) {
        std::wcerr << L"Stale station metadata resumed after tracking stopped.\n";
        DestroyWindow(view);
        plugin->quit();
        FreeLibrary(module);
        return 25;
    }

    const int stationCount = ListView_GetItemCount(gStationList);
    const HWND viewTooltips[] = {ListView_GetToolTips(gStationList),
        ListView_GetToolTips(locationList), ListView_GetToolTips(browseGenreList)};
    DestroyWindow(view);
    for (HWND tooltip : viewTooltips) {
        if (tooltip != nullptr && IsWindow(tooltip)) {
            std::wcerr << L"A list tooltip survived destruction of its owning view.\n";
            plugin->quit();
            FreeLibrary(module);
            return 74;
        }
    }
    if (!HasSkinWrapperCounts(CurrentSkinWrapperCounts(), 0, 0, 0, 0)) {
        std::wcerr << L"Full-host stability mode unexpectedly invoked Media Library skin wrappers.\n";
        plugin->quit();
        FreeLibrary(module);
        return 11;
    }

    const HWND cancellationView = reinterpret_cast<HWND>(
        plugin->MessageProc(ML_MSG_TREE_ONCREATEVIEW, kTreeId, reinterpret_cast<INT_PTR>(viewParent), 0));
    if (cancellationView == nullptr) {
        std::wcerr << L"Plug-in could not create the cancellation test view.\n";
        plugin->quit();
        FreeLibrary(module);
        return 10;
    }
    const int callsBeforeLookupDestroy = gPaletteCalls;
    gPaletteDestroyOnLookup = cancellationView;
    SendMessageW(cancellationView, WM_THEMECHANGED, 0, 0);
    if (IsWindow(cancellationView) || gPaletteCalls != callsBeforeLookupDestroy) {
        std::wcerr << L"Palette lookup continued after host destroyed the view.\n";
        plugin->quit();
        FreeLibrary(module);
        return 69;
    }
    const HWND colorCancellationView = reinterpret_cast<HWND>(
        plugin->MessageProc(ML_MSG_TREE_ONCREATEVIEW, kTreeId, reinterpret_cast<INT_PTR>(viewParent), 0));
    if (colorCancellationView == nullptr) {
        plugin->quit();
        FreeLibrary(module);
        return 70;
    }
    const int callsBeforeColorDestroy = gPaletteCalls;
    gPaletteDestroyOnColor = colorCancellationView;
    SendMessageW(colorCancellationView, WM_THEMECHANGED, 0, 0);
    if (IsWindow(colorCancellationView) || gPaletteCalls != callsBeforeColorDestroy + 1) {
        std::wcerr << L"Palette callbacks continued after host destroyed the view.\n";
        plugin->quit();
        FreeLibrary(module);
        return 71;
    }

    const HWND quitOrderingView = reinterpret_cast<HWND>(
        plugin->MessageProc(ML_MSG_TREE_ONCREATEVIEW, kTreeId, reinterpret_cast<INT_PTR>(viewParent), 0));
    if (quitOrderingView == nullptr) {
        std::wcerr << L"Plug-in could not create the quit-ordering test view.\n";
        plugin->quit();
        FreeLibrary(module);
        return 39;
    }
    const int quitPaletteLookups = gPaletteLookups;
    const int quitPaletteCalls = gPaletteCalls;
    const HWND quitTooltips[] = {
        ListView_GetToolTips(FindControl(quitOrderingView, kStationListId)),
        ListView_GetToolTips(FindControl(quitOrderingView, kLocationListId)),
        ListView_GetToolTips(FindControl(quitOrderingView, kGenreListId))};
    plugin->quit();
    bool tooltipSurvivedQuit = false;
    for (HWND tooltip : quitTooltips)
        tooltipSurvivedQuit = (tooltip != nullptr && IsWindow(tooltip)) || tooltipSurvivedQuit;
    WNDCLASSEXW viewClass{};
    viewClass.cbSize = sizeof(viewClass);
    if (tooltipSurvivedQuit || gPaletteLookups != quitPaletteLookups || gPaletteCalls != quitPaletteCalls ||
        IsWindow(quitOrderingView) != FALSE ||
        GetClassInfoExW(module, kViewClassName, &viewClass) != FALSE) {
        std::wcerr << L"PluginQuit did not destroy the live view before releasing its class.\n";
        if (IsWindow(quitOrderingView) != FALSE) {
            DestroyWindow(quitOrderingView);
        }
        FreeLibrary(module);
        return 40;
    }
    DestroyWindow(viewParent);
    DestroyWindow(library);
    DestroyWindow(winamp);
    FreeLibrary(module);

    std::wcout << L"Loaded " << stationCount << L" stations in the native view.\n";
    std::wcout << L"Resolved and handed a safe stream URL to the fake Winamp host.\n";
    std::wcout << L"Verified exact visible counts after load, filters, reset, and search.\n";
    std::wcout << L"Verified default Browse panes, measured provider counts, and bounded paging.\n";
    std::wcout << L"Verified compatibility filtering and the keyboard-accessible filter strip.\n";
    std::wcout << L"Verified minimize preservation plus narrow and restored result framing.\n";
    std::wcout << L"Verified font-metric details scrolling and narrow stacked fallback.\n";
    std::wcout << L"Verified initial nested layout, pinned toolbar under body scroll/focus, "
                  L"tab traversal, and fixed-width 12/22/36px font/viewport matrix.\n";
    std::wcout << L"Verified bounded station website/source/delivery details.\n";
    std::wcout << L"Verified metadata fallback, update, and stale-playback clearing.\n";
    std::wcout << L"Verified font-sized Reset, search Enter, and relocated tuner geometry.\n";
    if (fullFixture) {
        std::wcout << L"Verified focus-only category notifications retain selection/data and "
                      L"focused/unfocused selected rows render system highlight/text colors.\n";
        std::wcout << L"Verified in-memory selected-station Favorites action without user storage.\n";
    }
    std::wcout << L"Kept Media Library skin-wrapper IPC absent across the full-host lifecycle.\n";
    std::wcout << L"Destroyed a second view during startup without leaving a worker behind.\n";
    std::wcout << L"Stopped palette access after host destruction during lookup/color callbacks.\n";
    std::wcout << L"Verified native tooltip colors/fonts across palette changes and owner destruction.\n";
    std::wcout << L"Verified that PluginQuit destroys live views before releasing the class.\n";
    return 0;
}
