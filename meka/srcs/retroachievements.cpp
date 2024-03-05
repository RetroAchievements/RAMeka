#include "shared.h" // shared.h is pre-compiled. all other headers have to appear after it.

#include "retroachievements.h"

#include "../../RAInterface/RA_Emulators.h"
#include "RA_BuildVer.h"

#include "app_cheatfinder.h"
#include "app_mapview.h"
#include "app_memview.h"
#include "coleco.h"
#include "debugger.h"
#include "fskipper.h"
#include "mappers.h"
#include "vmachine.h"

static bool isInitialized = false;

void RA_EnforceHardcoreRestrictions()
{
    if (MemoryViewer_MainInstance->active)
        MemoryViewer_SwitchMainInstance();

    if (Debugger.enabled && Debugger.active)
        Debugger_Switch();

    if (g_CheatFinder_MainInstance->active)
        CheatFinder_SwitchMainInstance();

    if (TilemapViewer_MainInstance->active)
        TilemapViewer_SwitchMainInstance();

    if (!(opt.Layer_Mask & LAYER_SPRITES))
        Action_Switch_Layer_Sprites();

    if (!(opt.Layer_Mask & LAYER_BACKGROUND))
        Action_Switch_Layer_Background();

    if (fskipper.Throttled_Speed != 60)
    {
        fskipper.Mode = FRAMESKIP_MODE_THROTTLED; /* disable FF */
        fskipper.Throttled_Speed = 60; /* set to default speed */
        Frame_Skipper_Configure(0); /* non-changing update to apply changes */
    }
}

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
        const LPARAM nDialogId = LOWORD(wParam);
        if (nDialogId >= IDM_RA_MENUSTART && nDialogId < IDM_RA_MENUEND)
        {
            // Allegro's message loop does not contain TranslateMessage, so we need to
            // redirect the request to show a dialog to the Console window or the user
            // won't be able to type in any of the tool window text fields.
            PostMessage(ConsoleHWND(), WM_COMMAND, wParam, lParam);
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
    if (RA_HardcoreModeIsActive())
        RA_EnforceHardcoreRestrictions();

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
    RA_AttemptLogin(false);

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

static unsigned char RAMeka_RAMByteReadFnDahjee(unsigned int nAddress)
{
    if (nAddress < 0x2000)
    {
        // $C000-$DFFF = Normal memory - appears _after_ Dahjee expansion memory
        // most of this block is not used by this mapper
        return RAM[nAddress + 0x2000];
    }
    else
    {
        // $2000-$3FFF = Dahjee expansion memory - appears _first_ in RAM[]
        return RAM[nAddress - 0x2000];
    }
}

static void RAMeka_RAMByteWriteFnDahjee(unsigned int nAddress, unsigned char nVal)
{
    if (nAddress < 0x2000)
    {
        // $C000-$DFFF = Normal memory - appears _after_ Dahjee expansion memory
        RAM[nAddress + 0x2000] = nVal;
    }
    else
    {
        // $2000-$3FFF = Dahjee expansion memory - appears _first_ in RAM[]
        RAM[nAddress - 0x2000] = nVal;
    }
}

static unsigned char RAMeka_RAMByteReadFn32kRAM(unsigned int nAddress)
{
    if (nAddress < 0x2000)
    {
        // $C000-$DFFF = Normal memory - appears in third page
        return RAM[nAddress + 0x4000];
    }
    else if (nAddress < 0x4000)
    {
        // $2000-$3FFF = expansion memory - appears in second page
        return RAM[nAddress];
    }
    else
    {
        // $8000-$9FFF = Othello (2K)/The Castle (8K) - appears _first_ in RAM[]
        return RAM[nAddress - 0x4000];
    }
}

static void RAMeka_RAMByteWriteFn32kRAM(unsigned int nAddress, unsigned char nVal)
{
    if (nAddress < 0x2000)
    {
        // $C000-$DFFF = Normal memory - appears in third page
        RAM[nAddress + 0x4000] = nVal;
    }
    else if (nAddress < 0x4000)
    {
        // $2000-$3FFF = expansion memory - appears in second page
        RAM[nAddress] = nVal;
    }
    else
    {
        // $8000-$9FFF = Othello (2K)/The Castle (8K) - appears _first_ in RAM[]
        RAM[nAddress - 0x4000] = nVal;
    }
}

void RA_UpdateMemoryMap()
{
    static int mapper = MAPPER_Auto;
    if (mapper != g_machine.mapper)
    {
        RA_ClearMemoryBanks();

        mapper = g_machine.mapper;
        switch (mapper)
        {
            case MAPPER_SMS_NoMapper: // MasterSystem, GameGear (ROM <48KB)
            case MAPPER_Standard: // MasterSystem, GameGear
                RA_InstallMemoryBank(0, RAMeka_RAMByteReadFn, RAMeka_RAMByteWriteFn, 0x2000); // 8KB
                break;

            case MAPPER_ColecoVision:
                RA_InstallMemoryBank(0, RAMeka_RAMByteReadFn, RAMeka_RAMByteWriteFnColeco, 0x400); // 1KB
                break;

            case MAPPER_SG1000:
                // base SG-1000 only has 1KB of RAM that's mirrored from $C000-$DFFF. 
                // Expansion mode B exposes a full 8KB of RAM from $C000-$DFFF.
                // Meka exposes 4KB for this mapper (see Mapper_Get_RAM_Infos) - reasons unknown
                RA_InstallMemoryBank(0, RAMeka_RAMByteReadFn, RAMeka_RAMByteWriteFn, 0x1000); // 1KB
                break;
            case MAPPER_SG1000_Taiwan_MSX_Adapter_TypeA:
                // 8KB RAM from 0x2000->0x3FFF + regular 2KB ram in 0xC000-0xFFFF range
                RA_InstallMemoryBank(0, RAMeka_RAMByteReadFnDahjee, RAMeka_RAMByteWriteFnDahjee, 0x4000); // 16KB
                break;
            case MAPPER_32kRAM:
                // Othello mapper exposes 8KB at $8000, and The Castle exposes 32KB at $8000
                RA_InstallMemoryBank(0, RAMeka_RAMByteReadFn32kRAM, RAMeka_RAMByteWriteFn32kRAM, 0x6000);
                break;

            default:
                // until we actually support the other custom mappers, just attempt to expose the basic RAM supported by MAPPER_SMS_NoMapper
                RA_InstallMemoryBank(0, RAMeka_RAMByteReadFn, RAMeka_RAMByteWriteFn, 0x2000); // 8KB
                break;

        }
    }
}

void RA_LoadROM(ConsoleID consoleID)
{
    RA_SetConsoleID(consoleID);
    RA_OnLoadNewRom(ROM, tsms.Size_ROM);
}

bool RA_ProcessInputs()
{
    if (!RA_IsOverlayFullyVisible())
        return false;

    for (int src_index = 0; src_index < Inputs.Sources_Max; src_index++)
    {
        const t_input_src* src = Inputs.Sources[src_index];
        if (src->player == 0 && src->enabled)
        {
            ControllerInput input;
            input.m_bUpPressed = src->Map[INPUT_MAP_DIGITAL_UP].current_value ? 1 : 0;
            input.m_bDownPressed = src->Map[INPUT_MAP_DIGITAL_DOWN].current_value ? 1 : 0;
            input.m_bLeftPressed = src->Map[INPUT_MAP_DIGITAL_LEFT].current_value ? 1 : 0;
            input.m_bRightPressed = src->Map[INPUT_MAP_DIGITAL_RIGHT].current_value ? 1 : 0;
            input.m_bConfirmPressed = src->Map[INPUT_MAP_BUTTON1].current_value ? 1 : 0;
            input.m_bCancelPressed = src->Map[INPUT_MAP_BUTTON2].current_value ? 1 : 0;
            input.m_bQuitPressed = src->Map[INPUT_MAP_PAUSE_START].current_value ? 1 : 0;

            RA_NavigateOverlay(&input);
            break;
        }
    }

    return true;
}