/*
  Hatari - dlgHardDisk.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/
const char DlgHardDisk_fileid[] = "Hatari dlgHardDisk.c : " __DATE__ " " __TIME__;

#include <assert.h>
#include "main.h"
#include "configuration.h"
#include "dialog.h"
#include "sdlgui.h"
#include "file.h"


#define DISKDLG_CDROM0             3
#define DISKDLG_SCSIEJECT0         4
#define DISKDLG_SCSIBROWSE0        5
#define DISKDLG_SCSINAME0          6

#define DISKDLG_EXIT               31


/* The disks dialog: */
static SGOBJ diskdlg[] =
{
    { SGBOX, 0, 0, 0,0, 64,29, NULL },
	{ SGTEXT, 0, 0, 27,1, 10,1, "SCSI disks" },

	{ SGTEXT, 0, 0, 2,3, 14,1, "SCSI Disk 0:" },
    { SGCHECKBOX, 0, 0, 36, 3, 8, 1, "CD-ROM" },
	{ SGBUTTON, 0, 0, 46,3, 7,1, "Eject" },
	{ SGBUTTON, 0, 0, 54,3, 8,1, "Browse" },
	{ SGTEXT, 0, 0, 3,4, 58,1, NULL },
   
    /* experimental */
    { SGTEXT, 0, 0, 2, 6, 14,1, "SCSI Disk 1:" },
    { SGCHECKBOX, 0, 0, 36, 6, 8, 1, "CD-ROM" },
	{ SGBUTTON, 0, 0, 46,6, 7,1, "Eject" },
	{ SGBUTTON, 0, 0, 54,6, 8,1, "Browse" },
//	{ SGTEXT, 0, 0, 3,7, 58,1, NULL },

    { SGTEXT, 0, 0, 2, 9, 14,1, "SCSI Disk 2:" },
    { SGCHECKBOX, 0, 0, 36, 9, 8, 1, "CD-ROM" },
	{ SGBUTTON, 0, 0, 46,9, 7,1, "Eject" },
	{ SGBUTTON, 0, 0, 54,9, 8,1, "Browse" },
    //	{ SGTEXT, 0, 0, 3,10, 58,1, NULL },

    { SGTEXT, 0, 0, 2, 12, 14,1, "SCSI Disk 3:" },
    { SGCHECKBOX, 0, 0, 36, 12, 8, 1, "CD-ROM" },
	{ SGBUTTON, 0, 0, 46,12, 7,1, "Eject" },
	{ SGBUTTON, 0, 0, 54,12, 8,1, "Browse" },
    //	{ SGTEXT, 0, 0, 3,13, 58,1, NULL },

    { SGTEXT, 0, 0, 2, 15, 14,1, "SCSI Disk 4:" },
    { SGCHECKBOX, 0, 0, 36, 15, 8, 1, "CD-ROM" },
	{ SGBUTTON, 0, 0, 46,15, 7,1, "Eject" },
	{ SGBUTTON, 0, 0, 54,15, 8,1, "Browse" },
    //	{ SGTEXT, 0, 0, 3,16, 58,1, NULL },

    { SGTEXT, 0, 0, 2, 18, 14,1, "SCSI Disk 5:" },
    { SGCHECKBOX, 0, 0, 36, 18, 8, 1, "CD-ROM" },
	{ SGBUTTON, 0, 0, 46,18, 7,1, "Eject" },
	{ SGBUTTON, 0, 0, 54,18, 8,1, "Browse" },
    //	{ SGTEXT, 0, 0, 3,19, 58,1, NULL },

    { SGTEXT, 0, 0, 2, 21, 14,1, "SCSI Disk 6:" },
    { SGCHECKBOX, 0, 0, 36, 21, 8, 1, "CD-ROM" },
	{ SGBUTTON, 0, 0, 46,21, 7,1, "Eject" },
	{ SGBUTTON, 0, 0, 54,21, 8,1, "Browse" },
    //	{ SGTEXT, 0, 0, 3,22, 58,1, NULL },


    { SGBUTTON, SG_DEFAULT, 0, 22,26, 20,1, "Back to main menu" },
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
void DlgHardDisk_Main(void)
{
    int but;
    char dlgname_scsi0[64];
//	int but, i;
//	char dlgname_gdos[64], dlgname_acsi[64];
//	char dlgname_ide_master[64], dlgname_ide_slave[64];

	SDLGui_CenterDlg(diskdlg);

	/* Set up dialog to actual values: */

	/* SCSI hard disk image: */
	if (ConfigureParams.HardDisk.bUseHardDiskImage)
		File_ShrinkName(dlgname_scsi0, ConfigureParams.HardDisk.szHardDiskImage,
		                diskdlg[DISKDLG_SCSINAME0].w);
	else
		dlgname_scsi0[0] = '\0';
	diskdlg[DISKDLG_SCSINAME0].txt = dlgname_scsi0;


    
	/* Draw and process the dialog */
	do
	{
		but = SDLGui_DoDialog(diskdlg, NULL);
		switch (but)
		{
		 case DISKDLG_SCSIEJECT0:
			ConfigureParams.HardDisk.bUseHardDiskImage = false;
			dlgname_scsi0[0] = '\0';
			break;
		 case DISKDLG_SCSIBROWSE0:
			if (SDLGui_FileConfSelect(dlgname_scsi0,
			                          ConfigureParams.HardDisk.szHardDiskImage,
			                          diskdlg[DISKDLG_SCSINAME0].w, false))
				ConfigureParams.HardDisk.bUseHardDiskImage = true;
			break;
		}
	}
	while (but != DISKDLG_EXIT && but != SDLGUI_QUIT
	        && but != SDLGUI_ERROR && !bQuitProgram);
}
