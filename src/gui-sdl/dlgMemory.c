/*
  Hatari - dlgMemory.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/
const char DlgMemory_fileid[] = "Hatari dlgMemory.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "dialog.h"
#include "sdlgui.h"
#include "memorySnapShot.h"
#include "file.h"
#include "screen.h"


#define DLGMEM_8MB      4
#define DLGMEM_16MB     5
#define DLGMEM_32MB     6
#define DLGMEM_64MB     7
#define DLGMEM_128MB    8

#define DLGMEM_120NS    11
#define DLGMEM_100NS    12
#define DLGMEM_80NS     13
#define DLGMEM_60NS     14

#define DLGMEM_FILENAME 18
#define DLGMEM_SAVE     19
#define DLGMEM_RESTORE  20
#define DLGMEM_AUTOSAVE 21

#define DLGMEM_EXIT     22


static char dlgSnapShotName[36+1];


/* The memory dialog: */
static SGOBJ memorydlg[] =
{
	{ SGBOX, 0, 0, 0,0, 41,27, NULL },
    { SGTEXT, 0, 0, 14,1, 12,1, "Memory options" },

	{ SGBOX, 0, 0, 1,3, 19,9, NULL },
	{ SGTEXT, 0, 0, 2,4, 12,1, "Memory size" },
	{ SGRADIOBUT, 0, 0, 3,6, 6,1, "8 MB" },
	{ SGRADIOBUT, 0, 0, 3,7, 7,1, "16 MB" },
	{ SGRADIOBUT, 0, 0, 3,8, 7,1, "32 MB" },
	{ SGRADIOBUT, 0, 0, 3,9, 7,1, "64 MB" },
	{ SGRADIOBUT, 0, 0, 3,10, 8,1, "128 MB" },
    
    { SGBOX, 0, 0, 21,3, 19,9, NULL },
	{ SGTEXT, 0, 0, 22,4, 12,1, "Memory speed" },
	{ SGRADIOBUT, 0, 0, 23,6, 8,1, "120 ns" },
	{ SGRADIOBUT, 0, 0, 23,7, 8,1, "100 ns" },
	{ SGRADIOBUT, 0, 0, 23,8, 7,1, "80 ns" },
	{ SGRADIOBUT, 0, 0, 23,9, 7,1, "60 ns" },

	{ SGBOX, 0, 0, 1,13, 39,10, NULL },
	{ SGTEXT, 0, 0, 2,14, 17,1, "Load/Save memory state (untested)" },
	{ SGTEXT, 0, 0, 2,16, 20,1, "Snap-shot file name:" },
	{ SGTEXT, 0, 0, 2,17, 36,1, dlgSnapShotName },
	{ SGBUTTON, 0, 0, 8,19, 10,1, "Save" },
	{ SGBUTTON, 0, 0, 22,19, 10,1, "Restore" },
	{ SGCHECKBOX, 0, 0, 2,21, 37,1, "Load/save state at start-up/exit" },

	{ SGBUTTON, SG_DEFAULT, 0, 10,25, 20,1, "Back to main menu" },
	{ -1, 0, 0, 0,0, 0,0, NULL }
};

/* Variable objects */
SGOBJ disable_128mb_option = { SGTEXT, 0, 0, 3,10, 8,1, " " };
SGOBJ enable_128mb_option = { SGRADIOBUT, 0, 0, 3,10, 8,1, "128 MB" };
SGOBJ disable_64mb_option = { SGTEXT, 0, 0, 3,9, 7,1, " " };
SGOBJ enable_64mb_option = { SGRADIOBUT, 0, 0, 3,9, 7,1, "64 MB" };


/**
 * Show and process the memory dialog.
 * @return  true if a memory snapshot has been loaded, false otherwise
 */
bool Dialog_MemDlg(void)
{
	int i;
	int but;

	SDLGui_CenterDlg(memorydlg);

	for (i = DLGMEM_8MB; i <= DLGMEM_128MB; i++)
	{
		memorydlg[i].state &= ~SG_SELECTED;
	}
    
    for (i = DLGMEM_120NS; i <= DLGMEM_60NS; i++)
    {
        memorydlg[i].state &= ~SG_SELECTED;
    }
    
    /* Remove 64 and 128MB option if system is Color Slab,
     * remove 128MB option if system is not NeXTstation */
    if (ConfigureParams.System.nMachineType != NEXT_STATION) {
        memorydlg[DLGMEM_64MB] = enable_64mb_option;
        memorydlg[DLGMEM_128MB] = disable_128mb_option;
    } else {
        if (ConfigureParams.System.bColor) {
            memorydlg[DLGMEM_64MB] = disable_64mb_option;
            memorydlg[DLGMEM_128MB] = disable_128mb_option;
        } else {
            memorydlg[DLGMEM_64MB] = enable_64mb_option;
            memorydlg[DLGMEM_128MB] = enable_128mb_option;
        }
    }

    /* Draw dialog from actual values */
	switch (ConfigureParams.Memory.nMemorySize)
	{
        case 8:
            memorydlg[DLGMEM_8MB].state |= SG_SELECTED;
            break;
        case 16:
            memorydlg[DLGMEM_16MB].state |= SG_SELECTED;
            break;
        case 32:
            memorydlg[DLGMEM_32MB].state |= SG_SELECTED;
            break;
        case 64:
            if (ConfigureParams.System.nMachineType == NEXT_STATION && ConfigureParams.System.bColor) {
                ConfigureParams.Memory.nMemorySize = 32;
                memorydlg[DLGMEM_32MB].state |= SG_SELECTED;
            } else {
                memorydlg[DLGMEM_64MB].state |= SG_SELECTED;
            }
            break;
        case 128:
            if (ConfigureParams.System.nMachineType == NEXT_STATION) {
                if (ConfigureParams.System.bColor) {
                    ConfigureParams.Memory.nMemorySize = 32;
                    memorydlg[DLGMEM_32MB].state |= SG_SELECTED;
                } else {
                    memorydlg[DLGMEM_128MB].state |= SG_SELECTED;
                }
            } else {
                ConfigureParams.Memory.nMemorySize = 64;
                memorydlg[DLGMEM_64MB].state |= SG_SELECTED;
            }
            
            break;
        default:
            ConfigureParams.Memory.nMemorySize = 32;
            memorydlg[DLGMEM_32MB].state |= SG_SELECTED;
            break;
	}
    
    switch (ConfigureParams.Memory.nMemorySpeed) {
        case MEMORY_120NS:
            memorydlg[DLGMEM_120NS].state |= SG_SELECTED;
            break;
        case MEMORY_100NS:
            memorydlg[DLGMEM_100NS].state |= SG_SELECTED;
            break;
        case MEMORY_80NS:
            memorydlg[DLGMEM_80NS].state |= SG_SELECTED;
            break;
        case MEMORY_60NS:
            memorydlg[DLGMEM_60NS].state |= SG_SELECTED;
            break;
            
        default:
            ConfigureParams.Memory.nMemorySpeed = MEMORY_100NS;
            memorydlg[DLGMEM_100NS].state |= SG_SELECTED;
            break;
    }

	File_ShrinkName(dlgSnapShotName, ConfigureParams.Memory.szMemoryCaptureFileName, memorydlg[DLGMEM_FILENAME].w);


	if (ConfigureParams.Memory.bAutoSave)
		memorydlg[DLGMEM_AUTOSAVE].state |= SG_SELECTED;
	else
		memorydlg[DLGMEM_AUTOSAVE].state &= ~SG_SELECTED;

	do
	{
		but = SDLGui_DoDialog(memorydlg, NULL);

		switch (but)
		{
		 case DLGMEM_SAVE:              /* Save memory snap-shot */
			if (SDLGui_FileConfSelect(dlgSnapShotName,
			                          ConfigureParams.Memory.szMemoryCaptureFileName,
			                          memorydlg[DLGMEM_FILENAME].w, true))
			{
				MemorySnapShot_Capture(ConfigureParams.Memory.szMemoryCaptureFileName, true);
			}
			break;
		 case DLGMEM_RESTORE:           /* Load memory snap-shot */
			if (SDLGui_FileConfSelect(dlgSnapShotName,
			                          ConfigureParams.Memory.szMemoryCaptureFileName,
			                          memorydlg[DLGMEM_FILENAME].w, false))
			{
				MemorySnapShot_Restore(ConfigureParams.Memory.szMemoryCaptureFileName, true);
				return true;
			}
			break;
		}
	}
	while (but != DLGMEM_EXIT && but != SDLGUI_QUIT
	        && but != SDLGUI_ERROR && !bQuitProgram );

	/* Read new values from dialog: */

	if (memorydlg[DLGMEM_8MB].state & SG_SELECTED)
		ConfigureParams.Memory.nMemorySize = 8;
	else if (memorydlg[DLGMEM_16MB].state & SG_SELECTED)
		ConfigureParams.Memory.nMemorySize = 16;
	else if (memorydlg[DLGMEM_32MB].state & SG_SELECTED)
		ConfigureParams.Memory.nMemorySize = 32;
	else if (memorydlg[DLGMEM_64MB].state & SG_SELECTED)
		ConfigureParams.Memory.nMemorySize = 64;
	else
		ConfigureParams.Memory.nMemorySize = 128;

    if (memorydlg[DLGMEM_120NS].state & SG_SELECTED)
        ConfigureParams.Memory.nMemorySpeed = MEMORY_120NS;
    else if (memorydlg[DLGMEM_100NS].state & SG_SELECTED)
        ConfigureParams.Memory.nMemorySpeed = MEMORY_100NS;
    else if (memorydlg[DLGMEM_80NS].state & SG_SELECTED)
        ConfigureParams.Memory.nMemorySpeed = MEMORY_80NS;
    else
        ConfigureParams.Memory.nMemorySpeed = MEMORY_60NS;
    
	ConfigureParams.Memory.bAutoSave = (memorydlg[DLGMEM_AUTOSAVE].state & SG_SELECTED);

	return false;
}
