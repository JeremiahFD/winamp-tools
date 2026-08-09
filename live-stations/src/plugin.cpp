#include "radio_browser.h"
#include "winamp_media_library_abi.h"

#include <commctrl.h>
#include <windowsx.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr wchar_t kViewClassName[] = L"WinampToolsLiveStationsView";
constexpr wchar_t kPluginDescription[] = L"Winamp Tools Live Stations v0.1.0 alpha";
constexpr UINT kWorkComplete = WM_APP + 41;
constexpr int kSearchEditId = 1001;
constexpr int kSearchButtonId = 1002;
constexpr int kModeComboId = 1003;
constexpr int kStationListId = 1004;
constexpr int kPlayButtonId = 1005;
constexpr int kWinampPlayCommand = 40045;

enum class PendingKind {
    None,
    Search,
    Play,
};

struct ViewState {
    HWND window = nullptr;
    HWND searchEdit = nullptr;
    HWND searchButton = nullptr;
    HWND modeCombo = nullptr;
    HWND stationList = nullptr;
    HWND playButton = nullptr;
    HWND status = nullptr;
    std::vector<live_stations::Station> stations;
    std::atomic_bool cancelled = false;
    std::atomic_bool running = false;
    std::thread worker;
    std::mutex pendingMutex;
    PendingKind pendingKind = PendingKind::None;
    live_stations::SearchResult pendingSearch;
    live_stations::ResolveResult pendingResolve;
    std::wstring pendingStationName;
};

INT_PTR gTreeItemId = 0;
ATOM gViewClass = 0;

int __cdecl PluginInit();
void __cdecl PluginQuit();
INT_PTR __cdecl PluginMessageProc(int messageType, INT_PTR param1, INT_PTR param2, INT_PTR param3);
LRESULT CALLBACK ViewWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

winampMediaLibraryPlugin gPlugin = {
    MLHDR_VER,
    reinterpret_cast<const char*>(kPluginDescription),
    PluginInit,
    PluginQuit,
    PluginMessageProc,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};

void SetControlFont(HWND control) {
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
}

void SetStatus(ViewState* state, const std::wstring& text) {
    SetWindowTextW(state->status, text.c_str());
}

void SetWorking(ViewState* state, bool working) {
    EnableWindow(state->searchButton, working ? FALSE : TRUE);
    EnableWindow(state->playButton, working ? FALSE : TRUE);
    EnableWindow(state->modeCombo, working ? FALSE : TRUE);
    EnableWindow(state->searchEdit, working ? FALSE : TRUE);
}

void JoinWorker(ViewState* state) {
    if (state->worker.joinable()) {
        state->worker.join();
    }
    state->running.store(false);
}

void PopulateList(ViewState* state, live_stations::SearchResult result) {
    ListView_DeleteAllItems(state->stationList);
    state->stations = std::move(result.stations);
    for (size_t index = 0; index < state->stations.size(); ++index) {
        const auto& station = state->stations[index];
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = static_cast<int>(index);
        item.pszText = const_cast<wchar_t*>(station.name.c_str());
        const int row = ListView_InsertItem(state->stationList, &item);
        ListView_SetItemText(
            state->stationList, row, 1, const_cast<wchar_t*>(station.codec.c_str()));
        ListView_SetItemText(
            state->stationList, row, 2, const_cast<wchar_t*>(station.bitrate.c_str()));
        ListView_SetItemText(
            state->stationList, row, 3, const_cast<wchar_t*>(station.country.c_str()));
        ListView_SetItemText(
            state->stationList, row, 4, const_cast<wchar_t*>(station.tags.c_str()));
    }

    std::wstring status = std::to_wstring(state->stations.size());
    status += state->stations.size() == 1U ? L" station" : L" stations";
    if (!result.mirror.empty()) {
        status += L" from " + result.mirror;
    }
    status += L". Double-click one to play it.";
    SetStatus(state, status);
}

void StartSearch(ViewState* state) {
    if (state->running.exchange(true)) {
        return;
    }
    JoinWorker(state);
    state->running.store(true);
    state->cancelled.store(false);
    SetWorking(state, true);
    SetStatus(state, L"Loading current stations...");

    wchar_t searchText[256]{};
    GetWindowTextW(state->searchEdit, searchText, static_cast<int>(std::size(searchText)));
    live_stations::SearchRequest request;
    request.text = searchText;
    request.video = ComboBox_GetCurSel(state->modeCombo) == 1;

    try {
        state->worker = std::thread([state, request = std::move(request)]() {
            live_stations::SearchResult result = live_stations::Search(request, state->cancelled);
            {
                std::lock_guard lock(state->pendingMutex);
                state->pendingKind = PendingKind::Search;
                state->pendingSearch = std::move(result);
            }
            PostMessageW(state->window, kWorkComplete, 0, 0);
        });
    } catch (...) {
        state->running.store(false);
        SetWorking(state, false);
        SetStatus(state, L"Could not start the station search worker.");
    }
}

void EnqueueAndPlay(const std::wstring& url) {
    COPYDATASTRUCT copyData{};
    copyData.dwData = static_cast<ULONG_PTR>(IPC_PLAYFILEW);
    copyData.cbData = static_cast<DWORD>((url.size() + 1U) * sizeof(wchar_t));
    copyData.lpData = const_cast<wchar_t*>(url.c_str());
    SendMessageW(gPlugin.hwndWinampParent, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&copyData));

    const LRESULT length = SendMessageW(gPlugin.hwndWinampParent, WM_WA_IPC, 0, IPC_GETLISTLENGTH);
    if (length > 0) {
        SendMessageW(gPlugin.hwndWinampParent, WM_WA_IPC, length - 1, IPC_SETPLAYLISTPOS);
        SendMessageW(gPlugin.hwndWinampParent, WM_COMMAND, kWinampPlayCommand, 0);
    }
}

void StartPlay(ViewState* state) {
    if (state->running.exchange(true)) {
        return;
    }
    const int selected = ListView_GetNextItem(state->stationList, -1, LVNI_SELECTED);
    if (selected < 0 || static_cast<size_t>(selected) >= state->stations.size()) {
        state->running.store(false);
        SetStatus(state, L"Select a station first.");
        return;
    }

    JoinWorker(state);
    state->running.store(true);
    state->cancelled.store(false);
    SetWorking(state, true);
    const live_stations::Station station = state->stations[static_cast<size_t>(selected)];
    SetStatus(state, L"Resolving " + station.name + L"...");

    try {
        state->worker = std::thread([state, station]() {
            live_stations::ResolveResult result =
                live_stations::ResolveAndCountClick(station, state->cancelled);
            {
                std::lock_guard lock(state->pendingMutex);
                state->pendingKind = PendingKind::Play;
                state->pendingResolve = std::move(result);
                state->pendingStationName = station.name;
            }
            PostMessageW(state->window, kWorkComplete, 0, 0);
        });
    } catch (...) {
        state->running.store(false);
        SetWorking(state, false);
        SetStatus(state, L"Could not start the playback worker.");
    }
}

void HandleWorkerCompletion(ViewState* state) {
    JoinWorker(state);
    SetWorking(state, false);

    PendingKind kind = PendingKind::None;
    live_stations::SearchResult search;
    live_stations::ResolveResult resolve;
    std::wstring stationName;
    {
        std::lock_guard lock(state->pendingMutex);
        kind = state->pendingKind;
        state->pendingKind = PendingKind::None;
        search = std::move(state->pendingSearch);
        resolve = std::move(state->pendingResolve);
        stationName = std::move(state->pendingStationName);
    }

    if (kind == PendingKind::Search) {
        if (!search.error.empty()) {
            ListView_DeleteAllItems(state->stationList);
            state->stations.clear();
            SetStatus(state, search.error);
        } else {
            PopulateList(state, std::move(search));
        }
    } else if (kind == PendingKind::Play) {
        if (resolve.url.empty()) {
            SetStatus(state, resolve.error.empty() ? L"No playable URL was returned." : resolve.error);
        } else {
            EnqueueAndPlay(resolve.url);
            std::wstring status = L"Sent " + stationName + L" to Winamp.";
            if (!resolve.error.empty()) {
                status += L" " + resolve.error;
            }
            SetStatus(state, status);
        }
    }
}

void AddColumn(HWND list, int index, int width, const wchar_t* label) {
    LVCOLUMNW column{};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    column.iSubItem = index;
    column.cx = width;
    column.pszText = const_cast<wchar_t*>(label);
    ListView_InsertColumn(list, index, &column);
}

void LayoutControls(ViewState* state, int width, int height) {
    constexpr int margin = 10;
    constexpr int rowHeight = 24;
    constexpr int gap = 6;
    constexpr int modeWidth = 190;
    constexpr int searchButtonWidth = 76;
    constexpr int playButtonWidth = 90;
    constexpr int statusHeight = 36;

    const int searchWidth = (std::max)(80, width - (2 * margin) - modeWidth - searchButtonWidth - (2 * gap));
    MoveWindow(state->searchEdit, margin, margin, searchWidth, rowHeight, TRUE);
    MoveWindow(state->modeCombo, margin + searchWidth + gap, margin, modeWidth, 200, TRUE);
    MoveWindow(
        state->searchButton,
        margin + searchWidth + gap + modeWidth + gap,
        margin,
        searchButtonWidth,
        rowHeight,
        TRUE);

    const int listTop = margin + rowHeight + gap;
    const int footerTop = (std::max)(listTop + 40, height - margin - statusHeight);
    MoveWindow(
        state->stationList,
        margin,
        listTop,
        (std::max)(80, width - (2 * margin)),
        (std::max)(40, footerTop - listTop - gap),
        TRUE);
    MoveWindow(state->playButton, margin, footerTop, playButtonWidth, rowHeight, TRUE);
    MoveWindow(
        state->status,
        margin + playButtonWidth + gap,
        footerTop,
        (std::max)(80, width - margin - (margin + playButtonWidth + gap)),
        statusHeight,
        TRUE);
}

LRESULT OnCreate(HWND window, CREATESTRUCTW* create) {
    auto* state = static_cast<ViewState*>(create->lpCreateParams);
    state->window = window;
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

    state->searchEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        WC_EDITW,
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        0,
        0,
        0,
        0,
        window,
        reinterpret_cast<HMENU>(kSearchEditId),
        gPlugin.hDllInstance,
        nullptr);
    state->modeCombo = CreateWindowExW(
        0,
        WC_COMBOBOXW,
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
        0,
        0,
        0,
        0,
        window,
        reinterpret_cast<HMENU>(kModeComboId),
        gPlugin.hDllInstance,
        nullptr);
    ComboBox_AddString(state->modeCombo, L"Radio stations");
    ComboBox_AddString(state->modeCombo, L"Video (experimental)");
    ComboBox_SetCurSel(state->modeCombo, 0);

    state->searchButton = CreateWindowExW(
        0,
        WC_BUTTONW,
        L"Search",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        0,
        0,
        0,
        0,
        window,
        reinterpret_cast<HMENU>(kSearchButtonId),
        gPlugin.hDllInstance,
        nullptr);
    state->stationList = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        WC_LISTVIEWW,
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        0,
        0,
        0,
        0,
        window,
        reinterpret_cast<HMENU>(kStationListId),
        gPlugin.hDllInstance,
        nullptr);
    state->playButton = CreateWindowExW(
        0,
        WC_BUTTONW,
        L"Play",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        0,
        0,
        0,
        0,
        window,
        reinterpret_cast<HMENU>(kPlayButtonId),
        gPlugin.hDllInstance,
        nullptr);
    state->status = CreateWindowExW(
        0,
        WC_STATICW,
        L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        0,
        0,
        0,
        0,
        window,
        nullptr,
        gPlugin.hDllInstance,
        nullptr);

    const std::vector<HWND> controls = {
        state->searchEdit,
        state->modeCombo,
        state->searchButton,
        state->stationList,
        state->playButton,
        state->status,
    };
    for (HWND control : controls) {
        SetControlFont(control);
    }

    ListView_SetExtendedListViewStyle(
        state->stationList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
    AddColumn(state->stationList, 0, 260, L"Station");
    AddColumn(state->stationList, 1, 90, L"Codec");
    AddColumn(state->stationList, 2, 70, L"kbps");
    AddColumn(state->stationList, 3, 120, L"Country");
    AddColumn(state->stationList, 4, 300, L"Tags");
    StartSearch(state);
    return 0;
}

LRESULT CALLBACK ViewWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<ViewState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    switch (message) {
        case WM_CREATE:
            return OnCreate(window, reinterpret_cast<CREATESTRUCTW*>(lParam));
        case WM_SIZE:
            if (state != nullptr) {
                LayoutControls(state, LOWORD(lParam), HIWORD(lParam));
            }
            return 0;
        case WM_COMMAND:
            if (state != nullptr && HIWORD(wParam) == BN_CLICKED) {
                if (LOWORD(wParam) == kSearchButtonId) {
                    StartSearch(state);
                    return 0;
                }
                if (LOWORD(wParam) == kPlayButtonId) {
                    StartPlay(state);
                    return 0;
                }
            }
            if (state != nullptr && LOWORD(wParam) == kModeComboId &&
                HIWORD(wParam) == CBN_SELCHANGE) {
                StartSearch(state);
                return 0;
            }
            break;
        case WM_NOTIFY:
            if (state != nullptr) {
                const auto* notification = reinterpret_cast<NMHDR*>(lParam);
                if (notification->idFrom == kStationListId &&
                    (notification->code == NM_DBLCLK || notification->code == LVN_ITEMACTIVATE)) {
                    StartPlay(state);
                    return 0;
                }
            }
            break;
        case kWorkComplete:
            if (state != nullptr) {
                HandleWorkerCompletion(state);
            }
            return 0;
        case WM_DESTROY:
            if (state != nullptr) {
                state->cancelled.store(true);
                JoinWorker(state);
            }
            return 0;
        case WM_NCDESTROY:
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            delete state;
            return 0;
        default:
            break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

int __cdecl PluginInit() {
    InitCommonControls();
    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&controls);

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = ViewWindowProc;
    windowClass.hInstance = gPlugin.hDllInstance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = kViewClassName;
    gViewClass = RegisterClassExW(&windowClass);
    if (gViewClass == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return 3;
    }

    mlAddTreeItemStruct item{};
    item.parentId = 0;
    item.title = const_cast<char*>("Live Stations");
    item.hasChildren = 0;
    SendMessageW(
        gPlugin.hwndLibraryParent,
        WM_ML_IPC,
        reinterpret_cast<WPARAM>(&item),
        ML_IPC_ADDTREEITEM);
    gTreeItemId = item.thisId;
    return gTreeItemId == 0 ? 4 : 0;
}

void __cdecl PluginQuit() {
    if (gTreeItemId != 0 && gPlugin.hwndLibraryParent != nullptr) {
        SendMessageW(gPlugin.hwndLibraryParent, WM_ML_IPC, gTreeItemId, ML_IPC_DELTREEITEM);
        gTreeItemId = 0;
    }
    if (gViewClass != 0) {
        UnregisterClassW(kViewClassName, gPlugin.hDllInstance);
        gViewClass = 0;
    }
}

INT_PTR __cdecl PluginMessageProc(
    int messageType,
    INT_PTR param1,
    INT_PTR param2,
    INT_PTR /*param3*/) {
    if (messageType != ML_MSG_TREE_ONCREATEVIEW || param1 != gTreeItemId || param2 == 0) {
        return 0;
    }
    const HWND parent = reinterpret_cast<HWND>(param2);
    RECT bounds{};
    GetClientRect(parent, &bounds);
    auto state = std::make_unique<ViewState>();
    const HWND view = CreateWindowExW(
        0,
        kViewClassName,
        L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0,
        0,
        bounds.right - bounds.left,
        bounds.bottom - bounds.top,
        parent,
        nullptr,
        gPlugin.hDllInstance,
        state.get());
    if (view == nullptr) {
        return 0;
    }
    state.release();
    return reinterpret_cast<INT_PTR>(view);
}

}  // namespace

extern "C" __declspec(dllexport) winampMediaLibraryPlugin* __cdecl
winampGetMediaLibraryPlugin() {
    return &gPlugin;
}
