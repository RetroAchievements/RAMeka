#ifndef __RETROACHIEVEMENTS_H_
#define __RETROACHIEVEMENTS_H_

#include "../../RAInterface/RA_Interface.h"
#include "../../RAInterface/RA_Consoles.h"

void RA_Initialize();
bool RA_IsInitialized();
void RA_AddMenu();

void RA_LoadROM(ConsoleID consoleID);

#endif __RETROACHIEVEMENTS_H_