/*
  Hatari - dlgRom.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/
const char DlgRom_fileid[] = "Hatari dlgRom.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "configuration.h"
#include "dialog.h"
#include "sdlgui.h"
#include "file.h"
#include "paths.h"


#define DLGROM_ROM030_DEFAULT  4
#define DLGROM_ROM030_BROWSE   5
#define DLGROM_ROM030_NAME     6

#define DLGROM_ROM040_DEFAULT  9
#define DLGROM_ROM040_BROWSE  10
#define DLGROM_ROM040_NAME    11

#define DLGROM_EXIT           13


/* The ROM dialog: */
static SGOBJ romdlg[] =
{
	{ SGBOX, 0, 0, 0,0, 52,22, NULL },
    { SGTEXT, 0, 0, 22,1, 9,1, "ROM setup" },

	{ SGBOX, 0, 0, 1,4, 50,5, NULL },
	{ SGTEXT, 0, 0, 2,5, 30,1, "ROM for 68030 based systems:" },
    { SGBUTTON, 0, 0, 32,5, 9,1, "Default" },
	{ SGBUTTON, 0, 0, 42,5, 8,1, "Browse" },
	{ SGTEXT, 0, 0, 2,7, 46,1, NULL },
    
	{ SGBOX, 0, 0, 1,10, 50,5, NULL },
	{ SGTEXT, 0, 0, 2,11, 30,1, "ROM for 68040 based systems:" },
	{ SGBUTTON, 0, 0, 32,11, 9,1, "Default" },
	{ SGBUTTON, 0, 0, 42,11, 8,1, "Browse" },
	{ SGTEXT, 0, 0, 2,13, 46,1, NULL },
	{ SGTEXT, 0, 0, 2,17, 25,1, "A reset is needed after changing these options." },
	{ SGBUTTON, SG_DEFAULT, 0, 16,19, 20,1, "Back to main menu" },
	{ -1, 0, 0, 0,0, 0,0, NULL }
};


/*-----------------------------------------------------------------------*/
/**
 * Show and process the ROM dialog.
 */
void DlgRom_Main(void)
{
	char szDlgRom030Name[47];
	char szDlgRom040Name[47];
	int but;

	SDLGui_CenterDlg(romdlg);

	File_ShrinkName(szDlgRom030Name, ConfigureParams.Rom.szRom030FileName, sizeof(szDlgRom030Name)-1);
	romdlg[DLGROM_ROM030_NAME].txt = szDlgRom030Name;

	File_ShrinkName(szDlgRom040Name, ConfigureParams.Rom.szRom040FileName, sizeof(szDlgRom040Name)-1);
	romdlg[DLGROM_ROM040_NAME].txt = szDlgRom040Name;

	do
	{
		but = SDLGui_DoDialog(romdlg, NULL);
		switch (but)
		{
            case DLGROM_ROM030_DEFAULT:
                sprintf(ConfigureParams.Rom.szRom030FileName, "%s%cRev_1.0_v41.BIN",
                        Paths_GetWorkingDir(), PATHSEP);
                File_ShrinkName(szDlgRom030Name, ConfigureParams.Rom.szRom030FileName, sizeof(szDlgRom030Name)-1);
//                strcpy(szDlgRom030Name, "./Rev_1.0_v41.BIN");
//                strcpy(ConfigureParams.Rom.szRom030FileName, "./Rev_1.0_v41.BIN");
                break;
                
            case DLGROM_ROM030_BROWSE:
                /* Show and process the file selection dlg */
                SDLGui_FileConfSelect(szDlgRom030Name,
                                      ConfigureParams.Rom.szRom030FileName,
                                      sizeof(szDlgRom030Name)-1,
                                      false);
                break;
                
            case DLGROM_ROM040_DEFAULT:
                sprintf(ConfigureParams.Rom.szRom040FileName, "%s%cRev_2.5_v66.BIN",
                        Paths_GetWorkingDir(), PATHSEP);
                File_ShrinkName(szDlgRom040Name, ConfigureParams.Rom.szRom040FileName, sizeof(szDlgRom040Name)-1);
//                strcpy(szDlgRom040Name, "./Rev_2.5_v66.BIN");
//                strcpy(ConfigureParams.Rom.szRom040FileName, "./Rev_2.5_v66.BIN");
                break;
                
            case DLGROM_ROM040_BROWSE:
                /* Show and process the file selection dlg */
                SDLGui_FileConfSelect(szDlgRom040Name,
                                      ConfigureParams.Rom.szRom040FileName,
                                      sizeof(szDlgRom040Name)-1,
                                      false);
                break;
		}
	}
	while (but != DLGROM_EXIT && but != SDLGUI_QUIT
	       && but != SDLGUI_ERROR && !bQuitProgram);
}
