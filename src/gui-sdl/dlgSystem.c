/*
  Hatari - dlgSystem.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

 This file contains 2 system panels :
    - 1 for the old uae CPU
    - 1 for the new WinUae cpu

 The selection is made during compilation with the ENABLE_WINUAE_CPU define

*/
const char DlgSystem_fileid[] = "Hatari dlgSystem.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "configuration.h"
#include "dialog.h"
#include "sdlgui.h"


#define DLGSYS_CUBE030    4
#define DLGSYS_SLAB       5
#define DLGSYS_COLOR      6
#define DLGSYS_TURBO      7

#define DLGSYS_8MB        10
#define DLGSYS_16MB       11
#define DLGSYS_32MB       12
#define DLGSYS_64MB       13
#define DLGSYS_128MB      14

#define DLGSYS_ADB        17

#define DLGSYS_CPU        20
#define DLGSYS_SPEED      21
#define DLGSYS_FPU        23
#define DLGSYS_SCSI       25
#define DLGSYS_RTC        27

#define DLGSYS_EXIT       29



static SGOBJ systemdlg[] =
{
 	{ SGBOX, 0, 0, 0,0, 60,27, NULL },
 	{ SGTEXT, 0, 0, 23,1, 14,1, "System options" },
  
 	{ SGBOX, 0, 0, 2,3, 18,9, NULL },
 	{ SGTEXT, 0, 0, 3,4, 14,1, "Machine type" },
 	{ SGRADIOBUT, 0, 0, 3,6, 15,1, "NeXT Computer" },
 	{ SGRADIOBUT, 0, 0, 3,8, 13,1, "NeXTstation" },
    { SGCHECKBOX, 0, 0, 5,9, 7,1, "Color" },
    { SGCHECKBOX, 0, 0, 5,10, 7,1, "Turbo" },
    
    { SGBOX, 0, 0, 21,3, 16,9, NULL },
 	{ SGTEXT, 0, 0, 22,4, 15,1, "System memory" },
 	{ SGRADIOBUT, 0, 0, 22,6, 6,1, "8 MB" },
 	{ SGRADIOBUT, 0, 0, 22,7, 7,1, "16 MB" },
    { SGRADIOBUT, 0, 0, 22,8, 7,1, "32 MB" },
    { SGRADIOBUT, 0, 0, 22,9, 7,1, "64 MB" },
    { SGRADIOBUT, 0, 0, 22,10, 8,1, "128 MB" },
    
    { SGBOX, 0, 0, 38,3, 20,9, NULL },
 	{ SGTEXT, 0, 0, 39,4, 16,1, "Advanced options" },
    { SGCHECKBOX, 0, 0, 39,6, 12,1, "Enable ADB" },
    
 	{ SGTEXT, 0, 0, 3,13, 13,1, "System overview:" },
    { SGTEXT, 0, 0, 3,15, 13,1, "CPU type:" },
    { SGTEXT, 0, 0, 15,15, 13,1, "68030," },
    { SGTEXT, 0, 0, 22,15, 13,1, "25 MHz" },
    { SGTEXT, 0, 0, 3,16, 13,1, "FPU type:" },
    { SGTEXT, 0, 0, 15,16, 13,1, "68882" },
    { SGTEXT, 0, 0, 3,17, 13,1, "SCSI chip:" },
    { SGTEXT, 0, 0, 15,17, 13,1, "NCR53C90" },
    { SGTEXT, 0, 0, 3,18, 13,1, "RTC chip:" },
    { SGTEXT, 0, 0, 15,18, 13,1, "MC68HC68T1" },
    
    { SGTEXT, 0, 0, 3,21, 13,1, "Options may be unavailable depending on machine type." },
   
 	{ SGBUTTON, SG_DEFAULT, 0, 21,24, 20,1, "Back to main menu" },
 	{ -1, 0, 0, 0,0, 0,0, NULL }
 };

/* Variable objects */
SGOBJ disable_adb_option = { SGTEXT, 0, 0, 39,6, 8,1, " " };
SGOBJ enable_adb_option = { SGCHECKBOX, 0, 0, 39,6, 12,1, "Enable ADB" };

SGOBJ disable_128mb_option = { SGTEXT, 0, 0, 22,10, 8,1, " " };
SGOBJ enable_128mb_option = { SGRADIOBUT, 0, 0, 22,10, 8,1, "128 MB" };

SGOBJ show68030 = { SGTEXT, 0, 0, 15,15, 13,1, "68030," };
SGOBJ show68040 = { SGTEXT, 0, 0, 15,15, 13,1, "68040," };

SGOBJ show25MHz = { SGTEXT, 0, 0, 22,15, 13,1, "25 MHz" };
SGOBJ show33MHz = { SGTEXT, 0, 0, 22,15, 13,1, "33 MHz" };

SGOBJ show68882 = { SGTEXT, 0, 0, 15,16, 13,1, "68882" };
SGOBJ show040internal = { SGTEXT, 0, 0, 15,16, 13,1, "68040 internal" };

SGOBJ showOldSCSI = { SGTEXT, 0, 0, 15,17, 13,1, "NCR53C90" };
SGOBJ showNewSCSI = { SGTEXT, 0, 0, 15,17, 13,1, "NCR53C90A" };

SGOBJ showOldRTC = { SGTEXT, 0, 0, 15,18, 13,1, "MC68HC68T1" };
SGOBJ showNewRTC = { SGTEXT, 0, 0, 15,18, 13,1, "MCS1850" };


/*-----------------------------------------------------------------------*/
/**
  * Show and process the "System" dialog (specific to winUAE cpu).
  */
void Dialog_SystemDlg(void)
{
 	int i;
    int but;
 
 	SDLGui_CenterDlg(systemdlg);
 
 	/* Set up dialog from actual values: */
    
    /* System type: */
 	for (i = DLGSYS_CUBE030; i <= DLGSYS_SLAB; i++)
 	{
 		systemdlg[i].state &= ~SG_SELECTED;
 	}
 	systemdlg[DLGSYS_CUBE030 + ConfigureParams.System.nMachineType-1].state |= SG_SELECTED;
 
 
 	/* System memory: */
 	for (i = DLGSYS_8MB; i <= DLGSYS_64MB; i++)
 	{
 		systemdlg[i].state &= ~SG_SELECTED;
 	}
    if (ConfigureParams.System.nMachineType == NEXT_STATION) {
        systemdlg[DLGSYS_128MB] = enable_128mb_option;
        systemdlg[DLGSYS_128MB].state &= ~SG_SELECTED;
    } else {
        systemdlg[DLGSYS_128MB] = disable_128mb_option;
    }
    
    
	switch (ConfigureParams.Memory.nMemorySize)
	{
        case 8:
            systemdlg[DLGSYS_8MB].state |= SG_SELECTED;
            break;
        case 16:
            systemdlg[DLGSYS_16MB].state |= SG_SELECTED;
            break;
        case 32:
            systemdlg[DLGSYS_32MB].state |= SG_SELECTED;
            break;
        case 64:
            systemdlg[DLGSYS_64MB].state |= SG_SELECTED;
            break;
        case 128:
            if (ConfigureParams.System.nMachineType == NEXT_STATION) {
                systemdlg[DLGSYS_128MB].state |= SG_SELECTED;
                break;
            }
        default:
            systemdlg[DLGSYS_8MB].state |= SG_SELECTED;
            ConfigureParams.Memory.nMemorySize = 8;
            break;
	}
    
    /* Advanced options: */
    if (ConfigureParams.System.nMachineType == NEXT_CUBE030) {
        systemdlg[DLGSYS_ADB] = disable_adb_option;
    } else {
        systemdlg[DLGSYS_ADB] = enable_adb_option;
    }
    
    if (ConfigureParams.System.bADB && ConfigureParams.System.nMachineType != NEXT_CUBE030) {
        systemdlg[DLGSYS_ADB].state |= SG_SELECTED;
    } else if (!ConfigureParams.System.bADB && ConfigureParams.System.nMachineType != NEXT_CUBE030) {
        systemdlg[DLGSYS_ADB].state &= ~SG_SELECTED;
    }
    
    /* System overview */
    if (ConfigureParams.System.nMachineType == NEXT_CUBE030) {
        systemdlg[DLGSYS_CPU] = show68030;
        systemdlg[DLGSYS_SPEED] = show25MHz;
        systemdlg[DLGSYS_FPU] = show68882;
        systemdlg[DLGSYS_SCSI] = showOldSCSI;
        systemdlg[DLGSYS_RTC] = showOldRTC;
    } else {
        systemdlg[DLGSYS_CPU] = show68040;
        if (ConfigureParams.System.bTurbo)
            systemdlg[DLGSYS_SPEED] = show33MHz;
        else
            systemdlg[DLGSYS_SPEED] = show25MHz;
        systemdlg[DLGSYS_FPU] = show040internal;
        systemdlg[DLGSYS_SCSI] = showNewSCSI;
        systemdlg[DLGSYS_RTC] = showNewRTC;
    }
 
 		
 	/* Draw and process the dialog: */

    do
	{
        but = SDLGui_DoDialog(systemdlg, NULL);
        printf("Button: %i\n", but);
        switch (but) {
            case DLGSYS_CUBE030:
                ConfigureParams.System.nMachineType = NEXT_CUBE030;
                ConfigureParams.System.nCpuLevel = 3;
                ConfigureParams.System.nCpuFreq = 25;
                systemdlg[DLGSYS_CPU] = show68030;
                systemdlg[DLGSYS_SPEED] = show25MHz;
                systemdlg[DLGSYS_FPU] = show68882;
                systemdlg[DLGSYS_SCSI] = showOldSCSI;
                systemdlg[DLGSYS_RTC] = showOldRTC;
                ConfigureParams.System.nRTC = MC68HC68T1;
                ConfigureParams.System.nSCSI = NCR53C90;
                ConfigureParams.System.bADB = false;
                systemdlg[DLGSYS_ADB] = disable_adb_option;
                if (systemdlg[DLGSYS_128MB].state & SG_SELECTED) {
                    ConfigureParams.Memory.nMemorySize = 64;
                    systemdlg[DLGSYS_64MB].state |= SG_SELECTED;
                }
                systemdlg[DLGSYS_128MB] = disable_128mb_option;
                systemdlg[DLGSYS_COLOR].state &= ~SG_SELECTED;
                ConfigureParams.System.bColor = false;
                systemdlg[DLGSYS_TURBO].state &= ~SG_SELECTED;
                ConfigureParams.System.bTurbo = false;
                break;
                
            case DLGSYS_SLAB:
                ConfigureParams.System.nMachineType = NEXT_STATION;
                ConfigureParams.System.nCpuLevel = 4;
                systemdlg[DLGSYS_CPU] = show68040;
                systemdlg[DLGSYS_FPU] = show040internal;
                systemdlg[DLGSYS_SCSI] = showNewSCSI;
                systemdlg[DLGSYS_RTC] = showNewRTC;
                systemdlg[DLGSYS_ADB] = enable_adb_option;
                systemdlg[DLGSYS_128MB] = enable_128mb_option;
                ConfigureParams.System.nSCSI = NCR53C90A;
                ConfigureParams.System.nRTC = MCS1850;
                break;
                
            case DLGSYS_COLOR:
                if (systemdlg[DLGSYS_COLOR].state & SG_SELECTED) {
                    ConfigureParams.System.bColor = true;
                    ConfigureParams.System.nMachineType = NEXT_STATION;
                    ConfigureParams.System.nCpuLevel = 4;
                    systemdlg[DLGSYS_CPU] = show68040;
                    systemdlg[DLGSYS_FPU] = show040internal;
                    systemdlg[DLGSYS_SCSI] = showNewSCSI;
                    systemdlg[DLGSYS_RTC] = showNewRTC;
                    systemdlg[DLGSYS_CUBE030].state &= ~SG_SELECTED;
                    systemdlg[DLGSYS_SLAB].state |= SG_SELECTED;
                    systemdlg[DLGSYS_ADB] = enable_adb_option;
                    systemdlg[DLGSYS_128MB] = enable_128mb_option;
                    ConfigureParams.System.nSCSI = NCR53C90A;
                    ConfigureParams.System.nRTC = MCS1850;
                } else {
                    ConfigureParams.System.bColor = false;
                }
                printf("color: %i\n", ConfigureParams.System.bColor ? 1 : 0);
                break;
                
            case DLGSYS_TURBO:
                if (systemdlg[DLGSYS_TURBO].state & SG_SELECTED) {
                    ConfigureParams.System.bTurbo = true;
                    ConfigureParams.System.nMachineType = NEXT_STATION;
                    ConfigureParams.System.nCpuLevel = 4;
                    ConfigureParams.System.nCpuFreq = 33;
                    systemdlg[DLGSYS_CPU] = show68040;
                    systemdlg[DLGSYS_SPEED] = show33MHz;
                    systemdlg[DLGSYS_FPU] = show040internal;
                    systemdlg[DLGSYS_SCSI] = showNewSCSI;
                    systemdlg[DLGSYS_RTC] = showNewRTC;
                    systemdlg[DLGSYS_CUBE030].state &= ~SG_SELECTED;
                    systemdlg[DLGSYS_SLAB].state |= SG_SELECTED;
                    systemdlg[DLGSYS_ADB] = enable_adb_option;
                    systemdlg[DLGSYS_128MB] = enable_128mb_option;
                    ConfigureParams.System.nSCSI = NCR53C90A;
                    ConfigureParams.System.nRTC = MCS1850;
                } else {
                    ConfigureParams.System.bTurbo = false;
                    ConfigureParams.System.nCpuFreq = 25;
                    systemdlg[DLGSYS_SPEED] = show25MHz;
                }
                printf("turbo: %i\n", ConfigureParams.System.bTurbo ? 1 : 0);
                break;
                
            case DLGSYS_ADB:
                if (systemdlg[DLGSYS_ADB].state & SG_SELECTED) {
                    ConfigureParams.System.bADB = true;
                } else {
                    ConfigureParams.System.bADB = false;
                }
                break;

                
            default:
                break;
        }
    }
    while (but != DLGSYS_EXIT && but != SDLGUI_QUIT
           && but != SDLGUI_ERROR && !bQuitProgram);
    
 
 	/* Read values from dialog: */
 
 	for (i = DLGSYS_CUBE030; i <= DLGSYS_SLAB; i++)
 	{
 		if (systemdlg[i].state&SG_SELECTED)
 		{
 			ConfigureParams.System.nMachineType = i - DLGSYS_CUBE030;
 			break;
 		}
 	}
    
    for (i = DLGSYS_8MB; i <= DLGSYS_128MB; i++) {
        if (systemdlg[i].state&SG_SELECTED) {
            ConfigureParams.Memory.nMemorySize = 1 << (i - DLGSYS_8MB + 3);
        }
    }
 
 	ConfigureParams.System.bCompatibleCpu = 1;
 	ConfigureParams.System.bBlitter = 0;
 	ConfigureParams.System.bRealTimeClock = 0;
 	ConfigureParams.System.bPatchTimerD = 0;
 	ConfigureParams.System.bAddressSpace24 = 0;
 	ConfigureParams.System.bCycleExactCpu = 0;
 
 	/* FPU emulation */
 	ConfigureParams.System.n_FPUType = FPU_CPU;
 
 	ConfigureParams.System.bCompatibleFPU = 1;
 	ConfigureParams.System.bMMU = 1;
}

