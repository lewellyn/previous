/*
  Hatari - ioMemTables.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/

#ifndef HATARI_IOMEMTABLES_H
#define HATARI_IOMEMTABLES_H

/* Hardware address details */
typedef struct
{
    const Uint32 Address;     /* ST hardware address */
    const int SpanInBytes;    /* E.g. SIZE_BYTE, SIZE_WORD or SIZE_LONG */
    void (*ReadFunc)(void);   /* Read function */
    void (*WriteFunc)(void);  /* Write function */
} INTERCEPT_ACCESS_FUNC;

extern const INTERCEPT_ACCESS_FUNC IoMemTable_NEXT[];

static Uint32 intStat=0x04;
static Uint32 intMask=0x00000000;

#endif
