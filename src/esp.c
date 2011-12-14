/* Emulation of NCR53C90(A)
 Includes informations from QEMU-NeXT
 */

#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "esp.h"

#define IO_SEG_MASK	0x1FFFF

Uint8 interrupt;


void SCSI_DMA_Read(void) {
 	Log_Printf(LOG_WARN,"SCSI DMA read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_DMA_Write(void) {
 	Log_Printf(LOG_WARN,"SCSI DMA write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_14020_Read(void) {
 	Log_Printf(LOG_WARN,"SCSI CSR read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_14020_Write(void) {
    Uint8 value14020;
    value14020 = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
    if ((value14020 & 0x04) == 0x04) {
        Log_Printf(LOG_WARN, "FIFO flush");
    }
    if ((value14020 & 0x01) == 0x01) {
        Log_Printf(LOG_WARN, "Enable");
    }
    if ((value14020 & 0x02) == 0x02) {
        Log_Printf(LOG_WARN, "Reset");
    }
    if ((value14020 & 0x08) == 0x08) {
        Log_Printf(LOG_WARN, "DMADIR");
    }
    if ((value14020 & 0x10) == 0x10) {
        Log_Printf(LOG_WARN, "CPUDMA");
        intStat |= 0x4000000;
    }else{
        intStat &= ~(0x4000000);
    }
    if ((value14020 & 0x20) == 0x20) {
        Log_Printf(LOG_WARN, "INTMASK");
    }
    if ((value14020 & 0x80) == 0x80) {
        Log_Printf(LOG_WARN, "????");
    }
 	Log_Printf(LOG_WARN,"SCSI CSR write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_TransCountL_Read(void) {
 	Log_Printf(LOG_WARN,"ESP read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_TransCountL_Write(void) {
 	Log_Printf(LOG_WARN,"ESP write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_TransCountH_Read(void) {
 	Log_Printf(LOG_WARN,"ESP read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_TransCountH_Write(void) {
 	Log_Printf(LOG_WARN,"ESP write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_FIFO_Read(void) {
 	Log_Printf(LOG_WARN,"ESP read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_FIFO_Write(void) {
 	Log_Printf(LOG_WARN,"ESP write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_Command_Read(void) {
 	Log_Printf(LOG_WARN,"ESP read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_Command_Write(void) {
 	Log_Printf(LOG_WARN,"ESP write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    switch (IoMem[IoAccessCurrentAddress & IO_SEG_MASK]) {
        case 0x03:
            //Reset all Devices on SCSI bus
            if ((IoMem[0x02014008 & IO_SEG_MASK] & 0x40) == 0x40) {
                interrupt = 0x80; //raise irq
            }
            break;
            
        case 0x42:
            break;
            
        default: break;
    }
}

void SCSI_Status_Read(void) {
 	Log_Printf(LOG_WARN,"ESP read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_SelectBusID_Write(void) {
 	Log_Printf(LOG_WARN,"ESP write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_IntStatus_Read(void) {
    IoMem[0x02014005 & IO_SEG_MASK] = interrupt;
    Log_Printf(LOG_WARN,"ESP read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_SelectTimeout_Write(void) {
 	Log_Printf(LOG_WARN,"ESP write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_SeqStep_Read(void) {
 	Log_Printf(LOG_WARN,"ESP read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_SyncPeriod_Write(void) {
 	Log_Printf(LOG_WARN,"ESP write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_FIFOflags_Read(void) {
 	Log_Printf(LOG_WARN,"ESP read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_SyncOffset_Write(void) {
 	Log_Printf(LOG_WARN,"ESP write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_Configuration_Read(void) {
 	Log_Printf(LOG_WARN,"ESP read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_Configuration_Write(void) {
 	Log_Printf(LOG_WARN,"ESP write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_ClockConv_Write(void) {
 	Log_Printf(LOG_WARN,"ESP write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_Test_Write(void) {
 	Log_Printf(LOG_WARN,"ESP write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}
