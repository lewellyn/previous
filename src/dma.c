/* NeXT DMA Emulation 
 * Contains informations from QEMU-NeXT
 * NeXT DMA consists of 12 channel processors with 128 bytes internal buffer for each channel
 * 12 channels: SCSI, Sound in, Sound out, Optical disk, Printer, SCC, DSP,
 * Ethernet transmit, Ethernet receive, Video, Memory to register, Register to memory
 */

#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "scsi.h"
#include "esp.h"
#include "mo.h"
#include "sysReg.h"
#include "dma.h"
#include "configuration.h"
#include "ethernet.h"
#include "mmu_common.h"



#define LOG_DMA_LEVEL LOG_WARN

#define IO_SEG_MASK	0x1FFFF

/* DMA internal buffer size */
#define DMA_BURST_SIZE  16
int act_buf_size = 0;
int modma_buf_size = 0;


/* Read and write CSR bits for 68030 based NeXT Computer. */

/* read CSR bits */
#define DMA_ENABLE      0x01   /* enable dma transfer */
#define DMA_SUPDATE     0x02   /* single update */
#define DMA_COMPLETE    0x08   /* current dma has completed */
#define DMA_BUSEXC      0x10   /* bus exception occurred */
/* write CSR bits */
#define DMA_SETENABLE   0x01   /* set enable */
#define DMA_SETSUPDATE  0x02   /* set single update */
#define DMA_M2DEV       0x00   /* dma from mem to dev */
#define DMA_DEV2M       0x04   /* dma from dev to mem */
#define DMA_CLRCOMPLETE 0x08   /* clear complete conditional */
#define DMA_RESET       0x10   /* clr cmplt, sup, enable */
#define DMA_INITBUF     0x20   /* initialize DMA buffers */

/* CSR masks */
#define DMA_CMD_MASK    (DMA_SETENABLE|DMA_SETSUPDATE|DMA_CLRCOMPLETE|DMA_RESET|DMA_INITBUF)
#define DMA_STAT_MASK   (DMA_ENABLE|DMA_SUPDATE|DMA_COMPLETE|DMA_BUSEXC)


/* Read and write CSR bits for 68040 based Machines.
 * We convert these to 68030 values before using in functions.
 * read CSR bits *
 #define DMA_ENABLE      0x01000000
 #define DMA_SUPDATE     0x02000000
 #define DMA_COMPLETE    0x08000000
 #define DMA_BUSEXC      0x10000000
 * write CSR bits *
 #define DMA_SETENABLE   0x00010000
 #define DMA_SETSUPDATE  0x00020000
 #define DMA_M2DEV       0x00000000
 #define DMA_DEV2M       0x00040000
 #define DMA_CLRCOMPLETE 0x00080000
 #define DMA_RESET       0x00100000
 #define DMA_INITBUF     0x00200000
 */



static inline Uint32 dma_getlong(Uint8 *buf, Uint32 pos) {
	return (buf[pos] << 24) | (buf[pos+1] << 16) | (buf[pos+2] << 8) | buf[pos+3];
}

static inline void dma_putlong(Uint32 val, Uint8 *buf, Uint32 pos) {
	buf[pos] = val >> 24;
	buf[pos+1] = val >> 16;
	buf[pos+2] = val >> 8;
	buf[pos+3] = val;
}


int get_channel(Uint32 address) {
    int channel = address&IO_SEG_MASK;

    switch (channel) {
        case 0x010: Log_Printf(LOG_DMA_LEVEL,"channel SCSI:"); return CHANNEL_SCSI; break;
        case 0x040: Log_Printf(LOG_DMA_LEVEL,"channel Sound Out:"); return CHANNEL_SOUNDOUT; break;
        case 0x050: Log_Printf(LOG_DMA_LEVEL,"channel MO Disk:"); return CHANNEL_DISK; break;
        case 0x080: Log_Printf(LOG_DMA_LEVEL,"channel Sound in:"); return CHANNEL_SOUNDIN; break;
        case 0x090: Log_Printf(LOG_DMA_LEVEL,"channel Printer:"); return CHANNEL_PRINTER; break;
        case 0x0c0: Log_Printf(LOG_DMA_LEVEL,"channel SCC:"); return CHANNEL_SCC; break;
        case 0x0d0: Log_Printf(LOG_DMA_LEVEL,"channel DSP:"); return CHANNEL_DSP; break;
        case 0x110: Log_Printf(LOG_DMA_LEVEL,"channel Ethernet Tx:"); return CHANNEL_EN_TX; break;
        case 0x150: Log_Printf(LOG_DMA_LEVEL,"channel Ethernet Rx:"); return CHANNEL_EN_RX; break;
        case 0x180: Log_Printf(LOG_DMA_LEVEL,"channel Video:"); return CHANNEL_VIDEO; break;
        case 0x1d0: Log_Printf(LOG_DMA_LEVEL,"channel M2R:"); return CHANNEL_M2R; break;
        case 0x1c0: Log_Printf(LOG_DMA_LEVEL,"channel R2M:"); return CHANNEL_R2M; break;
            
        default:
            Log_Printf(LOG_WARN, "Unknown DMA channel!\n");
            return -1;
            break;
    }
}

int get_interrupt_type(int channel) {
    switch (channel) {
        case CHANNEL_SCSI: return INT_SCSI_DMA; break;
        case CHANNEL_SOUNDOUT: return INT_SND_OUT_DMA; break;
        case CHANNEL_DISK: return INT_DISK_DMA; break;
        case CHANNEL_SOUNDIN: return INT_SND_IN_DMA; break;
        case CHANNEL_PRINTER: return INT_PRINTER_DMA; break;
        case CHANNEL_SCC: return INT_SCC_DMA; break;
        case CHANNEL_DSP: return INT_DSP_DMA; break;
        case CHANNEL_EN_TX: return INT_EN_TX_DMA; break;
        case CHANNEL_EN_RX: return INT_EN_RX_DMA; break;
        case CHANNEL_VIDEO: return INT_VIDEO; break;
        case CHANNEL_M2R: return INT_M2R_DMA; break;
        case CHANNEL_R2M: return INT_R2M_DMA; break;
                        
        default:
            Log_Printf(LOG_WARN, "Unknown DMA interrupt!\n");
            return 0;
            break;
    }
}

void DMA_CSR_Read(void) { // 0x02000010, length of register is byte on 68030 based NeXT Computer
    int channel = get_channel(IoAccessCurrentAddress);
    
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = dma[channel].csr;
    IoMem[(IoAccessCurrentAddress+1) & IO_SEG_MASK] = IoMem[(IoAccessCurrentAddress+2) & IO_SEG_MASK] = IoMem[(IoAccessCurrentAddress+3) & IO_SEG_MASK] = 0x00; // just to be sure
    Log_Printf(LOG_DMA_LEVEL,"DMA CSR read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].csr, m68k_getpc());
}

void DMA_CSR_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress);
    int interrupt = get_interrupt_type(channel);
    Uint8 writecsr = IoMem[IoAccessCurrentAddress & IO_SEG_MASK]|IoMem[(IoAccessCurrentAddress+1) & IO_SEG_MASK]|IoMem[(IoAccessCurrentAddress+2) & IO_SEG_MASK]|IoMem[(IoAccessCurrentAddress+3) & IO_SEG_MASK];

    Log_Printf(LOG_DMA_LEVEL,"DMA CSR write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, writecsr, m68k_getpc());
    
    /* For debugging */
    if(writecsr&DMA_DEV2M)
        Log_Printf(LOG_DMA_LEVEL,"DMA from dev to mem");
    else
        Log_Printf(LOG_DMA_LEVEL,"DMA from mem to dev");
    
    switch (writecsr&DMA_CMD_MASK) {
        case DMA_RESET:
            Log_Printf(LOG_DMA_LEVEL,"DMA reset"); break;
        case DMA_INITBUF:
            Log_Printf(LOG_DMA_LEVEL,"DMA initialize buffers"); break;
        case (DMA_RESET | DMA_INITBUF):
            Log_Printf(LOG_DMA_LEVEL,"DMA reset and initialize buffers"); break;
        case DMA_CLRCOMPLETE:
            Log_Printf(LOG_DMA_LEVEL,"DMA end chaining"); break;
        case (DMA_SETSUPDATE | DMA_CLRCOMPLETE):
            Log_Printf(LOG_DMA_LEVEL,"DMA continue chaining"); break;
        case DMA_SETENABLE:
            Log_Printf(LOG_DMA_LEVEL,"DMA start single transfer"); break;
        case (DMA_SETENABLE | DMA_SETSUPDATE):
            Log_Printf(LOG_DMA_LEVEL,"DMA start chaining"); break;
        case 0:
            Log_Printf(LOG_DMA_LEVEL,"DMA no command"); break;
        default:
            Log_Printf(LOG_DMA_LEVEL,"DMA: unknown command!"); break;
    }

    /* Handle CSR bits */
    dma[channel].direction = writecsr&DMA_DEV2M;

    if (writecsr&DMA_RESET) {
        dma[channel].csr &= ~(DMA_COMPLETE | DMA_SUPDATE | DMA_ENABLE);
    }
    if (writecsr&DMA_INITBUF) {
        if (channel==CHANNEL_SCSI) {
            esp_dma.status = 0x00; /* just a guess */
            act_buf_size = 0;
        }
        if (channel==CHANNEL_DISK) {
            modma_buf_size = 0;
        }
    }
    if (writecsr&DMA_SETSUPDATE) {
        dma[channel].csr |= DMA_SUPDATE;
    }
    if (writecsr&DMA_SETENABLE) {
        dma[channel].csr |= DMA_ENABLE;
        switch (channel) {
            case CHANNEL_M2R:
            case CHANNEL_R2M:
                if (dma[channel].next==dma[channel].limit) {
                    dma[channel].csr&= ~DMA_ENABLE;
                }
                if ((dma[CHANNEL_M2R].csr&DMA_ENABLE)&&(dma[CHANNEL_R2M].csr&DMA_ENABLE)) {
                    /* Enable Memory to Memory DMA, if read and write channels are enabled */
                    dma_m2m_write_memory();
                }
                break;
                
            default: break;
        }
    }
    if (writecsr&DMA_CLRCOMPLETE) {
        dma[channel].csr &= ~DMA_COMPLETE;

        switch (channel) {
            case CHANNEL_SCSI:
                if (dma[channel].direction==DMA_DEV2M)
                    dma_esp_write_memory();
                else
                    dma_esp_read_memory();
                break;
#if 0
            case CHANNEL_DISK:
                if (dma[channel].direction==DMA_DEV2M)
                    dma_mo_write_memory();
                else
                    dma_mo_read_memory();
                break;
#endif
            default: break;
        }
    }

    set_interrupt(interrupt, RELEASE_INT); // experimental
}

void DMA_Saved_Next_Read(void) { // 0x02004000
    int channel = get_channel(IoAccessCurrentAddress-0x3FF0);
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma[channel].saved_next);
 	Log_Printf(LOG_DMA_LEVEL,"DMA SNext read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].saved_next, m68k_getpc());
}

void DMA_Saved_Next_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress-0x3FF0);
    dma[channel].saved_next = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA SNext write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].saved_next, m68k_getpc());
}

void DMA_Saved_Limit_Read(void) { // 0x02004004
    int channel = get_channel(IoAccessCurrentAddress-0x3FF4);
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma[channel].saved_limit);
 	Log_Printf(LOG_DMA_LEVEL,"DMA SLimit read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].saved_limit, m68k_getpc());
}

void DMA_Saved_Limit_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress-0x3FF4);
    dma[channel].saved_limit = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA SLimit write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].saved_limit, m68k_getpc());
}

void DMA_Saved_Start_Read(void) { // 0x02004008
    int channel = get_channel(IoAccessCurrentAddress-0x3FF8);
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma[channel].saved_start);
 	Log_Printf(LOG_DMA_LEVEL,"DMA SStart read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].saved_start, m68k_getpc());
}

void DMA_Saved_Start_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress-0x3FF8);
    dma[channel].saved_start = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA SStart write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].saved_start, m68k_getpc());
}

void DMA_Saved_Stop_Read(void) { // 0x0200400c
    int channel = get_channel(IoAccessCurrentAddress-0x3FFC);
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma[channel].saved_stop);
 	Log_Printf(LOG_DMA_LEVEL,"DMA SStop read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].saved_stop, m68k_getpc());
}

void DMA_Saved_Stop_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress-0x3FFC);
    dma[channel].saved_stop = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA SStop write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].saved_stop, m68k_getpc());
}

void DMA_Next_Read(void) { // 0x02004010
    int channel = get_channel(IoAccessCurrentAddress-0x4000);
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma[channel].next);
 	Log_Printf(LOG_DMA_LEVEL,"DMA Next read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].next, m68k_getpc());
}

void DMA_Next_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress-0x4000);
    dma[channel].next = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA Next write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].next, m68k_getpc());
}

void DMA_Limit_Read(void) { // 0x02004014
    int channel = get_channel(IoAccessCurrentAddress-0x4004);
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma[channel].limit);
 	Log_Printf(LOG_DMA_LEVEL,"DMA Limit read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].limit, m68k_getpc());
}

void DMA_Limit_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress-0x4004);
    dma[channel].limit = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA Limit write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].limit, m68k_getpc());
}

void DMA_Start_Read(void) { // 0x02004018
    int channel = get_channel(IoAccessCurrentAddress-0x4008);
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma[channel].start);
 	Log_Printf(LOG_DMA_LEVEL,"DMA Start read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].start, m68k_getpc());
}

void DMA_Start_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress-0x4008);
    dma[channel].start = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA Start write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].start, m68k_getpc());
}

void DMA_Stop_Read(void) { // 0x0200401c
    int channel = get_channel(IoAccessCurrentAddress-0x400C);
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma[channel].stop);
 	Log_Printf(LOG_DMA_LEVEL,"DMA Stop read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].stop, m68k_getpc());
}

void DMA_Stop_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress-0x400C);
    dma[channel].stop = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA Stop write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].stop, m68k_getpc());
}

void DMA_Init_Read(void) { // 0x02004210
    int channel = get_channel(IoAccessCurrentAddress-0x4200);
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma[channel].init);
 	Log_Printf(LOG_DMA_LEVEL,"DMA Init read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].init, m68k_getpc());
}

void DMA_Init_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress-0x4200);
    dma[channel].next = dma[channel].init = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK); /* hack */
    if (channel==CHANNEL_SCSI) {
        esp_dma.status = 0x00; /* just a guess */
        act_buf_size = 0;
    }
    if (channel==CHANNEL_DISK) {
        modma_buf_size = 0;
    }
    Log_Printf(LOG_DMA_LEVEL,"DMA Init write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].init, m68k_getpc());
}

void DMA_Size_Read(void) { // 0x02004214
    int channel = get_channel(IoAccessCurrentAddress-0x4204);
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma[channel].size);
 	Log_Printf(LOG_DMA_LEVEL,"DMA Size read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].size, m68k_getpc());
}

void DMA_Size_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress-0x4204);
    dma[channel].size = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA Size write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].size, m68k_getpc());
}


/* DMA interrupt functions */

void dma_interrupt(channel) {
    int interrupt = get_interrupt_type(channel);
    
    /* If we have reached limit, generate an interrupt and set the flags */
    if (dma[channel].next==dma[channel].limit) {
        
        dma[channel].csr |= DMA_COMPLETE;
        
        if(dma[channel].csr & DMA_SUPDATE) { /* if we are in chaining mode */
            dma[channel].next = dma[channel].start;
            dma[channel].limit = dma[channel].stop;
            /* Set bits in CSR */
            dma[channel].csr &= ~DMA_SUPDATE; /* 1st done */
        } else {
            dma[channel].csr &= ~DMA_ENABLE; /* all done */
        }
        set_interrupt(interrupt, SET_INT);
    } else if (dma[channel].csr&DMA_BUSEXC) {
        set_interrupt(interrupt, SET_INT);
    }
}

/* Functions for delayed interrupts */

/* Handler function for DMA ESP delayed interrupt */
void ESPDMA_InterruptHandler(void) {
    bool write = (dma[CHANNEL_SCSI].direction==DMA_DEV2M) ? true : false;
    
	CycInt_AcknowledgeInterrupt();
    dma_interrupt(CHANNEL_SCSI);
    
    /* Let ESP check if it needs to interrupt */
    esp_dma_done(write);
}

/* Handler function for DMA ESP delayed interrupt */
void MODMA_InterruptHandler(void) {
	CycInt_AcknowledgeInterrupt();
    dma_interrupt(CHANNEL_DISK);
}

/* Handler functions for DMA M2M delyed interrupts */
void M2RDMA_InterruptHandler(void) {
    CycInt_AcknowledgeInterrupt();
    dma_interrupt(CHANNEL_M2R);
}
void R2MDMA_InterruptHandler(void) {
    CycInt_AcknowledgeInterrupt();
    dma_interrupt(CHANNEL_R2M);
}


/* DMA Read and Write Memory Functions */

/* Channel SCSI */
#define DMAESP_DELAY 5000 /* Delay for interrupt in microseconds */

void dma_esp_write_memory(void) {
    Log_Printf(LOG_DMA_LEVEL, "[DMA] Channel SCSI: Write to memory at $%08x, %i bytes (ESP counter %i)",
               dma[CHANNEL_SCSI].next,dma[CHANNEL_SCSI].limit-dma[CHANNEL_SCSI].next,esp_counter);

    if (!(dma[CHANNEL_SCSI].csr&DMA_ENABLE)) {
        Log_Printf(LOG_WARN, "[DMA] Channel SCSI: Error! DMA not enabled!");
        return;
    }
    if ((dma[CHANNEL_SCSI].limit%DMA_BURST_SIZE) || (dma[CHANNEL_SCSI].next%4)) {
        Log_Printf(LOG_WARN, "[DMA] Channel SCSI: Error! Bad alignment! (Next: $%08X, Limit: $%08X)",
                   dma[CHANNEL_SCSI].next, dma[CHANNEL_SCSI].limit);
        abort();
    }

    /* TODO: Find out how we should handle non burst-size aligned start address. 
     * End address is always burst-size aligned. For now we use a hack. */
    
    TRY(prb) {
        /* This is a hack to handle non-burstsize-aligned DMA start */
        if (dma[CHANNEL_SCSI].next%DMA_BURST_SIZE) {
            Log_Printf(LOG_WARN, "[DMA] Channel SCSI: Start memory address is not 16 byte aligned ($%08X).",
                       dma[CHANNEL_SCSI].next);
            while ((dma[CHANNEL_SCSI].next+act_buf_size)%DMA_BURST_SIZE && esp_counter>0 && SCSIbus.phase==PHASE_DI) {
                esp_counter--;
                SCSIdisk_Send_Data();
                act_buf_size++;
            }
            while (act_buf_size>=4) {
                NEXTMemory_WriteLong(dma[CHANNEL_SCSI].next, dma_getlong(SCSIdata.buffer, SCSIdata.rpos-act_buf_size));
                dma[CHANNEL_SCSI].next+=4;
                act_buf_size-=4;
            }
        }

        while (dma[CHANNEL_SCSI].next<=dma[CHANNEL_SCSI].limit && act_buf_size==0) {
            /* Fill DMA internal buffer (no real buffer, we use an imaginary one) */
            while (act_buf_size<DMA_BURST_SIZE && esp_counter>0 && SCSIbus.phase==PHASE_DI) {
                esp_counter--;
                SCSIdisk_Send_Data();
                act_buf_size++;
            }
            ESP_DMA_set_status();
            
            //Log_Printf(LOG_DMA_LEVEL, "[DMA] Channel SCSI: Internal buffer size: %i bytes",act_buf_size);
            
            /* If buffer is full, burst write to memory */
            if (act_buf_size==DMA_BURST_SIZE && dma[CHANNEL_SCSI].next<dma[CHANNEL_SCSI].limit) {
                while (act_buf_size>0) {
                    NEXTMemory_WriteLong(dma[CHANNEL_SCSI].next, dma_getlong(SCSIdata.buffer, SCSIdata.rpos-act_buf_size));
                    dma[CHANNEL_SCSI].next+=4;
                    act_buf_size-=4;
                }
            } else { /* else do not write the bytes to memory but keep them inside the buffer */ 
                Log_Printf(LOG_DMA_LEVEL, "[DMA] Channel SCSI: Residual bytes in DMA buffer: %i bytes",act_buf_size);
                break;
            }
        }
    } CATCH(prb) {
        Log_Printf(LOG_WARN, "[DMA] Channel SCSI: Bus error while writing to %08x",dma[CHANNEL_SCSI].next);
        dma[CHANNEL_SCSI].csr &= ~DMA_ENABLE;
        dma[CHANNEL_SCSI].csr |= (DMA_COMPLETE|DMA_BUSEXC);
    } ENDTRY
    
#if DMAESP_DELAY > 0
    CycInt_AddRelativeInterrupt(DMAESP_DELAY*ConfigureParams.System.nCpuFreq, INT_CPU_CYCLE, INTERRUPT_ESPDMA);
#else
    dma_interrupt(CHANNEL_SCSI);
    
    /* Let ESP check if it needs to interrupt */
    esp_dma_done(true);
#endif
}

void dma_esp_flush_buffer(void) {

    TRY(prb) {
        if (dma[CHANNEL_SCSI].next<dma[CHANNEL_SCSI].limit) {
            Log_Printf(LOG_DMA_LEVEL, "[DMA] Channel SCSI: Flush buffer to memory at $%08x, 4 bytes",dma[CHANNEL_SCSI].next);

            /* Write one long word to memory */
            NEXTMemory_WriteLong(dma[CHANNEL_SCSI].next, dma_getlong(SCSIdata.buffer, SCSIdata.rpos-act_buf_size));

            dma[CHANNEL_SCSI].next += 4;
            /* TODO: Check if we should also change rpos if act_buf_size is exceeded 
             * to write correct data from SCSI buffer. */
            if (act_buf_size>4) {
                act_buf_size -= 4;
            } else {
                act_buf_size = 0;
            }
        }
    } CATCH(prb) {
        dma[CHANNEL_SCSI].csr &= ~DMA_ENABLE;
        dma[CHANNEL_SCSI].csr |= (DMA_COMPLETE|DMA_BUSEXC);
    } ENDTRY
}

void dma_esp_read_memory(void) {    
    Log_Printf(LOG_DMA_LEVEL, "[DMA] Channel SCSI: Read from memory at $%08x, %i bytes (ESP counter %i)",
               dma[CHANNEL_SCSI].next,dma[CHANNEL_SCSI].limit-dma[CHANNEL_SCSI].next,esp_counter);
    
    if (!(dma[CHANNEL_SCSI].csr&DMA_ENABLE)) {
        Log_Printf(LOG_WARN, "[DMA] Channel SCSI: Error! DMA not enabled!");
        return;
    }
    if ((dma[CHANNEL_SCSI].limit%DMA_BURST_SIZE) || (dma[CHANNEL_SCSI].next%4)) {
        Log_Printf(LOG_WARN, "[DMA] Channel SCSI: Error! Bad alignment! (Next: $%08X, Limit: $%08X)",
                   dma[CHANNEL_SCSI].next, dma[CHANNEL_SCSI].limit);
        abort();
    }
    
    /* TODO: Find out how we should handle non burst-size aligned start address.
     * End address should be always burst-size aligned. For now we use a hack. */
    
    TRY(prb) {
        /* This is a hack to handle non-burstsize-aligned DMA start */
        if (dma[CHANNEL_SCSI].next%DMA_BURST_SIZE) {
            Log_Printf(LOG_WARN, "[DMA] Channel SCSI: Start memory address is not 16 byte aligned ($%08X).",
                       dma[CHANNEL_SCSI].next);
            while (dma[CHANNEL_SCSI].next%DMA_BURST_SIZE) {
                dma_putlong(NEXTMemory_ReadLong(dma[CHANNEL_SCSI].next), SCSIdata.buffer, SCSIdata.rpos+act_buf_size);
                dma[CHANNEL_SCSI].next+=4;
                act_buf_size+=4;
            }
            while (act_buf_size>0 && esp_counter>0 && SCSIbus.phase==PHASE_DO) {
                esp_counter--;
                SCSIdisk_Receive_Data();
                act_buf_size--;
            }
        }

        while (dma[CHANNEL_SCSI].next<dma[CHANNEL_SCSI].limit && act_buf_size==0) {
            /* Read data from memory to internal DMA buffer (no real buffer, we use an imaginary one) */
            for (act_buf_size=0; act_buf_size<DMA_BURST_SIZE; act_buf_size+=4) {
                dma_putlong(NEXTMemory_ReadLong(dma[CHANNEL_SCSI].next+act_buf_size), SCSIdata.buffer, SCSIdata.rpos+act_buf_size);
            }
            dma[CHANNEL_SCSI].next+=DMA_BURST_SIZE;
            
            /* Empty DMA internal buffer */
            while (act_buf_size>0 && esp_counter>0 && SCSIbus.phase==PHASE_DO) {
                esp_counter--;
                SCSIdisk_Receive_Data();
                act_buf_size--;
            }

            ESP_DMA_set_status();
        }
    } CATCH(prb) {
        Log_Printf(LOG_WARN, "[DMA] Channel SCSI: Bus error while writing to %08x",dma[CHANNEL_SCSI].next+act_buf_size);
        dma[CHANNEL_SCSI].csr &= ~DMA_ENABLE;
        dma[CHANNEL_SCSI].csr |= (DMA_COMPLETE|DMA_BUSEXC);
    } ENDTRY
    
    if (act_buf_size!=0) {
        Log_Printf(LOG_DMA_LEVEL, "[DMA] Channel SCSI: Residual bytes in DMA buffer: %i bytes",act_buf_size);
    }
    if (SCSIbus.phase==PHASE_DO) {
        Log_Printf(LOG_WARN, "[DMA] Channel SCSI: Warning! Data not yet written to disk.");
        abort(); /* This should not happen */
    }
    
#if DMAESP_DELAY > 0
    CycInt_AddRelativeInterrupt(DMAESP_DELAY*ConfigureParams.System.nCpuFreq, INT_CPU_CYCLE, INTERRUPT_ESPDMA);
#else
    dma_interrupt(CHANNEL_SCSI);
    
    /* Let ESP check if it needs to interrupt */
    esp_dma_done(false);
#endif
}


/* Channel MO */
void dma_mo_write_memory(void) {
    Log_Printf(LOG_DMA_LEVEL, "[DMA] Channel MO: Write to memory at $%08x, %i bytes",
               dma[CHANNEL_DISK].next,dma[CHANNEL_DISK].limit-dma[CHANNEL_DISK].next);
    
    if (!(dma[CHANNEL_DISK].csr&DMA_ENABLE)) {
        Log_Printf(LOG_WARN, "[DMA] Channel MO: Error! DMA not enabled!");
        return;
    }
    if ((dma[CHANNEL_DISK].limit%DMA_BURST_SIZE) || (dma[CHANNEL_DISK].next%4)) {
        Log_Printf(LOG_WARN, "[DMA] Channel MO: Error! Bad alignment! (Next: $%08X, Limit: $%08X)",
                   dma[CHANNEL_DISK].next, dma[CHANNEL_DISK].limit);
    }
    /* TODO: Find out how we should handle non burst-size aligned start address.
     * End address is always burst-size aligned. For now we use a hack. */
    
    TRY(prb) {
        if (modma_buf_size>0) {
            Log_Printf(LOG_WARN, "[DMA] Channel MO: %i residual bytes in DMA buffer.", modma_buf_size);
            while (modma_buf_size>=4) {
                NEXTMemory_WriteLong(dma[CHANNEL_DISK].next, dma_getlong(ecc_buffer[ecc_act_buf].data, ecc_buffer[ecc_act_buf].limit-ecc_buffer[ecc_act_buf].size));
                dma[CHANNEL_DISK].next+=4;
                modma_buf_size-=4;
            }
        }
        
        /* This is a hack to handle non-burstsize-aligned DMA start */
        if (dma[CHANNEL_DISK].next%DMA_BURST_SIZE) {
            Log_Printf(LOG_WARN, "[DMA] Channel MO: Start memory address is not 16 byte aligned ($%08X).",
                       dma[CHANNEL_DISK].next);
            while ((dma[CHANNEL_DISK].next+modma_buf_size)%DMA_BURST_SIZE && ecc_buffer[ecc_act_buf].size>0) {
                ecc_buffer[ecc_act_buf].size--;
                modma_buf_size++;
            }
            while (modma_buf_size>=4) {
                NEXTMemory_WriteLong(dma[CHANNEL_DISK].next, dma_getlong(ecc_buffer[ecc_act_buf].data, ecc_buffer[ecc_act_buf].limit-ecc_buffer[ecc_act_buf].size-modma_buf_size));
                dma[CHANNEL_DISK].next+=4;
                modma_buf_size-=4;
            }
        }
        
        while (dma[CHANNEL_DISK].next<=dma[CHANNEL_DISK].limit && !(modma_buf_size%DMA_BURST_SIZE)) {
            /* Fill DMA internal buffer (no real buffer, we use an imaginary one) */
            while (modma_buf_size<DMA_BURST_SIZE && ecc_buffer[ecc_act_buf].size>0) {
                ecc_buffer[ecc_act_buf].size--;
                modma_buf_size++;
            }
                        
            /* If buffer is full, burst write to memory */
            if (modma_buf_size==DMA_BURST_SIZE && dma[CHANNEL_DISK].next<dma[CHANNEL_DISK].limit) {
                while (modma_buf_size>0) {
                    NEXTMemory_WriteLong(dma[CHANNEL_DISK].next, dma_getlong(ecc_buffer[ecc_act_buf].data, ecc_buffer[ecc_act_buf].limit-ecc_buffer[ecc_act_buf].size-modma_buf_size));
                    dma[CHANNEL_DISK].next+=4;
                    modma_buf_size-=4;
                }
            } else { /* else do not write the bytes to memory but keep them inside the buffer */
                Log_Printf(LOG_DMA_LEVEL, "[DMA] Channel MO: Residual bytes in DMA buffer: %i bytes",modma_buf_size);
                break;
            }
        }
    } CATCH(prb) {
        Log_Printf(LOG_WARN, "[DMA] Channel MO: Bus error while writing to %08x",dma[CHANNEL_DISK].next);
        dma[CHANNEL_DISK].csr &= ~DMA_ENABLE;
        dma[CHANNEL_DISK].csr |= (DMA_COMPLETE|DMA_BUSEXC);
    } ENDTRY
    
    dma_interrupt(CHANNEL_DISK);
}

void dma_mo_read_memory(void) {
    Log_Printf(LOG_DMA_LEVEL, "[DMA] Channel MO: Read from memory at $%08x, %i bytes",
               dma[CHANNEL_DISK].next,dma[CHANNEL_DISK].limit-dma[CHANNEL_DISK].next);
    
    if (!(dma[CHANNEL_DISK].csr&DMA_ENABLE)) {
        Log_Printf(LOG_WARN, "[DMA] Channel MO: Error! DMA not enabled!");
        return;
    }
    if ((dma[CHANNEL_DISK].limit%DMA_BURST_SIZE) || (dma[CHANNEL_DISK].next%4)) {
        Log_Printf(LOG_WARN, "[DMA] Channel MO: Error! Bad alignment! (Next: $%08X, Limit: $%08X)",
                   dma[CHANNEL_DISK].next, dma[CHANNEL_DISK].limit);
        abort();
    }
    
    /* TODO: Find out how we should handle non burst-size aligned start address.
     * End address should be always burst-size aligned. For now we use a hack. */
    
    TRY(prb) {
        if (modma_buf_size>0) {
            Log_Printf(LOG_WARN, "[DMA] Channel MO: %i residual bytes in DMA buffer.", modma_buf_size);
            while (modma_buf_size>=4) {
                dma_putlong(NEXTMemory_ReadLong(dma[CHANNEL_DISK].next-modma_buf_size), ecc_buffer[ecc_act_buf].data, ecc_buffer[ecc_act_buf].size);
                ecc_buffer[ecc_act_buf].size+=4;
                modma_buf_size-=4;
            }
        }

        /* This is a hack to handle non-burstsize-aligned DMA start */
        if (dma[CHANNEL_DISK].next%DMA_BURST_SIZE) {
            Log_Printf(LOG_WARN, "[DMA] Channel SCSI: Start memory address is not 16 byte aligned ($%08X).",
                       dma[CHANNEL_DISK].next);
            while (dma[CHANNEL_DISK].next%DMA_BURST_SIZE && ecc_buffer[ecc_act_buf].size<ecc_buffer[ecc_act_buf].limit) {
                dma_putlong(NEXTMemory_ReadLong(dma[CHANNEL_DISK].next), ecc_buffer[ecc_act_buf].data, ecc_buffer[ecc_act_buf].size+modma_buf_size);
                dma[CHANNEL_DISK].next+=4;
                modma_buf_size+=4;
            }
            while (modma_buf_size>0) {
                ecc_buffer[ecc_act_buf].size++;
                modma_buf_size--;
            }
        }

        while (dma[CHANNEL_DISK].next<dma[CHANNEL_DISK].limit && modma_buf_size==0) {
            /* Read data from memory to internal DMA buffer (no real buffer, we use an imaginary one) */
            while (modma_buf_size<DMA_BURST_SIZE) {
                dma_putlong(NEXTMemory_ReadLong(dma[CHANNEL_DISK].next), ecc_buffer[ecc_act_buf].data, ecc_buffer[ecc_act_buf].size+modma_buf_size);
                dma[CHANNEL_DISK].next+=4;
                modma_buf_size+=4;
            }
            /* Empty DMA internal buffer */
            while (modma_buf_size>0 && ecc_buffer[ecc_act_buf].size<ecc_buffer[ecc_act_buf].limit) {
                ecc_buffer[ecc_act_buf].size++;
                modma_buf_size--;
            }
        }
    } CATCH(prb) {
        Log_Printf(LOG_WARN, "[DMA] Channel MO: Bus error while writing to %08x",dma[CHANNEL_DISK].next);
        dma[CHANNEL_DISK].csr &= ~DMA_ENABLE;
        dma[CHANNEL_DISK].csr |= (DMA_COMPLETE|DMA_BUSEXC);
    } ENDTRY
    
    if (modma_buf_size!=0) {
        Log_Printf(LOG_DMA_LEVEL, "[DMA] Channel MO: Residual bytes in DMA buffer: %i bytes",modma_buf_size);
    }

    dma_interrupt(CHANNEL_DISK);
}



/* Memory to Memory */
#define DMA_M2M_CYCLES    1//((DMA_BURST_SIZE * 3) / 4)

void dma_m2m_write_memory(void) {
    int i;
    int time = 0;
    Uint32 m2m_buffer[DMA_BURST_SIZE/4];
    
    if (((dma[CHANNEL_R2M].limit-dma[CHANNEL_R2M].next)%DMA_BURST_SIZE) ||
        ((dma[CHANNEL_M2R].limit-dma[CHANNEL_M2R].next)%DMA_BURST_SIZE)) {
        Log_Printf(LOG_WARN, "[DMA] Channel M2M: Error! Memory not burst size aligned!");
    }
    
    Log_Printf(LOG_DMA_LEVEL, "[DMA] Channel M2M: Copying %i bytes from $%08X to $%08X.",
               dma[CHANNEL_R2M].limit-dma[CHANNEL_R2M].next,dma[CHANNEL_M2R].next,dma[CHANNEL_R2M].next);
    
    while (dma[CHANNEL_R2M].next<dma[CHANNEL_R2M].limit) {
        time+=DMA_M2M_CYCLES;

        if (dma[CHANNEL_M2R].next<dma[CHANNEL_M2R].limit) {
            TRY(prb) {
                /* (Re)fill the buffer, if there is still data to read */
                for (i=0; i<DMA_BURST_SIZE; i+=4) {
                    m2m_buffer[i/4]=NEXTMemory_ReadLong(dma[CHANNEL_M2R].next+i);
                }
                dma[CHANNEL_M2R].next+=DMA_BURST_SIZE;
            } CATCH(prb) {
                Log_Printf(LOG_WARN, "[DMA] Channel M2M: Bus error while reading from %08x",dma[CHANNEL_M2R].next+i);
                dma[CHANNEL_M2R].csr &= ~DMA_ENABLE;
                dma[CHANNEL_M2R].csr |= (DMA_COMPLETE|DMA_BUSEXC);
            } ENDTRY
            
            if ((dma[CHANNEL_M2R].next==dma[CHANNEL_M2R].limit)||(dma[CHANNEL_M2R].csr&DMA_BUSEXC)) {
                CycInt_AddRelativeInterrupt(time/4, INT_CPU_CYCLE, INTERRUPT_M2R);
            }
        }
        
        TRY(prb) {
            /* Write the contents of the buffer to memory */
            for (i=0; i<DMA_BURST_SIZE; i+=4) {
                NEXTMemory_WriteLong(dma[CHANNEL_R2M].next+i, m2m_buffer[i/4]);
            }
            dma[CHANNEL_R2M].next+=DMA_BURST_SIZE;
        } CATCH(prb) {
            Log_Printf(LOG_WARN, "[DMA] Channel M2M: Bus error while writing to %08x",dma[CHANNEL_R2M].next+i);
            dma[CHANNEL_R2M].csr &= ~DMA_ENABLE;
            dma[CHANNEL_R2M].csr |= (DMA_COMPLETE|DMA_BUSEXC);
        } ENDTRY
    }
    CycInt_AddRelativeInterrupt(time/4, INT_CPU_CYCLE, INTERRUPT_R2M);
}



/* ---------------------- DMA Scratchpad ---------------------- */

/* This is used to interrupt at vertical screen retrace.
 * TODO: find out how the interrupt is generated in real
 * hardware using the Limit register of the DMA chip.
 * (0xEA * 1024 = visible videomem size)
 */


/* Interrupt Handler (called from Video_InterruptHandler_VBL in video.c) */
void Video_InterruptHandler(void) {
    if (dma[CHANNEL_VIDEO].limit==0xEA) {
        set_interrupt(INT_VIDEO, SET_INT); /* interrupt is released by writing to CSR */
    } else if (dma[CHANNEL_VIDEO].limit && dma[CHANNEL_VIDEO].limit!=0xEA) {
        abort();
    }
}