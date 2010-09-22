/*
  Previous - ioMemTabNEXT.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  Table with hardware IO handlers for the NEXT.
*/


const char IoMemTabST_fileid[] = "Previous ioMemTabST.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "ioMem.h"
#include "ioMemTables.h"
#include "video.h"
#include "configuration.h"
#include "sysdeps.h"
#include "m68000.h"

#define IO_SEG_MASK	0x1FFFF
/*
 *
 */
static Uint8 scr2_0=0;
static Uint8 scr2_1=0;
static Uint8 scr2_2=0;
static Uint8 scr2_3=0;

void SCR2_Write0(void)
{	
//	Log_Printf(LOG_WARN,"SCR2 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & IO_SEG_MASK],regs.pc);
	scr2_0=IoMem[IoAccessCurrentAddress & 0x1FFFF];
}

void SCR2_Read0(void)
{
//	Log_Printf(LOG_WARN,"SCR2 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,regs.pc);
	IoMem[IoAccessCurrentAddress & 0x1FFFF]=scr2_0;
}

void SCR2_Write1(void)
{	
//	Log_Printf(LOG_WARN,"SCR2 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & IO_SEG_MASK],regs.pc);
	// scr2_1=IoMem[IoAccessCurrentAddress & 0x1FFFF];
}

void SCR2_Read1(void)
{
//	Log_Printf(LOG_WARN,"SCR2 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,regs.pc);
	IoMem[IoAccessCurrentAddress & 0x1FFFF]=0x01;
}


#define SCR2_RTDATA		0x04
#define SCR2_RTCLK		0x02
#define SCR2_RTCE		0x01

#define SCR2_ROM		0x01

Uint8 rtc_ram[32];

void SCR2_Write2(void)
{	
	static int phase=0;
	static Uint8 rtc_command=0;
	static Uint8 rtc_value=0;
	static Uint8 rtc_return=0;
	static Uint8 rtc_status=0x90;	// FTU at startup MCS1850

	Uint8 old_scr2_2=scr2_2;
//	Log_Printf(LOG_WARN,"SCR2 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & IO_SEG_MASK],regs.pc);
	scr2_2=IoMem[IoAccessCurrentAddress & 0x1FFFF];

/*
	if ((old_scr2_2&SCR2_RTCE)!=(scr2_2&SCR2_RTCE))
		Log_Printf(LOG_WARN,"SCR2 RTCE change at $%08x val=%x PC=$%08x\n",
			   IoAccessCurrentAddress,scr2_2&SCR2_RTCE,regs.pc);

	if ((old_scr2_2&SCR2_RTCLK)!=(scr2_2&SCR2_RTCLK))
		Log_Printf(LOG_WARN,"SCR2 RTCLK change at $%08x val=%x PC=$%08x\n",
			   IoAccessCurrentAddress,scr2_2&SCR2_RTCLK,regs.pc);

	if ((old_scr2_2&SCR2_RTDATA)!=(scr2_2&SCR2_RTDATA))
		Log_Printf(LOG_WARN,"SCR2 RTDATA change at $%08x val=%x PC=$%08x\n",
			   IoAccessCurrentAddress,scr2_2&SCR2_RTDATA,regs.pc);
*/

// and now some primitive handling
// treat only if CE is set to 1
	if (scr2_2&SCR2_RTCE) {
			if (phase==-1) phase=0;
			// if we are in going down clock... do something
			if (((old_scr2_2&SCR2_RTCLK)!=(scr2_2&SCR2_RTCLK)) && ((scr2_2&SCR2_RTCLK)==0) ) {
				if (phase<8)
					rtc_command=(rtc_command<<1)|((scr2_2&SCR2_RTDATA)?1:0);
				if ((phase>=8) && (phase<16)) {
					rtc_value=(rtc_value<<1)|((scr2_2&SCR2_RTDATA)?1:0);

					// if we read RAM register, output RT_DATA bit
					if (rtc_command<=0x1F) {
						scr2_2=scr2_2&(~SCR2_RTDATA);
						if (rtc_ram[rtc_command]&(0x80>>(phase-8)))
							scr2_2|=SCR2_RTDATA;
						rtc_return=(rtc_return<<1)|((scr2_2&SCR2_RTDATA)?1:0);
					}
					// read the status 0x30
					if (rtc_command==0x30) {
						scr2_2=scr2_2&(~SCR2_RTDATA);
						// for now status = 0x98 (new rtc + FTU)
						if (rtc_status&(0x80>>(phase-8)))
							scr2_2|=SCR2_RTDATA;
						rtc_return=(rtc_return<<1)|((scr2_2&SCR2_RTDATA)?1:0);
					}
					// read the status 0x31
					if (rtc_command==0x31) {
						scr2_2=scr2_2&(~SCR2_RTDATA);
						// for now 0x00
						if (0x00&(0x80>>(phase-8)))
							scr2_2|=SCR2_RTDATA;
						rtc_return=(rtc_return<<1)|((scr2_2&SCR2_RTDATA)?1:0);
					}

				}

				phase++;
				if (phase==16) {
					Log_Printf(LOG_WARN,"SCR2 RTC command complete %x %x %x at PC=$%08x\n",
					   		rtc_command,rtc_value,rtc_return,regs.pc);
					if ((rtc_command>=0x80) && (rtc_command<=0x9F))
						rtc_ram[rtc_command-0x80]=rtc_value;

					// write to x30 register
					if (rtc_command==0xB1) {
						// clear FTU
						if (rtc_value & 0x04) {
							rtc_status=rtc_status&(~0x18);
						}
					}
				}
			}
	} else {
// else end or abort
		phase=-1;
		rtc_command=0;
		rtc_value=0;
	}	

}

void SCR2_Read2(void)
{
//	Log_Printf(LOG_WARN,"SCR2 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,regs.pc);
	IoMem[IoAccessCurrentAddress & 0x1FFFF]=(scr2_2 & SCR2_RTDATA)|0x08; // + data
}

void SCR2_Write3(void)
{	
	Uint8 old_scr2_3=scr2_3;
//	Log_Printf(LOG_WARN,"SCR2 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & IO_SEG_MASK],regs.pc);
	scr2_3=IoMem[IoAccessCurrentAddress & 0x1FFFF];
	if ((old_scr2_3&SCR2_ROM)!=(scr2_3&SCR2_ROM))
		Log_Printf(LOG_WARN,"SCR2 ROM change at $%08x val=%x PC=$%08x\n",
			   IoAccessCurrentAddress,scr2_3&SCR2_ROM,regs.pc);
}

void SCR2_Read3(void)
{
//	Log_Printf(LOG_WARN,"SCR2 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,regs.pc);
	IoMem[IoAccessCurrentAddress & 0x1FFFF]=0x80;
}

void SCR1_Read0(void)
{
	Log_Printf(LOG_WARN,"SCR1 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,regs.pc);
	IoMem[IoAccessCurrentAddress & 0x1FFFF]=0xFF;
}
void SCR1_Read1(void)
{
	Log_Printf(LOG_WARN,"SCR1 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,regs.pc);
	IoMem[IoAccessCurrentAddress & 0x1FFFF]=0xFF;
}
void SCR1_Read2(void)
{
	Log_Printf(LOG_WARN,"SCR1 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,regs.pc);
	IoMem[IoAccessCurrentAddress & 0x1FFFF]=0x5F;
}
void SCR1_Read3(void)
{
	Log_Printf(LOG_WARN,"SCR1 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,regs.pc);
	IoMem[IoAccessCurrentAddress & 0x1FFFF]=0xCF;
}


/*-----------------------------------------------------------------------*/
/*
  List of functions to handle read/write hardware interceptions.
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
	{ 0x0200c000, SIZE_BYTE, SCR1_Read0, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200c001, SIZE_BYTE, SCR1_Read1, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200c002, SIZE_BYTE, SCR1_Read2, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200c003, SIZE_BYTE, SCR1_Read3, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200c800, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200d000, SIZE_BYTE, SCR2_Read0, SCR2_Write0 },
	{ 0x0200d001, SIZE_BYTE, SCR2_Read1, SCR2_Write1 },
	{ 0x0200d002, SIZE_BYTE, SCR2_Read2, SCR2_Write2 },
	{ 0x0200d003, SIZE_BYTE, SCR2_Read3, SCR2_Write3 },
	{ 0x0200e002, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200e003, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200e004, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200e005, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02010000, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02012004, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0, 0, NULL, NULL }
};
