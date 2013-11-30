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
#define DLGSYS_CUBE       5
#define DLGSYS_CUBETURBO  6
#define DLGSYS_SLAB       7
#define DLGSYS_SLABTURBO  8
#define DLGSYS_SLABCOLOR  9

#define DLGSYS_CUSTOMIZE  10
#define DLGSYS_RESET      11

#define DLGSYS_EXIT       27

/* Variable strings */
char cpu_type[16] = "68030";
char cpu_clock[16] = "25 MHz";
char fpu_type[16] = "68882";
char memory_size[16] = "8 MB";
char scsi_controller[16] = "NCR53C90";
char rtc_chip[16] = "MC68HC68T1";
char emulate_adb[16] = " ";

/* Additional functions */
void print_system_overview(void);
void update_system_selection(void);
void get_default_values(void);




static SGOBJ systemdlg[] =
{
 	{ SGBOX, 0, 0, 0,0, 60,27, NULL },
 	{ SGTEXT, 0, 0, 23,1, 14,1, "System options" },
  
 	{ SGBOX, 0, 0, 2,3, 26,17, NULL },
 	{ SGTEXT, 0, 0, 3,4, 14,1, "Machine type" },
 	{ SGRADIOBUT, 0, 0, 5,6, 15,1, "NeXT Computer" },
    { SGRADIOBUT, 0, 0, 5,8, 13,1, "NeXTcube" },
    { SGCHECKBOX, 0, 0, 7,9, 7,1, "Turbo" },
 	{ SGRADIOBUT, 0, 0, 5,11, 13,1, "NeXTstation" },
    { SGCHECKBOX, 0, 0, 7,12, 7,1, "Turbo" },
    { SGCHECKBOX, 0, 0, 7,13, 7,1, "Color" },
    
    { SGBUTTON, SG_DEFAULT, 0, 5,16, 20,1, "Customize" },
    { SGBUTTON, SG_DEFAULT, 0, 5,18, 20,1, "System defaults" },
        
 	{ SGTEXT, 0, 0, 30,4, 13,1, "System overview:" },
    { SGTEXT, 0, 0, 30,6, 13,1, "CPU type:" },
    { SGTEXT, 0, 0, 44,6, 13,1, cpu_type },
    { SGTEXT, 0, 0, 30,7, 13,1, "CPU clock:" },
    { SGTEXT, 0, 0, 44,7, 13,1, cpu_clock },
    { SGTEXT, 0, 0, 30,8, 13,1, "FPU type:" },
    { SGTEXT, 0, 0, 44,8, 13,1, fpu_type },
    { SGTEXT, 0, 0, 30,9, 13,1, "Memory size:" },
    { SGTEXT, 0, 0, 44,9, 13,1, memory_size },
    { SGTEXT, 0, 0, 30,10, 13,1, "SCSI chip:" },
    { SGTEXT, 0, 0, 44,10, 13,1, scsi_controller },
    { SGTEXT, 0, 0, 30,11, 13,1, "RTC chip:" },
    { SGTEXT, 0, 0, 44,11, 13,1, rtc_chip },
    { SGTEXT, 0, 0, 30,13, 13,1, emulate_adb },
    
    { SGTEXT, 0, 0, 4,21, 13,1, "Changing machine type resets all advanced options." },
    
 	{ SGBUTTON, SG_DEFAULT, 0, 21,24, 20,1, "Back to main menu" },
 	{ -1, 0, 0, 0,0, 0,0, NULL }
 };


/* Function to print system overview */

void print_system_overview(void) {
    switch (ConfigureParams.System.nCpuLevel) {
        case 0:
            sprintf(cpu_type, "68000"); break;
        case 1:
            sprintf(cpu_type, "68010"); break;
        case 2:
            sprintf(cpu_type, "68020"); break;
        case 3:
            sprintf(cpu_type, "68030"); break;
        case 4:
            sprintf(cpu_type, "68040"); break;
        case 5:
            sprintf(cpu_type, "68060"); break;
        default: break;
    }
    
    sprintf(cpu_clock, "%i MHz", ConfigureParams.System.nCpuFreq);
    
    sprintf(memory_size, "%i MB", Configuration_CheckMemory(ConfigureParams.Memory.nMemoryBankSize));
    
    switch (ConfigureParams.System.n_FPUType) {
        case FPU_NONE:
            sprintf(fpu_type, "none"); break;
        case FPU_68881:
            sprintf(fpu_type, "68881"); break;
        case FPU_68882:
            sprintf(fpu_type, "68882"); break;
        case FPU_CPU:
            sprintf(fpu_type, "68040 internal"); break;
        default: break;
    }
    
    switch (ConfigureParams.System.nSCSI) {
        case NCR53C90:
            sprintf(scsi_controller, "NCR53C90"); break;
        case NCR53C90A:
            sprintf(scsi_controller, "NCR53C90A"); break;
        default: break;
    }
    
    switch (ConfigureParams.System.nRTC) {
        case MC68HC68T1:
            sprintf(rtc_chip, "MC68HC68T1"); break;
        case MCCS1850:
            sprintf(rtc_chip, "MCCS1850"); break;
        default: break;
    }
    
    if (ConfigureParams.System.bADB)
        sprintf(emulate_adb, "ADB emulated");
    else
        sprintf(emulate_adb, " ");
    
    update_system_selection();
}


/* Function to select and unselect system options */
void update_system_selection(void) {
    switch (ConfigureParams.System.nMachineType) {
        case NEXT_CUBE030:
            systemdlg[DLGSYS_CUBE030].state |= SG_SELECTED;
            systemdlg[DLGSYS_CUBE].state &= ~SG_SELECTED;
            systemdlg[DLGSYS_CUBETURBO].state &= ~SG_SELECTED;
            systemdlg[DLGSYS_SLAB].state &= ~SG_SELECTED;
            systemdlg[DLGSYS_SLABCOLOR].state &= ~SG_SELECTED;
            systemdlg[DLGSYS_SLABTURBO].state &= ~SG_SELECTED;
            break;
        case NEXT_CUBE040:
            systemdlg[DLGSYS_CUBE030].state &= ~SG_SELECTED;
            systemdlg[DLGSYS_CUBE].state |= SG_SELECTED;
            if (ConfigureParams.System.bTurbo) {
                systemdlg[DLGSYS_CUBETURBO].state |= SG_SELECTED;
            } else {
                systemdlg[DLGSYS_CUBETURBO].state &= ~SG_SELECTED;
            }
            systemdlg[DLGSYS_SLAB].state &= ~SG_SELECTED;
            systemdlg[DLGSYS_SLABCOLOR].state &= ~SG_SELECTED;
            systemdlg[DLGSYS_SLABTURBO].state &= ~SG_SELECTED;
            break;
        case NEXT_STATION:
            systemdlg[DLGSYS_CUBE030].state &= ~SG_SELECTED;
            systemdlg[DLGSYS_CUBE].state &= ~SG_SELECTED;
            systemdlg[DLGSYS_CUBETURBO].state &= ~SG_SELECTED;
            systemdlg[DLGSYS_SLAB].state |= SG_SELECTED;
            if (ConfigureParams.System.bTurbo) {
                systemdlg[DLGSYS_SLABTURBO].state |= SG_SELECTED;
            } else {
                systemdlg[DLGSYS_SLABTURBO].state &= ~SG_SELECTED;
            }
            if (ConfigureParams.System.bColor) {
                systemdlg[DLGSYS_SLABCOLOR].state |= SG_SELECTED;
            } else {
                systemdlg[DLGSYS_SLABCOLOR].state &= ~SG_SELECTED;
            }
            break;
            
        default:
            break;
    }
}


/* Function to get default values for each system */
void get_default_values(void) {
    Configuration_SetSystemDefaults();
}


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
    if (ConfigureParams.System.nMachineType == NEXT_CUBE030)
        systemdlg[DLGSYS_CUBE030].state |= SG_SELECTED;
    else if (ConfigureParams.System.nMachineType == NEXT_STATION)
        systemdlg[DLGSYS_SLAB].state |= SG_SELECTED;
 
         
    /* System overview */
    print_system_overview();
     
 		
 	/* Draw and process the dialog: */

    do
	{
        but = SDLGui_DoDialog(systemdlg, NULL);
        switch (but) {
            case DLGSYS_CUBE030:
                ConfigureParams.System.nMachineType = NEXT_CUBE030;
                get_default_values();
                print_system_overview();
                break;
                
            case DLGSYS_CUBE:
                ConfigureParams.System.nMachineType = NEXT_CUBE040;
                get_default_values();
                print_system_overview();
                break;
                
            case DLGSYS_CUBETURBO:
                ConfigureParams.System.nMachineType = NEXT_CUBE040;
                if (ConfigureParams.System.bTurbo) {
                    ConfigureParams.System.bTurbo = false;
                } else {
                    ConfigureParams.System.bTurbo = true;
                }
                get_default_values();
                print_system_overview();
                break;
                
            case DLGSYS_SLAB:
                ConfigureParams.System.nMachineType = NEXT_STATION;
                get_default_values();
                print_system_overview();
                break;
                
            case DLGSYS_SLABCOLOR:
                ConfigureParams.System.nMachineType = NEXT_STATION;
                if (ConfigureParams.System.bColor) {
                    ConfigureParams.System.bColor = false;
                } else {
                    ConfigureParams.System.bColor = true;
                }
                get_default_values();
                print_system_overview();
                break;
                
            case DLGSYS_SLABTURBO:
                ConfigureParams.System.nMachineType = NEXT_STATION;
                if (ConfigureParams.System.bTurbo) {
                    ConfigureParams.System.bTurbo = false;
                } else {
                    ConfigureParams.System.bTurbo = true;
                }
                get_default_values();
                print_system_overview();
                break;
                                
            case DLGSYS_CUSTOMIZE:
                Dialog_AdvancedDlg();
                print_system_overview();
                break;
                
            case DLGSYS_RESET:
                get_default_values();
                print_system_overview();
                break;

            default:
                break;
        }
    }
    while (but != DLGSYS_EXIT && but != SDLGUI_QUIT
           && but != SDLGUI_ERROR && !bQuitProgram);
    
 
 	/* Read values from dialog: */
 
//    if (systemdlg[DLGSYS_CUBE030].state&SG_SELECTED)
//        ConfigureParams.System.nMachineType = NEXT_CUBE030;
//    else if (systemdlg[DLGSYS_SLAB].state&SG_SELECTED)
//        ConfigureParams.System.nMachineType = NEXT_STATION;
     
    /* Obsolete */
 	ConfigureParams.System.bCompatibleCpu = 1;
 	ConfigureParams.System.bBlitter = 0;
 	ConfigureParams.System.bRealTimeClock = 0;
 	ConfigureParams.System.bPatchTimerD = 0;
 	ConfigureParams.System.bAddressSpace24 = 0;
 	ConfigureParams.System.bCycleExactCpu = 0;
 	ConfigureParams.System.bCompatibleFPU = 1;
 	ConfigureParams.System.bMMU = 1;
}

