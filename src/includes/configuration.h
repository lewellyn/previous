/*
  Hatari - configuration.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/

#ifndef HATARI_CONFIGURATION_H
#define HATARI_CONFIGURATION_H

/* Logging and tracing */
typedef struct
{
  char sLogFileName[FILENAME_MAX];
  char sTraceFileName[FILENAME_MAX];
  int nTextLogLevel;
  int nAlertDlgLogLevel;
  bool bConfirmQuit;
} CNF_LOG;


/* debugger */
typedef struct
{
  int nNumberBase;
  int nDisasmLines;
  int nMemdumpLines;
} CNF_DEBUGGER;


/* ROM (TOS + cartridge) configuration */
typedef struct
{
  char szTosImageFileName[FILENAME_MAX];
  char szCartridgeImageFileName[FILENAME_MAX];
} CNF_ROM;


/* Sound configuration */
typedef struct
{
  bool bEnableSound;
  int nPlaybackFreq;
  char szYMCaptureFileName[FILENAME_MAX];
  int SdlAudioBufferSize;
} CNF_SOUND;



/* RS232 configuration */
typedef struct
{
  bool bEnableRS232;
  char szOutFileName[FILENAME_MAX];
  char szInFileName[FILENAME_MAX];
} CNF_RS232;


/* Dialog Keyboard */
typedef enum
{
  KEYMAP_SYMBOLIC,  /* Use keymapping with symbolic (ASCII) key codes */
  KEYMAP_SCANCODE,  /* Use keymapping with PC keyboard scancodes */
  KEYMAP_LOADED     /* Use keymapping with a map configuration file */
} KEYMAPTYPE;

typedef struct
{
  bool bDisableKeyRepeat;
  KEYMAPTYPE nKeymapType;
  char szMappingFileName[FILENAME_MAX];
} CNF_KEYBOARD;


typedef enum {
  SHORTCUT_OPTIONS,
  SHORTCUT_FULLSCREEN,
  SHORTCUT_MOUSEGRAB,
  SHORTCUT_COLDRESET,
  SHORTCUT_WARMRESET,
  SHORTCUT_SCREENSHOT,
  SHORTCUT_BOSSKEY,
  SHORTCUT_CURSOREMU,
  SHORTCUT_FASTFORWARD,
  SHORTCUT_RECANIM,
  SHORTCUT_RECSOUND,
  SHORTCUT_SOUND,
  SHORTCUT_DEBUG,
  SHORTCUT_PAUSE,
  SHORTCUT_QUIT,
  SHORTCUT_LOADMEM,
  SHORTCUT_SAVEMEM,
  SHORTCUT_INSERTDISKA,
  SHORTCUT_KEYS,  /* number of shortcuts */
  SHORTCUT_NONE
} SHORTCUTKEYIDX;

typedef struct
{
  int withModifier[SHORTCUT_KEYS];
  int withoutModifier[SHORTCUT_KEYS];
} CNF_SHORTCUT;


typedef struct
{
  int nMemorySize;
  bool bAutoSave;
  char szMemoryCaptureFileName[FILENAME_MAX];
  char szAutoSaveFileName[FILENAME_MAX];
} CNF_MEMORY;


/* Joystick configuration */
typedef enum
{
  JOYSTICK_DISABLED,
  JOYSTICK_REALSTICK,
  JOYSTICK_KEYBOARD
} JOYSTICKMODE;

typedef struct
{
  JOYSTICKMODE nJoystickMode;
  bool bEnableAutoFire;
  bool bEnableJumpOnFire2;
  int nJoyId;
  int nKeyCodeUp, nKeyCodeDown, nKeyCodeLeft, nKeyCodeRight, nKeyCodeFire;
} JOYSTICK;

#define JOYSTICK_COUNT 6

typedef struct
{
  JOYSTICK Joy[JOYSTICK_COUNT];
} CNF_JOYSTICKS;


/* Disk image configuration */

typedef enum
{
  WRITEPROT_OFF,
  WRITEPROT_ON,
  WRITEPROT_AUTO
} WRITEPROTECTION;

#define MAX_FLOPPYDRIVES 2

typedef struct
{
  bool bAutoInsertDiskB;
  bool bSlowFloppy;                  /* true to slow down FDC emulation */
  WRITEPROTECTION nWriteProtection;
  char szDiskZipPath[MAX_FLOPPYDRIVES][FILENAME_MAX];
  char szDiskFileName[MAX_FLOPPYDRIVES][FILENAME_MAX];
  char szDiskImageDirectory[FILENAME_MAX];
} CNF_DISKIMAGE;


/* Hard drives configuration */
#define MAX_HARDDRIVES  23

typedef enum
{
  DRIVE_C,
  DRIVE_D,
  DRIVE_E,
  DRIVE_F
} DRIVELETTER;

typedef struct
{
  int nHardDiskDir;
  bool bUseHardDiskDirectories;
  bool bUseHardDiskImage;
  bool bUseIdeMasterHardDiskImage;
  bool bUseIdeSlaveHardDiskImage;
  WRITEPROTECTION nWriteProtection;
  bool bBootFromHardDisk;
  char szHardDiskDirectories[MAX_HARDDRIVES][FILENAME_MAX];
  char szHardDiskImage[FILENAME_MAX];
  char szIdeMasterHardDiskImage[FILENAME_MAX];
  char szIdeSlaveHardDiskImage[FILENAME_MAX];
} CNF_HARDDISK;

/* Falcon register $FFFF8006 bits 6 & 7 (mirrored in $FFFF82C0 bits 0 & 1):
 * 00 Monochrome
 * 01 RGB - Colormonitor
 * 10 VGA - Colormonitor
 * 11 TV
 */
#define FALCON_MONITOR_MASK 0x3F
#define FALCON_MONITOR_MONO 0x00  /* SM124 */
#define FALCON_MONITOR_RGB  0x40
#define FALCON_MONITOR_VGA  0x80
#define FALCON_MONITOR_TV   0xC0

typedef enum
{
  MONITOR_TYPE_MONO,
  MONITOR_TYPE_RGB,
  MONITOR_TYPE_VGA,
  MONITOR_TYPE_TV
} MONITORTYPE;

/* Screen configuration */
typedef struct
{
  MONITORTYPE nMonitorType;
  int nFrameSkips;
  bool bFullScreen;
  bool bAllowOverscan;
  bool bAspectCorrect;
  bool bUseExtVdiResolutions;
  int nSpec512Threshold;
  int nForceBpp;
  int nVdiColors;
  int nVdiWidth;
  int nVdiHeight;
  bool bShowStatusbar;
  bool bShowDriveLed;
  bool bCaptureChange;
  int nMaxWidth;
  int nMaxHeight;
} CNF_SCREEN;


/* Printer configuration */
typedef struct
{
  bool bEnablePrinting;
  bool bPrintToFile;
  char szPrintToFileName[FILENAME_MAX];
} CNF_PRINTER;


/* Midi configuration */
typedef struct
{
  bool bEnableMidi;
  char sMidiInFileName[FILENAME_MAX];
  char sMidiOutFileName[FILENAME_MAX];
} CNF_MIDI;


/* Dialog System */
typedef enum
{
  MACHINE_ST,
  MACHINE_STE,
  MACHINE_TT,
  MACHINE_FALCON
} MACHINETYPE;

typedef enum
{
  DSP_TYPE_NONE,
  DSP_TYPE_DUMMY,
  DSP_TYPE_EMU
} DSPTYPE;

typedef struct
{
  int nCpuLevel;
  int nCpuFreq;
  bool bCompatibleCpu;
  /*bool bAddressSpace24;*/
  MACHINETYPE nMachineType;
  bool bBlitter;                  /* TRUE if Blitter is enabled */
  DSPTYPE nDSPType;               /* how to "emulate" DSP */
  bool bRealTimeClock;
  bool bPatchTimerD;
  bool bFastForward;
} CNF_SYSTEM;


/* State of system is stored in this structure */
/* On reset, variables are copied into system globals and used. */
typedef struct
{
  /* Configure */
  CNF_LOG Log;
  CNF_DEBUGGER Debugger;
  CNF_SCREEN Screen;
  CNF_JOYSTICKS Joysticks;
  CNF_KEYBOARD Keyboard;
  CNF_SHORTCUT Shortcut;
  CNF_SOUND Sound;
  CNF_MEMORY Memory;
  CNF_DISKIMAGE DiskImage;
  CNF_HARDDISK HardDisk;
  CNF_ROM Rom;
  CNF_RS232 RS232;
  CNF_PRINTER Printer;
  CNF_MIDI Midi;
  CNF_SYSTEM System;
} CNF_PARAMS;


extern CNF_PARAMS ConfigureParams;
extern char sConfigFileName[FILENAME_MAX];

extern void Configuration_SetDefault(void);
extern void Configuration_Apply(bool bReset);
extern void Configuration_Load(const char *psFileName);
extern void Configuration_Save(void);
extern void Configuration_MemorySnapShot_Capture(bool bSave);

#endif
