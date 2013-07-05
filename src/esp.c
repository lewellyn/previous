/* Emulation of NCR53C90(A)
 Includes informations from QEMU-NeXT
 */

#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "configuration.h"
#include "esp.h"
#include "sysReg.h"
#include "dma.h"
#include "scsi.h"

#define LOG_SCSI_LEVEL LOG_DEBUG


#define IO_SEG_MASK	0x1FFFF

typedef enum {
    DISCONNECTED,
    INITIATOR,
    TARGET
} SCSI_STATE;

SCSI_STATE esp_state;


/* ESP Registers */
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

Uint32 esp_counter;

/* ESP Status Variables */
Uint8 irq_status;
Uint8 mode_dma;

/* ESP FIFO */
#define ESP_FIFO_SIZE 16
Uint8 esp_fifo[ESP_FIFO_SIZE];
Uint8 fifo_read_ptr;
Uint8 fifo_write_ptr;

/* Experimental */
#define ESP_CLOCK_FREQ  20 /* ESP is clocked at 20 MHz */


/* ESP DMA control and status registers */

void ESP_DMA_CTRL_Read(void) {
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = esp_dma.control;
 	Log_Printf(LOG_SCSI_LEVEL,"SCSI DMA control read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_DMA_CTRL_Write(void) {
    Log_Printf(LOG_SCSI_LEVEL,"SCSI DMA control write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    esp_dma.control = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
        
    if (esp_dma.control&ESPCTRL_FLUSH) {
        Log_Printf(LOG_SCSI_LEVEL, "flush DMA buffer\n");
        dma_esp_flush_buffer();
    }
    if (esp_dma.control&ESPCTRL_CHIP_TYPE) {
        Log_Printf(LOG_SCSI_LEVEL, "SCSI controller is WD33C92\n");
    } else {
        Log_Printf(LOG_SCSI_LEVEL, "SCSI controller is NCR53C90\n");
    }
    if (esp_dma.control&ESPCTRL_RESET) {
        Log_Printf(LOG_SCSI_LEVEL, "reset SCSI controller\n");
        esp_reset_hard();
    }
    if (esp_dma.control&ESPCTRL_DMA_READ) {
        Log_Printf(LOG_SCSI_LEVEL, "DMA from SCSI to mem\n");
    } else {
        Log_Printf(LOG_SCSI_LEVEL, "DMA from mem to SCSI\n");
    }
    if (esp_dma.control&ESPCTRL_MODE_DMA) {
        Log_Printf(LOG_SCSI_LEVEL, "mode DMA\n");
    }else{
        Log_Printf(LOG_SCSI_LEVEL, "mode PIO\n");
    }
    if (esp_dma.control&ESPCTRL_ENABLE_INT) {
        Log_Printf(LOG_SCSI_LEVEL, "enable ESP interrupt");
    }
    switch (esp_dma.control&ESPCTRL_CLKMASK) {
        case ESPCTRL_CLK10MHz:
            Log_Printf(LOG_SCSI_LEVEL, "10 MHz clock\n");
            break;
        case ESPCTRL_CLK12MHz:
            Log_Printf(LOG_SCSI_LEVEL, "12.5 MHz clock\n");
            break;
        case ESPCTRL_CLK16MHz:
            Log_Printf(LOG_SCSI_LEVEL, "16.6 MHz clock\n");
            break;
        case ESPCTRL_CLK20MHz:
            Log_Printf(LOG_SCSI_LEVEL, "20 MHz clock\n");
            break;
        default:
            break;
    }
}

void ESP_DMA_FIFO_STAT_Read(void) {
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = esp_dma.status;
 	Log_Printf(LOG_SCSI_LEVEL,"SCSI DMA FIFO status read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_DMA_FIFO_STAT_Write(void) {
 	Log_Printf(LOG_SCSI_LEVEL,"SCSI DMA FIFO status write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    esp_dma.status = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
}

void ESP_DMA_set_status(void) { /* this is just a guess */
    if ((esp_dma.status&ESPSTAT_STATE_MASK) == ESPSTAT_STATE_D0S1) {
        //Log_Printf(LOG_WARN,"DMA in buffer 0, SCSI in buffer 1\n");
        esp_dma.status = (esp_dma.status&~ESPSTAT_STATE_MASK)|ESPSTAT_STATE_D1S0;
    } else {
        //Log_Printf(LOG_WARN,"DMA in buffer 1, SCSI in buffer 0\n");
        esp_dma.status = (esp_dma.status&~ESPSTAT_STATE_MASK)|ESPSTAT_STATE_D0S1;
    }
}

/* ESP Registers */

void ESP_TransCountL_Read(void) { // 0x02014000
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=esp_counter&0xFF;
 	Log_Printf(LOG_SCSI_LEVEL,"ESP TransCountL read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_TransCountL_Write(void) {
    writetranscountl=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_WARN,"ESP TransCountL write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_TransCountH_Read(void) { // 0x02014001
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=(esp_counter>>8)&0xFF;
 	Log_Printf(LOG_SCSI_LEVEL,"ESP TransCoundH read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_TransCountH_Write(void) {
    writetranscounth=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_WARN,"ESP TransCountH write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_FIFO_Read(void) { // 0x02014002
    if ((fifo_write_ptr - fifo_read_ptr) > 0) {
        IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = esp_fifo[fifo_read_ptr];
        Log_Printf(LOG_SCSI_LEVEL,"ESP FIFO read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
        Log_Printf(LOG_WARN,"ESP FIFO Read, size = %i, val=%02x", fifo_write_ptr - fifo_read_ptr, IoMem[IoAccessCurrentAddress & IO_SEG_MASK]);
        fifo_read_ptr++;
        fifoflags = fifoflags - 1;
    } else {
        IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = 0x00;
        Log_Printf(LOG_WARN, "ESP FIFO is empty!\n");
    } 
}

void ESP_FIFO_Write(void) {
    esp_fifo[fifo_write_ptr] = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
    fifo_write_ptr++;
    fifoflags = fifoflags + 1;
    Log_Printf(LOG_SCSI_LEVEL,"ESP FIFO write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    
    if (fifo_write_ptr > (ESP_FIFO_SIZE - 1)) {
        Log_Printf(LOG_SCSI_LEVEL, "ESP FIFO overflow! Resetting FIFO!\n");
        esp_flush_fifo();
    }
    Log_Printf(LOG_SCSI_LEVEL,"ESP FIFO size = %i", fifo_write_ptr - fifo_read_ptr);
}

void ESP_Command_Read(void) { // 0x02014003
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=command;
 	Log_Printf(LOG_SCSI_LEVEL,"ESP Command read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_Command_Write(void) {
    command=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_SCSI_LEVEL,"ESP Command write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
        
    /* Check if command is valid for actual state */
    if ((command&CMD_TYP_MASK)!=CMD_TYP_MSC) {
        if ((esp_state==TARGET && !command&CMD_TYP_TGT) ||
            (esp_state==INITIATOR && !command&CMD_TYP_INR) ||
            (esp_state==DISCONNECTED && !command&CMD_TYP_DIS)) {
            Log_Printf(LOG_WARN, "ESP Command: Illegal command for actual ESP state!\n");
            intstatus |= INTR_ILL;
            esp_raise_irq();
            return;
        }
    }
    
    /* Check if the command is a DMA command */
    if (command & CMD_DMA) {
        /* Load the internal counter on every DMA command, do not decrement actual registers! */
        esp_counter = writetranscountl | (writetranscounth << 8);
        if (esp_counter == 0) { /* 0 means maximum value */
            esp_counter = 0x10000;
        }
        status &= ~STAT_TC;
        mode_dma = 1;
    } else {
        mode_dma = 0;
    }
    
    if ((command & CMD_CMD) != CMD_NOP)
    	status = (status&STAT_MASK)|STAT_CD;
    
    switch (command & CMD_CMD) {
            /* Miscellaneous */
        case CMD_NOP:
            Log_Printf(LOG_WARN, "ESP Command: NOP\n");
            break;
        case CMD_FLUSH:
            Log_Printf(LOG_WARN,"ESP Command: flush FIFO\n");
            esp_flush_fifo();
    	    status = (status&STAT_MASK)|STAT_ST;
            break;
        case CMD_RESET:
            Log_Printf(LOG_WARN,"ESP Command: reset chip\n");
            esp_reset_hard();
            break;
        case CMD_BUSRESET:
            Log_Printf(LOG_WARN, "ESP Command: reset SCSI bus\n");
            esp_bus_reset();
            break;
            /* Disconnected */
        case CMD_RESEL:
            Log_Printf(LOG_WARN, "ESP Command: reselect sequence\n");
            abort();
            break;
        case CMD_SEL:
            Log_Printf(LOG_WARN, "ESP Command: select without ATN sequence\n");
            abort();
            break;
        case CMD_SELATN:
            Log_Printf(LOG_WARN, "ESP Command: select with ATN sequence\n");
            esp_select(true);
            break;
        case CMD_SELATNS:
            Log_Printf(LOG_WARN, "ESP Command: select with ATN and stop sequence\n");
            abort();
            break;
        case CMD_ENSEL:
            Log_Printf(LOG_WARN, "ESP Command: enable selection/reselection\n");
#if 0
            if (no_target) {
                Log_Printf(LOG_SCSI_LEVEL, "ESP retry timeout");
                intstatus = INTR_RESEL;
            	seqstep = SEQ_SELTIMEOUT;
            } else {
                Log_Printf(LOG_WARN, "Reselect: target %i, lun %i\n", SCSIbus.target, SCSIcommand.lun);
                status = (status&STAT_MASK)|STAT_MI;
                intstatus = INTR_RESEL|INTR_FC;
                esp_fifo[0] = (1<<SCSIbus.target); // target bit
                esp_fifo[1] = 0x80|(SCSIcommand.lun&0x07); // identify message (identifymask|(lun&lun_mask))
                fifoflags = 2;
                fifo_write_ptr = 2;
                fifo_read_ptr = 0;
//                esp_raise_irq();
            }
#endif
            break;
        case CMD_DISSEL:
            Log_Printf(LOG_WARN, "ESP Command: disable selection/reselection\n");
            intstatus = INTR_FC;
            abort();
            esp_raise_irq();
            break;
            /* Initiator */
        case CMD_TI:
            Log_Printf(LOG_WARN, "ESP Command: transfer information\n");
            esp_transfer_info();
            break;
        case CMD_ICCS:
            Log_Printf(LOG_WARN, "ESP Command: initiator command complete sequence\n");
            esp_initiator_command_complete();
            break;
        case CMD_MSGACC:
            Log_Printf(LOG_WARN, "ESP Command: message accepted\n");
            esp_message_accepted();
            break;            
        case CMD_PAD:
            Log_Printf(LOG_WARN, "ESP Command: transfer pad\n");
            esp_transfer_pad();
            break;
        case CMD_SATN:
            Log_Printf(LOG_WARN, "ESP Command: set ATN\n");
            break;
            /* Target */
        case CMD_SEMSG:
        case CMD_SESTAT:
        case CMD_SEDAT:
        case CMD_DISSEQ:
        case CMD_TERMSEQ:
        case CMD_TCCS:
        case CMD_RMSGSEQ:
        case CMD_RCOMM:
        case CMD_RDATA:
        case CMD_RCSEQ:
            Log_Printf(LOG_WARN, "ESP Command: Target commands not emulated!\n");
            abort();
            status = (status&STAT_MASK)|STAT_ST;	   
            intstatus = INTR_ILL;
            seqstep = SEQ_0;
            fifoflags = 0x00;
            esp_raise_irq();
            break;
        case CMD_DIS:
            Log_Printf(LOG_WARN, "ESP Command: DISCONNECT not emulated!\n");
            abort();
            status = (status&STAT_MASK)|STAT_ST;	   
            intstatus = INTR_DC; 
            seqstep = SEQ_0;
            break;

            
        default:
            Log_Printf(LOG_WARN, "ESP Command: Illegal command!\n");
            intstatus |= INTR_ILL;
            esp_raise_irq();
            break;
    }
}

void ESP_Status_Read(void) { // 0x02014004
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=status;
 	Log_Printf(LOG_SCSI_LEVEL,"ESP Status read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_SelectBusID_Write(void) {
    selectbusid=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_SCSI_LEVEL,"ESP SelectBusID write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_IntStatus_Read(void) { // 0x02014005
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=intstatus;
    Log_Printf(LOG_SCSI_LEVEL,"ESP IntStatus read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    
    if (irq_status == 1) {
            intstatus = 0x00;
            status &= ~(STAT_VGC | STAT_PE | STAT_GE );
            seqstep = SEQ_0;
            esp_lower_irq();
    }
}

void ESP_SelectTimeout_Write(void) {
    selecttimeout=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_SCSI_LEVEL,"ESP SelectTimeout write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_SeqStep_Read(void) { // 0x02014006
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=seqstep;
 	Log_Printf(LOG_SCSI_LEVEL,"ESP SeqStep read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_SyncPeriod_Write(void) {
    syncperiod=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_SCSI_LEVEL,"ESP SyncPeriod write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_FIFOflags_Read(void) { // 0x02014007
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=fifoflags;
 	Log_Printf(LOG_WARN,"ESP FIFOflags read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_SyncOffset_Write(void) {
    syncoffset=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_SCSI_LEVEL,"ESP SyncOffset write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_Configuration_Read(void) { // 0x02014008
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=configuration;
 	Log_Printf(LOG_SCSI_LEVEL,"ESP Configuration read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    //esp_raise_irq(); // experimental!
}

void ESP_Configuration_Write(void) {
    configuration=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_SCSI_LEVEL,"ESP Configuration write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_ClockConv_Write(void) { // 0x02014009
    clockconv=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_SCSI_LEVEL,"ESP ClockConv write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_Test_Write(void) { // 0x0201400a
    esptest=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_SCSI_LEVEL,"ESP Test write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

/* System reads this register to check if we use old or new SCSI controller.
 * Return 0 to report old chip. */
void ESP_Conf2_Read(void) { // 0x0201400b
    if (ConfigureParams.System.nSCSI == NCR53C90)
        IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = 0x00;
}


/* Helper functions */

/* This is the handler function for ESP delayed interrupts */
void ESP_InterruptHandler(void) {
	CycInt_AcknowledgeInterrupt();
    esp_raise_irq();
}


void esp_raise_irq(void) {
    if(!(status & STAT_INT)) {
        status |= STAT_INT;
        irq_status = 1;
        
        if (esp_dma.control&ESPCTRL_ENABLE_INT) {
            set_interrupt(INT_SCSI, SET_INT);
        }
        
        printf("[ESP] Raise IRQ: state=");
        switch (esp_state) {
            case DISCONNECTED: printf("disconnected"); break;
            case INITIATOR: printf("initiator"); break;
            case TARGET: printf("target"); break;
            default: printf("unknown"); break;
        }
        printf(", phase=");
        switch (status&STAT_MI) {
            case STAT_DO: printf("data out"); break;
            case STAT_DI: printf("data in"); break;
            case STAT_CD: printf("command"); break;
            case STAT_ST: printf("status"); break;
            case STAT_MI: printf("msg in"); break;
            case STAT_MO: printf("msg out"); break;
            default: printf("unknown"); break;
        }
        if (status&STAT_TC) {
            printf(", transfer complete");
        } else {
            printf(", transfer not complete");
        }
        printf(", sequence step=%i", seqstep);
        printf(", interrupt status:\n");
        if (intstatus&INTR_RST) printf("bus reset\n");
        if (intstatus&INTR_BS) printf("bus service\n");
        if (intstatus&INTR_DC) printf("disconnected\n");
        if (intstatus&INTR_FC) printf("function complete\n");
        if (intstatus&INTR_ILL) printf("illegal command\n");
        if (intstatus&INTR_RESEL) printf("reselected\n");
        if (intstatus&INTR_SEL) printf("selected\n");
        if (intstatus&INTR_SELATN) printf("selected with ATN\n");
    }
}

void esp_lower_irq(void) {
    if (status & STAT_INT) {
        status &= ~STAT_INT;
        irq_status = 0;
        
        set_interrupt(INT_SCSI, RELEASE_INT);
        
        Log_Printf(LOG_SCSI_LEVEL, "Lower IRQ\n");
    }
}

/* Functions */

/* Reset chip */
void esp_reset_hard(void) {
    Log_Printf(LOG_WARN, "ESP hard reset\n");

    clockconv = 0x02;
    configuration &= ~0xF8; // clear chip test mode, parity enable, parity test, scsi request/int disable, slow cable mode
    esp_flush_fifo();
    syncperiod = 0x05;
    syncoffset = 0x00;
    esp_lower_irq();
    intstatus = 0x00;
    status &= ~(STAT_VGC | STAT_PE | STAT_GE); // need valid group code bit? clear transfer complete aka valid group code, parity error, gross error
    esp_reset_soft();
}



void esp_reset_soft(void) {
    status &= ~STAT_TC; /* clear transfer count zero */
    
    /* check, if this is complete */
    mode_dma = 0;
    esp_counter = 0; /* reset counter, but not actual registers! */
    
    seqstep = 0x00;
    
    /* writetranscountl, writetranscounth, selectbusid, selecttimeout are not initialized by reset */

    /* This part is "disconnect reset" */
    command = 0x00;
    esp_state = DISCONNECTED;
}


/* Reset SCSI bus */
void esp_bus_reset(void) {
    
    esp_reset_soft();
    if (!(configuration & CFG1_RESREPT)) {
        intstatus = INTR_RST;
        status = (status&STAT_MASK)|STAT_MI; /* CHECK: why message in phase? */
        Log_Printf(LOG_SCSI_LEVEL,"Bus Reset raising IRQ configuration=%x\n",configuration);
        CycInt_AddRelativeInterrupt(50*33, INT_CPU_CYCLE, INTERRUPT_ESP); /* TODO: find correct timing, use actual cpu clock from preferences */
    } else
        Log_Printf(LOG_SCSI_LEVEL,"Bus Reset not interrupting configuration=%x\n",configuration);
}


/* Flush FIFO */
void esp_flush_fifo(void) {
    fifo_read_ptr = 0;
    fifo_write_ptr = 0;
    esp_fifo[0] = 0;
    fifoflags &= 0xE0;
}


/* Select with or without ATN */
void esp_select(bool atn) {
    int cmd_size;
    Uint8 identify_msg = 0;
    Uint8 commandbuf[SCSI_CDB_MAX_SIZE];

    seqstep = 0;
    
    /* First select our target */
    Uint8 target = selectbusid & BUSID_DID; /* Get bus ID from register */
    bool timeout = SCSIdisk_Select(target);
    if (timeout) {
        /* If a timeout occurs, generate disconnect interrupt */
        intstatus = INTR_DC;
        status = (status&STAT_MASK)|scsi_phase; /* check status */
        esp_state = DISCONNECTED;
        int seltout = (selecttimeout * 8192 * clockconv) / ESP_CLOCK_FREQ; /* timeout in microseconds */
        Log_Printf(LOG_WARN, "[ESP] Select: Timeout after %i microseconds",seltout);
        CycInt_AddRelativeInterrupt(seltout*ConfigureParams.System.nCpuFreq, INT_CPU_CYCLE, INTERRUPT_ESP);
        return;
    }
    
    /* Next get our command */
    if(mode_dma == 1) {
        cmd_size = esp_counter;
        Log_Printf(LOG_WARN, "[ESP] Select: Reading command using DMA, size %i byte",cmd_size);
        //memcpy(commandbuf, dma_read_buffer, cmd_size); /* add DMA function here */
        abort();
    } else {
        if (atn) { /* Read identify message from FIFO */
            scsi_phase = STAT_MO;
            seqstep = 1;
            identify_msg = esp_fifo[fifo_read_ptr];
            fifo_read_ptr++;
        }
        
        /* Read command from FIFO */
        scsi_phase = STAT_CD;
        seqstep = 3;
        for (cmd_size = 0; cmd_size < SCSI_CDB_MAX_SIZE && fifo_read_ptr<fifo_write_ptr; cmd_size++) {
            commandbuf[cmd_size] = esp_fifo[fifo_read_ptr];
            fifo_read_ptr++;
        }

        Log_Printf(LOG_WARN, "[ESP] Select: Reading command from FIFO, size: %i byte",cmd_size);

        esp_flush_fifo();
    }
    
    Log_Printf(LOG_WARN, "[ESP] Select: Identify Message: $%02X",identify_msg);
    Log_Printf(LOG_WARN, "[ESP] Select: Target: %i, Lun: %i",target,identify_msg&0x07);

    SCSIdisk_Receive_Command(commandbuf, identify_msg);
    seqstep = 4;

    status = (status&STAT_MASK)|scsi_phase;
    intstatus = INTR_BS | INTR_FC;
    seqstep = SEQ_CD;
    
    esp_state = INITIATOR;
    esp_raise_irq();
}


/* DMA done: this is called as part of transfer info or transfer pad
 * after DMA transfer has completed. */
void esp_dma_done(bool write) {
    Log_Printf(LOG_WARN, "ESP DMA transfer done: ESP counter = %i, SCSI residual bytes: %i",
               esp_counter,SCSIdata.size-SCSIdata.rpos);
    
    status = (status&STAT_MASK)|scsi_phase;

    if (esp_counter == 0) { /* Transfer done */
        intstatus = INTR_FC;
        status |= STAT_TC;
        esp_raise_irq();
    } else if ((write && scsi_phase!=STAT_DI) || (!write && scsi_phase!=STAT_DO)) { /* Phase change detected */
        intstatus = INTR_BS;
        esp_raise_irq();
    } /* else continue transfering data using DMA, no interrupt */
}


/* Transfer information */
void esp_transfer_info(void) {
    if(mode_dma) {
        status &= ~STAT_TC;
        
        switch (scsi_phase) {
            case STAT_DI:
                Log_Printf(LOG_WARN, "ESP start DMA transfer from device to memory: ESP counter = %i\n", esp_counter);
                dma_esp_write_memory();
                break;
            case STAT_DO:
                Log_Printf(LOG_WARN, "ESP start DMA transfer from memory to device: ESP counter = %i\n", esp_counter);
                dma_esp_read_memory();
                break;
            default:
                Log_Printf(LOG_WARN, "ESP transfer info: illegal phase");
                abort();
                break;
        }
        /* Function continues after DMA transfer (esp_dma_done) */

    } else {
        Log_Printf(LOG_WARN, "ESP start PIO transfer (not implemented!)");
        abort();
    }
}

/* Transfer padding */
void esp_transfer_pad(void) {
    Log_Printf(LOG_WARN, "[ESP] Transfer padding, ESP counter: %i bytes, SCSI resid: %i bytes\n",
               esp_counter, SCSIdata.size-SCSIdata.rpos);
    
    switch (scsi_phase) {
        case STAT_DI:
            while (scsi_phase==STAT_DI && esp_counter>0) {
                SCSIdisk_Send_Data();
                esp_counter--;
            }
            esp_dma_done(true);
            break;
        case STAT_DO:
            while (scsi_phase==STAT_DO && esp_counter>0) {
                SCSIdisk_Receive_Data();
                esp_counter--;
            }
            esp_dma_done(false);
            break;
            
        default:
            abort();
            break;
    }
}


/* Initiator command complete */
void esp_initiator_command_complete(void) {
    
    if(mode_dma == 1) {
        Log_Printf(LOG_WARN, "ESP initiator command complete via DMA not implemented!");
        abort();
        esp_fifo[0] = SCSIdisk_Send_Status(); // status
        esp_fifo[1] = SCSIdisk_Send_Message(); // message
        dma_memory_write(esp_fifo, 2, CHANNEL_SCSI);
        status = (status & STAT_MASK) | STAT_TC | STAT_ST;
        intstatus = INTR_BS | INTR_FC;
        seqstep = SEQ_CD;
    } else {
        /* Receive status byte */
        esp_fifo[fifo_write_ptr] = SCSIdisk_Send_Status();
        fifo_write_ptr++;
        fifoflags = (fifoflags & 0xE0) | fifo_write_ptr;
        scsi_phase = STAT_MI;

        /* Receive message byte */
        esp_fifo[fifo_write_ptr] = SCSIdisk_Send_Message(); /* 0x00 = command complete */
        fifo_write_ptr++;
        fifoflags = (fifoflags & 0xE0) | fifo_write_ptr;
    }

    intstatus = INTR_FC;
    status = (status&STAT_MASK)|scsi_phase;
    esp_raise_irq();
}


/* Message accepted */
void esp_message_accepted(void) {
    scsi_phase = STAT_ST; /* set at the end of iccs? */
    status = (status&STAT_MASK)|scsi_phase;
    intstatus = INTR_BS;
    esp_raise_irq();
}



#if 0 /* this is for target commands! */
/* Decode command to determine the command group and thus the
 * length of the incoming command. Set "valid group code" bit
 * in status register if the group is 0, 1, 5, 6, or 7 (group
 * 2 is also valid on NCR53C90A).
 */
Uint8 scsi_command_group = (commandbuf[0] & 0xE0) >> 5;
if(scsi_command_group < 3 || scsi_command_group > 4) {
    if(ConfigureParams.System.nSCSI == NCR53C90 && scsi_command_group == 2) {
        Log_Printf(LOG_WARN, "[ESP] Select: Invalid command group %i on NCR53C90\n", scsi_command_group);
        status &= ~STAT_VGC;
    } else {
        status |= STAT_VGC;
    }
} else {
    Log_Printf(LOG_WARN, "[ESP] Select: Invalid command group %i on NCR53C90A\n", scsi_command_group);
    status &= ~STAT_VGC;
}
#endif