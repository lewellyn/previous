/*
  Hatari - dlgRom.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/
const char DlgMouse_fileid[] = "Previous dlgMouse.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "configuration.h"
#include "dialog.h"
#include "sdlgui.h"
#include "file.h"
#include "paths.h"


#define DLGMOUSE_AUTOGRAB       10
#define DLGMOUSE_EXIT           11

char accel_normal[8];
char accel_locked[8];

/* The Boot options dialog: */
static SGOBJ mousedlg[] =
{
	{ SGBOX, 0, 0, 0,0, 46,22, NULL },
    { SGTEXT, 0, 0, 16,1, 13,1, "Mouse options" },

	{ SGBOX, 0, 0, 1,4, 43,5, NULL },
	{ SGTEXT, 0, 0, 2,5, 34,1, "Mouse acceleration in normal mode:" },
    { SGTEXT, 0, 0, 2,7, 35,1, "Enter a value between 0.1 and 10.0:" },
	{ SGEDITFIELD, 0, 0, 38,7, 4,1, accel_normal },
    
    { SGBOX, 0, 0, 1,10, 43,5, NULL },
    { SGTEXT, 0, 0, 2,11, 34,1, "Mouse acceleration in locked mode:" },
    { SGTEXT, 0, 0, 2,13, 35,1, "Enter a value between 0.1 and 10.0:" },
	{ SGEDITFIELD, 0, 0, 38,13, 4,1, accel_locked },
    
	{ SGCHECKBOX, 0, 0, 2,16, 21,1, "Enable auto-locking" },
    
	{ SGBUTTON, SG_DEFAULT, 0, 14,19, 20,1, "Back to main menu" },
	{ -1, 0, 0, 0,0, 0,0, NULL }
};

float read_float_string(char *s)
{
    int i;
    float result=0.0;
    
    for (i=0; i<8; i++) {
        if (*s>=(0+'0') && *s<=(9+'0')) {
            result *= 10.0;
            result += (float)(*s-'0');
            s++;
        } else {
            if (i==0 && *s!='.' && *s!=',') /* bad input, default to 1.0 */
                result=1.0;
            break;
        }
    }

    if (*s == '.' || *s == ',') {
        s++;
        if (*s>=(0+'0') && *s<=(9+'0')) {
            result += (float)(*s-'0')/10.0;
            s++;
        }
        if (*s>=(0+'0') && *s<=(9+'0')) {
            if ((*s-'0')>=5) {
                result+=0.1;
            }
        }
    }
    
    if (result<0.1)
        result=0.1;
    if (result>10.0) {
        result=10.0;
    }
    
    printf("%f\n",result);
    
    return result;
}


/*-----------------------------------------------------------------------*/
/**
 * Show and process the Mouse options dialog.
 */
void Dialog_MouseDlg(void)
{
	int but;

	SDLGui_CenterDlg(mousedlg);

    /* Set up the dialog from actual values */
    
    mousedlg[DLGMOUSE_AUTOGRAB].state &= ~SG_SELECTED;
    if (ConfigureParams.Mouse.bEnableAutoGrab)
        mousedlg[DLGMOUSE_AUTOGRAB].state |= SG_SELECTED;
    
    sprintf(accel_normal, "%#.1f", ConfigureParams.Mouse.fAccelerationNormal);
    sprintf(accel_locked, "%#.1f", ConfigureParams.Mouse.fAccelerationLocked);
    
    
    /* Draw and process the dialog */
    
	do
	{
		but = SDLGui_DoDialog(mousedlg, NULL);
    }
	while (but != DLGMOUSE_EXIT && but != SDLGUI_QUIT
	       && but != SDLGUI_ERROR && !bQuitProgram);
    

    /* Read values from dialog */
    ConfigureParams.Mouse.bEnableAutoGrab = mousedlg[DLGMOUSE_AUTOGRAB].state&SG_SELECTED ? true : false;
    ConfigureParams.Mouse.fAccelerationNormal = read_float_string(accel_normal);
    ConfigureParams.Mouse.fAccelerationLocked = read_float_string(accel_locked);
}