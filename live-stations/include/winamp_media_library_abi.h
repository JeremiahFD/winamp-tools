/*
** Copyright (C) 2003-2014 Winamp SA
**
** This software is provided 'as-is', without any express or implied warranty. In no event will the authors be held
** liable for any damages arising from the use of this software.
**
** Permission is granted to anyone to use this software for any purpose, including commercial applications, and to
** alter it and redistribute it freely, subject to the following restrictions:
**
**   1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software.
**      If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
**
**   2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
**
**   3. This notice may not be removed or altered from any source distribution.
*/

#pragma once

#include <cstddef>
#include <windows.h>

// Minimal, independently maintained interoperability declarations derived from
// Winamp's public ml.h and wa_ipc.h. This is not a copy of the Winamp SDK.

constexpr int MLHDR_VER = 0x17;
constexpr UINT WM_ML_IPC = WM_USER + 0x1000;
constexpr LPARAM ML_IPC_ADDTREEITEM = 0x0101;
constexpr LPARAM ML_IPC_DELTREEITEM = 0x0103;
constexpr LPARAM ML_IPC_TREEITEM_ADDW = 0x0133;
constexpr LPARAM ML_IPC_SKIN_LISTVIEW = 0x0500;
constexpr LPARAM ML_IPC_UNSKIN_LISTVIEW = 0x0501;
constexpr LPARAM ML_IPC_LISTVIEW_UPDATE = 0x0502;
constexpr LPARAM ML_IPC_SKIN_COMBOBOX = 0x0508;
constexpr LPARAM ML_IPC_UNSKIN_COMBOBOX = 0x0509;
constexpr LPARAM ML_IPC_SKIN_WADLG_GETFUNC = 0x0600;
constexpr int ML_MSG_TREE_ONCREATEVIEW = 0x100;

// Selectors exposed by ML_IPC_SKIN_WADLG_GETFUNC. Function pointers and the
// font remain owned by Winamp's Media Library plug-in.
constexpr WPARAM ML_WADLG_GETCOLOR = 1;
constexpr WPARAM ML_WADLG_HANDLEDIALOGMSGS = 2;
constexpr WPARAM ML_WADLG_DRAWCHILDWINDOWBORDERS = 3;
constexpr WPARAM ML_WADLG_SKINFONT = 66;

// Indices from Winamp's public wa_dlg.h color table.
enum WinampDialogColor {
    WADLG_ITEMBG = 0,
    WADLG_ITEMFG = 1,
    WADLG_WNDBG = 2,
    WADLG_BUTTONFG = 3,
    WADLG_WNDFG = 4,
    WADLG_HILITE = 5,
};

constexpr int DCW_SUNKENBORDER = 0x00010000;

using WADlgGetColorFunction = int(__cdecl*)(int index);
using WADlgHandleDialogMessagesFunction =
    LRESULT(__cdecl*)(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
using WADlgDrawChildWindowBordersFunction =
    void(__cdecl*)(HWND window, int* controls, int controlCount);

struct winampMediaLibraryPlugin {
    int version;
    const char* description;
    int(__cdecl* init)();
    void(__cdecl* quit)();
    INT_PTR(__cdecl* MessageProc)(int messageType, INT_PTR param1, INT_PTR param2, INT_PTR param3);
    HWND hwndWinampParent;
    HWND hwndLibraryParent;
    HINSTANCE hDllInstance;
    void* service;
};

struct mlAddTreeItemStruct {
    INT_PTR parentId;
    char* title;
    int hasChildren;
    INT_PTR thisId;
};

struct mlTreeItemW {
    std::size_t size;
    UINT_PTR id;
    UINT_PTR parentId;
    wchar_t* title;
    std::size_t titleLength;
    BOOL hasChildren;
    int imageIndex;
};

constexpr UINT WM_WA_IPC = WM_USER;
constexpr LPARAM IPC_PLAYFILEW = 1100;
constexpr LPARAM IPC_ISPLAYING = 104;
constexpr LPARAM IPC_SETPLAYLISTPOS = 121;
constexpr LPARAM IPC_GETLISTLENGTH = 124;
constexpr LPARAM IPC_GETLISTPOS = 125;
constexpr LPARAM IPC_GETPLAYLISTFILEW = 214;
constexpr LPARAM IPC_STARTPLAY = 102;
constexpr LPARAM IPC_GET_PLAYING_TITLE = 3034;

// IPC_GETPLAYLISTFILEW and IPC_GET_PLAYING_TITLE return Winamp-owned string
// pointers for in-process callers. Consumers must copy the text immediately.
