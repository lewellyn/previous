/*  Previous - iethernet.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

   Network adapter for the NEXT. 

   constants and struct netBSD mb8795reg.h file

*/

#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "configuration.h"
#include "esp.h"
#include "sysReg.h"
#include "dma.h"
#include "scsi.h"
#include "ethernet.h"

#define LOG_EN_LEVEL LOG_WARN
#define IO_SEG_MASK	0x1FFFF

/* Ethernet Regisers */
#define EN_TXSTAT   0
#define EN_TXMASK   1
#define EN_RXSTAT   2
#define EN_RXMASK   3
#define EN_TXMODE   4
#define EN_RXMODE   5

#define EN_RESET    6


typedef struct {
    Uint8 tx_status;
    Uint8 tx_mask;
    Uint8 tx_mode;
    Uint8 rx_status;
    Uint8 rx_mask;
    Uint8 rx_mode;
} ETHERNET_CONTROLLER;

ETHERNET_CONTROLLER ethernet;


Uint8 MACaddress[6];

void Ethernet_Read(void) {
	int addr=IoAccessCurrentAddress & IO_SEG_MASK;
	int reg=addr%16;
	int val=0;
    
    Log_Printf(LOG_EN_LEVEL, "[Ethernet] read reg %d val %02x PC=%x %s at %d",reg,val,m68k_getpc(),__FILE__,__LINE__);
    
	switch (reg) {
		case EN_TXSTAT:
			IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=0x80;
			break;
        case EN_TXMASK:
            break;
        case EN_RXSTAT:
            break;
        case EN_RXMASK:
            break;
        case EN_TXMODE:
            break;
        case EN_RXMODE:
            break;
        /* Read MAC Address */
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
            IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = MACaddress[reg-8];
            break;
	}    	
}

void Ethernet_Write(void) {
	int val=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
	int reg=IoAccessCurrentAddress%16;
    
    Log_Printf(LOG_EN_LEVEL, "[Ethernet] write reg %d val %02x PC=%x %s at %d",reg,val,m68k_getpc(),__FILE__,__LINE__);
    
    switch (reg) {
        /* Write MAC Address */
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
            MACaddress[reg-8] = IoMem[IoAccessCurrentAddress&IO_SEG_MASK];
            break;
            
        default:
            break;
    }
}

