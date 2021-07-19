#include "shared.h" // shared.h is pre-compiled. all other headers have to appear after it.

#include "retroachievements.h"

#include "../../RAInterface/RA_Emulators.h"
#include "RA_BuildVer.h"

#include "vmachine.h"
//#include "app_memview.h"
//#include "app_cheatfinder.h"
//#include "debugger.h"
#include "coleco.h"

static bool isInitialized = false;

static int GetMenuItemIndex(HMENU hMenu, const char* ItemName)
{
    int index = 0;
    char buf[256];

    while (index < GetMenuItemCount(hMenu))
    {
        if (GetMenuString(hMenu, index, buf, sizeof(buf) - 1, MF_BYPOSITION))
        {
            if (!strcmp(ItemName, buf))
                return index;
        }
        index++;
    }
    return -1;
}

static void RebuildMenu()
{
    const HWND hWnd = al_get_win_window_handle(g_display);
    HMENU hMainMenu = GetMenu(hWnd);
    if (!hMainMenu)
        return;

    // if RetroAchievements submenu exists, destroy it
    int index = GetMenuItemIndex(hMainMenu, "&RetroAchievements");
    if (index >= 0)
        DeleteMenu(hMainMenu, index, MF_BYPOSITION);

    // append RetroAchievements menu
    AppendMenu(hMainMenu, MF_POPUP | MF_STRING, (UINT_PTR)RA_CreatePopupMenu(), TEXT("&RetroAchievements"));

    // repaint
    DrawMenuBar(hWnd);
}

static bool RA_MenuDispatch(ALLEGRO_DISPLAY* display, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* lResult, void* userdata)
{
    if (message == WM_COMMAND)
    {
        if (LOWORD(wParam) >= IDM_RA_MENUSTART &&
            LOWORD(wParam) < IDM_RA_MENUEND)
        {
            RA_InvokeDialog(LOWORD(wParam));
            return true;
        }
    }

    return false;
}

void RA_AddMenu()
{
    const HWND hWnd = al_get_win_window_handle(g_display);

    HMENU hMainMenu = GetMenu(hWnd);
    if (hMainMenu)
        return;

    // get the client rectangle before we add the menu so we no how much to resize the window by.
    RECT rcClientRect;
    GetClientRect(hWnd, &rcClientRect);
    int nHeight = rcClientRect.bottom - rcClientRect.top;

    // create the menu bar and add the "RetroAchievements" item
    hMainMenu = CreateMenu();

    HMENU hRAMenu = RA_CreatePopupMenu();
    if (hRAMenu)
    {
        AppendMenu(hMainMenu, MF_POPUP | MF_STRING, (UINT_PTR)RA_CreatePopupMenu(), TEXT("&RetroAchievements"));
    }
    else
    {
        // create placeholder menu called "RetroAchievements" and populate it with a dummy "not loaded" entry
        HMENU hMenuItem = CreatePopupMenu();
        AppendMenu(hMenuItem, MF_POPUP | MF_STRING, 100001, "(Not Yet Loaded)");
        AppendMenu(hMainMenu, MF_STRING | MF_POPUP, (UINT)hMenuItem, "&RetroAchievements");
    }

    SetMenu(hWnd, hMainMenu);
    al_win_add_window_callback(g_display, RA_MenuDispatch, NULL);

    // get the updated client rectangle
    GetClientRect(hWnd, &rcClientRect);
    int nAdjust = nHeight - (rcClientRect.bottom - rcClientRect.top);

    // adjust the window size to account for the menu bar
    RECT rcRect;
    GetWindowRect(hWnd, &rcRect);
    MoveWindow(hWnd, rcRect.left, rcRect.top, rcRect.right - rcRect.left, rcRect.bottom - rcRect.top + nAdjust, FALSE);

    // force repaint
    InvalidateRect(hWnd, NULL, TRUE);
    DrawMenuBar(hWnd);
}

static void CauseUnpause()
{
    if (g_machine_flags & MACHINE_PAUSED)
        Machine_Pause(); // Machine_Pause works as a toggle
}

static void CausePause()
{
    if (!(g_machine_flags & MACHINE_PAUSED))
        Machine_Pause(); // Machine_Pause works as a toggle
}

static void GetEstimatedGameTitle(char* sNameOut)
{
    // sNameOut points to a 64 character buffer.
    // sNameOut should have copied into it the estimated game title 
    // for the ROM, if one can be inferred from the ROM.
    strncpy(sNameOut, g_env.Paths.MediaImageFile, 64);
    sNameOut[63] = '\0';
    StrPath_RemoveExtension(sNameOut);
}

static void ResetEmulation()
{
    Machine_Reset();
}

static void LoadROMFromEmu(const char* sFullPath)
{
}

bool RA_IsInitialized()
{
    return isInitialized;
}

void RA_Initialize()
{
    // initialize the DLL
    HWND hWnd = al_get_win_window_handle(g_display);
    RA_Init(hWnd, RA_Meka, RAMEKA_VERSION);
    isInitialized = true;

    // provide callbacks to the DLL
    RA_InstallSharedFunctions(NULL, &CauseUnpause, &CausePause, &RebuildMenu, &GetEstimatedGameTitle, &ResetEmulation, &LoadROMFromEmu);

    // add a placeholder menu item and start the login process - the menu will be updated when the login completes
    RebuildMenu();
    RA_AttemptLogin(true);

    // ensure the titlebar text matches the expected format
    RA_UpdateAppTitle("");
}

static unsigned char RAMeka_RAMByteReadFn(unsigned int nAddress) 
{
    return RAM[nAddress];
}

static void RAMeka_RAMByteWriteFn(unsigned int nAddress, unsigned char nVal)
{
    RAM[nAddress] = nVal;
}

static void RAMeka_RAMByteWriteFnColeco(unsigned int nAddress, unsigned char nVal)
{
    // special case for ColecoVision crazy mirroring
    Write_Mapper_Coleco(0x6000 + nAddress, nVal);
}

void RA_LoadROM(ConsoleID consoleID)
{
    static ConsoleID currentConsoleID = UnknownConsoleID;
    if (consoleID != currentConsoleID)
    {
        RA_ClearMemoryBanks();
        RA_SetConsoleID(consoleID);

        switch (consoleID)
        {
            case MasterSystem:
                RA_InstallMemoryBank(0, RAMeka_RAMByteReadFn, RAMeka_RAMByteWriteFn, 0x2000); // 8KB
                break;
            case GameGear:
                RA_InstallMemoryBank(0, RAMeka_RAMByteReadFn, RAMeka_RAMByteWriteFn, 0x2000); // 8KB
                break;
            case Colecovision:
                RA_InstallMemoryBank(0, RAMeka_RAMByteReadFn, RAMeka_RAMByteWriteFnColeco, 0x400); // 1KB
                break;
            case SG1000:
                RA_InstallMemoryBank(0, RAMeka_RAMByteReadFn, RAMeka_RAMByteWriteFn, 0x400); // 1KB
                break;
        }
    }

    RA_OnLoadNewRom(ROM, tsms.Size_ROM);
}
