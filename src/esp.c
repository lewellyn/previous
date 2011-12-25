/* Emulation of NCR53C90(A)
 Includes informations from QEMU-NeXT
 */

#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "esp.h"
#include "sysReg.h"

/* Command Register */
#define CMD_DMA      0x80
#define CMD_CMD      0x7f

#define CMD_NOP      0x00
#define CMD_FLUSH    0x01
#define CMD_RESET    0x02
#define CMD_BUSRESET 0x03
#define CMD_TI       0x10
#define CMD_ICCS     0x11
#define CMD_MSGACC   0x12
#define CMD_PAD      0x18
#define CMD_SATN     0x1a
#define CMD_SEL      0x41
#define CMD_SELATN   0x42
#define CMD_SELATNS  0x43
#define CMD_ENSEL    0x44

/* Interrupt Status Register */
#define STAT_DO      0x00
#define STAT_DI      0x01
#define STAT_CD      0x02
#define STAT_ST      0x03
#define STAT_MO      0x06
#define STAT_MI      0x07
#define STAT_PIO_MASK 0x06

#define STAT_TC      0x10
#define STAT_PE      0x20
#define STAT_GE      0x40
#define STAT_INT     0x80

/* Bus ID Register */
#define BUSID_DID    0x07

/* Interrupt Register */
#define INTR_FC      0x08
#define INTR_BS      0x10
#define INTR_DC      0x20
#define INTR_RST     0x80

/*Sequence Step Register */
#define SEQ_0        0x00
#define SEQ_CD       0x04

/*Configuration Register */
#define CFG1_RESREPT 0x40

#define CFG2_ENFEA   0x40

#define TCHI_FAS100A 0x04

#define IO_SEG_MASK	0x1FFFF

#define ESP_FIFO_SIZE 16

Uint8 readtranscountl;
Uint8 readtranscounth;
Uint8 writetranscountl;
Uint8 writetranscounth;
Uint8 fifo;
Uint8 command;
Uint8 status;
Uint8 selectbusid;
Uint8 intstatus;
Uint8 selecttimeout;
Uint8 seqstep;
Uint8 syncperiod;
Uint8 fifoflags;
Uint8 syncoffset;
Uint8 configuration;
Uint8 clockconv;
Uint8 esptest;

Uint8 irq_status;


/* ESP FIFO */

Uint8 esp_fifo[ESP_FIFO_SIZE];
Uint8 fifo_read_ptr;
Uint8 fifo_write_ptr;


void SCSI_DMA_Read(void) {
 	Log_Printf(LOG_WARN,"SCSI DMA read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_DMA_Write(void) {
 	Log_Printf(LOG_WARN,"SCSI DMA write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_CSR0_Read(void) {
 	Log_Printf(LOG_WARN,"SCSI CSR read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_CSR0_Write(void) {
    Log_Printf(LOG_WARN,"SCSI CSR0 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    Uint8 csr_value0;
    csr_value0 = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
    if ((csr_value0 & 0x04) == 0x04) {
        Log_Printf(LOG_WARN, "FIFO flush");
    }
    if ((csr_value0 & 0x01) == 0x01) {
        Log_Printf(LOG_WARN, "Enable");
    }
    if ((csr_value0 & 0x02) == 0x02) {
        Log_Printf(LOG_WARN, "Reset");
    }
    if ((csr_value0 & 0x08) == 0x08) {
        Log_Printf(LOG_WARN, "DMADIR");
    }
    if ((csr_value0 & 0x10) == 0x10) {
        Log_Printf(LOG_WARN, "CPUDMA");
        set_interrupt(INT_SCSI_DMA, SET_INT); //        intStat |= 0x4000000;
    }else{
        set_interrupt(INT_SCSI_DMA, RELEASE_INT); //        intStat &= ~(0x4000000);
    }
    if ((csr_value0 & 0x20) == 0x20) {
        Log_Printf(LOG_WARN, "INTMASK");
    }
    if ((csr_value0 & 0x80) == 0x80) {
        Log_Printf(LOG_WARN, "????");
    }
}

void SCSI_TransCountL_Read(void) { // 0x02014000
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=readtranscountl;
 	Log_Printf(LOG_WARN,"ESP TransCountL read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_TransCountL_Write(void) {
    readtranscountl=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_WARN,"ESP TransCountL write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_TransCountH_Read(void) { // 0x02014001
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=writetranscounth;
 	Log_Printf(LOG_WARN,"ESP TransCoundH read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_TransCountH_Write(void) {
    writetranscounth=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_WARN,"ESP TransCountH write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_FIFO_Read(void) { // 0x02014002
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=fifo;
 	Log_Printf(LOG_WARN,"ESP FIFO read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_FIFO_Write(void) {
    fifo=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
    esp_fifo[fifo_write_ptr] = fifo;
    fifo_write_ptr = (fifo_write_ptr + 1) % ESP_FIFO_SIZE;
    Log_Printf(LOG_WARN,"ESP FIFO size = %i)", (fifo_write_ptr - fifo_read_ptr));
 	Log_Printf(LOG_WARN,"ESP FIFO write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_Command_Read(void) { // 0x02014003
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=command;
 	Log_Printf(LOG_WARN,"ESP Command read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_Command_Write(void) {
    command=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_WARN,"ESP Command write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    switch (command & CMD_CMD) {
        case CMD_FLUSH:
            // flush fifo
            fifoflags &= 0xe0;
            Log_Printf(LOG_WARN,"FIFO flush");
            break;
            
        case CMD_BUSRESET:
            //Reset all Devices on SCSI bus
            if ((configuration & CFG1_RESREPT) == CFG1_RESREPT) {
                intstatus = INTR_RST; //raise irq
            }
            break;
            
        case CMD_SELATN:
            handle_satn();
            break;
            
        default: break;
    }
}

void SCSI_Status_Read(void) { // 0x02014004
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=status;
 	Log_Printf(LOG_WARN,"ESP Status read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_SelectBusID_Write(void) {
    selectbusid=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_WARN,"ESP SelectBusID write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_IntStatus_Read(void) { // 0x02014005
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=intstatus;
    if (irq_status == 1) {
            intstatus = 0x00;
            status &= ~STAT_TC;
            seqstep = SEQ_CD;
            esp_lower_irq();
    }
    Log_Printf(LOG_WARN,"ESP IntStatus read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_SelectTimeout_Write(void) {
    selecttimeout=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_WARN,"ESP SelectTimeout write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_SeqStep_Read(void) { // 0x02014006
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=seqstep;
 	Log_Printf(LOG_WARN,"ESP SeqStep read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_SyncPeriod_Write(void) {
    syncperiod=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_WARN,"ESP SyncPeriod write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_FIFOflags_Read(void) { // 0x02014007
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=fifoflags;
 	Log_Printf(LOG_WARN,"ESP FIFOflags read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_SyncOffset_Write(void) {
    syncoffset=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_WARN,"ESP SyncOffset write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_Configuration_Read(void) { // 0x02014008
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=configuration;
 	Log_Printf(LOG_WARN,"ESP Configuration read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_Configuration_Write(void) {
    configuration=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_WARN,"ESP Configuration write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_ClockConv_Write(void) { // 0x02014009
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=clockconv;
 	Log_Printf(LOG_WARN,"ESP ClockConv write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_Test_Write(void) {
    esptest=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_WARN,"ESP Test write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}



void esp_raise_irq(void) {
    if((status & STAT_INT) != STAT_INT) {
        status = status | STAT_INT;
        irq_status = 1;
        
        set_interrupt(INT_SCSI, SET_INT);
        
        Log_Printf(LOG_WARN, "Raise IRQ");
    }
}

void esp_lower_irq(void) {
    if ((status & STAT_INT) == STAT_INT) {
        status &= ~STAT_INT;
        irq_status = 0;
        
        set_interrupt(INT_SCSI, RELEASE_INT);
        
        Log_Printf(LOG_WARN, "Lower IRQ");
    }
}

Uint32 get_cmd (void) {
    Uint32 transfer_len;
    int target = selectbusid & BUSID_DID;
    
    transfer_len = fifo_write_ptr - fifo_read_ptr;
    // TODO: copy fifo to some buffer and then reset ...
    //fifo_read_ptr = fifo_write_ptr = 0; // reset fifo!
    Log_Printf(LOG_WARN, "get_cmd: len %i target %i", transfer_len, target);
    return transfer_len;
}

void do_cmd(void) {
    Uint8 busid = esp_fifo[0]; // in future read this from buffer (yet to be implemented)
    do_busid_cmd(busid);
}

void handle_satn(void) {
    int len;
    len = get_cmd(); // transfer_len;
    if(!(len == 0 || len == 7 || len == 11 || len == 13)) {
        Log_Printf(LOG_WARN, "Invalid command length %i", len);
        abort();
    } else {
        do_cmd();
    }
}

void do_busid_cmd(Uint8 busid) {
    int lun;
    Log_Printf(LOG_WARN, "do_busid_cmd: busid $%02x",busid);
    lun = busid & 7;
    
    status = STAT_TC;
    
    status = status | STAT_DI;
    
    intstatus = INTR_BS | INTR_FC;
    seqstep = SEQ_CD;
    
    esp_raise_irq();   
}