/*
  Hatari - dlgMissingFile.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/
const char DlgMissingFile_fileid[] = "Hatari dlgMissingFile.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "configuration.h"
#include "dialog.h"
#include "sdlgui.h"
#include "file.h"
#include "paths.h"


/* Missing ROM dialog */
#define DLGMISSINGROM_MACHINE   3

#define DLGMISSINGROM_BROWSE    6
#define DLGMISSINGROM_DEFAULT   7
#define DLGMISSINGROM_NAME      8

#define DLGMISSINGROM_SELECT    9
#define DLGMISSINGROM_QUIT      10


static SGOBJ missingromdlg[] =
{
    { SGBOX, 0, 0, 0,0, 52,15, NULL },
    { SGTEXT, 0, 0, 16,1, 9,1, "ROM file not found!" },
    { SGTEXT, 0, 0, 2,4, 9,1, "Please select a compatible ROM for machine type" },
    { SGTEXT, 0, 0, 2,5, 9,1, NULL },
    
    { SGBOX, 0, 0, 1,7, 50,4, NULL },
    { SGTEXT, 0, 0, 2,8, 46,1, "Filename:" },
    { SGBUTTON, 0, 0, 13,8, 8,1, "Browse" },
    { SGBUTTON, 0, 0, 22,8, 9,1, "Default" },
    { SGTEXT, 0, 0, 2,9, 46,1, NULL },
    
    { SGBUTTON, SG_DEFAULT, 0, 9,13, 14,1, "Select" },
    { SGBUTTON, 0, 0, 29,13, 14,1, "Quit" },
    { -1, 0, 0, 0,0, 0,0, NULL }
};


/* Missing SCSI dialog */
#define DLGMISSINGSCSI_ALERT     1
#define DLGMISSINGSCSI_TARGET    3

#define DLGMISSINGSCSI_BROWSE    5
#define DLGMISSINGSCSI_EJECT     6
#define DLGMISSINGSCSI_CDROM     7
#define DLGMISSINGSCSI_NAME      8

#define DLGMISSINGSCSI_SELECT    9
#define DLGMISSINGSCSI_QUIT      10


static SGOBJ missingscsidlg[] =
{
    { SGBOX, 0, 0, 0,0, 52,15, NULL },
    { SGTEXT, 0, 0, 9,1, 9,1, NULL },
    { SGTEXT, 0, 0, 2,4, 9,1, "Please eject or select a valid disk image for" },
    { SGTEXT, 0, 0, 2,5, 9,1, NULL },
    
    { SGBOX, 0, 0, 1,7, 50,4, NULL },
    { SGBUTTON, 0, 0, 2,8, 10,1, "Browse" },
    { SGBUTTON, 0, 0, 14,8, 9,1, "Eject" },
    { SGCHECKBOX, 0, 0, 27,8, 8,1, "CD-ROM" },
    { SGTEXT, 0, 0, 2,9, 46,1, NULL },
    
    { SGBUTTON, SG_DEFAULT, 0, 9,13, 14,1, "Select" },
    { SGBUTTON, 0, 0, 29,13, 14,1, "Quit" },
    { -1, 0, 0, 0,0, 0,0, NULL }
};






/*-----------------------------------------------------------------------*/
/**
 * Show and process the Missing ROM dialog.
 */
void DlgMissing_Rom(void) {
	bool bOldMouseVisibility;
	int nOldMouseX, nOldMouseY;
        
	SDL_GetMouseState(&nOldMouseX, &nOldMouseY);
	bOldMouseVisibility = SDL_ShowCursor(SDL_QUERY);
	SDL_ShowCursor(SDL_ENABLE);


    int but;
    char szDlgMissingRom[47];
    
    SDLGui_CenterDlg(missingromdlg);
    
    switch (ConfigureParams.System.nMachineType) {
        case NEXT_CUBE030:
            missingromdlg[DLGMISSINGROM_MACHINE].txt = "NeXT Computer (68030):";
            break;
        case NEXT_CUBE040:
            if (ConfigureParams.System.bTurbo)
                missingromdlg[DLGMISSINGROM_MACHINE].txt = "NeXTcube Turbo:";
            else
                missingromdlg[DLGMISSINGROM_MACHINE].txt = "NeXTcube:";
            break;
        case NEXT_STATION:
            if (ConfigureParams.System.bColor) {
                if (ConfigureParams.System.bTurbo)
                    missingromdlg[DLGMISSINGROM_MACHINE].txt = "NeXTstation Turbo Color:";
                else
                    missingromdlg[DLGMISSINGROM_MACHINE].txt = "NeXTstation Color:";
            } else {
                if (ConfigureParams.System.bTurbo)
                    missingromdlg[DLGMISSINGROM_MACHINE].txt = "NeXTstation Turbo:";
                else
                    missingromdlg[DLGMISSINGROM_MACHINE].txt = "NeXTstation:";
            }
            break;
        default:
            break;
    }
    
    szDlgMissingRom[0] = '\0';
    missingromdlg[DLGMISSINGROM_NAME].txt = szDlgMissingRom;
    
    do
	{
		but = SDLGui_DoDialog(missingromdlg, NULL);
		switch (but)
		{
            case DLGMISSINGROM_DEFAULT:
                switch (ConfigureParams.System.nMachineType) {
                    case NEXT_CUBE030:
                        sprintf(ConfigureParams.Rom.szRom030FileName, "%s%cRev_1.0_v41.BIN",
                                Paths_GetWorkingDir(), PATHSEP);
                        File_ShrinkName(szDlgMissingRom, ConfigureParams.Rom.szRom030FileName, sizeof(szDlgMissingRom)-1);
                        break;
                        
                    case NEXT_CUBE040:
                    case NEXT_STATION:
                        if (ConfigureParams.System.bTurbo) {
                            sprintf(ConfigureParams.Rom.szRomTurboFileName, "%s%cRev_3.3_v74.BIN",
                                    Paths_GetWorkingDir(), PATHSEP);
                            File_ShrinkName(szDlgMissingRom, ConfigureParams.Rom.szRomTurboFileName, sizeof(szDlgMissingRom)-1);
                        } else {
                            sprintf(ConfigureParams.Rom.szRom040FileName, "%s%cRev_2.5_v66.BIN",
                                    Paths_GetWorkingDir(), PATHSEP);
                            File_ShrinkName(szDlgMissingRom, ConfigureParams.Rom.szRom040FileName, sizeof(szDlgMissingRom)-1);
                        }
                        break;
                        
                    default:
                        break;
                }
                break;
                
            case DLGMISSINGROM_BROWSE:
                /* Show and process the file selection dlg */
                switch (ConfigureParams.System.nMachineType) {
                    case NEXT_CUBE030:
                        SDLGui_FileConfSelect(szDlgMissingRom,
                                              ConfigureParams.Rom.szRom030FileName,
                                              sizeof(szDlgMissingRom)-1,
                                              false);
                        break;
                        
                    case NEXT_CUBE040:
                    case NEXT_STATION:
                        if (ConfigureParams.System.bTurbo) {
                            SDLGui_FileConfSelect(szDlgMissingRom,
                                                  ConfigureParams.Rom.szRomTurboFileName,
                                                  sizeof(szDlgMissingRom)-1,
                                                  false);
                        } else {
                            SDLGui_FileConfSelect(szDlgMissingRom,
                                                  ConfigureParams.Rom.szRom040FileName,
                                                  sizeof(szDlgMissingRom)-1,
                                                  false);
                        }
                        break;
                        
                    default:
                        break;
                }
                break;
            case DLGMISSINGROM_QUIT:
                bQuitProgram = true;
                break;
		}
	}
	while (but != DLGMISSINGROM_SELECT && but != SDLGUI_QUIT
	       && but != SDLGUI_ERROR && !bQuitProgram);
    
    SDL_ShowCursor(bOldMouseVisibility);
	Main_WarpMouse(nOldMouseX, nOldMouseY);
}


/*-----------------------------------------------------------------------*/
/**
 * Show and process the Missing SCSI disk dialog.
 */
void DlgMissing_SCSIdisk(int target)
{
    int but;
    bool bOldMouseVisibility;
	int nOldMouseX, nOldMouseY;

    char dlgname_missingscsi[47];
    char missingscsi_alert[64];
    char missingscsi_target[64];
    
    SDL_GetMouseState(&nOldMouseX, &nOldMouseY);
	bOldMouseVisibility = SDL_ShowCursor(SDL_QUERY);
	SDL_ShowCursor(SDL_ENABLE);
    
	SDLGui_CenterDlg(missingscsidlg);
    
	/* Set up dialog to actual values: */
    sprintf(missingscsi_alert, "SCSI disk %i: disk image not found!", target);
    missingscsidlg[DLGMISSINGSCSI_ALERT].txt = missingscsi_alert;
    
    sprintf(missingscsi_target, "SCSI disk %i:", target);
    missingscsidlg[DLGMISSINGSCSI_TARGET].txt = missingscsi_target;
    
    switch (target) {
        case 0:
            File_ShrinkName(dlgname_missingscsi, ConfigureParams.HardDisk.szSCSIDiskImage0, missingscsidlg[DLGMISSINGSCSI_NAME].w);
            if (ConfigureParams.HardDisk.bCDROM0)
                missingscsidlg[DLGMISSINGSCSI_CDROM].state |= SG_SELECTED;
            else
                missingscsidlg[DLGMISSINGSCSI_CDROM].state &= ~SG_SELECTED;
            break;
        case 1:
            File_ShrinkName(dlgname_missingscsi, ConfigureParams.HardDisk.szSCSIDiskImage1, missingscsidlg[DLGMISSINGSCSI_NAME].w);
            if (ConfigureParams.HardDisk.bCDROM1)
                missingscsidlg[DLGMISSINGSCSI_CDROM].state |= SG_SELECTED;
            else
                missingscsidlg[DLGMISSINGSCSI_CDROM].state &= ~SG_SELECTED;
            break;
        case 2:
            File_ShrinkName(dlgname_missingscsi, ConfigureParams.HardDisk.szSCSIDiskImage2, missingscsidlg[DLGMISSINGSCSI_NAME].w);
            if (ConfigureParams.HardDisk.bCDROM2)
                missingscsidlg[DLGMISSINGSCSI_CDROM].state |= SG_SELECTED;
            else
                missingscsidlg[DLGMISSINGSCSI_CDROM].state &= ~SG_SELECTED;
            break;
        case 3:
            File_ShrinkName(dlgname_missingscsi, ConfigureParams.HardDisk.szSCSIDiskImage3, missingscsidlg[DLGMISSINGSCSI_NAME].w);
            if (ConfigureParams.HardDisk.bCDROM3)
                missingscsidlg[DLGMISSINGSCSI_CDROM].state |= SG_SELECTED;
            else
                missingscsidlg[DLGMISSINGSCSI_CDROM].state &= ~SG_SELECTED;
            break;
        case 4:
            File_ShrinkName(dlgname_missingscsi, ConfigureParams.HardDisk.szSCSIDiskImage4, missingscsidlg[DLGMISSINGSCSI_NAME].w);
            if (ConfigureParams.HardDisk.bCDROM4)
                missingscsidlg[DLGMISSINGSCSI_CDROM].state |= SG_SELECTED;
            else
                missingscsidlg[DLGMISSINGSCSI_CDROM].state &= ~SG_SELECTED;
            break;
        case 5:
            File_ShrinkName(dlgname_missingscsi, ConfigureParams.HardDisk.szSCSIDiskImage5, missingscsidlg[DLGMISSINGSCSI_NAME].w);
            if (ConfigureParams.HardDisk.bCDROM5)
                missingscsidlg[DLGMISSINGSCSI_CDROM].state |= SG_SELECTED;
            else
                missingscsidlg[DLGMISSINGSCSI_CDROM].state &= ~SG_SELECTED;
            break;
        case 6:
            File_ShrinkName(dlgname_missingscsi, ConfigureParams.HardDisk.szSCSIDiskImage6, missingscsidlg[DLGMISSINGSCSI_NAME].w);
            if (ConfigureParams.HardDisk.bCDROM6)
                missingscsidlg[DLGMISSINGSCSI_CDROM].state |= SG_SELECTED;
            else
                missingscsidlg[DLGMISSINGSCSI_CDROM].state &= ~SG_SELECTED;
            break;
            
        default:
            break;
    }
    
	missingscsidlg[DLGMISSINGSCSI_NAME].txt = dlgname_missingscsi;
        
    
	/* Draw and process the dialog */
	do
	{
		but = SDLGui_DoDialog(missingscsidlg, NULL);
		switch (but)
		{
            case DLGMISSINGSCSI_EJECT:
                switch (target) {
                    case 0:
                        ConfigureParams.HardDisk.bSCSIImageAttached0 = false;
                        ConfigureParams.HardDisk.szSCSIDiskImage0[0] = '\0';
                        break;
                    case 1:
                        ConfigureParams.HardDisk.bSCSIImageAttached1 = false;
                        ConfigureParams.HardDisk.szSCSIDiskImage1[0] = '\0';
                        break;
                    case 2:
                        ConfigureParams.HardDisk.bSCSIImageAttached2 = false;
                        ConfigureParams.HardDisk.szSCSIDiskImage2[0] = '\0';
                        break;
                    case 3:
                        ConfigureParams.HardDisk.bSCSIImageAttached3 = false;
                        ConfigureParams.HardDisk.szSCSIDiskImage3[0] = '\0';
                        break;
                    case 4:
                        ConfigureParams.HardDisk.bSCSIImageAttached4 = false;
                        ConfigureParams.HardDisk.szSCSIDiskImage4[0] = '\0';
                        break;
                    case 5:
                        ConfigureParams.HardDisk.bSCSIImageAttached5 = false;
                        ConfigureParams.HardDisk.szSCSIDiskImage5[0] = '\0';
                        break;
                    case 6:
                        ConfigureParams.HardDisk.bSCSIImageAttached6 = false;
                        ConfigureParams.HardDisk.szSCSIDiskImage6[0] = '\0';
                        break;
                        
                    default:
                        break;
                }
                dlgname_missingscsi[0] = '\0';
                break;
            case DLGMISSINGSCSI_BROWSE:
                switch (target) {
                    case 0:
                        if (SDLGui_FileConfSelect(dlgname_missingscsi,
                                                  ConfigureParams.HardDisk.szSCSIDiskImage0,
                                                  missingscsidlg[DLGMISSINGSCSI_NAME].w, false))
                            ConfigureParams.HardDisk.bSCSIImageAttached0 = true;
                        break;
                    case 1:
                        if (SDLGui_FileConfSelect(dlgname_missingscsi,
                                                  ConfigureParams.HardDisk.szSCSIDiskImage1,
                                                  missingscsidlg[DLGMISSINGSCSI_NAME].w, false))
                            ConfigureParams.HardDisk.bSCSIImageAttached1 = true;
                        break;
                    case 2:
                        if (SDLGui_FileConfSelect(dlgname_missingscsi,
                                                  ConfigureParams.HardDisk.szSCSIDiskImage2,
                                                  missingscsidlg[DLGMISSINGSCSI_NAME].w, false))
                            ConfigureParams.HardDisk.bSCSIImageAttached2 = true;
                        break;
                    case 3:
                        if (SDLGui_FileConfSelect(dlgname_missingscsi,
                                                  ConfigureParams.HardDisk.szSCSIDiskImage3,
                                                  missingscsidlg[DLGMISSINGSCSI_NAME].w, false))
                            ConfigureParams.HardDisk.bSCSIImageAttached3 = true;
                        break;
                    case 4:
                        if (SDLGui_FileConfSelect(dlgname_missingscsi,
                                                  ConfigureParams.HardDisk.szSCSIDiskImage4,
                                                  missingscsidlg[DLGMISSINGSCSI_NAME].w, false))
                            ConfigureParams.HardDisk.bSCSIImageAttached4 = true;
                        break;
                    case 5:
                        if (SDLGui_FileConfSelect(dlgname_missingscsi,
                                                  ConfigureParams.HardDisk.szSCSIDiskImage5,
                                                  missingscsidlg[DLGMISSINGSCSI_NAME].w, false))
                            ConfigureParams.HardDisk.bSCSIImageAttached5 = true;
                        break;
                    case 6:
                        if (SDLGui_FileConfSelect(dlgname_missingscsi,
                                                  ConfigureParams.HardDisk.szSCSIDiskImage6,
                                                  missingscsidlg[DLGMISSINGSCSI_NAME].w, false))
                            ConfigureParams.HardDisk.bSCSIImageAttached6 = true;
                        break;

                    default:
                        break;
                }
                break;
            case DLGMISSINGSCSI_QUIT:
                bQuitProgram = true;
                break;
		}
	}
	while (but != DLGMISSINGSCSI_SELECT && but != SDLGUI_QUIT
           && but != SDLGUI_ERROR && !bQuitProgram);
    
    /* Read values from dialog: */
    switch (target) {
        case 0: ConfigureParams.HardDisk.bCDROM0 = (missingscsidlg[DLGMISSINGSCSI_CDROM].state & SG_SELECTED); break;
        case 1: ConfigureParams.HardDisk.bCDROM1 = (missingscsidlg[DLGMISSINGSCSI_CDROM].state & SG_SELECTED); break;
        case 2: ConfigureParams.HardDisk.bCDROM2 = (missingscsidlg[DLGMISSINGSCSI_CDROM].state & SG_SELECTED); break;
        case 3: ConfigureParams.HardDisk.bCDROM3 = (missingscsidlg[DLGMISSINGSCSI_CDROM].state & SG_SELECTED); break;
        case 4: ConfigureParams.HardDisk.bCDROM4 = (missingscsidlg[DLGMISSINGSCSI_CDROM].state & SG_SELECTED); break;
        case 5: ConfigureParams.HardDisk.bCDROM5 = (missingscsidlg[DLGMISSINGSCSI_CDROM].state & SG_SELECTED); break;
        case 6: ConfigureParams.HardDisk.bCDROM6 = (missingscsidlg[DLGMISSINGSCSI_CDROM].state & SG_SELECTED); break;

        default:
            break;
    }
    
    SDL_ShowCursor(bOldMouseVisibility);
	Main_WarpMouse(nOldMouseX, nOldMouseY);
}
