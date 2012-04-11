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


/* Function Prototypes */
void EnTx_Lower_IRQ(void);
void EnRx_Lower_IRQ(void);
void EnTx_Raise_IRQ(void);
void EnRx_Raise_IRQ(void);


/* Ethernet Regisers */
#define EN_TXSTAT   0
#define EN_TXMASK   1
#define EN_RXSTAT   2
#define EN_RXMASK   3
#define EN_TXMODE   4
#define EN_RXMODE   5

#define EN_RESET    6


/* Ethernet Register Constants */
#define TXSTAT_RDY  0x80    // Ready for Packet

#define RESET_VAL   0x80    // Generate Reset

typedef struct {
    Uint8 tx_status;
    Uint8 tx_mask;
    Uint8 tx_mode;
    Uint8 rx_status;
    Uint8 rx_mask;
    Uint8 rx_mode;
} ETHERNET_CONTROLLER;

ETHERNET_CONTROLLER ethernet;

Uint8 ethernet_buffer[1600];


Uint8 MACaddress[6];

void Ethernet_Read(void) {
	Uint8 reg = IoAccessCurrentAddress&0x0F;
	Uint8 val;
    
	switch (reg) {
		case EN_TXSTAT:
			val = ethernet.tx_status;
			break;
        case EN_TXMASK:
            val = ethernet.tx_mask;
            break;
        case EN_RXSTAT:
            val = ethernet.rx_status;
            break;
        case EN_RXMASK:
            val = ethernet.rx_mask;
            break;
        case EN_TXMODE:
            val = ethernet.tx_mode;
            break;
        case EN_RXMODE:
            val = ethernet.rx_mode;
            break;
        case EN_RESET:
            break;
            
        /* Read MAC Address */
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
            val = MACaddress[reg-8];
            break;
        default:
            break;
	}
    IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = val;
    
    Log_Printf(LOG_EN_LEVEL, "[Ethernet] read reg %d val %02x PC=%x %s at %d",reg,val,m68k_getpc(),__FILE__,__LINE__);
}

void Ethernet_Write(void) {
	Uint8 val = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
	Uint8 reg = IoAccessCurrentAddress&0xF;
    
    Log_Printf(LOG_EN_LEVEL, "[Ethernet] write reg %d val %02x PC=%x %s at %d",reg,val,m68k_getpc(),__FILE__,__LINE__);
    
    switch (reg) {
        case EN_TXSTAT:
            if (val == 0xFF)
                ethernet.tx_status = 0x80; // ? temp hack
            else
                ethernet.tx_status = val; //ethernet.tx_status & (0xF0 | ~val); ?
            EnTx_Raise_IRQ(); // check irq
			break;
        case EN_TXMASK:
            ethernet.tx_mask = val & 0xAF;
            EnTx_Raise_IRQ(); // check irq
            break;
        case EN_RXSTAT:
            if (val == 0xFF)
                ethernet.rx_status = 0x00;
            else
                ethernet.rx_status = val; //ethernet.rx_status & (0x07 | ~val); ?
            EnRx_Raise_IRQ(); // check irq
            break;
        case EN_RXMASK:
            ethernet.rx_mask = val & 0x9F;
            EnRx_Raise_IRQ(); // check irq
            break;
        case EN_TXMODE:
            ethernet.tx_mode = val;
            break;
        case EN_RXMODE:
            ethernet.rx_mode = val;
            break;
        case EN_RESET:
            if (val&RESET_VAL)
                Ethernet_Reset();
            break;

        /* Write MAC Address */
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
            MACaddress[reg-8] = val;
            break;
            
        default:
            break;
    }
}


void Ethernet_Reset(void) {
    ethernet.tx_status = TXSTAT_RDY;
    ethernet.tx_mask = 0x00;
    ethernet.tx_mode = 0x00;
    ethernet.rx_status = 0x00;
    ethernet.rx_mask = 0x00;
    ethernet.rx_mode = 0x00;
    
    EnTx_Lower_IRQ();
    EnRx_Lower_IRQ();
    
    // txlen = rxlen = txcount = 0;
    // set_promisc(true);
    // start_send();
}

void Ethernet_Transmit(void) {
    Uint32 size;
    dma_memory_read(ethernet_buffer, &size, CHANNEL_EN_TX);
    ethernet.tx_status = TXSTAT_RDY;
    
    printf("DMA TRANSMIT: Ethernet, size = %i byte\n", size);
    dma_memory_write(ethernet_buffer, size, CHANNEL_EN_RX); // loop back for experiment
}


void EnTx_Lower_IRQ(void) {
    
}

void EnRx_Lower_IRQ(void) {
    
}

void EnTx_Raise_IRQ(void) {
    
}

void EnRx_Raise_IRQ(void) {
    
}
