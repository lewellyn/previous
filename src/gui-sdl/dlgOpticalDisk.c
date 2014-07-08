/*
  Hatari - dlgHardDisk.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/
const char DlgOpticalDisk_fileid[] = "Previous dlgOpticalDisk.c : " __DATE__ " " __TIME__;

#include <assert.h>
#include "main.h"
#include "configuration.h"
#include "dialog.h"
#include "sdlgui.h"
#include "file.h"
#include "mo.h"


#define MODLG_CONNECTED0        3
#define MODLG_INSERT0           6
#define MODLG_PROTECTED0        7
#define MODLG_DISKNAME0         8

#define MODLG_CONNECTED1        10
#define MODLG_INSERT1           13
#define MODLG_PROTECTED1        14
#define MODLG_DISKNAME1         15

#define DISKDLG_EXIT            17

/* Constant strings */
#define MODLG_EJECT_WARNING     "WARNING: Don't eject manually if a guest system is running. Risk of data loss. Eject now?"
#define MODLG_PROTECT_ERROR     "ERROR: Can't change protection of cartridge while it is inserted. Please eject first."

/* Variable strings */
char inserteject0[16] = "Insert";
char inserteject1[16] = "Insert";


/* The magneto optical drive dialog: */
static SGOBJ modlg[] =
{
    { SGBOX, 0, 0, 0,0, 64,30, NULL },
	{ SGTEXT, 0, 0, 21,1, 10,1, "Magneto-optical drives" },

	{ SGTEXT, 0, 0, 2,4, 14,1, "MO Drive 0:" },
    { SGCHECKBOX, 0, 0, 16, 4, 11, 1, "Connected" },
    
    { SGBOX, 0, 0, 2,6, 60,6, NULL },
    { SGTEXT, 0, 0, 4,7, 14,1, "Optical disk:" },
	{ SGBUTTON, 0, 0, 20,7, 10,1, inserteject0 },
	{ SGCHECKBOX, 0, 0, 32,7, 17,1, "Write protected" },
	{ SGTEXT, 0, 0, 4,9, 56,1, NULL },

	{ SGTEXT, 0, 0, 2,14, 14,1, "MO Drive 1:                (disabled)" },
    { SGCHECKBOX, 0, 0, 16, 14, 11, 1, "Connected" },
    
    { SGBOX, 0, 0, 2,16, 60,6, NULL },
    { SGTEXT, 0, 0, 4,17, 14,1, "Optical disk:" },
	{ SGBUTTON, 0, 0, 20,17, 10,1, inserteject1 },
	{ SGCHECKBOX, 0, 0, 32,17, 17,1, "Write protected" },
	{ SGTEXT, 0, 0, 4,19, 56,1, NULL },
    
    { SGTEXT, 0, 0, 2,24, 14,1, "Note: Magneto-optical drives only work with non-turbo Cubes." },

    { SGBUTTON, SG_DEFAULT, 0, 22,27, 20,1, "Back to main menu" },
	{ -1, 0, 0, 0,0, 0,0, NULL }
};


/**
 * Let user browse given directory, set directory if one selected.
 * return false if none selected, otherwise return true.
 */
/*static bool DlgDisk_BrowseDir(char *dlgname, char *confname, int maxlen)
{
	char *str, *selname;

	selname = SDLGui_FileSelect(confname, NULL, false);
	if (selname)
	{
		strcpy(confname, selname);
		free(selname);

		str = strrchr(confname, PATHSEP);
		if (str != NULL)
			str[1] = 0;
		File_CleanFileName(confname);
		File_ShrinkName(dlgname, confname, maxlen);
		return true;
	}
	return false;
}*/


/**
 * Show and process the hard disk dialog.
 */
void DlgOptical_Main(void)
{
    int but;
    char dlgname_mo[MO_MAX_DRIVES][64];

	SDLGui_CenterDlg(modlg);

	/* Set up dialog to actual values: */

	/* MO disk image: */
    if (ConfigureParams.MO.drive[0].bDriveConnected && ConfigureParams.MO.drive[0].bDiskInserted) {
        File_ShrinkName(dlgname_mo[0], ConfigureParams.MO.drive[0].szImageName,
                        modlg[MODLG_DISKNAME0].w);
        sprintf(inserteject0, "Eject");
    } else {
        dlgname_mo[0][0] = '\0';
        sprintf(inserteject0, "Insert");
    }
    modlg[MODLG_DISKNAME0].txt = dlgname_mo[0];
    
    if (ConfigureParams.MO.drive[1].bDriveConnected && ConfigureParams.MO.drive[1].bDiskInserted) {
        File_ShrinkName(dlgname_mo[1], ConfigureParams.MO.drive[1].szImageName,
                        modlg[MODLG_DISKNAME1].w);
        sprintf(inserteject1, "Eject");
    } else {
        dlgname_mo[1][0] = '\0';
        sprintf(inserteject1, "Insert");
    }
    modlg[MODLG_DISKNAME1].txt = dlgname_mo[1];


    /* Drive connected true or false? */
    if (ConfigureParams.MO.drive[0].bDriveConnected)
        modlg[MODLG_CONNECTED0].state |= SG_SELECTED;
    else
        modlg[MODLG_CONNECTED0].state &= ~SG_SELECTED;
    
    if (ConfigureParams.MO.drive[1].bDriveConnected)
        modlg[MODLG_CONNECTED1].state |= SG_SELECTED;
    else
        modlg[MODLG_CONNECTED1].state &= ~SG_SELECTED;

    /* Write protection true or false? */
    if (ConfigureParams.MO.drive[0].bWriteProtected)
        modlg[MODLG_PROTECTED0].state |= SG_SELECTED;
    else
        modlg[MODLG_PROTECTED0].state &= ~SG_SELECTED;
    
    if (ConfigureParams.MO.drive[1].bWriteProtected)
        modlg[MODLG_PROTECTED1].state |= SG_SELECTED;
    else
        modlg[MODLG_PROTECTED1].state &= ~SG_SELECTED;

    
	/* Draw and process the dialog */
	do
	{
		but = SDLGui_DoDialog(modlg, NULL);

		switch (but)
		{
            case MODLG_INSERT0:
                if (!ConfigureParams.MO.drive[0].bDiskInserted) {
                    if (SDLGui_FileConfSelect(dlgname_mo[0], ConfigureParams.MO.drive[0].szImageName, modlg[MODLG_DISKNAME0].w, false)) {
                        ConfigureParams.MO.drive[0].bDiskInserted = true;
                        sprintf(inserteject0, "Eject");
                        if (!ConfigureParams.MO.drive[0].bDriveConnected) {
                            ConfigureParams.MO.drive[0].bDriveConnected = true;
                            modlg[MODLG_CONNECTED0].state |= SG_SELECTED;
                        }
                        MO_Insert(0);
                    }
                } else {
                    if (DlgAlert_Query(MODLG_EJECT_WARNING)) {
                        ConfigureParams.MO.drive[0].bDiskInserted = false;
                        sprintf(inserteject0, "Insert");
                        ConfigureParams.MO.drive[0].szImageName[0] = '\0';
                        dlgname_mo[0][0] = '\0';
                        MO_Eject(0);
                    }
                }
                break;
            case MODLG_CONNECTED0:
                if (ConfigureParams.MO.drive[0].bDriveConnected) {
                    ConfigureParams.MO.drive[0].bDriveConnected = false;
                    ConfigureParams.MO.drive[0].bDiskInserted = false;
                    sprintf(inserteject0, "Insert");
                    ConfigureParams.MO.drive[0].bWriteProtected = false;
                    modlg[MODLG_PROTECTED0].state &= ~SG_SELECTED;
                    ConfigureParams.MO.drive[0].szImageName[0] = '\0';
                    dlgname_mo[0][0] = '\0';
                } else {
                    ConfigureParams.MO.drive[0].bDriveConnected = true;
                }
                break;
            case MODLG_PROTECTED0:
                if (ConfigureParams.MO.drive[0].bDiskInserted) {
                    DlgAlert_Notice(MODLG_PROTECT_ERROR);
                    if (ConfigureParams.MO.drive[0].bWriteProtected) {
                        modlg[MODLG_PROTECTED0].state|=SG_SELECTED;
                    } else {
                        modlg[MODLG_PROTECTED0].state&=~SG_SELECTED;
                    }
                } else {
                    ConfigureParams.MO.drive[0].bWriteProtected = modlg[MODLG_PROTECTED0].state&SG_SELECTED;
                }
                break;
#if 0   /* FIXME: Enable this after fixing dual drive issues */
            case MODLG_INSERT1:
                if (!ConfigureParams.MO.drive[1].bDiskInserted) {
                    if (SDLGui_FileConfSelect(dlgname_mo[1], ConfigureParams.MO.drive[1].szImageName, modlg[MODLG_DISKNAME1].w, false)) {
                        ConfigureParams.MO.drive[1].bDiskInserted = true;
                        sprintf(inserteject1, "Eject");
                        if (!ConfigureParams.MO.drive[1].bDriveConnected) {
                            ConfigureParams.MO.drive[1].bDriveConnected = true;
                            modlg[MODLG_CONNECTED1].state |= SG_SELECTED;
                        }
                        MO_Insert(1);
                    }
                } else {
                    if (DlgAlert_Query(MODLG_EJECT_WARNING)) {
                        ConfigureParams.MO.drive[1].bDiskInserted = false;
                        sprintf(inserteject1, "Insert");
                        ConfigureParams.MO.drive[1].szImageName[0] = '\0';
                        dlgname_mo[1][0] = '\0';
                        MO_Eject(1);
                    }
                }
                break;
            case MODLG_CONNECTED1:
                if (ConfigureParams.MO.drive[1].bDriveConnected) {
                    ConfigureParams.MO.drive[1].bDriveConnected = false;
                    ConfigureParams.MO.drive[1].bDiskInserted = false;
                    sprintf(inserteject1, "Insert");
                    ConfigureParams.MO.drive[1].bWriteProtected = false;
                    modlg[MODLG_PROTECTED1].state &= ~SG_SELECTED;
                    ConfigureParams.MO.drive[1].szImageName[0] = '\0';
                    dlgname_mo[1][0] = '\0';
                } else {
                    ConfigureParams.MO.drive[1].bDriveConnected = true;
                }
                break;
            case MODLG_PROTECTED1:
                if (ConfigureParams.MO.drive[1].bDiskInserted) {
                    DlgAlert_Notice(MODLG_PROTECT_ERROR);
                    if (ConfigureParams.MO.drive[1].bWriteProtected) {
                        modlg[MODLG_PROTECTED1].state|=SG_SELECTED;
                    } else {
                        modlg[MODLG_PROTECTED1].state&=~SG_SELECTED;
                    }
                } else {
                    ConfigureParams.MO.drive[1].bWriteProtected = modlg[MODLG_PROTECTED1].state&SG_SELECTED;
                }
                break;
#endif
            default:
                break;
        }
	}
	while (but != DISKDLG_EXIT && but != SDLGUI_QUIT
	        && but != SDLGUI_ERROR && !bQuitProgram);
}

