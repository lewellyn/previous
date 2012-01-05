/* Emulation of NCR53C90(A)
 Includes informations from QEMU-NeXT
 */

#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "esp.h"
#include "sysReg.h"
#include "dma.h"

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
Uint8 intstatus = 0x00;
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

/* buf */
#define SCSI_CMD_BUF_SIZE 16
Uint8 buf[SCSI_CMD_BUF_SIZE];


/* Experimental */
Uint32 dma;
Uint32 do_cmdvar;
Uint32 dma_counter;
int32_t data_len;
Uint32 dma_left;
Uint32 cmdlen;
Uint32 async_len;
Uint8 *async_buf;
int dma_enabled = 1;
void (*dma_cb);
#define ESP_MAX_DEVS 7



void SCSI_CSR0_Read(void) {
 	Log_Printf(LOG_WARN,"SCSI CSR0 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
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
        set_interrupt(INT_SCSI_DMA, SET_INT); //        intStat |= 0x4000000;
        esp_raise_irq(); 
        Log_Printf(LOG_WARN, "CPUDMA");
    }else{
        set_interrupt(INT_SCSI_DMA, RELEASE_INT); //        intStat &= ~(0x4000000);
        esp_lower_irq();
    }
    if ((csr_value0 & 0x20) == 0x20) {
        Log_Printf(LOG_WARN, "INTMASK");
    }
    if ((csr_value0 & 0x80) == 0x80) {
    //    Log_Printf(LOG_WARN, "????");
    }
}

void SCSI_CSR1_Read(void) {
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = 0x40;
 	Log_Printf(LOG_WARN,"SCSI CSR1 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_CSR1_Write(void) {
 	Log_Printf(LOG_WARN,"SCSI CSR1 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}


/* ESP Registers */

void SCSI_TransCountL_Read(void) { // 0x02014000
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=readtranscountl;
 	Log_Printf(LOG_WARN,"ESP TransCountL read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_TransCountL_Write(void) {
    writetranscountl=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_WARN,"ESP TransCountL write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_TransCountH_Read(void) { // 0x02014001
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=readtranscounth;
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
    
    dma = (command & CMD_DMA ? 1 : 0);
//    cmd_count++;
    switch (command & CMD_CMD) {
        case CMD_FLUSH:
            fifo_read_ptr = 0;
            fifo_write_ptr = 0;
            esp_fifo[0] = 0;
            fifoflags &= 0xe0;
            Log_Printf(LOG_WARN,"FIFO flush");
            break;
            
        case CMD_BUSRESET:
            //Reset all Devices on SCSI bus
            if (!(configuration & CFG1_RESREPT)) {
                intstatus = INTR_RST;
                Log_Printf(LOG_WARN,"Bus Reset raising IRQ\n");
                esp_raise_irq();
            }
            break;
            
        case CMD_TI:
            handle_ti();
            break;
            
        case CMD_SELATN:
            handle_satn();
            break;
            
        case CMD_PAD:
//            esp_raise_irq();
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
    Log_Printf(LOG_WARN,"ESP IntStatus read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    
    if (irq_status == 1) {
            intstatus = 0x00;
            status &= ~STAT_TC;
            seqstep = SEQ_CD;
            esp_lower_irq();
    }
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
    if(!(status & STAT_INT)) {
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
    Uint32 xfer_len = 0;
    int target = selectbusid & BUSID_DID;
    
    if(dma) {
        xfer_len = readtranscountl | (readtranscounth << 8);
        //dma_memory_read(dma_opaque, buf, xfer_len);
    } else {
        xfer_len = fifo_write_ptr - fifo_read_ptr;
        memcpy(buf, esp_fifo, (fifo_write_ptr - fifo_read_ptr));
                
        fifo_read_ptr = 0;
        fifo_write_ptr = 0;
        esp_fifo[0] = 0;
        fifoflags = 0xE0;
    }
    Log_Printf(LOG_WARN, "get_cmd: len %i target %i", xfer_len, target);
    
//    if(current_req) {
//        scsi_req_cancel(current_req);
//        async_len = 0;
//    }
    
    if(target >= ESP_MAX_DEVS /*|| bus.devs[target]*/) {
        status = 0;
        intstatus = INTR_DC;
        seqstep = SEQ_0;
        esp_raise_irq();
        return 0;
    }
    
//    current_dev = bus.devs[target];
    return xfer_len;
}

void do_cmd(void) {
    Uint8 busid = buf[0];
    do_busid_cmd(busid);
}

void handle_satn(void) {
    int len;
    
    if(!(dma_enabled)) {
        dma_cb = handle_satn; // experimental
    }
    
    len = get_cmd(); // transfer_len;
    if(!(len == 0 || len == 7 || len == 11 || len == 13)) {
        Log_Printf(LOG_WARN, "Invalid command length %i", len);
        abort();
    }
    if(len)
        do_cmd();
}

void do_busid_cmd(Uint8 busid) {
    int lun;
    Log_Printf(LOG_WARN, "do_busid_cmd: busid $%02x",busid);
    lun = busid & 7;
    
//    current_req = scsi_req_new(current_dev, 0, lun, NULL);
//    data_len = scsi_req_enqueue(current_req, buf);
    data_len = buf[5]; // for experimenting
    
    if (data_len != 0) {
        Log_Printf(LOG_WARN, "executing command\n");
        status = STAT_TC;
        
        dma_left = 0;
        dma_counter = 0;
        
        if(data_len > 0) {
            Log_Printf(LOG_WARN, "DATA IN\n");
            status |= STAT_DI;
        } else {
            Log_Printf(LOG_WARN, "DATA OUT\n");
            status |= STAT_DO;
        }
    esp_transfer_data();
    }
    
    intstatus = INTR_BS | INTR_FC;
    seqstep = SEQ_CD;
    
    esp_raise_irq();   
}


void esp_transfer_data(void) {
    Log_Printf(LOG_WARN, "transfer %d/%d\n", dma_left, data_len);
    
//    async_len = len;
//    async_buf = scsi_req_get_buf(req);
    if(dma_left) {
        esp_do_dma();
    } else if(dma_counter != 0 && data_len <= 0) {
        esp_dma_done();
    }
}


void esp_do_dma(void) {
    
    Log_Printf(LOG_WARN, "call esp_do_dma\n");

    Uint32 len;
    int to_device;
    
    to_device = (data_len < 0);
    
    len = dma_left;
    
    if(do_cmdvar){
        abort();
    /*
        Log_Printf(LOG_WARN, "command len %d + %d\n", cmdlen, len);
        dma_memory_read(fifo_buf[cmdlen], len);
        fifo_size = 0;
        cmdlen = 0;
        do_cmdvar = 0;
        // do_cmd(cmd_buf);
        return;
     */
    }
    if(async_len == 0) {
        return;
    }
    if(len > async_len) {
        len = async_len;
    }
    if(to_device) {
//        dma_memory_read(async_buf, len);
    } else {
//        dma_memory_write(async_buf, len);
    }
    
    dma_left -= len;
    async_buf += len;
    async_len -= len;
    
    if(to_device)
        data_len += len;
    else
        data_len -= len;
    
    if(async_len == 0) {
//        scsi_req_continue(current_req);
        nextdma_write(buf, data_len, NEXTDMA_SCSI);//experimental !!
        
        if (to_device || dma_left != 0 || (fifo_write_ptr - fifo_read_ptr) == 0) {
            return;
        }
    }
    
    esp_dma_done();
}

void esp_dma_done(void) {
    Log_Printf(LOG_WARN, "call esp_dma_done\n");
    status |= STAT_TC;
    intstatus = INTR_BS;
    seqstep = 0;
    fifoflags = 0;
    readtranscountl = 0;
    readtranscounth = 0;
    esp_raise_irq();
}

void handle_ti(void){
    Uint32 dma_len, min_len;
    
    dma_len = writetranscountl | (writetranscounth << 8);
    
    if(dma_len == 0) {
        dma_len = 0x10000;
    }
    
    dma_counter = dma_len;
    
    if(do_cmdvar)
        min_len = (dma_len < 32) ? dma_len : 32;
    else if (data_len < 0)
        min_len = (dma_len < (-data_len)) ? dma_len : data_len;
    else
        min_len = (dma_len > data_len) ? dma_len : data_len;
    Log_Printf(LOG_WARN, "Transfer Information len %d\n", min_len);
    
    if(dma) {
        dma_left = min_len;
        status &= ~STAT_TC;
        esp_do_dma();
    }else if(do_cmdvar){
        Log_Printf(LOG_WARN, "Command len: %i",cmdlen);
        data_len = 0;
        cmdlen = 0;
        do_cmdvar = 0;
        do_cmd();
        return;
    }
}