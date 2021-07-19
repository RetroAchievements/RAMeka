#ifndef __RETROACHIEVEMENTS_H_
#define __RETROACHIEVEMENTS_H_

#include "../../RAInterface/RA_Interface.h"
#include "../../RAInterface/RA_Consoles.h"

void RA_Initialize();
bool RA_IsInitialized();
void RA_AddMenu();
bool RA_ProcessInputs();

void RA_EnforceHardcoreRestrictions();

void RA_LoadROM(ConsoleID consoleID);
void RA_UpdateMemoryMap();

#endif __RETROACHIEVEMENTS_H_