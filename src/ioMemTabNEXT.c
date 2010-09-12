/*
  Hatari - ioMemTabNEXT.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  Table with hardware IO handlers for the NEXT.
*/


const char IoMemTabST_fileid[] = "Hatari ioMemTabST.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "ioMem.h"
#include "ioMemTables.h"
#include "video.h"


/*-----------------------------------------------------------------------*/
/*
  List of functions to handle read/write hardware interceptions for a plain ST.
*/
const INTERCEPT_ACCESS_FUNC IoMemTable_NEXT[] =
{
	{ 0x20000000, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception }, /* Memory configuration */
	{ 0, 0, NULL, NULL }
};
