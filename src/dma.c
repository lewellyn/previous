/* NeXT DMA Emulation 
 * Contains informations from QEMU-NeXT
 */

#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "esp.h"
#include "sysReg.h"
#include "dma.h"
#include "configuration.h"


#define LOG_DMA_LEVEL LOG_DEBUG

#define IO_SEG_MASK	0x1FFFF

/* read CSR bits */
#define DMA_ENABLE      0x01000000 /* enable dma transfer */
#define DMA_SUPDATE     0x02000000 /* single update */
#define DMA_COMPLETE    0x08000000 /* current dma has completed */
#define DMA_BUSEXC      0x10000000 /* bus exception occurred */
/* write CSR bits */
#define DMA_SETENABLE   0x00010000 /* set enable */
#define DMA_SETSUPDATE  0x00020000 /* set single update */
#define DMA_M2DEV       0x00000000 /* dma from mem to dev */
#define DMA_DEV2M       0x00040000 /* dma from dev to mem */
#define DMA_CLRCOMPLETE 0x00080000 /* clear complete conditional */
#define DMA_RESET       0x00100000 /* clr cmplt, sup, enable */
#define DMA_INITBUF     0x00200000 /* initialize DMA buffers */


/* Read and write CSR bits for 68030 based NeXT Computer.
 * We convert these to 68040 values before using in functions.
 * read CSR bits *
 #define DMA_ENABLE      0x01
 #define DMA_SUPDATE     0x02
 #define DMA_COMPLETE    0x08
 #define DMA_BUSEXC      0x10
 * write CSR bits *
 #define DMA_SETENABLE   0x01
 #define DMA_SETSUPDATE  0x02
 #define DMA_M2DEV       0x00
 #define DMA_DEV2M       0x04
 #define DMA_CLRCOMPLETE 0x08
 #define DMA_RESET       0x10
 #define DMA_INITBUF     0x20
 */



/* DMA registers */

typedef struct {
    Uint32 read_csr;
    Uint32 write_csr;
    Uint32 saved_next;
    Uint32 saved_limit;
    Uint32 saved_start;
    Uint32 saved_stop;
    Uint32 next;
    Uint32 limit;
    Uint32 start;
    Uint32 stop;
    Uint32 init;
    Uint32 size;
} DMA_CONTROL;

DMA_CONTROL dma[16];



int get_channel(Uint32 address) {
    int channel = address&IO_SEG_MASK;
    switch (channel) {
        case 0x010: return CHANNEL_SCSI; break;
        case 0x040: return CHANNEL_SOUNDOUT; break;
        case 0x050: return CHANNEL_DISK; break;
        case 0x080: return CHANNEL_SOUNDIN; break;
        case 0x090: return CHANNEL_PRINTER; break;
        case 0x0c0: return CHANNEL_SCC; break;
        case 0x0d0: return CHANNEL_DSP; break;
        case 0x110: return CHANNEL_ENETX; break;
        case 0x150: return CHANNEL_ENETR; break;
        case 0x180: return CHANNEL_VIDEO; break;
        case 0x1d0: return CHANNEL_M2R; break;
        case 0x1c0: return CHANNEL_R2M; break;
            
        default:
            Log_Printf(LOG_DMA_LEVEL, "Unknown DMA channel!\n");
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
        case CHANNEL_ENETX: return INT_ENETX_DMA; break;
        case CHANNEL_ENETR: return INT_ENETR_DMA; break;
        case CHANNEL_VIDEO: return 0; break;                   // no interrupt? CHECK THIS
        case CHANNEL_M2R: return INT_M2R_DMA; break;
        case CHANNEL_R2M: return INT_R2M_DMA; break;
                        
        default:
            Log_Printf(LOG_DMA_LEVEL, "Unknown DMA interrupt!\n");
            return 0;
            break;
    }
}

void DMA_CSR_Read(void) { // 0x02000010, length of register is byte on 68030 based NeXT Computer
    int channel = get_channel(IoAccessCurrentAddress);
    if(ConfigureParams.System.nMachineType == NEXT_CUBE030) { // for 68030 based NeXT Computer
        IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = dma[channel].read_csr >> 24;
        Log_Printf(LOG_DMA_LEVEL,"DMA SCSI CSR read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].read_csr >> 24, m68k_getpc());
    } else {
        IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma[channel].read_csr);
        Log_Printf(LOG_DMA_LEVEL,"DMA SCSI CSR read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].read_csr, m68k_getpc());
    }
}

void DMA_CSR_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress);
    int interrupt = get_interrupt_type(channel);
    if(ConfigureParams.System.nMachineType == NEXT_CUBE030) { // for 68030 based NeXT Computer
        dma[channel].write_csr = IoMem[IoAccessCurrentAddress & IO_SEG_MASK] << 16;
        Log_Printf(LOG_DMA_LEVEL,"DMA SCSI CSR write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress,dma[channel].write_csr >> 16, m68k_getpc());
    } else {
        dma[channel].write_csr = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
        Log_Printf(LOG_DMA_LEVEL,"DMA SCSI CSR write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].write_csr, m68k_getpc());
    }
     
    if(dma[channel].write_csr & DMA_DEV2M) {
        if(ConfigureParams.System.nMachineType == NEXT_CUBE030) {
            dma[channel].read_csr |= (0x04 << 24); // use 8 bit DMA_DEV2M value for 68030 based NeXT Computer
        } else {
            dma[channel].read_csr |= DMA_DEV2M;
        }
        Log_Printf(LOG_DMA_LEVEL,"DMA from dev to mem");
    }
    if(dma[channel].write_csr & DMA_SETENABLE) {
        dma[channel].read_csr |= DMA_ENABLE;
        Log_Printf(LOG_DMA_LEVEL,"DMA enable transfer");
    }
    if(dma[channel].write_csr & DMA_SETSUPDATE) {
        dma[channel].read_csr |= DMA_SUPDATE;
        Log_Printf(LOG_DMA_LEVEL,"DMA set single update");
    }
    if(dma[channel].write_csr & DMA_CLRCOMPLETE) {
        dma[channel].read_csr &= ~DMA_COMPLETE;
        Log_Printf(LOG_DMA_LEVEL,"DMA clear complete conditional");

	set_interrupt(interrupt, RELEASE_INT); // also somewhat experimental...
    }
    if(dma[channel].write_csr & DMA_RESET) {
        dma[channel].read_csr &= ~(DMA_COMPLETE | DMA_SUPDATE | DMA_ENABLE | DMA_DEV2M);
        Log_Printf(LOG_WARN,"DMA reset");
        
	set_interrupt(interrupt, RELEASE_INT); // also somewhat experimental...
    }
    if(dma[channel].write_csr & DMA_INITBUF) { // needs to be filled
        Log_Printf(LOG_DMA_LEVEL,"DMA initialize buffers");
    }
}

void DMA_Saved_Next_Read(void) { // 0x02004000
    int channel = get_channel(IoAccessCurrentAddress-0x3FF0);
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma[channel].saved_next);
 	Log_Printf(LOG_DMA_LEVEL,"DMA SCSI SNext read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].saved_next, m68k_getpc());
}

void DMA_Saved_Next_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress-0x3FF0);
    dma[channel].saved_next = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA SCSI SNext write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].saved_next, m68k_getpc());
}

void DMA_Saved_Limit_Read(void) { // 0x02004004
    int channel = get_channel(IoAccessCurrentAddress-0x3FF4);
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma[channel].saved_limit);
 	Log_Printf(LOG_DMA_LEVEL,"DMA SCSI SLimit read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].saved_limit, m68k_getpc());
}

void DMA_Saved_Limit_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress-0x3FF4);
    dma[channel].saved_limit = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA SCSI SLimit write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].saved_limit, m68k_getpc());
}

void DMA_Saved_Start_Read(void) { // 0x02004008
    int channel = get_channel(IoAccessCurrentAddress-0x3FF8);
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma[channel].saved_start);
 	Log_Printf(LOG_DMA_LEVEL,"DMA SCSI SStart read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].saved_start, m68k_getpc());
}

void DMA_Saved_Start_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress-0x3FF8);
    dma[channel].saved_start = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA SCSI SStart write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].saved_start, m68k_getpc());
}

void DMA_Saved_Stop_Read(void) { // 0x0200400c
    int channel = get_channel(IoAccessCurrentAddress-0x3FFC);
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma[channel].saved_stop);
 	Log_Printf(LOG_DMA_LEVEL,"DMA SCSI SStop read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].saved_stop, m68k_getpc());
}

void DMA_Saved_Stop_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress-0x3FFC);
    dma[channel].saved_stop = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA SCSI SStop write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].saved_stop, m68k_getpc());
}

void DMA_Next_Read(void) { // 0x02004010
    int channel = get_channel(IoAccessCurrentAddress-0x4000);
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma[channel].next);
 	Log_Printf(LOG_DMA_LEVEL,"DMA SCSI Next read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].next, m68k_getpc());
}

void DMA_Next_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress-0x4000);
    dma[channel].next = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA SCSI Next write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].next, m68k_getpc());
}

void DMA_Limit_Read(void) { // 0x02004014
    int channel = get_channel(IoAccessCurrentAddress-0x4004);
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma[channel].limit);
 	Log_Printf(LOG_DMA_LEVEL,"DMA SCSI Limit read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].limit, m68k_getpc());
}

void DMA_Limit_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress-0x4004);
    dma[channel].limit = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA SCSI Limit write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].limit, m68k_getpc());
}

void DMA_Start_Read(void) { // 0x02004018
    int channel = get_channel(IoAccessCurrentAddress-0x4008);
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma[channel].start);
 	Log_Printf(LOG_DMA_LEVEL,"DMA SCSI Start read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].start, m68k_getpc());
}

void DMA_Start_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress-0x4008);
    dma[channel].start = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA SCSI Start write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].start, m68k_getpc());
}

void DMA_Stop_Read(void) { // 0x0200401c
    int channel = get_channel(IoAccessCurrentAddress-0x400C);
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma[channel].stop);
 	Log_Printf(LOG_DMA_LEVEL,"DMA SCSI Stop read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].stop, m68k_getpc());
}

void DMA_Stop_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress-0x400C);
    dma[channel].stop = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA SCSI Stop write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].stop, m68k_getpc());
}

void DMA_Init_Read(void) { // 0x02004210
    int channel = get_channel(IoAccessCurrentAddress-0x4200);
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma[channel].init);
 	Log_Printf(LOG_DMA_LEVEL,"DMA SCSI Init read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].init, m68k_getpc());
}

void DMA_Init_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress-0x4200);
    dma[channel].init = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA SCSI Init write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].init, m68k_getpc());
}

void DMA_Size_Read(void) { // 0x02004214
    int channel = get_channel(IoAccessCurrentAddress-0x4204);
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma[channel].size);
 	Log_Printf(LOG_DMA_LEVEL,"DMA SCSI Size read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].size, m68k_getpc());
}

void DMA_Size_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress-0x4204);
    dma[channel].size = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA SCSI Size write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].size, m68k_getpc());
}



/* DMA Functions */

/*void copy_to_scsidma_buffer(Uint8 device_outbuf[], int outbuf_size) {
    memcpy(dma_buffer, device_outbuf, outbuf_size);
}*/

/*void dma_clear_memory(Uint32 datalength) {
    Uint32 start_addr;
    Uint32 end_addr;
    
    if(dma_init == 0)
        start_addr = dma_next;
    else
        start_addr = dma_init;
    
    end_addr = start_addr + datalength;
    
    NEXTMemory_Clear(start_addr, end_addr);
}*/

void dma_memory_read(Uint32 datalength) {
    Uint32 base_addr;
    Uint32 lencount;
    
/*    if(dma_init == 0)
        base_addr = dma_next;
    else
        base_addr = dma_init;
    
    for (lencount = 0; lencount < datalength; lencount++) {
        dma_read_buffer[lencount] = NEXTMemory_ReadByte(base_addr + lencount);
    }*/
}


void dma_memory_write(Uint8 *buf, int size, int channel) {
    Uint32 base_addr;
    Uint8 align = 16;
    int size_count = 0;
    Uint32 write_addr;
    int interrupt = get_interrupt_type(channel);
    
    if(channel == CHANNEL_ENETR || channel == CHANNEL_ENETX)
        align = 32;
        
    if((size % align) != 0) {
        size -= size % align;
        size += align;
    }

    
    if(dma[channel].init == 0)
        base_addr = dma[channel].next;
    else
        base_addr = dma[channel].init;

    Log_Printf(LOG_WARN, "[DMA] Write to mem: at $%08x, $%x bytes",base_addr,size);
    for (size_count = 0; size_count < size; size_count++) {
        write_addr = base_addr + size_count;
        NEXTMemory_WriteByte(write_addr, buf[size_count]);
    }
    
    /* Test read/write */
//    Log_Printf(LOG_DMA_LEVEL, "DMA Write Test: $%02x,$%02x,$%02x,$%02x\n", NEXTMemory_ReadByte(base_addr),NEXTMemory_ReadByte(base_addr+16),NEXTMemory_ReadByte(base_addr+32),NEXTMemory_ReadByte(base_addr+384));
//    NEXTMemory_WriteByte(base_addr, 0x77);
//    Uint8 testvar = NEXTMemory_ReadByte(base_addr);
//    Log_Printf(LOG_DMA_LEVEL, "Write Test: $%02x at $%08x", testvar, base_addr);
    
    dma[channel].init = 0;
    
    /* saved limit is checked to calculate packet size
     by both the rom and netbsd */ 
    dma[channel].saved_limit = dma[channel].next + size;
    dma[channel].saved_next  = dma[channel].next;
    
    if(!(dma[channel].read_csr & DMA_SUPDATE)) {
        dma[channel].next = dma[channel].start;
        dma[channel].limit = dma[channel].stop;
    }

    dma[channel].read_csr |= DMA_COMPLETE;
    
    set_interrupt(interrupt, SET_INT);
//    set_interrupt(INT_SCSI_DMA, RELEASE_INT);
}
