#include "radio_browser.h"
#include "favorites.h"
#include "resolver_contract.h"
#include "winamp_media_library_abi.h"

#include <commctrl.h>
#include <windowsx.h>
#include <uxtheme.h>

#pragma comment(lib, "uxtheme.lib")

#include <algorithm>
#include <atomic>
#include <cwchar>
#include <cwctype>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr wchar_t kViewClassName[] = L"WinampToolsLiveStationsView";
#if defined(LIVE_STATIONS_OFFLINE_FIXTURE)
constexpr char kPluginDescription[] =
    "Winamp Tools Live Stations OFFLINE FIXTURE (NOT FOR INSTALL)";
#else
constexpr char kPluginDescription[] = "Winamp Tools Live Stations v0.1.0 alpha";
#endif
constexpr wchar_t kTreeTitle[] = L"Live Stations";
constexpr UINT kWorkComplete = WM_APP + 41;
constexpr UINT kStartDeferredSearch = WM_APP + 42;
constexpr UINT kRelayout = WM_APP + 43;
constexpr UINT kRefreshLocationPane = WM_APP + 44;
constexpr int kSearchEditId = 1001;
constexpr int kSearchButtonId = 1002;
constexpr int kModeComboId = 1003;
constexpr int kStationListId = 1004;
constexpr int kTitleId = 1006;
constexpr int kDirectoryLabelId = 1007;
constexpr int kFilterLabelId = 1008;
constexpr int kTunerLabelId = 1009;
constexpr int kSelectionNameId = 1010;
constexpr int kSelectionDetailsId = 1011;
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
constexpr int kLocationListId = 1023;
constexpr int kGenreListId = 1024;
constexpr int kLocationPaneLabelId = 1025;
constexpr int kGenrePaneLabelId = 1026;
constexpr int kProviderTotalId = 1027;
constexpr int kPageSizeComboId = 1028;
constexpr int kPreviousPageButtonId = 1029;
constexpr int kNextPageButtonId = 1030;
constexpr int kStatusId = 1031;
constexpr int kFilterToggleButtonId = 1032;
constexpr int kCompatibilityComboId = 1033;
constexpr int kFavoritesViewButtonId = 1034;
constexpr int kAddFavoriteCommandId = 1040;
constexpr int kAddWinampPlaylistCommandId = 1041;
constexpr int kBodyViewportId = 1042;
constexpr int kToolbarViewportId = 1043;
constexpr int kWinampPlayCommand = 40045;
constexpr UINT_PTR kNowPlayingTimerId = 1;
constexpr UINT kNowPlayingPollMilliseconds = 1500;

enum class PendingKind {
    None,
    Search,
    Facets,
    FacetPage,
    Play,
};

enum class ViewMode {
    Stations,
    Browse,
    Favorites,
};

enum class StationSortColumn : int {
    None = -1,
    Station = 0,
    Codec = 1,
    Bitrate = 2,
    Country = 3,
    Compatibility = 4,
    Tags = 5,
};

enum class PendingFacetTarget {
    None,
    Genres,
    Countries,
    States,
};

struct ViewState {
    HWND window = nullptr;
    HWND body = nullptr;
    HWND toolbar = nullptr;
    HWND title = nullptr;
    HWND directoryLabel = nullptr;
    HWND filterLabel = nullptr;
    HWND browseLabel = nullptr;
    HWND searchEdit = nullptr;
    HWND searchButton = nullptr;
    HWND resetButton = nullptr;
    HWND filterToggleButton = nullptr;
    HWND stationsViewButton = nullptr;
    HWND browseViewButton = nullptr;
    HWND favoritesViewButton = nullptr;
    HWND tunerToggleButton = nullptr;
    HWND codecCombo = nullptr;
    HWND bitrateCombo = nullptr;
    HWND compatibilityCombo = nullptr;
    HWND genreCombo = nullptr;
    HWND countryCombo = nullptr;
    HWND languageCombo = nullptr;
    HWND locationPaneLabel = nullptr;
    HWND genrePaneLabel = nullptr;
    HWND providerTotal = nullptr;
    HWND locationList = nullptr;
    HWND genreList = nullptr;
    HWND pageSizeCombo = nullptr;
    HWND previousPageButton = nullptr;
    HWND nextPageButton = nullptr;
    HWND stationList = nullptr;
    HWND resultCount = nullptr;
    HWND tunerLabel = nullptr;
    HWND selectionName = nullptr;
    HWND selectionDetails = nullptr;
    HWND nowPlaying = nullptr;
    HWND status = nullptr;
    HBRUSH windowBrush = nullptr;
    HBRUSH itemBrush = nullptr;
    HFONT controlFont = nullptr;
    COLORREF windowForeground = GetSysColor(COLOR_WINDOWTEXT);
    COLORREF windowBackground = GetSysColor(COLOR_WINDOW);
    COLORREF itemForeground = GetSysColor(COLOR_WINDOWTEXT);
    COLORREF itemBackground = GetSysColor(COLOR_WINDOW);
    COLORREF dividerColor = GetSysColor(COLOR_3DSHADOW);
    bool facetsLoaded = false;
    bool facetsLoading = true;
    bool tunerCollapsed = false;
    bool filterStripCollapsed = false;
    bool layingOut = false;
    bool layoutQueued = false;
    bool refreshingSkin = false;
    bool destroying = false;
    int scrollX = 0;
    int scrollY = 0;
    int toolbarScrollX = 0;
    int toolbarScrollY = 0;
    int layoutDpi = 0;
    int laidOutScrollX = 0;
    int laidOutScrollY = 0;
    int categorySplitPermille = 500;
    int categoryPaneHeight = 0;
    int detailsPaneWidth = 0;
    int detailsPaneHeight = 0;
    int locationSizedWidth = -1;
    int genreSizedWidth = -1;
    RECT categoryDivider{};
    RECT categoryBottomDivider{};
    RECT detailsDivider{};
    int draggingDivider = 0;
    POINT dragStart{};
    int dragStartExtent = 0;
    bool pairedCategories = true;
    bool sideDetails = true;
    ViewMode radioViewMode = ViewMode::Browse;
    ViewMode viewMode = ViewMode::Browse;
    bool locationDrilled = false;
    bool updatingBrowseLists = false;
    bool genresHaveMore = false;
    bool countriesHaveMore = false;
    bool statesHaveMore = false;
    std::uint32_t genreOffset = 0;
    std::uint32_t countryOffset = 0;
    std::uint32_t stateOffset = 0;
    std::uint32_t stationOffset = 0;
    std::uint32_t stationPageSize = 200;
    bool stationsHaveMore = false;
    StationSortColumn stationSortColumn = StationSortColumn::None;
    bool stationSortDescending = false;
    bool resultsFiltered = false;
    bool pendingSearchFiltered = false;
    std::uint64_t requestedSearchGeneration = 0;
    std::uint64_t pendingSearchGeneration = 0;
    ViewMode pendingSearchViewMode = ViewMode::Browse;
    bool searchQueued = false;
    int trackedPlaylistPosition = -1;
    std::wstring trackedStreamUrl;
    std::wstring trackedStationName;
    std::wstring suppressedPreviousTitle;
    unsigned int inactivePlaybackPolls = 0;
    bool trackedPlaybackObserved = false;
    std::vector<std::wstring> genres;
    std::vector<std::wstring> countries;
    std::vector<std::wstring> languages;
    std::vector<live_stations::BrowseFacet> genreFacets;
    std::vector<live_stations::BrowseFacet> countryFacets;
    std::vector<live_stations::BrowseFacet> stateFacets;
    live_stations::ProviderStats providerStats;
    std::wstring browseGenre;
    std::wstring browseCountry;
    std::wstring browseCountryCode;
    std::wstring browseState;
    std::vector<live_stations::Station> stations;
    live_stations::FavoritesModel favorites;
    std::wstring favoritesStorageWarning;
    std::atomic_bool cancelled = false;
    std::atomic_bool running = false;
    std::thread worker;
    std::mutex pendingMutex;
    PendingKind pendingKind = PendingKind::None;
    live_stations::SearchResult pendingSearch;
    live_stations::BrowseFacets pendingFacets;
    live_stations::BrowseFacetPage pendingFacetPage;
    PendingFacetTarget pendingFacetTarget = PendingFacetTarget::None;
    live_stations::ResolveResult pendingResolve;
    std::wstring pendingStationName;
};

// Ownership transfers from PluginMessageProc to the window only after
// WM_NCCREATE has accepted this context. That keeps failed CreateWindowExW
// calls owned by the caller while allowing WM_NCDESTROY to own failures that
// occur later in WM_CREATE.
struct ViewCreationContext {
    ViewState* state = nullptr;
    bool ownershipTransferred = false;
};

INT_PTR gTreeItemId = 0;
ATOM gViewClass = 0;
std::atomic_uint gActiveViewCount = 0;
std::vector<HWND> gActiveViewWindows;
bool gPluginQuitting = false;

const live_stations::ProviderOperations& ActiveProviderOperations() {
#if defined(LIVE_STATIONS_OFFLINE_FIXTURE)
    return live_stations::OfflineFixtureProviderOperations();
#else
    return live_stations::RadioBrowserProviderOperations();
#endif
}

void TraceViewLifecycle(const wchar_t* event, unsigned int activeViews) {
    std::wstring text = L"Winamp Tools Live Stations: ";
    text += event;
    text += L" (active views: ";
    text += std::to_wstring(activeViews);
    text += L").\n";
    OutputDebugStringW(text.c_str());
}

int __cdecl PluginInit();
void __cdecl PluginQuit();
INT_PTR __cdecl PluginMessageProc(int messageType, INT_PTR param1, INT_PTR param2, INT_PTR param3);
LRESULT CALLBACK ViewWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK SearchEditSubclassProc(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    UINT_PTR subclassId,
    DWORD_PTR referenceData);
void LayoutControls(ViewState* state, int width, int height);
LRESULT CALLBACK ViewportSubclassProc(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam,
    UINT_PTR subclassId, DWORD_PTR reference);

winampMediaLibraryPlugin gPlugin = {
    MLHDR_VER,
    kPluginDescription,
    PluginInit,
    PluginQuit,
    PluginMessageProc,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};

void ReplaceBrush(HBRUSH& brush, COLORREF color) {
    if (brush != nullptr) {
        DeleteObject(brush);
    }
    brush = CreateSolidBrush(color);
}

void SetControlFont(HWND control, HFONT font) {
    if (control != nullptr) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
}

void ApplyControlFonts(ViewState* state) {
    NONCLIENTMETRICSW metrics{sizeof(metrics)};
    HFONT font = nullptr;
    if (SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0,
                                  GetDpiForWindow(state->window))) {
        font = CreateFontIndirectW(&metrics.lfMessageFont);
    }
    HFONT oldFont = state->controlFont;
    state->controlFont = font;
    if (font == nullptr) font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    const std::vector<HWND> controls = {
        state->title,
        state->directoryLabel,
        state->filterLabel,
        state->browseLabel,
        state->searchEdit,
        state->searchButton,
        state->resetButton,
        state->filterToggleButton,
        state->stationsViewButton,
        state->browseViewButton,
        state->favoritesViewButton,
        state->tunerToggleButton,
        state->codecCombo,
        state->bitrateCombo,
        state->compatibilityCombo,
        state->genreCombo,
        state->countryCombo,
        state->languageCombo,
        state->locationPaneLabel,
        state->genrePaneLabel,
        state->providerTotal,
        state->locationList,
        state->genreList,
        state->pageSizeCombo,
        state->previousPageButton,
        state->nextPageButton,
        state->stationList,
        state->resultCount,
        state->tunerLabel,
        state->selectionName,
        state->selectionDetails,
        state->nowPlaying,
        state->status,
    };
    for (HWND control : controls) {
        SetControlFont(control, font);
    }
    for (HWND list : {state->stationList, state->locationList, state->genreList}) {
        if (list != nullptr) SetControlFont(ListView_GetToolTips(list), font);
    }
    if (oldFont != nullptr) DeleteObject(oldFont);
}

int MeasureControlTextWidth(
    HWND control,
    const wchar_t* text,
    int minimumWidth,
    int horizontalPadding) {
    int width = minimumWidth;
    const HDC dc = GetDC(control);
    if (dc == nullptr) {
        return width;
    }

    const HFONT font = reinterpret_cast<HFONT>(SendMessageW(control, WM_GETFONT, 0, 0));
    const HGDIOBJ previous = font != nullptr ? SelectObject(dc, font) : nullptr;
    SIZE textSize{};
    if (GetTextExtentPoint32W(dc, text, static_cast<int>(wcslen(text)), &textSize) != FALSE) {
        width = (std::max)(
            minimumWidth, static_cast<int>(textSize.cx) + horizontalPadding);
    }
    if (previous != nullptr) {
        SelectObject(dc, previous);
    }
    ReleaseDC(control, dc);
    return width;
}

void SizeBrowseColumns(ViewState* state, HWND list) {
    if (list == nullptr) {
        return;
    }
    RECT client{};
    if (GetClientRect(list, &client) == FALSE) {
        return;
    }
    const int clientWidth = (std::max)(0L, client.right - client.left);
    int& sizedWidth = list == state->locationList ? state->locationSizedWidth : state->genreSizedWidth;
    if (sizedWidth == clientWidth) return; // Preserve user header widths between actual resizes.
    sizedWidth = clientWidth;
    const int scrollbarReserve = GetSystemMetrics(SM_CXVSCROLL) + 6;
    const int usableWidth = (std::max)(80, clientWidth - scrollbarReserve);
    int countWidth = (std::max)(
        MeasureControlTextWidth(list, L"Stations", 76, 18),
        MeasureControlTextWidth(
            list,
            state->providerStats.stationTotal.empty()
                ? L"999999"
                : state->providerStats.stationTotal.c_str(),
            76,
            18));
    countWidth = (std::min)(countWidth, (std::max)(48, usableWidth / 3));
    countWidth = (std::max)(countWidth, (std::min)(
        ListView_GetColumnWidth(list, 1), (std::max)(48, usableWidth / 2)));
    ListView_SetColumnWidth(list, 0, (std::max)(40, usableWidth - countWidth));
    ListView_SetColumnWidth(list, 1, countWidth);
}

void UnskinControls(ViewState* state) {
    // Deliberately no Media Library skin-wrapper IPC. Returned wrapper handles
    // and function pointers have host-owned lifetimes that are not safe to
    // retain across real Winamp display changes or plug-in shutdown.
    (void)state;
}

bool IsLiveView(HWND window, const ViewState* state) {
    return !gPluginQuitting && IsWindow(window) &&
        GetWindowLongPtrW(window, GWLP_USERDATA) == reinterpret_cast<LONG_PTR>(state) &&
        !state->destroying;
}

void ApplyHoverLabelPalette(HWND list, COLORREF foreground, COLORREF background) {
    // This tooltip belongs to this list control, not to Winamp's skin wrapper.
    // Look it up afresh on each palette refresh; never retain or destroy it.
    const HWND tooltip = ListView_GetToolTips(list);
    if (tooltip == nullptr) return;
    // TTM_SETTIP*COLOR is ignored by themed tooltips. Remove visual styling
    // from this popup only so the native full-label preview uses our copied RGB
    // palette. Other Winamp windows/buttons/headers keep their existing theme.
    if (FAILED(SetWindowTheme(tooltip, L"", L""))) return;
    SendMessageW(tooltip, TTM_SETTIPTEXTCOLOR, foreground, 0);
    SendMessageW(tooltip, TTM_SETTIPBKCOLOR, background, 0);
    SendMessageW(tooltip, WM_SETFONT, SendMessageW(list, WM_GETFONT, 0, 0), FALSE);
    InvalidateRect(tooltip, nullptr, FALSE);
}

bool RefreshSkin(ViewState* state, bool reskinControls) {
    const HWND window = state->window;
    if (!IsLiveView(window, state) || state->refreshingSkin) return false;
    state->refreshingSkin = true;
    if (reskinControls) {
        UnskinControls(state);
    }
    COLORREF colors[] = {GetSysColor(COLOR_WINDOWTEXT), GetSysColor(COLOR_WINDOW),
        GetSysColor(COLOR_WINDOWTEXT), GetSysColor(COLOR_WINDOW), GetSysColor(COLOR_3DSHADOW)};
    constexpr int indices[] = {WADLG_WNDFG, WADLG_WNDBG, WADLG_ITEMFG, WADLG_ITEMBG, WADLG_HILITE};
    const HWND library = gPlugin.hwndLibraryParent;
    const auto providerIsLive = [&]() {
        return IsWindow(library) &&
            GetWindowThreadProcessId(library, nullptr) == GetCurrentThreadId();
    };
    if (providerIsLive()) {
        // Query only on the owning UI thread; copy RGB values immediately.
        // Never store the host function, font or a skin-wrapper handle.
        const LRESULT address = SendMessageW(
            library, WM_ML_IPC, ML_WADLG_GETCOLOR, ML_IPC_SKIN_WADLG_GETFUNC);
        if (!IsLiveView(window, state)) return false;
        MEMORY_BASIC_INFORMATION memory{};
        constexpr DWORD executable = PAGE_EXECUTE | PAGE_EXECUTE_READ |
                                     PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        if (providerIsLive() && static_cast<UINT_PTR>(address) > 0xFFFFU &&
            VirtualQuery(reinterpret_cast<const void*>(address), &memory, sizeof(memory)) &&
            memory.State == MEM_COMMIT && (memory.Protect & executable) != 0 &&
            (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0) {
            const auto getColor = reinterpret_cast<WADlgGetColorFunction>(address);
            COLORREF snapshot[std::size(indices)]{};
            bool valid = true;
            for (size_t i = 0; i < std::size(indices); ++i) {
                snapshot[i] = static_cast<COLORREF>(getColor(indices[i]));
                if (!IsLiveView(window, state)) return false;
                if (!providerIsLive() || (snapshot[i] & 0xFF000000U) != 0) {
                    valid = false;
                    break;
                }
            }
            if (valid) std::copy(std::begin(snapshot), std::end(snapshot), std::begin(colors));
        }
    }
    state->windowForeground = colors[0];
    state->windowBackground = colors[1];
    state->itemForeground = colors[2];
    state->itemBackground = colors[3];
    state->dividerColor = colors[4];
    ReplaceBrush(state->windowBrush, state->windowBackground);
    ReplaceBrush(state->itemBrush, state->itemBackground);

    ApplyControlFonts(state);
    for (HWND list : {state->stationList, state->locationList, state->genreList}) {
        if (list != nullptr) {
            ListView_SetTextColor(list, state->itemForeground);
            ListView_SetTextBkColor(list, state->itemBackground);
            ListView_SetBkColor(list, state->itemBackground);
            ApplyHoverLabelPalette(list, state->itemForeground, state->itemBackground);
        }
    }
    // A genuine palette change affects every label/edit as well as the lists.
    // Request repaint, not parent background erasure or wrapper recreation.
    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_NOERASE);
    state->refreshingSkin = false;
    return true;
}

void SetStatus(ViewState* state, const std::wstring& text) {
    SetWindowTextW(state->status, text.c_str());
}

class ControlRedrawBatch {
public:
    explicit ControlRedrawBatch(HWND control) : control_(control) {
        // WM_SETREDRAW(TRUE) can resurrect WS_VISIBLE on a hidden category
        // list after switching views. Only suspend windows that are visible.
        if (control_ != nullptr &&
            (GetWindowLongPtrW(control_, GWL_STYLE) & WS_VISIBLE) == 0) control_ = nullptr;
        if (control_ != nullptr) {
            SendMessageW(control_, WM_SETREDRAW, FALSE, 0);
        }
    }

    ~ControlRedrawBatch() {
        if (control_ == nullptr) {
            return;
        }
        SendMessageW(control_, WM_SETREDRAW, TRUE, 0);
        // Publish the complete replacement in one paint. Erasing the list first
        // exposes the gray parent background between DeleteAllItems and the new
        // rows, which looks like the entire result box flashing on every action.
        RedrawWindow(
            control_,
            nullptr,
            nullptr,
            RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW | RDW_NOERASE);
    }

    ControlRedrawBatch(const ControlRedrawBatch&) = delete;
    ControlRedrawBatch& operator=(const ControlRedrawBatch&) = delete;

private:
    HWND control_ = nullptr;
};

void UpdateResultCount(ViewState* state) {
    const int count = state->stationList == nullptr
                          ? 0
                          : ListView_GetItemCount(state->stationList);
    std::wstring text = std::to_wstring(count);
    if (state->viewMode == ViewMode::Favorites) {
        text += count == 1 ? L" favorite shown" : L" favorites shown";
    } else if (state->resultsFiltered) {
        text += count == 1 ? L" matching station shown on this page"
                           : L" matching stations shown on this page";
    } else {
        text += count == 1 ? L" station shown" : L" stations shown";
    }
    if (state->stationOffset > 0U || state->stationsHaveMore) {
        const std::uint32_t first = count == 0 ? state->stationOffset : state->stationOffset + 1U;
        const std::uint32_t last = state->stationOffset + static_cast<std::uint32_t>(count);
        text += L"  /  rows " + std::to_wstring(first) + L"-" + std::to_wstring(last);
    }
    SetWindowTextW(state->resultCount, text.c_str());
    if (!state->running.load()) {
        const bool hasRows = count > 0;
        SetWindowTextW(
            state->previousPageButton,
            hasRows && state->stationOffset == 0U ? L"First page" : L"Previous");
        SetWindowTextW(
            state->nextPageButton,
            hasRows && !state->stationsHaveMore ? L"Last page" : L"Next");
        // Some classic skins make disabled buttons effectively unreadable. Keep
        // the pager reachable and make its boundary state explicit instead.
        EnableWindow(state->previousPageButton, TRUE);
        EnableWindow(state->nextPageButton, TRUE);
    }
}

bool EqualTextIgnoreCase(const std::wstring& left, const std::wstring& right) {
    return !left.empty() && !right.empty() &&
           CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_EQUAL;
}

std::wstring CopyWinampText(LRESULT result, size_t maximumLength) {
    if (result == 0 || result == 1) {
        return {};
    }
    const auto* value = reinterpret_cast<const wchar_t*>(result);
    const size_t length = wcsnlen_s(value, maximumLength);
    return length < maximumLength ? std::wstring(value, length) : std::wstring{};
}

std::wstring SanitizeNowPlayingTitle(const std::wstring& value) {
    std::wstring sanitized;
    sanitized.reserve((std::min)(value.size(), static_cast<size_t>(256)));
    bool pendingSpace = false;
    for (wchar_t character : value) {
        if (sanitized.size() >= 256U) {
            break;
        }
        if (std::iswspace(character) != 0 || std::iswcntrl(character) != 0) {
            pendingSpace = !sanitized.empty();
            continue;
        }
        if (pendingSpace) {
            sanitized.push_back(L' ');
            pendingSpace = false;
        }
        sanitized.push_back(character);
    }
    return sanitized;
}

void StopNowPlayingTracking(ViewState* state, bool clearText) {
    if (state->window != nullptr) {
        KillTimer(state->window, kNowPlayingTimerId);
    }
    state->trackedPlaylistPosition = -1;
    state->trackedStreamUrl.clear();
    state->trackedStationName.clear();
    state->suppressedPreviousTitle.clear();
    state->inactivePlaybackPolls = 0;
    state->trackedPlaybackObserved = false;
    if (clearText && state->nowPlaying != nullptr) {
        SetWindowTextW(state->nowPlaying, L"");
    }
}

void RefreshNowPlaying(ViewState* state) {
    if (state->trackedPlaylistPosition < 0 || gPlugin.hwndWinampParent == nullptr) {
        return;
    }

    const LRESULT currentPosition =
        SendMessageW(gPlugin.hwndWinampParent, WM_WA_IPC, 0, IPC_GETLISTPOS);
    if (currentPosition != state->trackedPlaylistPosition) {
        StopNowPlayingTracking(state, true);
        return;
    }

    const std::wstring currentFile = CopyWinampText(
        SendMessageW(
            gPlugin.hwndWinampParent,
            WM_WA_IPC,
            static_cast<WPARAM>(state->trackedPlaylistPosition),
            IPC_GETPLAYLISTFILEW),
        8192U);
    if (currentFile.empty() || currentFile != state->trackedStreamUrl) {
        StopNowPlayingTracking(state, true);
        return;
    }

    const LRESULT playbackState =
        SendMessageW(gPlugin.hwndWinampParent, WM_WA_IPC, 0, IPC_ISPLAYING);
    if (playbackState != 1 && playbackState != 3) {
        ++state->inactivePlaybackPolls;
        if (state->trackedPlaybackObserved || state->inactivePlaybackPolls >= 10U) {
            StopNowPlayingTracking(state, true);
        } else {
            SetWindowTextW(state->nowPlaying, L"");
        }
        return;
    }
    state->trackedPlaybackObserved = true;
    state->inactivePlaybackPolls = 0;

    std::wstring title = SanitizeNowPlayingTitle(CopyWinampText(
        SendMessageW(gPlugin.hwndWinampParent, WM_WA_IPC, 0, IPC_GET_PLAYING_TITLE),
        1024U));
    if (title.empty() ||
        EqualTextIgnoreCase(title, state->suppressedPreviousTitle) ||
        EqualTextIgnoreCase(title, state->trackedStreamUrl) ||
        EqualTextIgnoreCase(title, state->trackedStationName)) {
        SetWindowTextW(
            state->nowPlaying,
            L"Now playing\r\nTrack information not provided by this stream.");
        return;
    }
    state->suppressedPreviousTitle.clear();
    title = L"Now playing\r\n" + title;
    SetWindowTextW(state->nowPlaying, title.c_str());
}

void StartNowPlayingTracking(
    ViewState* state,
    int playlistPosition,
    const std::wstring& streamUrl,
    const std::wstring& stationName,
    const std::wstring& previousTitle) {
    StopNowPlayingTracking(state, true);
    if (playlistPosition < 0 || streamUrl.empty()) {
        return;
    }
    state->trackedPlaylistPosition = playlistPosition;
    state->trackedStreamUrl = streamUrl;
    state->trackedStationName = stationName;
    state->suppressedPreviousTitle = previousTitle;
    if (SetTimer(
            state->window,
            kNowPlayingTimerId,
            kNowPlayingPollMilliseconds,
            nullptr) == 0) {
        StopNowPlayingTracking(state, true);
        return;
    }
    RefreshNowPlaying(state);
}

void SetWorking(ViewState* /*state*/, bool /*working*/) {
    // A classic Media Library skin repaints every disabled child in the control
    // strip with its gray state. Disabling the whole strip for each async
    // request therefore looks like the entire view flashing. Requests are
    // already generation-bound and cancellable, so keep controls interactive;
    // the status line communicates loading while the current page stays drawn.
}

std::wstring Uppercase(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t character) {
        return static_cast<wchar_t>(std::towupper(character));
    });
    return value;
}

const wchar_t* CompatibilityLabel(const live_stations::Station& station) {
    const std::wstring codec = Uppercase(station.codec);
    if (codec == L"MP3") {
        return L"Best";
    }
    if (codec == L"AAC" || codec == L"AAC+") {
        return L"Usually";
    }
    if (codec == L"OGG" || codec == L"OPUS" || codec == L"FLAC") {
        return L"Decoder needed";
    }
    return L"Unknown";
}

std::wstring BitrateDisplay(const live_stations::Station& station) {
    if (station.bitrate.empty()) {
        return L"Not reported";
    }
    if (station.bitrate == L"0") {
        return L"Unknown (0)";
    }
    return station.bitrate;
}

const wchar_t* StationSortColumnLabel(StationSortColumn column) {
    switch (column) {
        case StationSortColumn::Station:
            return L"Station";
        case StationSortColumn::Codec:
            return L"Codec";
        case StationSortColumn::Bitrate:
            return L"kbps";
        case StationSortColumn::Country:
            return L"Country";
        case StationSortColumn::Compatibility:
            return L"Compatibility";
        case StationSortColumn::Tags:
            return L"Tags";
        case StationSortColumn::None:
            return L"";
    }
    return L"";
}

int CompareTextIgnoreCase(const std::wstring& left, const std::wstring& right) {
    const int comparison =
        CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE);
    if (comparison == CSTR_LESS_THAN) {
        return -1;
    }
    if (comparison == CSTR_GREATER_THAN) {
        return 1;
    }
    return 0;
}

bool TryGetReportedBitrate(const live_stations::Station& station, unsigned long& bitrate) {
    if (station.bitrate.empty() || station.bitrate == L"0") {
        return false;
    }
    wchar_t* end = nullptr;
    const unsigned long value = std::wcstoul(station.bitrate.c_str(), &end, 10);
    if (end == station.bitrate.c_str() || end == nullptr || *end != L'\0' || value == 0UL) {
        return false;
    }
    bitrate = value;
    return true;
}

int CompareStationsByColumn(
    const live_stations::Station& left,
    const live_stations::Station& right,
    StationSortColumn column) {
    switch (column) {
        case StationSortColumn::Station:
            return CompareTextIgnoreCase(left.name, right.name);
        case StationSortColumn::Codec:
            return CompareTextIgnoreCase(left.codec, right.codec);
        case StationSortColumn::Bitrate: {
            unsigned long leftBitrate = 0;
            unsigned long rightBitrate = 0;
            const bool leftReported = TryGetReportedBitrate(left, leftBitrate);
            const bool rightReported = TryGetReportedBitrate(right, rightBitrate);
            // Keep unknown/not-reported values explicit and at the end in both
            // numeric directions instead of pretending they are low bitrate.
            if (leftReported != rightReported) {
                return leftReported ? -1 : 1;
            }
            if (!leftReported || leftBitrate == rightBitrate) {
                return 0;
            }
            return leftBitrate < rightBitrate ? -1 : 1;
        }
        case StationSortColumn::Country:
            return CompareTextIgnoreCase(left.country, right.country);
        case StationSortColumn::Compatibility:
            return CompareTextIgnoreCase(CompatibilityLabel(left), CompatibilityLabel(right));
        case StationSortColumn::Tags:
            return CompareTextIgnoreCase(left.tags, right.tags);
        case StationSortColumn::None:
            return 0;
    }
    return 0;
}

void UpdateStationSortHeader(ViewState* state) {
    if (state == nullptr || state->stationList == nullptr) {
        return;
    }
    const HWND header = ListView_GetHeader(state->stationList);
    if (header == nullptr) {
        return;
    }
    for (int index = 0; index != 6; ++index) {
        HDITEMW item{};
        item.mask = HDI_FORMAT;
        if (Header_GetItem(header, index, &item) == FALSE) {
            continue;
        }
        item.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
        if (static_cast<int>(state->stationSortColumn) == index) {
            item.fmt |= state->stationSortDescending ? HDF_SORTDOWN : HDF_SORTUP;
        }
        Header_SetItem(header, index, &item);
    }
}

void SortStationsForDisplay(ViewState* state) {
    if (state == nullptr || state->stationSortColumn == StationSortColumn::None) {
        return;
    }
    const StationSortColumn column = state->stationSortColumn;
    const bool descending = state->stationSortDescending;
    std::stable_sort(
        state->stations.begin(),
        state->stations.end(),
        [column, descending](const live_stations::Station& left,
                             const live_stations::Station& right) {
            const int comparison = CompareStationsByColumn(left, right, column);
            if (comparison == 0) {
                return false;
            }
            return descending ? comparison > 0 : comparison < 0;
        });
}

std::wstring StreamHost(const std::wstring& url) {
    const size_t scheme = url.find(L"://");
    if (scheme == std::wstring::npos) {
        return {};
    }
    const size_t hostStart = scheme + 3U;
    const size_t hostEnd = url.find_first_of(L"/?#", hostStart);
    std::wstring authority = url.substr(hostStart, hostEnd - hostStart);
    const size_t userInfo = authority.rfind(L'@');
    if (userInfo != std::wstring::npos) {
        authority.erase(0, userInfo + 1U);
    }
    if (!authority.empty() && authority.front() == L'[') {
        const size_t close = authority.find(L']');
        return close == std::wstring::npos ? std::wstring{} : authority.substr(0, close + 1U);
    }
    const size_t port = authority.rfind(L':');
    if (port != std::wstring::npos) {
        authority.resize(port);
    }
    return authority;
}

void UpdateSelectionSummary(ViewState* state) {
    const int selected = ListView_GetNextItem(state->stationList, -1, LVNI_SELECTED);
    if (selected < 0 || static_cast<size_t>(selected) >= state->stations.size()) {
        SetWindowTextW(
            state->selectionName,
            state->viewMode == ViewMode::Favorites ? L"Live Stations Favorites"
                                                    : L"Choose a station");
        SetWindowTextW(
            state->selectionDetails,
            state->viewMode == ViewMode::Favorites
                ? (state->stations.empty()
                       ? L"No saved Favorites yet. Right-click a station and choose Add to Live Stations Favorites."
                       : L"Select a saved favorite to see its details. Double-click it or press Enter to play.")
                : L"Double-click a row or select one and press Enter.\r\n\r\nMP3 is the safest choice for classic Winamp.");
        return;
    }

    const live_stations::Station& station = state->stations[static_cast<size_t>(selected)];
    SetWindowTextW(state->selectionName, station.name.c_str());
    std::wstring details = station.codec.empty() ? L"Unknown codec" : station.codec;
    const std::wstring bitrate = BitrateDisplay(station);
    details += L"  /  ";
    details += bitrate;
    if (station.bitrate != L"0" && !station.bitrate.empty()) {
        details += L" kbps";
    }
    if (!station.country.empty()) {
        details += L"\r\n" + station.country;
    }
    if (!station.state.empty()) {
        details += L" / " + station.state;
    }
    if (!station.language.empty()) {
        details += L"\r\nLanguage: " + station.language;
    }
    details += L"\r\nCompatibility: ";
    details += CompatibilityLabel(station);
    if (!station.tags.empty()) {
        details += L"\r\nTags: " + station.tags;
    }
    details += L"\r\nDirectory: Radio Browser";
    const std::wstring streamHost = StreamHost(station.fallbackUrlAlias);
    if (!streamHost.empty()) {
        details += L"\r\nStream host: " + streamHost;
    }
    details += station.hls ? L"\r\nDelivery: HLS" : L"\r\nDelivery: Direct / resolved audio stream";
    if (!station.homepageUrl.empty()) {
        details += L"\r\nStation website: " + station.homepageUrl;
    }
    if (!station.lastSuccessfulCheck.empty()) {
        details += L"\r\nLast successful directory check: " + station.lastSuccessfulCheck;
    }
    if (state->favorites.FindStation(station.identity) != nullptr) {
        details += L"\r\nFavorite: Live Stations / Favorites";
    }
    SetWindowTextW(state->selectionDetails, details.c_str());
}

const live_stations::Station* SelectedStation(ViewState* state) {
    if (state == nullptr || state->stationList == nullptr) {
        return nullptr;
    }
    const int selected = ListView_GetNextItem(state->stationList, -1, LVNI_SELECTED);
    return selected < 0 || static_cast<size_t>(selected) >= state->stations.size()
               ? nullptr
               : &state->stations[static_cast<size_t>(selected)];
}

void AddSelectedStationToFavorites(ViewState* state) {
    const live_stations::Station* station = SelectedStation(state);
    if (station == nullptr || !station->identity.IsValid()) {
        SetStatus(state, L"Choose a station before adding it to Live Stations Favorites.");
        return;
    }
    const bool alreadyFavorite = state->favorites.FindStation(station->identity) != nullptr;
    if (!state->favorites.AddOrUpdateStation(
            {station->identity,
             live_stations::kFavoritesRootFolderId,
             station->name,
             station->fallbackUrlAlias})) {
        SetStatus(state, L"The selected station could not be added to Live Stations Favorites.");
        return;
    }

#if defined(LIVE_STATIONS_OFFLINE_FIXTURE)
    SetStatus(
        state,
        alreadyFavorite ? L"Updated the fixture favorite in this test session."
                        : L"Added the fixture station to Favorites for this test session.");
#else
    std::wstring storageError;
    if (!live_stations::SaveFavoritesToDefaultPath(state->favorites, &storageError)) {
        state->favoritesStorageWarning = storageError;
        SetStatus(
            state,
            L"Favorite updated for this view, but could not be saved: " + storageError);
    } else {
        state->favoritesStorageWarning.clear();
        SetStatus(
            state,
            alreadyFavorite ? L"Updated Live Stations Favorite."
                            : L"Added to Live Stations Favorites.");
    }
#endif
    UpdateSelectionSummary(state);
}

int EnqueueUrl(const std::wstring& url) {
    COPYDATASTRUCT copyData{};
    copyData.dwData = static_cast<ULONG_PTR>(IPC_PLAYFILEW);
    copyData.cbData = static_cast<DWORD>((url.size() + 1U) * sizeof(wchar_t));
    copyData.lpData = const_cast<wchar_t*>(url.c_str());
    const LRESULT accepted = SendMessageW(
        gPlugin.hwndWinampParent, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&copyData));
    if (accepted == FALSE) {
        return -1;
    }

    const LRESULT length = SendMessageW(gPlugin.hwndWinampParent, WM_WA_IPC, 0, IPC_GETLISTLENGTH);
    if (length <= 0) {
        return -1;
    }
    const int position = static_cast<int>(length - 1);
    const std::wstring queuedFile = CopyWinampText(
        SendMessageW(
            gPlugin.hwndWinampParent,
            WM_WA_IPC,
            static_cast<WPARAM>(position),
            IPC_GETPLAYLISTFILEW),
        8192U);
    return queuedFile == url ? position : -1;
}

void AddSelectedStationToWinampPlaylist(ViewState* state) {
#if defined(LIVE_STATIONS_UI_ONLY)
    (void)state;
#else
    const live_stations::Station* station = SelectedStation(state);
    if (station == nullptr || !station->identity.IsValid()) {
        SetStatus(state, L"Choose a station before adding it to Winamp's playlist.");
        return;
    }
    const std::wstring stableUrl =
        live_stations::resolver::BuildLoopbackUrl(station->identity);
    if (stableUrl.empty()) {
        SetStatus(state, L"This station provider cannot create a refreshable Winamp playlist link.");
        return;
    }
    if (EnqueueUrl(stableUrl) < 0) {
        SetStatus(state, L"Winamp did not confirm the refreshable station playlist entry.");
        return;
    }
    SetStatus(
        state,
        L"Added a refreshable station link to Winamp's playlist. The bundled Live Stations Resolver refreshes it when played.");
#endif
}

void ShowStationContextMenu(ViewState* state, LPARAM location) {
    if (state == nullptr || state->stationList == nullptr) {
        return;
    }
    POINT point{GET_X_LPARAM(location), GET_Y_LPARAM(location)};
    if (point.x == -1 && point.y == -1) {
        const int selected = ListView_GetNextItem(state->stationList, -1, LVNI_SELECTED);
        RECT itemBounds{};
        if (selected < 0 || ListView_GetItemRect(state->stationList, selected, &itemBounds, LVIR_BOUNDS) == FALSE) {
            return;
        }
        point.x = itemBounds.left;
        point.y = itemBounds.bottom;
        ClientToScreen(state->stationList, &point);
    } else {
        POINT clientPoint = point;
        ScreenToClient(state->stationList, &clientPoint);
        LVHITTESTINFO hit{};
        hit.pt = clientPoint;
        const int hitIndex = ListView_HitTest(state->stationList, &hit);
        if (hitIndex >= 0) {
            ListView_SetItemState(
                state->stationList,
                hitIndex,
                LVIS_SELECTED | LVIS_FOCUSED,
                LVIS_SELECTED | LVIS_FOCUSED);
        }
    }
    const live_stations::Station* station = SelectedStation(state);
    if (station == nullptr || !station->identity.IsValid()) {
        return;
    }
    HMENU menu = CreatePopupMenu();
    if (menu == nullptr) {
        return;
    }
    const bool alreadyFavorite = state->favorites.FindStation(station->identity) != nullptr;
    AppendMenuW(
        menu,
        MF_STRING,
        kAddFavoriteCommandId,
        alreadyFavorite ? L"Update Live Stations Favorite" : L"Add to Live Stations Favorites");
#if !defined(LIVE_STATIONS_UI_ONLY)
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(
        menu,
        MF_STRING,
        kAddWinampPlaylistCommandId,
        L"Add refreshable link to Winamp playlist");
#endif
    const UINT command = TrackPopupMenu(
        menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, point.x, point.y, 0, state->window, nullptr);
    DestroyMenu(menu);
    if (command == kAddFavoriteCommandId) {
        AddSelectedStationToFavorites(state);
    } else if (command == kAddWinampPlaylistCommandId) {
        AddSelectedStationToWinampPlaylist(state);
    }
}

constexpr LPARAM kBrowseRowAll = -1;
constexpr LPARAM kBrowseRowPrevious = -2;
constexpr LPARAM kBrowseRowNext = -3;
constexpr LPARAM kBrowseRowBack = -4;
constexpr LPARAM kBrowseRowCountryAll = -5;
constexpr LPARAM kBrowseRowLoading = -6;

int InsertBrowseRow(
    HWND list,
    int index,
    const std::wstring& name,
    const std::wstring& count,
    LPARAM data) {
    LVITEMW item{};
    item.mask = LVIF_TEXT | LVIF_PARAM;
    item.iItem = index;
    item.pszText = const_cast<wchar_t*>(name.c_str());
    item.lParam = data;
    const int row = ListView_InsertItem(list, &item);
    if (row >= 0 && !count.empty()) {
        ListView_SetItemText(list, row, 1, const_cast<wchar_t*>(count.c_str()));
    }
    return row;
}

void UpdateProviderTotal(ViewState* state) {
    std::wstring text = state->facetsLoading
                            ? L"Loading Radio Browser category totals..."
                            : L"Radio Browser provider total unavailable";
    if (!state->providerStats.stationTotal.empty()) {
        text = L"Radio Browser total: " + state->providerStats.stationTotal + L" stations";
    }
    SetWindowTextW(state->providerTotal, text.c_str());
    std::wstring directory = L"LIVE DIRECTORY  /  RADIO BROWSER";
    if (!state->providerStats.stationTotal.empty()) {
        directory = L"RADIO BROWSER  /  " + state->providerStats.stationTotal +
                    L" STATIONS TOTAL";
    }
    SetWindowTextW(state->directoryLabel, directory.c_str());
}

void PopulateGenrePane(ViewState* state) {
    state->updatingBrowseLists = true;
    ControlRedrawBatch redraw(state->genreList);
    ListView_DeleteAllItems(state->genreList);
    int row = InsertBrowseRow(
        state->genreList,
        0,
        L"All Genres",
        state->facetsLoading && state->providerStats.stationTotal.empty()
            ? L"Loading..."
            : state->providerStats.stationTotal,
        kBrowseRowAll);
    if (state->browseGenre.empty()) {
        ListView_SetItemState(
            state->genreList, row, LVIS_SELECTED, LVIS_SELECTED);
    }
    if (state->genreOffset > 0U) {
        InsertBrowseRow(
            state->genreList, ++row, L"Previous categories...", L"", kBrowseRowPrevious);
    }
    if (state->facetsLoading && state->genreFacets.empty()) {
        InsertBrowseRow(
            state->genreList, ++row, L"Loading genre counts...", L"", kBrowseRowLoading);
    }
    for (size_t index = 0; index < state->genreFacets.size(); ++index) {
        const auto& facet = state->genreFacets[index];
        const int facetRow = InsertBrowseRow(
            state->genreList,
            ++row,
            facet.name,
            facet.stationCount,
            static_cast<LPARAM>(index));
        if (EqualTextIgnoreCase(facet.name, state->browseGenre)) {
            ListView_SetItemState(
                state->genreList, facetRow, LVIS_SELECTED, LVIS_SELECTED);
        }
    }
    if (state->genresHaveMore) {
        InsertBrowseRow(
            state->genreList, ++row, L"Next categories...", L"", kBrowseRowNext);
    }
    state->updatingBrowseLists = false;
}

void PopulateLocationPane(ViewState* state) {
    state->updatingBrowseLists = true;
    ControlRedrawBatch redraw(state->locationList);
    ListView_DeleteAllItems(state->locationList);
    int row = InsertBrowseRow(
        state->locationList,
        0,
        L"All Locations",
        state->facetsLoading && state->providerStats.stationTotal.empty()
            ? L"Loading..."
            : state->providerStats.stationTotal,
        kBrowseRowAll);
    if (state->browseCountry.empty()) {
        ListView_SetItemState(
            state->locationList, row, LVIS_SELECTED, LVIS_SELECTED);
    }
    if (state->locationDrilled) {
        InsertBrowseRow(
            state->locationList, ++row, L"Back to countries...", L"", kBrowseRowBack);
        const int countryRow = InsertBrowseRow(
            state->locationList,
            ++row,
            state->browseCountry + L" - all regions",
            L"",
            kBrowseRowCountryAll);
        if (state->browseState.empty()) {
            ListView_SetItemState(
                state->locationList, countryRow, LVIS_SELECTED, LVIS_SELECTED);
        }
        if (state->stateOffset > 0U) {
            InsertBrowseRow(
                state->locationList, ++row, L"Previous regions...", L"", kBrowseRowPrevious);
        }
        for (size_t index = 0; index < state->stateFacets.size(); ++index) {
            const auto& facet = state->stateFacets[index];
            const int facetRow = InsertBrowseRow(
                state->locationList,
                ++row,
                L"  " + facet.name,
                facet.stationCount,
                static_cast<LPARAM>(index));
            if (EqualTextIgnoreCase(facet.name, state->browseState)) {
                ListView_SetItemState(
                    state->locationList, facetRow, LVIS_SELECTED, LVIS_SELECTED);
            }
        }
        if (state->statesHaveMore) {
            InsertBrowseRow(
                state->locationList, ++row, L"Next regions...", L"", kBrowseRowNext);
        }
    } else {
        if (state->facetsLoading && state->countryFacets.empty()) {
            InsertBrowseRow(
                state->locationList,
                ++row,
                L"Loading location counts...",
                L"",
                kBrowseRowLoading);
        }
        if (state->countryOffset > 0U) {
            InsertBrowseRow(
                state->locationList, ++row, L"Previous countries...", L"", kBrowseRowPrevious);
        }
        for (size_t index = 0; index < state->countryFacets.size(); ++index) {
            const auto& facet = state->countryFacets[index];
            const int facetRow = InsertBrowseRow(
                state->locationList,
                ++row,
                facet.name,
                facet.stationCount,
                static_cast<LPARAM>(index));
            if (EqualTextIgnoreCase(facet.name, state->browseCountry)) {
                ListView_SetItemState(
                    state->locationList, facetRow, LVIS_SELECTED, LVIS_SELECTED);
            }
        }
        if (state->countriesHaveMore) {
            InsertBrowseRow(
                state->locationList, ++row, L"Next countries...", L"", kBrowseRowNext);
        }
    }
    state->updatingBrowseLists = false;
}

void JoinWorker(ViewState* state) {
    if (state->worker.joinable()) {
        state->worker.join();
    }
    state->running.store(false);
}

void PopulateFacetCombo(
    HWND combo,
    const wchar_t* allLabel,
    const std::vector<live_stations::BrowseFacet>& facets,
    std::vector<std::wstring>& values,
    bool hasMore = false,
    const wchar_t* browseAllLabel = nullptr) {
    ComboBox_ResetContent(combo);
    ComboBox_AddString(combo, allLabel);
    values.clear();
    values.reserve(facets.size());
    for (const live_stations::BrowseFacet& facet : facets) {
        if (facet.name.empty()) {
            continue;
        }
        values.push_back(facet.name);
        std::wstring label = facet.name;
        if (!facet.stationCount.empty()) {
            label += L"  (" + facet.stationCount + L")";
        }
        ComboBox_AddString(combo, label.c_str());
    }
    if (hasMore && browseAllLabel != nullptr) {
        ComboBox_AddString(combo, browseAllLabel);
    }
    ComboBox_SetCurSel(combo, 0);
}

void PopulateBrowseFacets(ViewState* state, const live_stations::BrowseFacets& facets) {
    if (!facets.genres.empty()) {
        state->genreFacets = facets.genres;
        state->genreOffset = 0;
        state->genresHaveMore = facets.genresHaveMore;
        PopulateFacetCombo(
            state->genreCombo,
            L"All genres",
            facets.genres,
            state->genres,
            facets.genresHaveMore,
            L"Browse all genres...");
    }
    if (!facets.countries.empty()) {
        state->countryFacets = facets.countries;
        state->countryOffset = 0;
        state->countriesHaveMore = facets.countriesHaveMore;
        PopulateFacetCombo(
            state->countryCombo,
            L"All countries",
            facets.countries,
            state->countries,
            facets.countriesHaveMore,
            L"Browse all locations...");
    }
    if (!facets.languages.empty()) {
        PopulateFacetCombo(
            state->languageCombo, L"All languages", facets.languages, state->languages);
    }
    if (!facets.stats.stationTotal.empty()) {
        state->providerStats = facets.stats;
    }
    UpdateProviderTotal(state);
    PopulateGenrePane(state);
    PopulateLocationPane(state);
    for (HWND list : {state->locationList, state->genreList}) {
        SizeBrowseColumns(state, list);
        InvalidateRect(list, nullptr, TRUE);
    }
    InvalidateRect(state->providerTotal, nullptr, TRUE);
    state->facetsLoaded = !state->genres.empty() || !state->countries.empty() ||
                          !state->languages.empty();
}

void PopulateStationRows(ViewState* state) {
    ControlRedrawBatch redraw(state->stationList);
    ListView_DeleteAllItems(state->stationList);
    for (size_t index = 0; index < state->stations.size(); ++index) {
        const auto& station = state->stations[index];
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = static_cast<int>(index);
        item.pszText = const_cast<wchar_t*>(station.name.c_str());
        const int row = ListView_InsertItem(state->stationList, &item);
        ListView_SetItemText(
            state->stationList, row, 1, const_cast<wchar_t*>(station.codec.c_str()));
        std::wstring bitrate = BitrateDisplay(station);
        ListView_SetItemText(state->stationList, row, 2, bitrate.data());
        ListView_SetItemText(
            state->stationList, row, 3, const_cast<wchar_t*>(station.country.c_str()));
        ListView_SetItemText(
            state->stationList,
            row,
            4,
            const_cast<wchar_t*>(CompatibilityLabel(station)));
        ListView_SetItemText(
            state->stationList, row, 5, const_cast<wchar_t*>(station.tags.c_str()));
    }
}

void PopulateList(ViewState* state, live_stations::SearchResult result, bool filtered) {
    if (!result.facets.genres.empty() || !result.facets.countries.empty() ||
        !result.facets.languages.empty()) {
        PopulateBrowseFacets(state, result.facets);
    }
    state->stations = std::move(result.stations);
    state->stationOffset = result.offset;
    state->stationsHaveMore = result.hasMore;
    state->resultsFiltered = filtered;
    SortStationsForDisplay(state);
    PopulateStationRows(state);
    UpdateStationSortHeader(state);

    UpdateResultCount(state);
    std::wstring status = filtered ? L"Matching station page loaded"
                                  : L"Station results loaded";
    if (!result.mirror.empty()) {
        status += L" from " + result.mirror;
    }
    status += filtered
                  ? L". The matching count is this bounded page, not a catalog total."
                  : L". Double-click one to play it.";
    if (!result.facets.error.empty()) {
        status += L" Some browse categories are temporarily unavailable.";
    }
    SetStatus(state, status);
    UpdateSelectionSummary(state);
}

void PopulateFavoritesList(ViewState* state) {
    ControlRedrawBatch redraw(state->stationList);
    ListView_DeleteAllItems(state->stationList);
    state->stations.clear();
    state->stationOffset = 0;
    state->stationsHaveMore = false;
    state->resultsFiltered = false;
    for (const auto& [identity, favorite] : state->favorites.stations()) {
        if (favorite.folderId != live_stations::kFavoritesRootFolderId || !identity.IsValid()) {
            continue;
        }
        live_stations::Station station;
        station.identity = identity;
        station.name = favorite.displayName.empty() ? identity.stationId : favorite.displayName;
        station.fallbackUrlAlias = favorite.fallbackUrlAlias;
        station.tags = L"Saved favorite";
        state->stations.push_back(std::move(station));
    }
    if (state->stationSortColumn == StationSortColumn::None) {
        std::sort(state->stations.begin(), state->stations.end(), [](const auto& left, const auto& right) {
            return CompareStringOrdinal(left.name.c_str(), -1, right.name.c_str(), -1, TRUE) ==
                   CSTR_LESS_THAN;
        });
    } else {
        SortStationsForDisplay(state);
    }
    PopulateStationRows(state);
    UpdateStationSortHeader(state);
    UpdateResultCount(state);
    SetStatus(
        state,
        state->stations.empty()
            ? L"No saved Favorites yet. Right-click a station and choose Add to Live Stations Favorites."
            : L"Saved Live Stations Favorites. Double-click a station or press Enter to play.");
    UpdateSelectionSummary(state);
}

void SortVisibleStationPage(ViewState* state, int columnIndex) {
    if (state == nullptr || columnIndex < static_cast<int>(StationSortColumn::Station) ||
        columnIndex > static_cast<int>(StationSortColumn::Tags)) {
        return;
    }
    const StationSortColumn column = static_cast<StationSortColumn>(columnIndex);
    if (state->stationSortColumn == column) {
        state->stationSortDescending = !state->stationSortDescending;
    } else {
        state->stationSortColumn = column;
        // Numbers read most naturally as highest first; text begins A-Z.
        state->stationSortDescending = column == StationSortColumn::Bitrate;
    }
    SortStationsForDisplay(state);
    PopulateStationRows(state);
    UpdateStationSortHeader(state);
    UpdateResultCount(state);
    UpdateSelectionSummary(state);
    std::wstring status = L"Sorted this visible page by ";
    status += StationSortColumnLabel(column);
    status += state->stationSortDescending ? L", descending." : L", ascending.";
    SetStatus(state, status);
}

bool HasVisibleText(const wchar_t* text) {
    if (text == nullptr) {
        return false;
    }
    for (const wchar_t* cursor = text; *cursor != L'\0'; ++cursor) {
        if (std::iswspace(*cursor) == 0) {
            return true;
        }
    }
    return false;
}

void StartQueuedSearch(ViewState* state);

void StartSearch(ViewState* state) {
    ++state->requestedSearchGeneration;
    if (state->viewMode == ViewMode::Favorites) {
        state->searchQueued = false;
        state->cancelled.store(true);
        PopulateFavoritesList(state);
        return;
    }
    state->searchQueued = true;
    if (state->running.load()) {
        // A completion may already be queued. Its generation and view mode are
        // checked before it can replace the visible page.
        state->cancelled.store(true);
        SetStatus(state, L"Loading updated stations...");
        return;
    }
    StartQueuedSearch(state);
}

void StartQueuedSearch(ViewState* state) {
    if (state->viewMode == ViewMode::Favorites || !state->searchQueued) {
        return;
    }
    if (state->running.exchange(true)) {
        return;
    }
    JoinWorker(state);
    state->running.store(true);
    state->searchQueued = false;
    state->cancelled.store(false);
    SetWorking(state, true);
    SetStatus(state, L"Loading current stations...");

    const std::uint64_t generation = state->requestedSearchGeneration;
    const ViewMode requestViewMode = state->viewMode;

    wchar_t searchText[256]{};
    GetWindowTextW(state->searchEdit, searchText, static_cast<int>(std::size(searchText)));
    live_stations::SearchRequest request;
    request.filters.name.value = searchText;
    request.mode = live_stations::BrowseMode::Radio;
    request.includeFacets = false;
    request.offset = state->stationOffset;
    request.limit = state->stationPageSize;

    auto selectedFacet = [](HWND combo, const std::vector<std::wstring>& values) {
        const int selected = ComboBox_GetCurSel(combo);
        return selected > 0 && static_cast<size_t>(selected - 1) < values.size()
                   ? values[static_cast<size_t>(selected - 1)]
                   : std::wstring{};
    };
    if (requestViewMode == ViewMode::Stations) {
        state->browseGenre = selectedFacet(state->genreCombo, state->genres);
        const std::wstring selectedCountry =
            selectedFacet(state->countryCombo, state->countries);
        if (!EqualTextIgnoreCase(selectedCountry, state->browseCountry)) {
            state->browseCountryCode.clear();
            for (const auto& facet : state->countryFacets) {
                if (EqualTextIgnoreCase(facet.name, selectedCountry)) {
                    state->browseCountryCode = facet.providerId;
                    break;
                }
            }
        }
        state->browseCountry = selectedCountry;
        state->browseState.clear();
    }
    request.filters.tag = {
        state->browseGenre,
        live_stations::TextMatch::Exact};
    request.filters.country = {
        state->browseCountryCode.empty() ? state->browseCountry : std::wstring{},
        live_stations::TextMatch::Exact};
    request.filters.countryCode = state->browseCountryCode;
    request.filters.state = {state->browseState, live_stations::TextMatch::Exact};
    request.filters.language = {
        selectedFacet(state->languageCombo, state->languages),
        live_stations::TextMatch::Exact};
    switch (ComboBox_GetCurSel(state->codecCombo)) {
        case 1:
            request.filters.codec = L"MP3";
            break;
        case 2:
            request.filters.codec = L"AAC";
            break;
        case 3:
            request.filters.codec = L"AAC+";
            break;
        case 4:
            request.filters.codec = L"OGG";
            break;
        case 5:
            request.filters.codec = L"OPUS";
            break;
        default:
            break;
    }
    switch (ComboBox_GetCurSel(state->compatibilityCombo)) {
        case 1:
            request.filters.compatibility = live_stations::RadioCompatibilityPreset::Best;
            break;
        case 2:
            request.filters.compatibility = live_stations::RadioCompatibilityPreset::Usually;
            break;
        case 3:
            request.filters.compatibility =
                live_stations::RadioCompatibilityPreset::DecoderRequired;
            break;
        case 4:
            request.filters.compatibility = live_stations::RadioCompatibilityPreset::Unknown;
            break;
        default:
            break;
    }
    switch (ComboBox_GetCurSel(state->bitrateCombo)) {
        case 1:
            request.filters.bitrate = live_stations::BitrateRangeForRadioPreset(
                live_stations::RadioBitratePreset::Under100);
            break;
        case 2:
            request.filters.bitrate = live_stations::BitrateRangeForRadioPreset(
                live_stations::RadioBitratePreset::From100To159);
            break;
        case 3:
            request.filters.bitrate = live_stations::BitrateRangeForRadioPreset(
                live_stations::RadioBitratePreset::From160To255);
            break;
        case 4:
            request.filters.bitrate = live_stations::BitrateRangeForRadioPreset(
                live_stations::RadioBitratePreset::AtLeast256);
            break;
        case 5:
            request.filters.bitrate = live_stations::BitrateRangeForRadioPreset(
                live_stations::RadioBitratePreset::Unknown);
            break;
        default:
            break;
    }

    const bool filtered = HasVisibleText(searchText) ||
                          !request.filters.tag.value.empty() ||
                          !request.filters.country.value.empty() ||
                          !request.filters.countryCode.empty() ||
                          !request.filters.state.value.empty() ||
                          ComboBox_GetCurSel(state->languageCombo) > 0 ||
                          ComboBox_GetCurSel(state->codecCombo) > 0 ||
                          ComboBox_GetCurSel(state->compatibilityCombo) > 0 ||
                          ComboBox_GetCurSel(state->bitrateCombo) > 0;
    // Keep the last complete station page and its details painted until the
    // replacement request finishes. The status line carries the busy state.

    try {
        const live_stations::ProviderOperations* provider = &ActiveProviderOperations();
        state->worker = std::thread([state,
                                     request = std::move(request),
                                     filtered,
                                     generation,
                                     requestViewMode,
                                     provider]() {
            live_stations::SearchResult result = provider->search(request, state->cancelled);
            {
                std::lock_guard lock(state->pendingMutex);
                state->pendingKind = PendingKind::Search;
                state->pendingSearch = std::move(result);
                state->pendingSearchFiltered = filtered;
                state->pendingSearchGeneration = generation;
                state->pendingSearchViewMode = requestViewMode;
            }
            PostMessageW(state->window, kWorkComplete, 0, 0);
        });
    } catch (...) {
        state->running.store(false);
        SetWorking(state, false);
        SetStatus(state, L"Could not start the station search worker.");
    }
}

LRESULT CALLBACK SearchEditSubclassProc(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    UINT_PTR subclassId,
    DWORD_PTR referenceData) {
    auto* state = reinterpret_cast<ViewState*>(referenceData);
    if (message == WM_GETDLGCODE && wParam == VK_RETURN) {
        return DefSubclassProc(window, message, wParam, lParam) | DLGC_WANTMESSAGE;
    }
    if (message == WM_KEYDOWN && wParam == VK_RETURN) {
        // Start one search for the initial key press. Repeated WM_KEYDOWN messages
        // and the following WM_CHAR are consumed so Enter cannot beep, enqueue a
        // station, or queue duplicate requests.
        constexpr LPARAM kPreviousKeyState = static_cast<LPARAM>(1) << 30;
        if ((lParam & kPreviousKeyState) == 0 && state != nullptr &&
            IsWindow(state->window) != FALSE) {
            state->stationOffset = 0;
            PostMessageW(state->window, kStartDeferredSearch, 0, 0);
        }
        return 0;
    }
    if ((message == WM_CHAR || message == WM_KEYUP) && wParam == VK_RETURN) {
        return 0;
    }
    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(window, SearchEditSubclassProc, subclassId);
    }
    return DefSubclassProc(window, message, wParam, lParam);
}

void StartFacetLoad(ViewState* state) {
    if (state->facetsLoaded || state->running.exchange(true)) {
        return;
    }
    JoinWorker(state);
    state->running.store(true);
    state->cancelled.store(false);
    state->facetsLoading = true;
    SetWorking(state, true);
    SetStatus(state, L"Loading locations, genres, languages, and provider counts...");
    UpdateProviderTotal(state);
    PopulateGenrePane(state);
    PopulateLocationPane(state);

    try {
        const live_stations::ProviderOperations* provider = &ActiveProviderOperations();
        state->worker = std::thread([state, provider]() {
            live_stations::BrowseFacets facets = provider->loadBrowseFacets(state->cancelled);
            {
                std::lock_guard lock(state->pendingMutex);
                state->pendingKind = PendingKind::Facets;
                state->pendingFacets = std::move(facets);
            }
            PostMessageW(state->window, kWorkComplete, 0, 0);
        });
    } catch (...) {
        state->running.store(false);
        state->facetsLoading = false;
        SetWorking(state, false);
        UpdateProviderTotal(state);
        PopulateGenrePane(state);
        PopulateLocationPane(state);
        SetStatus(state, L"Stations loaded; browse categories could not start.");
    }
}

void StartBrowseFacetPage(
    ViewState* state,
    PendingFacetTarget target,
    std::uint32_t offset) {
    if (target == PendingFacetTarget::None || state->running.exchange(true)) {
        return;
    }
    JoinWorker(state);
    state->running.store(true);
    state->cancelled.store(false);
    SetWorking(state, true);
    SetStatus(state, L"Loading the next bounded category page...");

    live_stations::BrowseFacetPageRequest request;
    request.offset = offset;
    request.limit = 80U;
    switch (target) {
        case PendingFacetTarget::Genres:
            request.kind = live_stations::BrowseFacetKind::Genre;
            break;
        case PendingFacetTarget::Countries:
            request.kind = live_stations::BrowseFacetKind::Country;
            break;
        case PendingFacetTarget::States:
            request.kind = live_stations::BrowseFacetKind::State;
            request.country = state->browseCountry;
            break;
        case PendingFacetTarget::None:
            state->running.store(false);
            SetWorking(state, false);
            return;
    }

    try {
        const live_stations::ProviderOperations* provider = &ActiveProviderOperations();
        state->worker = std::thread([state, request = std::move(request), target, provider]() {
            live_stations::BrowseFacetPage page =
                provider->loadBrowseFacetPage(request, state->cancelled);
            {
                std::lock_guard lock(state->pendingMutex);
                state->pendingKind = PendingKind::FacetPage;
                state->pendingFacetPage = std::move(page);
                state->pendingFacetTarget = target;
            }
            PostMessageW(state->window, kWorkComplete, 0, 0);
        });
    } catch (...) {
        state->running.store(false);
        SetWorking(state, false);
        SetStatus(state, L"Could not start the category-page worker.");
    }
}

void ResetBrowseFilters(ViewState* state) {
    // Invalidate any result before the deferred reset search is started. This
    // makes Reset atomic from the user's perspective even if a provider
    // completion is already on the window-message queue.
    ++state->requestedSearchGeneration;
    state->searchQueued = false;
    state->cancelled.store(true);
    SetWindowTextW(state->searchEdit, L"");
    ComboBox_SetCurSel(state->genreCombo, 0);
    ComboBox_SetCurSel(state->countryCombo, 0);
    ComboBox_SetCurSel(state->languageCombo, 0);
    ComboBox_SetCurSel(state->codecCombo, 0);
    ComboBox_SetCurSel(state->compatibilityCombo, 0);
    ComboBox_SetCurSel(state->bitrateCombo, 0);
    state->browseGenre.clear();
    state->browseCountry.clear();
    state->browseCountryCode.clear();
    state->browseState.clear();
    state->locationDrilled = false;
    state->stationOffset = 0;
    PopulateGenrePane(state);
    PopulateLocationPane(state);
    PostMessageW(state->window, kStartDeferredSearch, 0, 0);
}

int EnqueueAndPlay(const std::wstring& url) {
    const int position = EnqueueUrl(url);
    if (position < 0) {
        return -1;
    }
    SendMessageW(gPlugin.hwndWinampParent, WM_WA_IPC, position, IPC_SETPLAYLISTPOS);
    SendMessageW(gPlugin.hwndWinampParent, WM_COMMAND, kWinampPlayCommand, 0);
    return position;
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
    std::wstring resolving = L"Resolving " + station.name;
    if (!station.codec.empty()) {
        resolving += L" (" + station.codec + L")";
    }
    resolving += L"...";
    SetStatus(state, resolving);

    try {
        const live_stations::ProviderOperations* provider = &ActiveProviderOperations();
        state->worker = std::thread([state, station, provider]() {
            live_stations::ResolveResult result =
                provider->resolveAndCountClick(station, state->cancelled);
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
    live_stations::BrowseFacets facets;
    live_stations::BrowseFacetPage facetPage;
    PendingFacetTarget facetTarget = PendingFacetTarget::None;
    live_stations::ResolveResult resolve;
    std::wstring stationName;
    bool searchFiltered = false;
    std::uint64_t searchGeneration = 0;
    ViewMode searchViewMode = ViewMode::Browse;
    {
        std::lock_guard lock(state->pendingMutex);
        kind = state->pendingKind;
        state->pendingKind = PendingKind::None;
        search = std::move(state->pendingSearch);
        facets = std::move(state->pendingFacets);
        facetPage = std::move(state->pendingFacetPage);
        facetTarget = state->pendingFacetTarget;
        state->pendingFacetTarget = PendingFacetTarget::None;
        resolve = std::move(state->pendingResolve);
        stationName = std::move(state->pendingStationName);
        searchFiltered = state->pendingSearchFiltered;
        searchGeneration = state->pendingSearchGeneration;
        searchViewMode = state->pendingSearchViewMode;
    }

    if (kind == PendingKind::Search) {
        const bool currentSearch =
            state->viewMode != ViewMode::Favorites && searchViewMode == state->viewMode &&
            searchGeneration == state->requestedSearchGeneration;
        if (!currentSearch) {
            // Never let an older Browse/Stations result overwrite Favorites or
            // a newer Reset/search request. Start only the latest queued page.
            if (state->searchQueued && state->viewMode != ViewMode::Favorites) {
                StartQueuedSearch(state);
            }
            return;
        }
        if (!search.error.empty()) {
            if (!search.facets.genres.empty() || !search.facets.countries.empty() ||
                !search.facets.languages.empty()) {
                PopulateBrowseFacets(state, search.facets);
            }
            state->facetsLoading = false;
            UpdateProviderTotal(state);
            PopulateGenrePane(state);
            PopulateLocationPane(state);
            SetStatus(state, search.error);
        } else {
            PopulateList(state, std::move(search), searchFiltered);
            StartFacetLoad(state);
        }
    } else if (kind == PendingKind::Facets) {
        state->facetsLoading = false;
        PopulateBrowseFacets(state, facets);
        if (state->facetsLoaded) {
            SetStatus(
                state,
                facets.error.empty()
                    ? L"Browse categories ready. Double-click a station or press Enter to play."
                    : L"Some browse categories loaded; others are temporarily unavailable.");
        } else {
            SetStatus(state, L"Stations loaded; browse categories are temporarily unavailable.");
        }
        if (state->searchQueued && state->viewMode != ViewMode::Favorites) {
            StartQueuedSearch(state);
        }
    } else if (kind == PendingKind::FacetPage) {
        if (!facetPage.error.empty() && facetPage.items.empty()) {
            SetStatus(state, facetPage.error);
            if (state->searchQueued && state->viewMode != ViewMode::Favorites) {
                StartQueuedSearch(state);
            }
            return;
        }
        if (facetTarget == PendingFacetTarget::Genres) {
            state->genreFacets = std::move(facetPage.items);
            state->genreOffset = facetPage.offset;
            state->genresHaveMore = facetPage.hasMore;
            PopulateGenrePane(state);
        } else if (facetTarget == PendingFacetTarget::Countries) {
            state->countryFacets = std::move(facetPage.items);
            state->countryOffset = facetPage.offset;
            state->countriesHaveMore = facetPage.hasMore;
            PopulateLocationPane(state);
        } else if (facetTarget == PendingFacetTarget::States) {
            state->stateFacets = std::move(facetPage.items);
            state->stateOffset = facetPage.offset;
            state->statesHaveMore = facetPage.hasMore;
            state->locationDrilled = true;
            PopulateLocationPane(state);
        }
        SetStatus(
            state,
            facetPage.hasMore
                ? L"Category page loaded; Previous and Next keep memory bounded."
                : L"Final category page loaded.");
        if (state->searchQueued && state->viewMode != ViewMode::Favorites) {
            StartQueuedSearch(state);
        }
    } else if (kind == PendingKind::Play) {
        if (resolve.url.empty()) {
            SetStatus(state, resolve.error.empty() ? L"No playable URL was returned." : resolve.error);
        } else {
            const std::wstring previousTitle = SanitizeNowPlayingTitle(CopyWinampText(
                SendMessageW(
                    gPlugin.hwndWinampParent,
                    WM_WA_IPC,
                    0,
                    IPC_GET_PLAYING_TITLE),
                1024U));
            const int playlistPosition = EnqueueAndPlay(resolve.url);
            if (playlistPosition < 0) {
                SetStatus(state, L"Winamp did not confirm the station playlist entry.");
                return;
            }
            StartNowPlayingTracking(
                state, playlistPosition, resolve.url, stationName, previousTitle);
            std::wstring status = L"Connecting " + stationName +
                                  L" in Winamp; the station may need time to buffer.";
            if (!resolve.error.empty()) {
                status += L" " + resolve.error;
            }
            SetStatus(state, status);
        }
    }
}

void AddColumn(
    HWND list,
    int index,
    int width,
    const wchar_t* label,
    int format = LVCFMT_LEFT) {
    LVCOLUMNW column{};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT;
    column.iSubItem = index;
    column.cx = width;
    column.fmt = format;
    column.pszText = const_cast<wchar_t*>(label);
    ListView_InsertColumn(list, index, &column);
}

LPARAM SelectedBrowseRowData(HWND list) {
    const int selected = ListView_GetNextItem(list, -1, LVNI_SELECTED);
    if (selected < 0) {
        return kBrowseRowAll;
    }
    LVITEMW item{};
    item.mask = LVIF_PARAM;
    item.iItem = selected;
    return ListView_GetItem(list, &item) != FALSE ? item.lParam : kBrowseRowAll;
}

void RequestStationFirstPage(ViewState* state) {
    state->stationOffset = 0;
    PostMessageW(state->window, kStartDeferredSearch, 0, 0);
}

void HandleGenrePaneSelection(ViewState* state) {
    if (state->updatingBrowseLists) {
        return;
    }
    const LPARAM data = SelectedBrowseRowData(state->genreList);
    if (data == kBrowseRowPrevious) {
        StartBrowseFacetPage(
            state,
            PendingFacetTarget::Genres,
            state->genreOffset >= 80U ? state->genreOffset - 80U : 0U);
        return;
    }
    if (data == kBrowseRowNext) {
        StartBrowseFacetPage(
            state, PendingFacetTarget::Genres, state->genreOffset + 80U);
        return;
    }
    if (data == kBrowseRowAll) {
        state->browseGenre.clear();
    } else if (data >= 0 && static_cast<size_t>(data) < state->genreFacets.size()) {
        state->browseGenre = state->genreFacets[static_cast<size_t>(data)].name;
    } else {
        return;
    }
    RequestStationFirstPage(state);
}

void HandleLocationPaneSelection(ViewState* state) {
    if (state->updatingBrowseLists) {
        return;
    }
    const LPARAM data = SelectedBrowseRowData(state->locationList);
    if (data == kBrowseRowBack) {
        state->locationDrilled = false;
        state->browseState.clear();
        PostMessageW(state->window, kRefreshLocationPane, 0, 0);
        RequestStationFirstPage(state);
        return;
    }
    if (data == kBrowseRowPrevious || data == kBrowseRowNext) {
        const bool previous = data == kBrowseRowPrevious;
        if (state->locationDrilled) {
            const std::uint32_t offset = previous
                                             ? (state->stateOffset >= 80U
                                                    ? state->stateOffset - 80U
                                                    : 0U)
                                             : state->stateOffset + 80U;
            StartBrowseFacetPage(state, PendingFacetTarget::States, offset);
        } else {
            const std::uint32_t offset = previous
                                             ? (state->countryOffset >= 80U
                                                    ? state->countryOffset - 80U
                                                    : 0U)
                                             : state->countryOffset + 80U;
            StartBrowseFacetPage(state, PendingFacetTarget::Countries, offset);
        }
        return;
    }
    if (data == kBrowseRowAll) {
        const bool wasDrilled = state->locationDrilled;
        state->browseCountry.clear();
        state->browseCountryCode.clear();
        state->browseState.clear();
        state->locationDrilled = false;
        // Do not delete/reinsert the row while the native list is still
        // processing its selection notification. An ordinary All click needs
        // no rebuild at all; exiting a drilled page is deferred.
        if (wasDrilled) PostMessageW(state->window, kRefreshLocationPane, 0, 0);
    } else if (data == kBrowseRowCountryAll) {
        state->browseState.clear();
    } else if (data >= 0) {
        if (state->locationDrilled && static_cast<size_t>(data) < state->stateFacets.size()) {
            state->browseState = state->stateFacets[static_cast<size_t>(data)].name;
        } else if (!state->locationDrilled &&
                   static_cast<size_t>(data) < state->countryFacets.size()) {
            state->browseCountry = state->countryFacets[static_cast<size_t>(data)].name;
            state->browseCountryCode =
                state->countryFacets[static_cast<size_t>(data)].providerId;
            state->browseState.clear();
        } else {
            return;
        }
    } else {
        return;
    }
    RequestStationFirstPage(state);
}

void DrillSelectedCountry(ViewState* state) {
    if (state->locationDrilled || state->updatingBrowseLists) {
        return;
    }
    const LPARAM data = SelectedBrowseRowData(state->locationList);
    if (data < 0 || static_cast<size_t>(data) >= state->countryFacets.size()) {
        return;
    }
    state->browseCountry = state->countryFacets[static_cast<size_t>(data)].name;
    state->browseCountryCode =
        state->countryFacets[static_cast<size_t>(data)].providerId;
    state->browseState.clear();
    state->stateOffset = 0;
    StartBrowseFacetPage(state, PendingFacetTarget::States, 0U);
}

void SetViewMode(ViewState* state, ViewMode mode) {
    if (state->viewMode == mode) {
        return;
    }
    const ViewMode previousMode = state->viewMode;
    const bool queueReplacementForViewChange =
        mode != ViewMode::Favorites && state->running.load();
    if (queueReplacementForViewChange) {
        ++state->requestedSearchGeneration;
        state->searchQueued = true;
        state->cancelled.store(true);
    }
    if (mode == ViewMode::Browse) {
        auto selectedFacet = [](HWND combo, const std::vector<std::wstring>& values) {
            const int selected = ComboBox_GetCurSel(combo);
            return selected > 0 && static_cast<size_t>(selected - 1) < values.size()
                       ? values[static_cast<size_t>(selected - 1)]
                       : std::wstring{};
        };
        state->browseGenre = selectedFacet(state->genreCombo, state->genres);
        state->browseCountry = selectedFacet(state->countryCombo, state->countries);
        state->browseCountryCode.clear();
        for (const auto& facet : state->countryFacets) {
            if (EqualTextIgnoreCase(facet.name, state->browseCountry)) {
                state->browseCountryCode = facet.providerId;
                break;
            }
        }
        state->browseState.clear();
        PopulateGenrePane(state);
        PopulateLocationPane(state);
    } else if (mode == ViewMode::Stations) {
        auto selectValue = [](HWND combo,
                              std::vector<std::wstring>& values,
                              const std::wstring& selected) {
            if (selected.empty()) {
                ComboBox_SetCurSel(combo, 0);
                return;
            }
            for (size_t index = 0; index < values.size(); ++index) {
                if (EqualTextIgnoreCase(values[index], selected)) {
                    ComboBox_SetCurSel(combo, static_cast<int>(index + 1U));
                    return;
                }
            }
            values.push_back(selected);
            const std::wstring label = selected + L"  (selected in Browse)";
            ComboBox_AddString(combo, label.c_str());
            ComboBox_SetCurSel(combo, static_cast<int>(values.size()));
        };
        selectValue(state->genreCombo, state->genres, state->browseGenre);
        selectValue(state->countryCombo, state->countries, state->browseCountry);
    }
    state->viewMode = mode;
    if (mode != ViewMode::Favorites) state->radioViewMode = mode;
    state->scrollX = 0;
    state->scrollY = 0;
    if (mode == ViewMode::Favorites) {
        // Favorites owns the shared visible list. A prior provider completion
        // is no longer allowed to populate it after this point.
        ++state->requestedSearchGeneration;
        state->searchQueued = false;
        state->cancelled.store(true);
        SetWindowTextW(state->tunerLabel, L"FAVORITES  /  SELECTED STATION");
        PopulateFavoritesList(state);
    } else {
        SetWindowTextW(
            state->tunerLabel,
            mode == ViewMode::Browse ? L"STATION DETAILS" : L"TUNER  /  SELECTED STATION");
    }
    // View changes are generation-bound too, so preserve readable controls
    // instead of asking a skin to repaint the selected strip as disabled.
    EnableWindow(state->stationsViewButton, TRUE);
    EnableWindow(state->browseViewButton, TRUE);
    EnableWindow(state->favoritesViewButton, TRUE);
    RECT bounds{};
    GetClientRect(state->window, &bounds);
    LayoutControls(state, bounds.right - bounds.left, bounds.bottom - bounds.top);
    SetFocus(mode == ViewMode::Browse ? state->locationList : state->stationList);
    if (previousMode == ViewMode::Favorites && mode != ViewMode::Favorites) {
        state->stationOffset = 0;
        StartSearch(state);
    }
}

// The toolbar and body are separately clipped/scrollable child viewports.
// Body scrolling can never move the toolbar. Tiny hosts get toolbar overflow
// instead of clipped actions; ordinary hosts only need a wrapped pinned header.
void LayoutControls(ViewState* state, int width, int height) {
    if (state->layingOut || state->destroying || state->body == nullptr ||
        state->toolbar == nullptr || width <= 0 || height <= 0) {
        return;
    }
    state->layingOut = true;
    const RECT oldDividers[] = {state->categoryDivider, state->categoryBottomDivider,
                               state->detailsDivider};
    int line = 16;
    const HDC dc = GetDC(state->window);
    const HFONT font = reinterpret_cast<HFONT>(SendMessageW(state->title, WM_GETFONT, 0, 0));
    HGDIOBJ previous = nullptr;
    if (dc != nullptr) {
        previous = font != nullptr ? SelectObject(dc, font) : nullptr;
        TEXTMETRICW metrics{};
        if (GetTextMetricsW(dc, &metrics)) {
            line = (std::max)(12L, metrics.tmHeight + metrics.tmExternalLeading);
        }
    }
    const int margin = (std::max)(8, line / 2);
    const int gap = (std::max)(7, line / 3);
    const int controlHeight = line + (std::max)(9, line / 2);
    const bool favorites = state->viewMode == ViewMode::Favorites;
    if ((IsWindowEnabled(state->filterToggleButton) != FALSE) == favorites) {
        EnableWindow(state->filterToggleButton, !favorites);
    }
    const bool browse = state->viewMode == ViewMode::Browse;
    const bool details = !state->tunerCollapsed;
    state->categoryDivider = {};
    state->categoryBottomDivider = {};
    state->detailsDivider = {};

    auto measured = [&](HWND control, const wchar_t* text, int minimum = 60) {
        return MeasureControlTextWidth(control, text, minimum, (std::max)(32, line * 2));
    };
    const int dpi = static_cast<int>(GetDpiForWindow(state->window));
    if (state->layoutDpi > 0 && dpi > 0 && dpi != state->layoutDpi) {
        for (int* extent : {&state->categoryPaneHeight, &state->detailsPaneWidth,
                            &state->detailsPaneHeight}) {
            *extent = MulDiv(*extent, dpi, state->layoutDpi);
        }
        state->locationSizedWidth = state->genreSizedWidth = -1;
    }
    state->layoutDpi = dpi;
    auto textHeight = [&](HWND control, int available) {
        const int length = GetWindowTextLengthW(control);
        std::wstring text(static_cast<size_t>(length) + 1U, L'\0');
        GetWindowTextW(control, text.data(), length + 1);
        RECT bounds{0, 0, available, 0};
        if (dc != nullptr && length > 0) {
            DrawTextW(dc, text.c_str(), length, &bounds,
                      DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
        }
        return (std::max)(line + 4, static_cast<int>(bounds.bottom) + 4);
    };
    struct Placement { HWND control; RECT bounds; int windowHeight; };
    std::vector<Placement> positions;
    auto place = [&](HWND control, int x, int y, int w, int h, bool combo = false) {
        positions.push_back({control, {x, y, x + w, y + h},
                             combo ? (std::max)(h, line * 14) : h});
    };
    // Reuse the bounded old/new-region repaint policy for both viewport faces
    // and their controls. No redraw toggles, whole-view disable, or parent erase.
    auto moveControl = [&](HWND control, HWND parent, const RECT& target, int windowHeight) {
        RECT old{};
        GetWindowRect(control, &old);
        MapWindowPoints(nullptr, parent, reinterpret_cast<POINT*>(&old), 2);
        if (!EqualRect(&old, &target) || !IsWindowVisible(control)) {
            SetWindowPos(control, nullptr, target.left, target.top,
                target.right - target.left, windowHeight,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW | SWP_SHOWWINDOW);
            InvalidateRect(parent, &old, FALSE);
            InvalidateRect(parent, &target, FALSE);
            RedrawWindow(control, nullptr, nullptr,
                RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN | RDW_NOERASE);
        }
    };
    auto scrollRange = [&](HWND viewport, int bar, int extent, int page,
                           int& offset, bool keepBar) {
        offset = (std::clamp)(offset, 0, (std::max)(0, extent - page));
        SCROLLINFO info{sizeof(info), SIF_RANGE | SIF_PAGE | SIF_POS |
            (keepBar ? SIF_DISABLENOSCROLL : 0U)};
        info.nMax = (std::max)(0, extent - 1);
        info.nPage = static_cast<UINT>((std::max)(1, page));
        info.nPos = offset;
        SetScrollInfo(viewport, bar, &info, TRUE);
    };
    int x = margin;
    int y = margin;

    // Reserve both labels, so state changes never alter toolbar geometry.
    const wchar_t* viewLabel = state->radioViewMode == ViewMode::Browse
                                  ? L"&View: Browse" : L"&View: Stations";
    wchar_t oldLabel[64]{};
    GetWindowTextW(state->browseViewButton, oldLabel, static_cast<int>(std::size(oldLabel)));
    if (wcscmp(oldLabel, viewLabel) != 0) SetWindowTextW(state->browseViewButton, viewLabel);
    const int viewWidth = (std::max)(
        measured(state->browseViewButton, L"View: Browse", 90),
        measured(state->browseViewButton, L"View: Stations", 90));
    const int filterWidth = (std::max)(
        measured(state->filterToggleButton, L"Show filters", 90),
        measured(state->filterToggleButton, L"Hide filters", 90));
    const int detailWidth = (std::max)(
        measured(state->tunerToggleButton, L"Show details", 90),
        measured(state->tunerToggleButton, L"Hide details", 90));
    // Toolbar order and placement are independent of view/filter/details state.
    const std::pair<HWND, int> toolbarButtons[] = {
        {state->browseViewButton, viewWidth},
        {state->favoritesViewButton, measured(state->favoritesViewButton, L"Favorites", 90)},
        {state->filterToggleButton, filterWidth}, {state->tunerToggleButton, detailWidth}};
    int widestButton = 0;
    for (const auto& button : toolbarButtons) widestButton = (std::max)(widestButton, button.second);
    // Predict overflow using the full outer width so shrinking/growing does not
    // oscillate as native scrollbars appear/disappear. Reserve their metrics only
    // when needed, then wrap again at the reduced client width.
    const int scrollbarW = GetSystemMetricsForDpi(SM_CXVSCROLL, dpi);
    const int scrollbarH = GetSystemMetricsForDpi(SM_CYHSCROLL, dpi);
    const int maxToolbarHeight = (std::max)(1, height / 2);
    int headerLogicalWidth = 0, headerDocumentHeight = 0;
    bool headerHScroll = false, headerVScroll = false;
    auto planToolbar = [&](int available) {
        positions.clear(); x = margin; y = margin;
        headerLogicalWidth = (std::max)(available, widestButton + 2 * margin);
        for (const auto& button : toolbarButtons) {
            if (x != margin && x + button.second > headerLogicalWidth - margin) {
                x = margin; y += controlHeight + gap;
            }
            place(button.first, x, y, button.second, controlHeight);
            x += button.second + gap;
        }
        headerDocumentHeight = y + controlHeight + margin;
    };
    planToolbar(width);
    headerHScroll = headerLogicalWidth > width;
    headerVScroll = headerDocumentHeight + (headerHScroll ? scrollbarH : 0) > maxToolbarHeight;
    if (headerVScroll) {
        planToolbar((std::max)(1, width - scrollbarW));
        headerHScroll = headerLogicalWidth > width - scrollbarW;
    }
    const int toolbarHeight = (std::min)(maxToolbarHeight,
        headerDocumentHeight + (headerHScroll ? scrollbarH : 0));
    moveControl(state->toolbar, state->window, RECT{0, 0, width, toolbarHeight}, toolbarHeight);
    ShowScrollBar(state->toolbar, SB_HORZ, headerHScroll);
    ShowScrollBar(state->toolbar, SB_VERT, headerVScroll);
    RECT headerClient{};
    GetClientRect(state->toolbar, &headerClient);
    scrollRange(state->toolbar, SB_HORZ, headerLogicalWidth, headerClient.right,
                state->toolbarScrollX, false);
    scrollRange(state->toolbar, SB_VERT, headerDocumentHeight, headerClient.bottom,
                state->toolbarScrollY, false);
    for (const auto& position : positions) {
        RECT target = position.bounds;
        OffsetRect(&target, -state->toolbarScrollX, -state->toolbarScrollY);
        moveControl(position.control, state->toolbar, target, position.windowHeight);
    }
    positions.clear();
    moveControl(state->body, state->window, RECT{0, toolbarHeight, width, height}, height - toolbarHeight);
    RECT bodyClient{};
    GetClientRect(state->body, &bodyClient);
    width = bodyClient.right; height = bodyClient.bottom;
    const int logicalWidth = (std::max)(width, line * 24 + margin * 2);
    const int contentWidth = logicalWidth - margin * 2;
    const bool sideDetails = logicalWidth >= (std::max)(820, line * 48);
    const int detailsWidth = (std::clamp)(
        state->detailsPaneWidth > 0 ? state->detailsPaneWidth : line * 19,
        line * 15, (std::max)(line * 15, contentWidth / 2));
    state->sideDetails = sideDetails;
    x = margin; y = margin;
    auto row = [&](HWND control, int w, bool combo = false) {
        if (x != margin && x + w > logicalWidth - margin) {
            x = margin; y += controlHeight + gap;
        }
        place(control, x, y, w, controlHeight, combo);
        x += w + gap;
    };
    auto finishRow = [&]() { x = margin; y += controlHeight + gap; };
    const int titleHeight = textHeight(state->title, contentWidth);
    place(state->title, margin, y, contentWidth, titleHeight);
    y += titleHeight + gap;

    if (!favorites) {
        const int findWidth = measured(state->filterLabel, L"FIND");
        const int searchButtonWidth = measured(state->searchButton, L"Search", 72);
        row(state->filterLabel, findWidth);
        row(state->searchEdit, (std::max)(line * 12,
            contentWidth - findWidth - searchButtonWidth - 2 * gap));
        row(state->searchButton, searchButtonWidth);
        finishRow();
        const int providerHeight = textHeight(state->providerTotal, contentWidth);
        place(state->providerTotal, margin, y, contentWidth, providerHeight);
        y += providerHeight + gap;
    }

    if (browse) {
        const bool paired = logicalWidth >= (std::max)(650, line * 38);
        state->pairedCategories = paired;
        const int paneWidth = paired ? (std::clamp)(
            (contentWidth - gap) * state->categorySplitPermille / 1000,
            line * 16, contentWidth - gap - line * 16) : contentWidth;
        const int paneHeight = (std::clamp)(state->categoryPaneHeight > 0
            ? state->categoryPaneHeight : height / (paired ? 4 : 7), line * 5, line * 24);
        place(state->locationPaneLabel, margin, y, paneWidth, line + 4);
        place(state->locationList, margin, y + line + 4, paneWidth, paneHeight);
        const int genreX = paired ? margin + paneWidth + gap : margin;
        const int genreY = paired ? y : y + line + 4 + paneHeight + gap;
        const int genreWidth = logicalWidth - margin - genreX;
        place(state->genrePaneLabel, genreX, genreY, genreWidth, line + 4);
        place(state->genreList, genreX, genreY + line + 4, genreWidth, paneHeight);
        if (paired) state->categoryDivider =
            {margin + paneWidth, y, genreX, y + line + 4 + paneHeight};
        y = genreY + line + 4 + paneHeight + gap;
        state->categoryBottomDivider = {margin, y - gap, logicalWidth - margin, y};
    }
    if (!favorites && !state->filterStripCollapsed) {
        place(state->browseLabel, margin, y, contentWidth, line + 4);
        y += line + 4;
        x = margin;
        auto filter = [&](HWND combo) {
            wchar_t selected[512]{};
            GetWindowTextW(combo, selected, static_cast<int>(std::size(selected)));
            const int w = (std::min)(contentWidth,
                (std::max)(line * 14, measured(combo, selected)));
            row(combo, w, true);
            // Dropdowns remain scrollable and can reveal longer provider labels.
            SendMessageW(combo, CB_SETDROPPEDWIDTH, (std::min)(contentWidth, (std::max)(w, line * 28)), 0);
        };
        if (!browse) {
            filter(state->genreCombo);
            filter(state->countryCombo);
        }
        filter(state->languageCombo);
        filter(state->codecCombo);
        filter(state->bitrateCombo);
        filter(state->compatibilityCombo);
        row(state->resetButton, measured(state->resetButton, L"RESET", 70));
        finishRow();
    }

    const int countHeight = textHeight(state->resultCount, contentWidth);
    place(state->resultCount, margin, y, contentWidth, countHeight);
    y += countHeight + gap;
    if (!favorites) {
        x = margin;
        row(state->pageSizeCombo, measured(state->pageSizeCombo, L"200 per page", 100), true);
        row(state->previousPageButton, (std::max)(
            measured(state->previousPageButton, L"Previous", 72),
            measured(state->previousPageButton, L"First page", 72)));
        row(state->nextPageButton, (std::max)(
            measured(state->nextPageButton, L"Next", 58),
            measured(state->nextPageButton, L"Last page", 58)));
        finishRow();
    }

    const int statusHeight = textHeight(state->status, contentWidth);
    const int detailsMinimum = !sideDetails && state->detailsPaneHeight > 0
        ? (std::clamp)(state->detailsPaneHeight, line * 12, line * 32) : line * 12;
    if (!sideDetails) state->detailsPaneHeight = detailsMinimum;
    const int listMinimum = line * 7;
    const int belowDetails = details && !sideDetails ? detailsMinimum + gap : 0;
    const int listHeight = (std::max)(
        details && sideDetails ? detailsMinimum : listMinimum,
        height - margin - statusHeight - gap - y - belowDetails);
    const int listWidth = details && sideDetails ? contentWidth - detailsWidth - gap : contentWidth;
    place(state->stationList, margin, y, listWidth, listHeight);
    if (details) {
        const int detailX = sideDetails ? margin + listWidth + gap : margin;
        int detailY = sideDetails ? y : y + listHeight + gap;
        state->detailsDivider = sideDetails
            ? RECT{margin + listWidth, y, detailX, y + listHeight}
            : RECT{margin, y + listHeight, logicalWidth - margin, detailY};
        const int detailW = sideDetails ? detailsWidth : contentWidth;
        const int detailH = sideDetails ? listHeight : detailsMinimum;
        const int bottom = detailY + detailH;
        const int labelH = textHeight(state->tunerLabel, detailW);
        place(state->tunerLabel, detailX, detailY, detailW, labelH);
        detailY += labelH + gap;
        const int nameH = textHeight(state->selectionName, detailW);
        place(state->selectionName, detailX, detailY, detailW, nameH);
        detailY += nameH + gap;
        const int nowH = textHeight(state->nowPlaying, detailW);
        // Long names/metadata must extend the document instead of eating the edit.
        const int editH = (std::max)(line * 4, bottom - detailY - nowH - gap);
        place(state->selectionDetails, detailX, detailY, detailW, editH);
        detailY += editH + gap;
        place(state->nowPlaying, detailX, detailY, detailW, nowH);
        y = (std::max)(y + listHeight, detailY + nowH) + gap;
    } else {
        y += listHeight + gap;
    }
    place(state->status, margin, y, contentWidth, statusHeight);
    const int documentHeight = y + statusHeight + margin;
    if (dc != nullptr) {
        if (previous != nullptr) SelectObject(dc, previous);
        ReleaseDC(state->window, dc);
    }

    // Keep native scrollbars available even at the minimum window size. Page
    // scrolling never changes the user's station/category list scroll positions.
    scrollRange(state->body, SB_HORZ, logicalWidth, width, state->scrollX, true);
    scrollRange(state->body, SB_VERT, documentHeight, height, state->scrollY, true);
    for (HWND child = GetWindow(state->body, GW_CHILD); child != nullptr;
         child = GetWindow(child, GW_HWNDNEXT)) {
        const auto found = std::find_if(positions.begin(), positions.end(),
            [&](const Placement& p) { return p.control == child; });
        const bool visible = (GetWindowLongPtrW(child, GWL_STYLE) & WS_VISIBLE) != 0;
        if (found == positions.end()) {
            if (visible) {
                RECT old{};
                GetWindowRect(child, &old);
                MapWindowPoints(nullptr, state->body, reinterpret_cast<POINT*>(&old), 2);
                SetWindowPos(child, nullptr, 0, 0, 0, 0,
                    SWP_HIDEWINDOW | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                    SWP_NOACTIVATE | SWP_NOREDRAW);
                InvalidateRect(state->body, &old, FALSE);
            }
            continue;
        }
        RECT target = found->bounds;
        OffsetRect(&target, -state->scrollX, -state->scrollY);
        RECT old{};
        GetWindowRect(child, &old);
        MapWindowPoints(nullptr, state->body, reinterpret_cast<POINT*>(&old), 2);
        // A combo's window height includes its dropdown; compare the closed face.
        if (GetDlgCtrlID(child) == kModeComboId || GetDlgCtrlID(child) == kBitrateComboId ||
            GetDlgCtrlID(child) == kCompatibilityComboId || GetDlgCtrlID(child) == kGenreComboId ||
            GetDlgCtrlID(child) == kCountryComboId || GetDlgCtrlID(child) == kLanguageComboId ||
            GetDlgCtrlID(child) == kPageSizeComboId) {
            old.bottom = old.top + controlHeight;
        }
        if (!visible || !EqualRect(&old, &target)) {
            SetWindowPos(child, nullptr, target.left, target.top,
                target.right - target.left, found->windowHeight,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW | SWP_SHOWWINDOW);
            InvalidateRect(state->body, &old, FALSE);
            InvalidateRect(state->body, &target, FALSE);
            // SWP_NOREDRAW also suppresses nonclient borders/scrollbars. The
            // clipped parent cannot repair those pixels on the child's behalf.
            RedrawWindow(child, nullptr, nullptr,
                RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN | RDW_NOERASE);
        }
    }
    if (browse) {
        SizeBrowseColumns(state, state->locationList);
        SizeBrowseColumns(state, state->genreList);
    }
    for (RECT divider : oldDividers) {
        // Scroll/focus can change offsets before entry, and resizing can clamp
        // them above. Erase the former divider at its last rendered position.
        OffsetRect(&divider, -state->laidOutScrollX, -state->laidOutScrollY);
        InvalidateRect(state->body, &divider, FALSE);
    }
    for (RECT divider : {state->categoryDivider, state->categoryBottomDivider,
                         state->detailsDivider}) {
        OffsetRect(&divider, -state->scrollX, -state->scrollY);
        InvalidateRect(state->body, &divider, FALSE);
    }
    state->laidOutScrollX = state->scrollX;
    state->laidOutScrollY = state->scrollY;
    // Service only queued damage, including child frames, before the next drag
    // message. Do not invalidate/erase the whole view or toggle redraw/visibility.
    // Keep the guard set while synchronous paint notifications run.
    RedrawWindow(state->window, nullptr, nullptr,
        RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_NOERASE);
    state->layingOut = false;
}

int HitDivider(ViewState* state, POINT point) {
    point.x += state->scrollX;
    point.y += state->scrollY;
    if (PtInRect(&state->categoryDivider, point)) return 1;
    if (PtInRect(&state->categoryBottomDivider, point)) return 2;
    if (PtInRect(&state->detailsDivider, point)) return 3;
    return 0;
}

void ScrollView(ViewState* state, HWND viewport, int bar, int command) {
    SCROLLINFO info{sizeof(info), SIF_ALL};
    GetScrollInfo(viewport, bar, &info);
    int position = info.nPos;
    switch (command) {
        case SB_LINEUP: position -= 24; break;
        case SB_LINEDOWN: position += 24; break;
        case SB_PAGEUP: position -= static_cast<int>(info.nPage); break;
        case SB_PAGEDOWN: position += static_cast<int>(info.nPage); break;
        case SB_TOP: position = 0; break;
        case SB_BOTTOM: position = info.nMax; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: position = info.nTrackPos; break;
        default: return;
    }
    if (viewport == state->toolbar) {
        (bar == SB_VERT ? state->toolbarScrollY : state->toolbarScrollX) = position;
    } else {
        (bar == SB_VERT ? state->scrollY : state->scrollX) = position;
    }
    RECT bounds{};
    GetClientRect(state->window, &bounds);
    LayoutControls(state, bounds.right, bounds.bottom);
}

LRESULT CALLBACK FocusScrollSubclassProc(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam,
    UINT_PTR subclassId, DWORD_PTR reference) {
    auto* state = reinterpret_cast<ViewState*>(reference);
    if (message == WM_SETFONT && state != nullptr) {
        if (window == state->locationList) state->locationSizedWidth = -1;
        if (window == state->genreList) state->genreSizedWidth = -1;
        if (!state->destroying && !state->refreshingSkin && !state->layingOut && !state->layoutQueued)
            state->layoutQueued = PostMessageW(state->window, kRelayout, 0, 0) != FALSE;
    }
    if (message == WM_SETTEXT && state != nullptr && !state->layingOut) {
        const LRESULT result = DefSubclassProc(window, message, wParam, lParam);
        const int id = GetDlgCtrlID(window);
        if ((id == kStatusId || id == kResultCountId || id == kSelectionNameId ||
             id == kNowPlayingId || id == kProviderTotalId) && !state->layoutQueued) {
            state->layoutQueued = PostMessageW(state->window, kRelayout, 0, 0) != FALSE;
        }
        return result;
    }
    if (message == WM_SETFOCUS && state != nullptr && !state->layingOut && !state->destroying) {
        const HWND viewport = GetParent(window);
        RECT child{}, client{};
        GetWindowRect(window, &child);
        MapWindowPoints(nullptr, viewport, reinterpret_cast<POINT*>(&child), 2);
        GetClientRect(viewport, &client);
        // A partly visible large list already has an interaction surface. Do
        // not jump the document to its bottom on a mouse click/focus change.
        // Small controls are fully revealed; oversized controls reveal their
        // leading edge only when completely outside the viewport.
        auto reveal = [](int first, int last, int available) {
            if (last - first > available) {
                return last <= 0 || first >= available ? first : 0;
            }
            return first < 0 ? first : (last > available ? last - available : 0);
        };
        const int dx = reveal(child.left, child.right, client.right);
        const int dy = reveal(child.top, child.bottom, client.bottom);
        if (dx != 0 || dy != 0) {
            if (viewport == state->toolbar) {
                state->toolbarScrollX += dx; state->toolbarScrollY += dy;
            } else if (viewport == state->body) {
                state->scrollX += dx; state->scrollY += dy;
            }
            GetClientRect(state->window, &client);
            LayoutControls(state, client.right, client.bottom);
        }
    }
    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(window, FocusScrollSubclassProc, subclassId);
    }
    return DefSubclassProc(window, message, wParam, lParam);
}

void ToggleTunerPane(ViewState* state) {
    state->tunerCollapsed = !state->tunerCollapsed;
    SetWindowTextW(
        state->tunerToggleButton,
        state->tunerCollapsed ? L"&Show details" : L"&Hide details");
    RECT bounds{};
    GetClientRect(state->window, &bounds);
    LayoutControls(state, bounds.right - bounds.left, bounds.bottom - bounds.top);
    SetFocus(state->tunerToggleButton);
}

void ToggleFilterStrip(ViewState* state) {
    state->filterStripCollapsed = !state->filterStripCollapsed;
    SetWindowTextW(
        state->filterToggleButton,
        state->filterStripCollapsed ? L"&Show filters" : L"&Hide filters");
    RECT bounds{};
    GetClientRect(state->window, &bounds);
    LayoutControls(state, bounds.right - bounds.left, bounds.bottom - bounds.top);
    SetFocus(state->filterToggleButton);
}

// A viewport borrows state until its own WM_NCDESTROY, which precedes the
// root's WM_NCDESTROY. Never route lifecycle messages to the root procedure.
LRESULT CALLBACK ViewportSubclassProc(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam,
    UINT_PTR subclassId, DWORD_PTR reference) {
    auto* state = reinterpret_cast<ViewState*>(reference);
    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(window, ViewportSubclassProc, subclassId);
        return DefSubclassProc(window, message, wParam, lParam);
    }
    if (state == nullptr || state->destroying) return DefSubclassProc(window, message, wParam, lParam);
    const bool isBody = window == state->body;
    switch (message) {
        case WM_COMMAND:
        case WM_NOTIFY:
        case WM_CONTEXTMENU:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORBTN:
            return SendMessageW(state->window, message, wParam, lParam);
        case WM_SIZE:
            if (!state->layingOut && !state->layoutQueued)
                state->layoutQueued = PostMessageW(state->window, kRelayout, 0, 0) != FALSE;
            return 0;
        case WM_VSCROLL:
        case WM_HSCROLL:
            if (lParam == 0) {
                ScrollView(state, window, message == WM_VSCROLL ? SB_VERT : SB_HORZ, LOWORD(wParam));
                return 0;
            }
            break;
        case WM_MOUSEWHEEL:
            // Native lists/edits handle their own wheel; background wheel scrolls
            // this viewport only. Never scroll the pinned header with the body.
            if (GET_WHEEL_DELTA_WPARAM(wParam) != 0)
                ScrollView(state, window, SB_VERT,
                    GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? SB_LINEUP : SB_LINEDOWN);
            return 0;
        case WM_LBUTTONDOWN:
            if (isBody && state != nullptr) {
                const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                state->draggingDivider = HitDivider(state, point);
                if (state->draggingDivider != 0) {
                    state->dragStart = point;
                    const HWND pane = state->draggingDivider == 3
                        ? state->selectionDetails : state->locationList;
                    RECT bounds{};
                    GetWindowRect(pane, &bounds);
                    state->dragStartExtent = state->draggingDivider == 3
                        ? (state->sideDetails ? bounds.right - bounds.left : state->detailsPaneHeight)
                        : bounds.bottom - bounds.top;
                    SetCapture(window);
                    return 0;
                }
            }
            break;
        case WM_MOUSEMOVE:
            if (isBody && state != nullptr && state->draggingDivider != 0 && GetCapture() == window) {
                RECT bounds{};
                GetClientRect(window, &bounds);
                const int dx = GET_X_LPARAM(lParam) - state->dragStart.x;
                const int dy = GET_Y_LPARAM(lParam) - state->dragStart.y;
                if (state->draggingDivider == 1) {
                    state->categorySplitPermille = (std::clamp)(
                        static_cast<int>((GET_X_LPARAM(lParam) + state->scrollX) * 1000 /
                            (std::max)(1L, bounds.right)), 200, 800);
                } else if (state->draggingDivider == 2) {
                    state->categoryPaneHeight = state->dragStartExtent +
                        dy / (state->pairedCategories ? 1 : 2);
                } else if (state->sideDetails) {
                    state->detailsPaneWidth = state->dragStartExtent - dx;
                } else {
                    state->detailsPaneHeight = (std::max)(1, state->dragStartExtent - dy);
                }
                GetClientRect(state->window, &bounds);
                LayoutControls(state, bounds.right, bounds.bottom);
                return 0;
            }
            break;
        case WM_LBUTTONUP:
            if (isBody && state != nullptr && state->draggingDivider != 0) {
                state->draggingDivider = 0;
                if (GetCapture() == window) ReleaseCapture();
                return 0;
            }
            break;
        case WM_CAPTURECHANGED:
            if (isBody && state != nullptr) state->draggingDivider = 0;
            break;
        case WM_SETCURSOR:
            if (isBody && state != nullptr && LOWORD(lParam) == HTCLIENT) {
                POINT point{};
                GetCursorPos(&point);
                ScreenToClient(window, &point);
                const int divider = HitDivider(state, point);
                if (divider != 0) {
                    SetCursor(LoadCursorW(nullptr, divider == 1 ||
                        (divider == 3 && state->sideDetails) ? IDC_SIZEWE : IDC_SIZENS));
                    return TRUE;
                }
            }
            break;
        case WM_ERASEBKGND:
            // WM_PAINT fills precisely the damaged background; the root and all
            // child controls have their own clipped paint responsibilities.
            return TRUE;
        case WM_PAINT:
            if (state->windowBrush != nullptr) {
                PAINTSTRUCT paint{};
                const HDC dc = BeginPaint(window, &paint);
                FillRect(dc, &paint.rcPaint, state->windowBrush);
                if (isBody) for (RECT divider : {state->categoryDivider,
                        state->categoryBottomDivider, state->detailsDivider}) {
                    if (IsRectEmpty(&divider)) continue;
                    OffsetRect(&divider, -state->scrollX, -state->scrollY);
                    DrawEdge(dc, &divider, EDGE_ETCHED,
                        divider.right - divider.left < divider.bottom - divider.top ? BF_LEFT : BF_TOP);
                }
                EndPaint(window, &paint);
                return 0;
            }
            break;
    }
    return DefSubclassProc(window, message, wParam, lParam);
}

LRESULT OnCreate(HWND window, CREATESTRUCTW* /*create*/) {
    auto* state = reinterpret_cast<ViewState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (state == nullptr) {
        return -1;
    }
    state->window = window;
    // These containers only borrow state. They cannot create workers or own
    // ViewState teardown; root WM_NCDESTROY remains the single deletion point.
    for (const auto& entry : {std::pair<HWND*, int>{&state->toolbar, kToolbarViewportId},
                              {&state->body, kBodyViewportId}}) {
        *entry.first = CreateWindowExW(WS_EX_CONTROLPARENT, WC_STATICW, L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
                WS_HSCROLL | WS_VSCROLL, 0, 0, 0, 0, window,
            reinterpret_cast<HMENU>(entry.second), gPlugin.hDllInstance, nullptr);
        if (*entry.first == nullptr || !SetWindowSubclass(*entry.first,
                ViewportSubclassProc, 1, reinterpret_cast<DWORD_PTR>(state))) return -1;
    }
#if !defined(LIVE_STATIONS_OFFLINE_FIXTURE)
    live_stations::FavoritesLoadResult loadedFavorites = live_stations::LoadFavoritesFromDefaultPath();
    state->favorites = std::move(loadedFavorites.model);
    state->favoritesStorageWarning = std::move(loadedFavorites.error);
#endif
    const DWORD framedControlStyle = WS_EX_CLIENTEDGE;
    const DWORD buttonStyle = BS_PUSHBUTTON;

    state->title = CreateWindowExW(
        0,
        WC_STATICW,
        L"LIVE STATIONS  //  RADIO CONSOLE",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        0,
        0,
        0,
        0,
        state->body,
        reinterpret_cast<HMENU>(kTitleId),
        gPlugin.hDllInstance,
        nullptr);
    state->directoryLabel = CreateWindowExW(
        0,
        WC_STATICW,
        L"LIVE DIRECTORY  /  RADIO BROWSER",
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        0,
        0,
        0,
        0,
        state->body,
        reinterpret_cast<HMENU>(kDirectoryLabelId),
        gPlugin.hDllInstance,
        nullptr);
    state->stationsViewButton = CreateWindowExW(
        0,
        WC_BUTTONW,
        L"Stations",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | buttonStyle,
        0,
        0,
        0,
        0,
        state->body,
        reinterpret_cast<HMENU>(kStationsViewButtonId),
        gPlugin.hDllInstance,
        nullptr);
    state->browseViewButton = CreateWindowExW(
        0,
        WC_BUTTONW,
        L"Browse",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | buttonStyle,
        0,
        0,
        0,
        0,
        state->toolbar,
        reinterpret_cast<HMENU>(kBrowseViewButtonId),
        gPlugin.hDllInstance,
        nullptr);
    state->favoritesViewButton = CreateWindowExW(
        0,
        WC_BUTTONW,
        L"Favorites",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | buttonStyle,
        0,
        0,
        0,
        0,
        state->toolbar,
        reinterpret_cast<HMENU>(kFavoritesViewButtonId),
        gPlugin.hDllInstance,
        nullptr);
    state->filterLabel = CreateWindowExW(
        0,
        WC_STATICW,
        L"FIND",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        0,
        0,
        0,
        0,
        state->body,
        reinterpret_cast<HMENU>(kFilterLabelId),
        gPlugin.hDllInstance,
        nullptr);
    state->browseLabel = CreateWindowExW(
        0,
        WC_STATICW,
        L"FILTERS",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        0,
        0,
        0,
        0,
        state->body,
        reinterpret_cast<HMENU>(kBrowseLabelId),
        gPlugin.hDllInstance,
        nullptr);

    state->searchEdit = CreateWindowExW(
        framedControlStyle,
        WC_EDITW,
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        0,
        0,
        0,
        0,
        state->body,
        reinterpret_cast<HMENU>(kSearchEditId),
        gPlugin.hDllInstance,
        nullptr);
    state->codecCombo = CreateWindowExW(
        0,
        WC_COMBOBOXW,
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST |
            CBS_NOINTEGRALHEIGHT,
        0,
        0,
        0,
        0,
        state->body,
        reinterpret_cast<HMENU>(kModeComboId),
        gPlugin.hDllInstance,
        nullptr);
    ComboBox_AddString(state->codecCombo, L"All radio codecs");
    ComboBox_AddString(state->codecCombo, L"MP3 - best compatibility");
    ComboBox_AddString(state->codecCombo, L"AAC");
    ComboBox_AddString(state->codecCombo, L"AAC+");
    ComboBox_AddString(state->codecCombo, L"OGG - decoder dependent");
    ComboBox_AddString(state->codecCombo, L"OPUS - decoder dependent");
    ComboBox_SetCurSel(state->codecCombo, 0);
    state->bitrateCombo = CreateWindowExW(
        0,
        WC_COMBOBOXW,
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST |
            CBS_NOINTEGRALHEIGHT,
        0,
        0,
        0,
        0,
        state->body,
        reinterpret_cast<HMENU>(kBitrateComboId),
        gPlugin.hDllInstance,
        nullptr);
    ComboBox_AddString(state->bitrateCombo, L"All bitrates");
    ComboBox_AddString(state->bitrateCombo, L"Under 100 kbps");
    ComboBox_AddString(state->bitrateCombo, L"100-159 kbps");
    ComboBox_AddString(state->bitrateCombo, L"160-255 kbps");
    ComboBox_AddString(state->bitrateCombo, L"256+ kbps");
    ComboBox_AddString(state->bitrateCombo, L"Unknown / not reported");
    ComboBox_SetCurSel(state->bitrateCombo, 0);
    state->compatibilityCombo = CreateWindowExW(
        0,
        WC_COMBOBOXW,
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST |
            CBS_NOINTEGRALHEIGHT,
        0,
        0,
        0,
        0,
        state->body,
        reinterpret_cast<HMENU>(kCompatibilityComboId),
        gPlugin.hDllInstance,
        nullptr);
    ComboBox_AddString(state->compatibilityCombo, L"All compatibility");
    ComboBox_AddString(state->compatibilityCombo, L"Best - MP3");
    ComboBox_AddString(state->compatibilityCombo, L"Usually - AAC / AAC+");
    ComboBox_AddString(state->compatibilityCombo, L"Decoder needed - OGG / OPUS / FLAC");
    ComboBox_AddString(state->compatibilityCombo, L"Unknown codec report");
    ComboBox_SetCurSel(state->compatibilityCombo, 0);
    state->genreCombo = CreateWindowExW(
        0,
        WC_COMBOBOXW,
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST |
            CBS_NOINTEGRALHEIGHT,
        0,
        0,
        0,
        0,
        state->body,
        reinterpret_cast<HMENU>(kGenreComboId),
        gPlugin.hDllInstance,
        nullptr);
    ComboBox_AddString(state->genreCombo, L"Loading genres...");
    ComboBox_SetCurSel(state->genreCombo, 0);
    state->countryCombo = CreateWindowExW(
        0,
        WC_COMBOBOXW,
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST |
            CBS_NOINTEGRALHEIGHT,
        0,
        0,
        0,
        0,
        state->body,
        reinterpret_cast<HMENU>(kCountryComboId),
        gPlugin.hDllInstance,
        nullptr);
    ComboBox_AddString(state->countryCombo, L"Loading countries...");
    ComboBox_SetCurSel(state->countryCombo, 0);
    state->languageCombo = CreateWindowExW(
        0,
        WC_COMBOBOXW,
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST |
            CBS_NOINTEGRALHEIGHT,
        0,
        0,
        0,
        0,
        state->body,
        reinterpret_cast<HMENU>(kLanguageComboId),
        gPlugin.hDllInstance,
        nullptr);
    ComboBox_AddString(state->languageCombo, L"Loading languages...");
    ComboBox_SetCurSel(state->languageCombo, 0);

    state->locationPaneLabel = CreateWindowExW(
        0,
        WC_STATICW,
        L"LOCATION",
        WS_CHILD | SS_LEFT,
        0,
        0,
        0,
        0,
        state->body,
        reinterpret_cast<HMENU>(kLocationPaneLabelId),
        gPlugin.hDllInstance,
        nullptr);
    state->genrePaneLabel = CreateWindowExW(
        0,
        WC_STATICW,
        L"GENRE / CATEGORY",
        WS_CHILD | SS_LEFT,
        0,
        0,
        0,
        0,
        state->body,
        reinterpret_cast<HMENU>(kGenrePaneLabelId),
        gPlugin.hDllInstance,
        nullptr);
    state->providerTotal = CreateWindowExW(
        0,
        WC_STATICW,
        L"Radio Browser provider total unavailable",
        WS_CHILD | SS_LEFT | SS_NOPREFIX,
        0,
        0,
        0,
        0,
        state->body,
        reinterpret_cast<HMENU>(kProviderTotalId),
        gPlugin.hDllInstance,
        nullptr);
    const DWORD browseListStyle =
        WS_CHILD | WS_TABSTOP | WS_VSCROLL | LVS_REPORT | LVS_SINGLESEL |
        LVS_SHOWSELALWAYS;
    state->locationList = CreateWindowExW(
        framedControlStyle,
        WC_LISTVIEWW,
        L"",
        browseListStyle,
        0,
        0,
        0,
        0,
        state->body,
        reinterpret_cast<HMENU>(kLocationListId),
        gPlugin.hDllInstance,
        nullptr);
    state->genreList = CreateWindowExW(
        framedControlStyle,
        WC_LISTVIEWW,
        L"",
        browseListStyle,
        0,
        0,
        0,
        0,
        state->body,
        reinterpret_cast<HMENU>(kGenreListId),
        gPlugin.hDllInstance,
        nullptr);
    state->pageSizeCombo = CreateWindowExW(
        0,
        WC_COMBOBOXW,
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST |
            CBS_NOINTEGRALHEIGHT,
        0,
        0,
        0,
        0,
        state->body,
        reinterpret_cast<HMENU>(kPageSizeComboId),
        gPlugin.hDllInstance,
        nullptr);
    ComboBox_AddString(state->pageSizeCombo, L"50 per page");
    ComboBox_AddString(state->pageSizeCombo, L"100 per page");
    ComboBox_AddString(state->pageSizeCombo, L"200 per page");
    ComboBox_SetCurSel(state->pageSizeCombo, 2);

    state->searchButton = CreateWindowExW(
        0,
        WC_BUTTONW,
        L"Search",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | buttonStyle,
        0,
        0,
        0,
        0,
        state->body,
        reinterpret_cast<HMENU>(kSearchButtonId),
        gPlugin.hDllInstance,
        nullptr);
    state->resetButton = CreateWindowExW(
        0,
        WC_BUTTONW,
        L"RESET",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | buttonStyle,
        0,
        0,
        0,
        0,
        state->body,
        reinterpret_cast<HMENU>(kResetButtonId),
        gPlugin.hDllInstance,
        nullptr);
    state->filterToggleButton = CreateWindowExW(
        0,
        WC_BUTTONW,
        L"&Hide filters",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | buttonStyle,
        0,
        0,
        0,
        0,
        state->toolbar,
        reinterpret_cast<HMENU>(kFilterToggleButtonId),
        gPlugin.hDllInstance,
        nullptr);
    state->previousPageButton = CreateWindowExW(
        0,
        WC_BUTTONW,
        L"Previous",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | buttonStyle,
        0,
        0,
        0,
        0,
        state->body,
        reinterpret_cast<HMENU>(kPreviousPageButtonId),
        gPlugin.hDllInstance,
        nullptr);
    state->nextPageButton = CreateWindowExW(
        0,
        WC_BUTTONW,
        L"Next",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | buttonStyle,
        0,
        0,
        0,
        0,
        state->body,
        reinterpret_cast<HMENU>(kNextPageButtonId),
        gPlugin.hDllInstance,
        nullptr);
    state->tunerToggleButton = CreateWindowExW(
        0,
        WC_BUTTONW,
        L"&Hide details",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | buttonStyle,
        0,
        0,
        0,
        0,
        state->toolbar,
        reinterpret_cast<HMENU>(kTunerToggleButtonId),
        gPlugin.hDllInstance,
        nullptr);
    state->stationList = CreateWindowExW(
        framedControlStyle,
        WC_LISTVIEWW,
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        0,
        0,
        0,
        0,
        state->body,
        reinterpret_cast<HMENU>(kStationListId),
        gPlugin.hDllInstance,
        nullptr);
    state->resultCount = CreateWindowExW(
        0,
        WC_STATICW,
        L"0 stations shown",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
        0,
        0,
        0,
        0,
        state->body,
        reinterpret_cast<HMENU>(kResultCountId),
        gPlugin.hDllInstance,
        nullptr);
    state->tunerLabel = CreateWindowExW(
        0,
        WC_STATICW,
        L"STATION DETAILS",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        0,
        0,
        0,
        0,
        state->body,
        reinterpret_cast<HMENU>(kTunerLabelId),
        gPlugin.hDllInstance,
        nullptr);
    state->selectionName = CreateWindowExW(
        0,
        WC_STATICW,
        L"Choose a station",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
        0,
        0,
        0,
        0,
        state->body,
        reinterpret_cast<HMENU>(kSelectionNameId),
        gPlugin.hDllInstance,
        nullptr);
    state->selectionDetails = CreateWindowExW(
        framedControlStyle,
        WC_EDITW,
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | ES_LEFT | ES_MULTILINE |
            ES_AUTOVSCROLL | ES_READONLY,
        0,
        0,
        0,
        0,
        state->body,
        reinterpret_cast<HMENU>(kSelectionDetailsId),
        gPlugin.hDllInstance,
        nullptr);
    state->nowPlaying = CreateWindowExW(
        0,
        WC_STATICW,
        L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
        0,
        0,
        0,
        0,
        state->body,
        reinterpret_cast<HMENU>(kNowPlayingId),
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
        state->body,
        reinterpret_cast<HMENU>(kStatusId),
        gPlugin.hDllInstance,
        nullptr);

    SendMessageW(
        state->searchEdit,
        EM_SETCUEBANNER,
        TRUE,
        reinterpret_cast<LPARAM>(L"Station or network name"));
    if (state->searchEdit == nullptr ||
        SetWindowSubclass(
            state->searchEdit,
            SearchEditSubclassProc,
            kSearchEditId,
            reinterpret_cast<DWORD_PTR>(state)) == FALSE) {
        return -1;
    }

    for (HWND viewport : {state->toolbar, state->body}) {
        for (HWND child = GetWindow(viewport, GW_CHILD); child != nullptr;
             child = GetWindow(child, GW_HWNDNEXT)) {
            if (!SetWindowSubclass(child, FocusScrollSubclassProc, 1,
                                   reinterpret_cast<DWORD_PTR>(state))) return -1;
        }
    }

    ListView_SetExtendedListViewStyle(
        state->stationList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
    for (HWND list : {state->locationList, state->genreList}) {
        ListView_SetExtendedListViewStyle(
            list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
    }
    AddColumn(state->locationList, 0, 260, L"Location");
    AddColumn(state->locationList, 1, 76, L"Stations", LVCFMT_RIGHT);
    AddColumn(state->genreList, 0, 260, L"Genre / category");
    AddColumn(state->genreList, 1, 76, L"Stations", LVCFMT_RIGHT);
    for (HWND combo : {
             state->codecCombo,
             state->bitrateCombo,
             state->compatibilityCombo,
             state->genreCombo,
             state->countryCombo,
             state->languageCombo,
             state->pageSizeCombo}) {
        SendMessageW(combo, CB_SETMINVISIBLE, 12, 0);
    }
    AddColumn(state->stationList, 0, 260, L"Station");
    AddColumn(state->stationList, 1, 90, L"Codec");
    AddColumn(state->stationList, 2, 70, L"kbps");
    AddColumn(state->stationList, 3, 120, L"Country");
    AddColumn(state->stationList, 4, 110, L"Compatibility");
    AddColumn(state->stationList, 5, 300, L"Tags");
    if (!RefreshSkin(state, false)) return -1;
    UpdateResultCount(state);
    UpdateSelectionSummary(state);
    UpdateProviderTotal(state);
    PopulateGenrePane(state);
    PopulateLocationPane(state);
    // WM_SIZE can precede control creation. Establish a complete initial
    // layout before the host receives this view, not on the first resize.
    RECT initialClient{};
    GetClientRect(window, &initialClient);
    LayoutControls(state, initialClient.right, initialClient.bottom);
    StartSearch(state);
    return 0;
}

// Keep the real native selection state (and accessibility semantics), but paint
// selected report rows consistently even when keyboard focus leaves the list.
// Native unfocused selection otherwise ignores the copied dark item palette.
LRESULT DrawListSelection(NMLVCUSTOMDRAW* draw) {
    if (draw->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
    if (draw->nmcd.dwDrawStage != CDDS_ITEMPREPAINT) return CDRF_DODEFAULT;
    const HWND list = draw->nmcd.hdr.hwndFrom;
    const int row = static_cast<int>(draw->nmcd.dwItemSpec);
    const UINT state = ListView_GetItemState(list, row, LVIS_SELECTED | LVIS_FOCUSED);
    if ((state & LVIS_SELECTED) == 0U) return CDRF_DODEFAULT;
    RECT rowBounds{}, client{};
    if (!ListView_GetItemRect(list, row, &rowBounds, LVIR_BOUNDS)) return CDRF_DODEFAULT;
    GetClientRect(list, &client);
    rowBounds.left = 0; rowBounds.right = client.right;
    const int saved = SaveDC(draw->nmcd.hdc);
    if (saved == 0) return CDRF_DODEFAULT;
    FillRect(draw->nmcd.hdc, &rowBounds, GetSysColorBrush(COLOR_HIGHLIGHT));
    SetTextColor(draw->nmcd.hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
    SetBkMode(draw->nmcd.hdc, TRANSPARENT);
    const HWND header = ListView_GetHeader(list);
    const int padding = (std::max)(3, MulDiv(4, GetDpiForWindow(list), 96));
    for (int column = 0; column < Header_GetItemCount(header); ++column) {
        RECT cell{};
        if (!Header_GetItemRect(header, column, &cell)) continue;
        MapWindowPoints(header, list, reinterpret_cast<POINT*>(&cell), 2);
        cell.top = rowBounds.top; cell.bottom = rowBounds.bottom;
        cell.left += padding; cell.right -= padding;
        wchar_t text[4096]{};
        ListView_GetItemText(list, row, column, text, static_cast<int>(std::size(text)));
        LVCOLUMNW info{}; info.mask = LVCF_FMT;
        ListView_GetColumn(list, column, &info);
        const UINT alignment = (info.fmt & LVCFMT_JUSTIFYMASK) == LVCFMT_RIGHT ? DT_RIGHT :
            ((info.fmt & LVCFMT_JUSTIFYMASK) == LVCFMT_CENTER ? DT_CENTER : DT_LEFT);
        DrawTextW(draw->nmcd.hdc, text, -1, &cell,
            alignment | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    if (GetFocus() == list && (state & LVIS_FOCUSED) != 0U &&
        (SendMessageW(list, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS) == 0U)
        DrawFocusRect(draw->nmcd.hdc, &rowBounds);
    RestoreDC(draw->nmcd.hdc, saved);
    return CDRF_SKIPDEFAULT;
}

LRESULT CALLBACK ViewWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<ViewState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    switch (message) {
        case WM_NCCREATE: {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            auto* context = create == nullptr
                                ? nullptr
                                : static_cast<ViewCreationContext*>(create->lpCreateParams);
            if (context == nullptr || context->state == nullptr) {
                return FALSE;
            }
            SetWindowLongPtrW(
                window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(context->state));
            context->ownershipTransferred = true;
            gActiveViewWindows.push_back(window);
            const unsigned int activeViews = gActiveViewCount.fetch_add(1U) + 1U;
            TraceViewLifecycle(L"view ownership accepted", activeViews);
            return TRUE;
        }
        case WM_CREATE:
            return OnCreate(window, reinterpret_cast<CREATESTRUCTW*>(lParam));
        case WM_SIZE:
            // A minimized window reports a zero client area. Preserve the last
            // valid child geometry and rebuild it only on the restored size.
            if (state != nullptr && wParam != SIZE_MINIMIZED && LOWORD(lParam) > 0 &&
                HIWORD(lParam) > 0) {
                LayoutControls(state, LOWORD(lParam), HIWORD(lParam));
            }
            return 0;
        case kRelayout:
            if (state != nullptr) {
                state->layoutQueued = false;
                RECT bounds{};
                GetClientRect(window, &bounds);
                LayoutControls(state, bounds.right, bounds.bottom);
            }
            return 0;
        case WM_ERASEBKGND:
            if (state != nullptr && state->windowBrush != nullptr) {
                RECT bounds{};
                GetClientRect(window, &bounds);
                FillRect(reinterpret_cast<HDC>(wParam), &bounds, state->windowBrush);
                return TRUE;
            }
            break;
        case WM_PAINT:
            if (state != nullptr && state->windowBrush != nullptr) {
                PAINTSTRUCT paint{};
                const HDC dc = BeginPaint(window, &paint);
                FillRect(dc, &paint.rcPaint, state->windowBrush);
                EndPaint(window, &paint);
                return 0;
            }
            break;
        case WM_CTLCOLORSTATIC:
            if (state != nullptr &&
                reinterpret_cast<HWND>(lParam) == state->selectionDetails &&
                state->itemBrush != nullptr) {
                const HDC dc = reinterpret_cast<HDC>(wParam);
                SetBkColor(dc, state->itemBackground);
                SetTextColor(dc, state->itemForeground);
                return reinterpret_cast<LRESULT>(state->itemBrush);
            }
            if (state != nullptr && state->windowBrush != nullptr) {
                const HDC dc = reinterpret_cast<HDC>(wParam);
                SetBkColor(dc, state->windowBackground);
                SetTextColor(dc, state->windowForeground);
                return reinterpret_cast<LRESULT>(state->windowBrush);
            }
            break;
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
            if (state != nullptr && state->itemBrush != nullptr) {
                const HDC dc = reinterpret_cast<HDC>(wParam);
                SetBkColor(dc, state->itemBackground);
                SetTextColor(dc, state->itemForeground);
                return reinterpret_cast<LRESULT>(state->itemBrush);
            }
            break;
        case WM_DPICHANGED:
        case WM_DPICHANGED_AFTERPARENT:
        case WM_SETTINGCHANGE:
        case WM_DISPLAYCHANGE:
        case WM_THEMECHANGED:
        case WM_SYSCOLORCHANGE:
            if (state != nullptr) {
                if (!RefreshSkin(state, true)) return 0;
                RECT bounds{};
                GetClientRect(window, &bounds);
                LayoutControls(state, bounds.right - bounds.left, bounds.bottom - bounds.top);
            }
            return 0;
        case WM_COMMAND:
            if (state != nullptr && LOWORD(wParam) == kAddFavoriteCommandId && HIWORD(wParam) == 0) {
                AddSelectedStationToFavorites(state);
                return 0;
            }
            if (state != nullptr && LOWORD(wParam) == kAddWinampPlaylistCommandId &&
                HIWORD(wParam) == 0) {
                AddSelectedStationToWinampPlaylist(state);
                return 0;
            }
            if (state != nullptr && HIWORD(wParam) == BN_CLICKED) {
                if (LOWORD(wParam) == kSearchButtonId) {
                    state->stationOffset = 0;
                    StartSearch(state);
                    return 0;
                }
                if (LOWORD(wParam) == kResetButtonId) {
                    ResetBrowseFilters(state);
                    return 0;
                }
                if (LOWORD(wParam) == kTunerToggleButtonId) {
                    ToggleTunerPane(state);
                    return 0;
                }
                if (LOWORD(wParam) == kFilterToggleButtonId &&
                    state->viewMode != ViewMode::Favorites) {
                    ToggleFilterStrip(state);
                    return 0;
                }
                if (LOWORD(wParam) == kStationsViewButtonId) {
                    SetViewMode(state, ViewMode::Stations);
                    return 0;
                }
                if (LOWORD(wParam) == kBrowseViewButtonId) {
                    SetViewMode(state, state->viewMode == ViewMode::Favorites
                        ? state->radioViewMode
                        : (state->viewMode == ViewMode::Browse ? ViewMode::Stations : ViewMode::Browse));
                    return 0;
                }
                if (LOWORD(wParam) == kFavoritesViewButtonId) {
                    SetViewMode(state, ViewMode::Favorites);
                    return 0;
                }
                if (LOWORD(wParam) == kPreviousPageButtonId) {
                    if (state->stationOffset == 0U) {
                        SetStatus(state, L"Already at the first result page.");
                        return 0;
                    }
                    state->stationOffset = state->stationOffset >= state->stationPageSize
                                               ? state->stationOffset - state->stationPageSize
                                               : 0U;
                    StartSearch(state);
                    return 0;
                }
                if (LOWORD(wParam) == kNextPageButtonId) {
                    if (!state->stationsHaveMore) {
                        SetStatus(state, L"Already at the last result page.");
                        return 0;
                    }
                    state->stationOffset += state->stationPageSize;
                    StartSearch(state);
                    return 0;
                }
            }
            if (state != nullptr && HIWORD(wParam) == CBN_SELCHANGE &&
                LOWORD(wParam) == kCompatibilityComboId) {
                if (ComboBox_GetCurSel(state->compatibilityCombo) > 0) {
                    ComboBox_SetCurSel(state->codecCombo, 0);
                }
                state->stationOffset = 0;
                PostMessageW(window, kStartDeferredSearch, 0, 0);
                return 0;
            }
            if (state != nullptr && HIWORD(wParam) == CBN_SELCHANGE &&
                LOWORD(wParam) == kModeComboId) {
                if (ComboBox_GetCurSel(state->codecCombo) > 0) {
                    ComboBox_SetCurSel(state->compatibilityCombo, 0);
                }
                state->stationOffset = 0;
                PostMessageW(window, kStartDeferredSearch, 0, 0);
                return 0;
            }
            if (state != nullptr && HIWORD(wParam) == CBN_SELCHANGE &&
                LOWORD(wParam) == kGenreComboId && state->genresHaveMore &&
                ComboBox_GetCurSel(state->genreCombo) ==
                    static_cast<int>(state->genres.size() + 1U)) {
                SetViewMode(state, ViewMode::Browse);
                SetFocus(state->genreList);
                return 0;
            }
            if (state != nullptr && HIWORD(wParam) == CBN_SELCHANGE &&
                LOWORD(wParam) == kCountryComboId && state->countriesHaveMore &&
                ComboBox_GetCurSel(state->countryCombo) ==
                    static_cast<int>(state->countries.size() + 1U)) {
                SetViewMode(state, ViewMode::Browse);
                SetFocus(state->locationList);
                return 0;
            }
            if (state != nullptr &&
                (LOWORD(wParam) == kGenreComboId || LOWORD(wParam) == kCountryComboId ||
                  LOWORD(wParam) == kLanguageComboId ||
                  LOWORD(wParam) == kBitrateComboId) &&
                HIWORD(wParam) == CBN_SELCHANGE) {
                // Let the skinned combo finish closing and commit its selection
                // before disabling controls for the network request.
                state->stationOffset = 0;
                PostMessageW(window, kStartDeferredSearch, 0, 0);
                return 0;
            }
            if (state != nullptr && LOWORD(wParam) == kPageSizeComboId &&
                HIWORD(wParam) == CBN_SELCHANGE) {
                switch (ComboBox_GetCurSel(state->pageSizeCombo)) {
                    case 0:
                        state->stationPageSize = 50U;
                        break;
                    case 1:
                        state->stationPageSize = 100U;
                        break;
                    default:
                        state->stationPageSize = 200U;
                        break;
                }
                RequestStationFirstPage(state);
                return 0;
            }
            break;
        case WM_CONTEXTMENU:
            if (state != nullptr && reinterpret_cast<HWND>(wParam) == state->stationList) {
                ShowStationContextMenu(state, lParam);
                return 0;
            }
            break;
        case WM_NOTIFY:
            if (state != nullptr) {
                const auto* notification = reinterpret_cast<NMHDR*>(lParam);
                if (notification->code == NM_CUSTOMDRAW &&
                    (notification->hwndFrom == state->locationList ||
                     notification->hwndFrom == state->genreList ||
                     notification->hwndFrom == state->stationList))
                    return DrawListSelection(reinterpret_cast<NMLVCUSTOMDRAW*>(lParam));
                if (notification->idFrom == kGenreListId &&
                    notification->code == LVN_ITEMCHANGED) {
                    const auto* changed = reinterpret_cast<NMLISTVIEW*>(lParam);
                    if ((changed->uChanged & LVIF_STATE) != 0U &&
                        (changed->uOldState & LVIS_SELECTED) == 0U &&
                        (changed->uNewState & LVIS_SELECTED) != 0U) {
                        HandleGenrePaneSelection(state);
                    }
                    return 0;
                }
                if (notification->idFrom == kLocationListId &&
                    notification->code == LVN_ITEMCHANGED) {
                    const auto* changed = reinterpret_cast<NMLISTVIEW*>(lParam);
                    if ((changed->uChanged & LVIF_STATE) != 0U &&
                        (changed->uOldState & LVIS_SELECTED) == 0U &&
                        (changed->uNewState & LVIS_SELECTED) != 0U) {
                        HandleLocationPaneSelection(state);
                    }
                    return 0;
                }
                if (notification->idFrom == kLocationListId &&
                    (notification->code == NM_DBLCLK ||
                     notification->code == LVN_ITEMACTIVATE)) {
                    DrillSelectedCountry(state);
                    return 0;
                }
                if (notification->idFrom == kLocationListId &&
                    notification->code == LVN_KEYDOWN) {
                    const auto* key = reinterpret_cast<NMLVKEYDOWN*>(lParam);
                    if (key->wVKey == VK_RETURN || key->wVKey == VK_RIGHT) {
                        DrillSelectedCountry(state);
                        return 0;
                    }
                }
                if (notification->idFrom == kStationListId &&
                    (notification->code == NM_DBLCLK || notification->code == LVN_ITEMACTIVATE)) {
                    StartPlay(state);
                    return 0;
                }
                if (notification->idFrom == kStationListId &&
                    notification->code == LVN_ITEMCHANGED) {
                    UpdateSelectionSummary(state);
                    return 0;
                }
                if (notification->idFrom == kStationListId &&
                    notification->code == LVN_COLUMNCLICK) {
                    const auto* column = reinterpret_cast<NMLISTVIEW*>(lParam);
                    SortVisibleStationPage(state, column->iSubItem);
                    return 0;
                }
                if (notification->idFrom == kStationListId &&
                    notification->code == LVN_KEYDOWN) {
                    const auto* key = reinterpret_cast<NMLVKEYDOWN*>(lParam);
                    if (key->wVKey == VK_RETURN) {
                        StartPlay(state);
                        return 0;
                    }
                }
            }
            break;
        case kWorkComplete:
            if (state != nullptr) {
                HandleWorkerCompletion(state);
            }
            return 0;
        case kRefreshLocationPane:
            if (state != nullptr && !state->destroying) PopulateLocationPane(state);
            return 0;
#if defined(LIVE_STATIONS_OFFLINE_FIXTURE)
        // Narrow probe observability; no production message or ViewState ABI.
        case WM_APP + 47:
            return state == nullptr ? -1 :
                (state->running.load() ? 1 : 0) | (state->searchQueued ? 2 : 0) |
                (state->updatingBrowseLists ? 4 : 0);
#endif
        case kStartDeferredSearch:
            if (state != nullptr) {
                StartSearch(state);
            }
            return 0;
        case WM_TIMER:
            if (state != nullptr && wParam == kNowPlayingTimerId) {
                RefreshNowPlaying(state);
                return 0;
            }
            break;
        case WM_DESTROY:
            if (state != nullptr) {
                state->destroying = true;
                if (state->searchEdit != nullptr) {
                    RemoveWindowSubclass(
                        state->searchEdit, SearchEditSubclassProc, kSearchEditId);
                }
                StopNowPlayingTracking(state, true);
                state->cancelled.store(true);
                JoinWorker(state);
                UnskinControls(state);
                if (state->windowBrush != nullptr) {
                    DeleteObject(state->windowBrush);
                    state->windowBrush = nullptr;
                }
                if (state->itemBrush != nullptr) {
                    DeleteObject(state->itemBrush);
                    state->itemBrush = nullptr;
                }
            }
            return 0;
        case WM_NCDESTROY:
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            if (state != nullptr) {
                if (state->controlFont != nullptr) DeleteObject(state->controlFont);
                gActiveViewWindows.erase(
                    std::remove(gActiveViewWindows.begin(), gActiveViewWindows.end(), window),
                    gActiveViewWindows.end());
                unsigned int previousActiveViews = gActiveViewCount.load();
                while (previousActiveViews != 0U &&
                       !gActiveViewCount.compare_exchange_weak(
                           previousActiveViews, previousActiveViews - 1U)) {
                }
                TraceViewLifecycle(
                    L"view destroyed",
                    previousActiveViews > 0U ? previousActiveViews - 1U : 0U);
            }
            delete state;
            return 0;
        default:
            break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

int __cdecl PluginInit() {
    gPluginQuitting = false;
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
    // The view paints with the active Winamp Media Library skin brush. A null
    // class brush prevents Windows from flashing a white background first.
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = kViewClassName;
    gViewClass = RegisterClassExW(&windowClass);
    if (gViewClass == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return 3;
    }

    mlTreeItemW item{};
    item.size = sizeof(item);
    item.parentId = 0;
    item.title = const_cast<wchar_t*>(kTreeTitle);
    item.titleLength = std::size(kTreeTitle) - 1U;
    item.hasChildren = FALSE;
    SendMessageW(
        gPlugin.hwndLibraryParent,
        WM_ML_IPC,
        reinterpret_cast<WPARAM>(&item),
        ML_IPC_TREEITEM_ADDW);
    gTreeItemId = static_cast<INT_PTR>(item.id);
    return gTreeItemId == 0 ? 4 : 0;
}

void __cdecl PluginQuit() {
    gPluginQuitting = true;
    // Winamp can unload the plug-in after this callback. No child window may
    // retain our window procedure or state beyond that point.
    while (!gActiveViewWindows.empty()) {
        const HWND view = gActiveViewWindows.back();
        if (IsWindow(view) == FALSE) {
            gActiveViewWindows.pop_back();
            continue;
        }
        if (DestroyWindow(view) == FALSE) {
            TraceViewLifecycle(
                L"PluginQuit could not destroy a live view; class remains registered",
                gActiveViewCount.load());
            return;
        }
    }
    if (gTreeItemId != 0 && gPlugin.hwndLibraryParent != nullptr) {
        SendMessageW(gPlugin.hwndLibraryParent, WM_ML_IPC, gTreeItemId, ML_IPC_DELTREEITEM);
        gTreeItemId = 0;
    }
    const unsigned int activeViews = gActiveViewCount.load();
    if (activeViews != 0U) {
        TraceViewLifecycle(L"PluginQuit retained a live view unexpectedly; class remains registered", activeViews);
        return;
    }
    if (gViewClass != 0) {
        if (UnregisterClassW(kViewClassName, gPlugin.hDllInstance) != FALSE) {
            gViewClass = 0;
        } else {
            TraceViewLifecycle(L"PluginQuit could not unregister the view class", activeViews);
        }
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
    ViewCreationContext context{state.get(), false};
    const HWND view = CreateWindowExW(
        WS_EX_CONTROLPARENT,
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
        &context);
    if (context.ownershipTransferred) {
        state.release();
    }
    if (view == nullptr) {
        return 0;
    }
    if (state != nullptr) {
        DestroyWindow(view);
        return 0;
    }
    return reinterpret_cast<INT_PTR>(view);
}

}  // namespace

extern "C" __declspec(dllexport) winampMediaLibraryPlugin* __cdecl
winampGetMediaLibraryPlugin() {
    return &gPlugin;
}
