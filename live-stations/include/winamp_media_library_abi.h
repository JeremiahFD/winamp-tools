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

#include <windows.h>

// Minimal, independently maintained interoperability declarations derived from
// Winamp's public ml.h and wa_ipc.h. This is not a copy of the Winamp SDK.

constexpr int MLHDR_VER = 0x17;
constexpr UINT WM_ML_IPC = WM_USER + 0x1000;
constexpr LPARAM ML_IPC_ADDTREEITEM = 0x0101;
constexpr LPARAM ML_IPC_DELTREEITEM = 0x0103;
constexpr int ML_MSG_TREE_ONCREATEVIEW = 0x100;

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

constexpr UINT WM_WA_IPC = WM_USER;
constexpr LPARAM IPC_PLAYFILEW = 1100;
constexpr LPARAM IPC_SETPLAYLISTPOS = 121;
constexpr LPARAM IPC_GETLISTLENGTH = 124;
constexpr LPARAM IPC_STARTPLAY = 102;
