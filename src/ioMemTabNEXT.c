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
	{ 0x02004188, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02006010, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02006011, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02006012, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02006013, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02006014, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02007000, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02007800, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x0200c000, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200c002, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200c800, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200d000, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x0200e002, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200e003, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200e004, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200e005, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02010000, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02012004, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0, 0, NULL, NULL }
};
