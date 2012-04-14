/*  Previous - mo.c
 
 This file is distributed under the GNU Public License, version 2 or at
 your option any later version. Read the file gpl.txt for details.
 
 Canon Magneto-optical Disk Drive Emulation.
 
 Based on MESS source code.
 
 Dummy to pass POT.
 
 */

#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "configuration.h"
#include "mo.h"
#include "sysReg.h"
#include "dma.h"

#define LOG_MO_LEVEL LOG_WARN
#define IO_SEG_MASK	0x1FFFF


typedef struct {
    Uint16 track_num;
    Uint8 sector_incrnum;
    Uint8 sector_count;
    Uint8 int_status;
    Uint8 int_mask;
} mo_drive;

Uint8 ECC_buffer[1600];
Uint32 length;

Uint8 reg4;
Uint8 reg5;
Uint8 reg6;
Uint8 reg7;
Uint8 sector_position;

void check_ecc(void);
void compute_ecc(void);


void MOdrive_Read(void) {
    Uint8 val;
	Uint8 reg = IoAccessCurrentAddress&0x1F;
    
    switch (reg) {
        case 4:
            val = reg4;
            break;
            
        case 5:
            val = reg5;
            break;
            
        case 6:
            val = reg6;
            break;

        case 7:
            val = reg7;
            break;

        case 8:
            val = 0x00;
            break;

        case 9:
            val = 0x00;
            break;
            
        case 10:
            val = 0x00;
            break;

        case 11:
            val = 0x24;
            break;

        case 16:
            val = 0x00;
            break;

        default:
            break;
    }
    
    IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = val;
    Log_Printf(LOG_MO_LEVEL, "[MO Drive] read reg %d val %02x PC=%x %s at %d",reg,val,m68k_getpc(),__FILE__,__LINE__);
}


void MOdrive_Write(void) {
    Uint8 val = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
	Uint8 reg = IoAccessCurrentAddress&0x1F;
    
    Log_Printf(LOG_MO_LEVEL, "[MO Drive] write reg %d val %02x PC=%x %s at %d",reg,val,m68k_getpc(),__FILE__,__LINE__);
    
    switch (reg) {
        case 4:
            if (val) {
                //if (reg4&1)
                    //MOdrive_Reset();
                    
                reg4 = (reg4 & (~val & 0xfc)) | (val & 3);
            }
            break;
            
        case 5:
            reg5 = val;
            break;
            
        case 6:
            reg6 = val;
            break;
            
        case 7:
            reg7 = val;
            if (reg7 & 0xc0) {
                sector_position = 0;
                set_interrupt(INT_DISK_DMA, SET_INT);
            }
            
            if (reg7&0x40) { // ECC write
                dma_memory_read(ECC_buffer, &length, CHANNEL_DISK);
                reg4 |= 0xFF;
                if (reg6&0x20)
                    check_ecc();
                else
                    compute_ecc();
            }
            if (reg7&0x80) { // ECC read
                dma_memory_write(ECC_buffer, length, CHANNEL_DISK);
                reg4 |= 0xFF;
            }
            break;
            
        default:
            break;
    }
}

void check_ecc(void) {
    int i;
	for(i=0; i<0x400; i++)
		ECC_buffer[i] = i;
}

void compute_ecc(void) {
	memset(ECC_buffer+0x400, 0, 0x110);
}