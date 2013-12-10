/*  Previous - mo.c
 
 This file is distributed under the GNU Public License, version 2 or at
 your option any later version. Read the file gpl.txt for details.
 
 Canon Magneto-Optical Disk Drive and NeXT Optical Storage Processor emulation.
  
 NeXT Optical Storage Processor uses Reed-Solomon algorithm for error correction.
 It has 2 128 byte internal buffers and uses double-buffering to perform error correction.
  
 */

#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "configuration.h"
#include "mo.h"
#include "sysReg.h"
#include "dma.h"
#include "file.h"


#define LOG_MO_REG_LEVEL    LOG_DEBUG
#define LOG_MO_CMD_LEVEL    LOG_WARN

#define IO_SEG_MASK	0x1FFFF


/* Registers */

struct {
    Uint8 tracknuml;
    Uint8 tracknumh;
    Uint8 sector_num;
    Uint8 sector_count;
    Uint8 intstatus;
    Uint8 intmask;
    Uint8 ctrlr_csr2;
    Uint8 ctrlr_csr1;
    Uint8 csrl;
    Uint8 csrh;
    Uint8 err_stat;
    Uint8 ecc_cnt;
    Uint8 init;
    Uint8 format;
    Uint8 mark;
    Uint8 flag[7];
} mo;

struct {
    Uint16 status;
    Uint16 dstat;
    Uint16 estat;
    Uint16 hstat;
    
    Uint8 head;
    
    Uint32 head_pos;
    Uint32 ho_head_pos;
    Uint32 sec_offset;
    
    FILE* dsk;
    
    bool spinning;
    bool spiraling;
    
    bool protected;
    bool inserted;
    bool connected;
} modrv[2];

int dnum;


#define NO_HEAD     0
#define READ_HEAD   1
#define WRITE_HEAD  2
#define ERASE_HEAD  3
#define VERIFY_HEAD 4
#define RF_HEAD     5


/* Sector increment and number */
#define MOSEC_NUM_MASK      0x0F /* rw */
#define MOSEC_INCR_MASK     0xF0 /* wo */

/* Interrupt status */
#define MOINT_CMD_COMPL     0x01 /* ro */
#define MOINT_ATTN          0x02 /* ro */
#define MOINT_OPER_COMPL    0x04 /* rw */
#define MOINT_ECC_DONE      0x08 /* rw */
#define MOINT_TIMEOUT       0x10 /* rw */
#define MOINT_READ_FAULT    0x20 /* rw */
#define MOINT_PARITY_ERR    0x40 /* rw */
#define MOINT_DATA_ERR      0x80 /* rw */
#define MOINT_RESET         0x01 /* wo */
#define MOINT_GPO           0x02 /* wo */

/* Controller CSR 2 */
#define MOCSR2_DRIVE_SEL    0x01
#define MOCSR2_ECC_CMP      0x02
#define MOCSR2_BUF_TOGGLE   0x04
#define MOCSR2_CLR_BUFP     0x08
#define MOCSR2_ECC_BLOCKS   0x10
#define MOCSR2_ECC_MODE     0x20
#define MOCSR2_ECC_DIS      0x40
#define MOCSR2_SECT_TIMER   0x80

/* Controller CSR 1 */
/* see below (formatter commands) */

/* Drive CSR (lo and hi) */
/* see below (drive commands) */

/* Data error status */
#define ERRSTAT_ECC         0x01
#define ERRSTAT_CMP         0x02
#define ERRSTAT_TIMING      0x04
#define ERRSTAT_STARVE      0x08

/* Init */
#define MOINIT_ID_MASK      0x03
#define MOINIT_EVEN_PAR     0x04
#define MOINIT_DMA_STV_ENA  0x08
#define MOINIT_25_MHZ       0x10
#define MOINIT_ID_CMP_TRK   0x20
#define MOINIT_ECC_STV_DIS  0x40
#define MOINIT_SEC_GREATER  0x80

#define MOINIT_ID_34    0
#define MOINIT_ID_234   1
#define MOINIT_ID_1234  3
#define MOINIT_ID_0     2

/* Format */
#define MOFORM_RD_GATE_NOM  0x06
#define MOFORM_WR_GATE_NOM  0x30

#define MOFORM_RD_GATE_MIN  0x00
#define MOFORM_RD_GATE_MAX  0x0F
#define MOFORM_RD_GATE_MASK 0x0F


/* Disk layout */
#define MO_SEC_PER_TRACK    16
#define MO_TRACK_OFFSET     4096 /* offset to first logical sector of kernel driver is 4149 */
#define MO_TRACK_LIMIT      19819-(MO_TRACK_OFFSET) /* no more tracks beyond this offset */
#define MO_SECTORSIZE       1024

Uint32 get_logical_sector(Uint32 sector_id) {
    Sint32 tracknum = (sector_id&0xFFFF00)>>8;
    Uint8 sectornum = sector_id&0x0F;
#if 1
    tracknum-=MO_TRACK_OFFSET;
    if (tracknum<0 || tracknum>=MO_TRACK_LIMIT) {
        Log_Printf(LOG_WARN, "MO disk %i: Error! Bad sector (%i)! Disk limit exceeded.", dnum,
                   (tracknum*MO_SEC_PER_TRACK)+mo.sector_num);
        abort();
    }
#endif
    return (tracknum*MO_SEC_PER_TRACK)+sectornum;
}



/* Functions */
void mo_formatter_cmd(void);
void mo_drive_cmd(void);

void mo_eject_disk(void);
void mo_read_ecc(void);
void mo_write_ecc(void);

void mo_jump_head(Uint16 command);
void mo_read_id(void);

void mo_reset(void);
void mo_select(int drive);

void MO_Init(void);
void MO_Uninit(void);

/* Experimental */
#define SECTOR_IO_DELAY 5000
#define CMD_DELAY       2000

void fmt_read_sector(Uint32 sector_id);
void fmt_write_sector(Uint32 sector_id);
void fmt_erase_sector(Uint32 sector_id);
void fmt_verify_sector(Uint32 sector_id);

void mo_start_spinning(void);
void mo_stop_spinning(void);
void mo_start_spiraling(void);
void mo_stop_spiraling(void);
void mo_spiraling_operation(void);
void mo_reset_attn_status(void);
void mo_recalibrate(void);

int sector_increment = 0;
Uint8 delayed_intr = 0;
void mo_raise_irq(Uint8 interrupt, Uint32 delay);
bool no_disk(void) {
    if (!modrv[dnum].inserted) {
        return true;
    } else {
        return false;
    }
}

/* MO drive and controller registers */

void MO_TrackNumH_Read(void) { // 0x02012000
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.tracknumh;
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Track number hi read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_TrackNumH_Write(void) {
    mo.tracknumh=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Track number hi write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_TrackNumL_Read(void) { // 0x02012001
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.tracknuml;
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Track number lo read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_TrackNumL_Write(void) {
    mo.tracknuml=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Track number lo write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_SectorIncr_Read(void) { // 0x02012002
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.sector_num&MOSEC_NUM_MASK;
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Sector increment and number read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_SectorIncr_Write(void) {
    Uint8 val = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
    mo.sector_num = val&MOSEC_NUM_MASK;
    sector_increment = (val&MOSEC_INCR_MASK)>>4;
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Sector increment and number write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_SectorCnt_Read(void) { // 0x02012003
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.sector_count;
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Sector count read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_SectorCnt_Write(void) {
    mo.sector_count=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Sector count write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_IntStatus_Read(void) { // 0x02012004
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.intstatus;
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Interrupt status read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_IntStatus_Write(void) {
    Uint8 val = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
    mo.intstatus &= ~val;
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Interrupt status write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    /* TODO: check if correct */
    if (!(mo.intstatus&mo.intmask)||(mo.intstatus&mo.intmask)==MOINT_CMD_COMPL) {
        set_interrupt(INT_DISK, RELEASE_INT);
    }
    if (val&MOINT_RESET) {
        Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Hard reset\n");
        mo_reset();
    }
}

void MO_IntMask_Read(void) { // 0x02012005
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.intmask;
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Interrupt mask read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_IntMask_Write(void) {
    mo.intmask=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Interrupt mask write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MOctrl_CSR2_Read(void) { // 0x02012006
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.ctrlr_csr2;
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO Controller] CSR2 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MOctrl_CSR2_Write(void) {
    mo.ctrlr_csr2=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO Controller] CSR2 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    
    mo_select(mo.ctrlr_csr2&MOCSR2_DRIVE_SEL);
}

void MOctrl_CSR1_Read(void) { // 0x02012007
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.ctrlr_csr1;
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO Controller] CSR1 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MOctrl_CSR1_Write(void) {
    mo.ctrlr_csr1=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO Controller] CSR1 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    
    mo_formatter_cmd();
}

void MO_CSR_H_Read(void) { // 0x02012009
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.csrh;
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] CSR hi read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_CSR_H_Write(void) {
    mo.csrh=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] CSR hi write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_CSR_L_Read(void) { // 0x02012008
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.csrl;
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] CSR lo read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_CSR_L_Write(void) {
    mo.csrl=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] CSR lo write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    
    mo_drive_cmd();
}

void MO_ErrStat_Read(void) { // 0x0201200a
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.err_stat;
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Error status read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_EccCnt_Read(void) { // 0x0201200b
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.ecc_cnt;
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] ECC count read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Init_Write(void) { // 0x0201200c
    mo.init=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Init write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Format_Write(void) { // 0x0201200d
    mo.format=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Format write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Mark_Write(void) { // 0x0201200e
    mo.mark=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Mark write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Flag0_Read(void) { // 0x02012010
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.flag[0];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Flag 0 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Flag0_Write(void) {
    mo.flag[0]=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Flag 0 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Flag1_Read(void) { // 0x02012011
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.flag[1];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Flag 1 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Flag1_Write(void) {
    mo.flag[1]=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Flag 1 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Flag2_Read(void) { // 0x02012012
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.flag[2];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Flag 2 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Flag2_Write(void) {
    mo.flag[2]=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Flag 2 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Flag3_Read(void) { // 0x02012013
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.flag[3];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Flag 3 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Flag3_Write(void) {
    mo.flag[3]=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Flag 3 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Flag4_Read(void) { // 0x02012014
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.flag[4];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Flag 4 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Flag4_Write(void) {
    mo.flag[4]=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Flag 4 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Flag5_Read(void) { // 0x02012015
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.flag[5];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Flag 5 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Flag5_Write(void) {
    mo.flag[5]=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Flag 5 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Flag6_Read(void) { // 0x02012016
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.flag[6];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Flag 6 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Flag6_Write(void) {
    mo.flag[6]=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Flag 6 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

/* Register debugging */
void print_regs(void) {
    int i;
    Log_Printf(LOG_WARN,"sector ID:  %02X%02X%02X",mo.tracknumh,mo.tracknuml,mo.sector_num);
    Log_Printf(LOG_WARN,"head pos:   %04X",modrv[dnum].head_pos);
    Log_Printf(LOG_WARN,"sector cnt: %02X",mo.sector_count);
    Log_Printf(LOG_WARN,"intstatus:  %02X",mo.intstatus);
    Log_Printf(LOG_WARN,"intmask:    %02X",mo.intmask);
    Log_Printf(LOG_WARN,"ctrlr csr2: %02X",mo.ctrlr_csr2);
    Log_Printf(LOG_WARN,"ctrlr csr1: %02X",mo.ctrlr_csr1);
    Log_Printf(LOG_WARN,"drive csrl: %02X",mo.csrl);
    Log_Printf(LOG_WARN,"drive csrh: %02X",mo.csrh);
    Log_Printf(LOG_WARN,"errstat:    %02X",mo.err_stat);
    Log_Printf(LOG_WARN,"ecc count:  %02X",mo.ecc_cnt);
    Log_Printf(LOG_WARN,"init:       %02X",mo.init);
    Log_Printf(LOG_WARN,"format:     %02X",mo.format);
    Log_Printf(LOG_WARN,"mark:       %02X",mo.mark);
    for (i=0; i<7; i++) {
        Log_Printf(LOG_WARN,"flag %i:     %02X",i+1,mo.flag[i]);
    }
}

/* Drive selection (formatter command 2) */
/* FIXME: Selecting a drive connects its actual command complete
 * signal to the interrupt register. If there is no drive 
 * connected, the signal will always be low.
 */
void mo_select(int drive) {
    Log_Printf(LOG_MO_CMD_LEVEL, "[MO] Selecting drive %i",drive);
    dnum=drive;
    mo.intstatus &= ~MOINT_CMD_COMPL;
    if (modrv[dnum].connected) {
        mo_raise_irq(MOINT_CMD_COMPL, CMD_DELAY);
    } else {
        Log_Printf(LOG_MO_CMD_LEVEL, "[MO] Selection failed! Drive %i not connected.",drive);
    }
}

/* Formatter commands */

#define FMT_RESET       0x00
#define FMT_ECC_READ    0x80
#define FMT_ECC_WRITE   0x40
#define FMT_RD_STAT     0x20
#define FMT_ID_READ     0x10
#define FMT_VERIFY      0x08
#define FMT_ERASE       0x04
#define FMT_READ        0x02
#define FMT_WRITE       0x01

enum {
    FMT_MODE_READ,
    FMT_MODE_WRITE,
    FMT_MODE_ERASE,
    FMT_MODE_VERIFY,
    FMT_MODE_READ_ID,
    FMT_MODE_IDLE
} fmt_mode;

void mo_formatter_cmd(void) { /* TODO: commands can be combined! (read|eccread)*/
    
    switch (mo.ctrlr_csr1) {
        case FMT_RESET:
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: Reset (%02X)\n", mo.ctrlr_csr1);
            fmt_mode = FMT_MODE_IDLE;
            break;
        case FMT_ECC_READ:
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: ECC Read (%02X)\n", mo.ctrlr_csr1);
            mo_read_ecc();
            break;
        case FMT_ECC_WRITE:
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: ECC Write (%02X)\n", mo.ctrlr_csr1);
            mo_write_ecc();
            break;
        case FMT_RD_STAT:
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: Read Status (%02X)\n", mo.ctrlr_csr1);
            mo.csrh = (modrv[dnum].status>>8)&0xFF;
            mo.csrl = modrv[dnum].status&0xFF;
            break;
        case FMT_ID_READ:
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: ID Read (%02X)\n", mo.ctrlr_csr1);
            fmt_mode = FMT_MODE_READ_ID;
            break;
        case FMT_VERIFY:
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: Verify (%02X)\n", mo.ctrlr_csr1);
            fmt_mode = FMT_MODE_VERIFY;
            break;
        case FMT_ERASE:
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: Erase (%02X)\n", mo.ctrlr_csr1);
            fmt_mode = FMT_MODE_ERASE;
            break;
        case FMT_READ:
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: Read (%02X)\n", mo.ctrlr_csr1);
            fmt_mode = FMT_MODE_READ;
            break;
        case FMT_WRITE:
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: Write (%02X)\n", mo.ctrlr_csr1);
            fmt_mode = FMT_MODE_WRITE;
            break;
        /* Combined commands */
        case (FMT_READ|FMT_ECC_READ):
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: Read using ECC (%02X)\n", mo.ctrlr_csr1);
            fmt_mode = FMT_MODE_READ;
            mo_read_ecc();
            break;
        case (FMT_WRITE|FMT_ECC_WRITE):
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: Write using ECC (%02X)\n", mo.ctrlr_csr1);
            fmt_mode = FMT_MODE_WRITE;
            mo_write_ecc();
            break;
            
        default:
            Log_Printf(LOG_WARN,"[MO] Formatter command: Unknown command! (%02X)\n", mo.ctrlr_csr1);
            abort();
            break;
    }
}

void fmt_sector_done(void) {
    Uint16 track = (mo.tracknumh<<8)|mo.tracknuml;
    mo.sector_num+=sector_increment;
    track+=mo.sector_num/MO_SEC_PER_TRACK;
    mo.sector_num%=MO_SEC_PER_TRACK;
    mo.tracknumh = (track>>8)&0xFF;
    mo.tracknuml = track&0xFF;
    /* CHECK: decrement with sector_increment value? */
    if (mo.sector_count==0) {
        mo.sector_count=255;
    } else {
        mo.sector_count--;
    }
}

int sector_timer=0;
#define SECTOR_TIMEOUT_COUNT    100 /* FIXME: what is the correct value? */
bool fmt_match_id(Uint32 sector_id) {
    if ((mo.init&MOINIT_ID_MASK)==MOINIT_ID_0) {
        Log_Printf(LOG_MO_CMD_LEVEL, "MO disk %i: Sector ID matching disabled!",dnum);
        abort(); /* CHECK: this routine is critical to disk image corruption, check if it gives correct results */
        return true;
    }
    
    Uint32 fmt_id = (mo.tracknumh<<16)|(mo.tracknuml<<8)|mo.sector_num;
    
    if (mo.init&MOINIT_ID_CMP_TRK) {
        Log_Printf(LOG_MO_CMD_LEVEL, "MO disk %i: Compare only track.",dnum);
        fmt_id=(fmt_id>>8)&0xFFFF;
        sector_id=(sector_id>>8)&0xFFFF;
    }
    
    if (sector_id==fmt_id) {
        sector_timer=0;
        return true;
    } else {
        Log_Printf(LOG_MO_CMD_LEVEL, "MO disk %i: Sector ID mismatch (Sector ID=%06X, Looking for %06X)",
                   dnum,sector_id,fmt_id);
        if (mo.ctrlr_csr2&MOCSR2_SECT_TIMER) {
            sector_timer++;
            if (sector_timer>SECTOR_TIMEOUT_COUNT) {
                Log_Printf(LOG_MO_CMD_LEVEL, "MO disk %i: Sector timeout!",dnum);
                sector_timer=0;
                fmt_mode=FMT_MODE_IDLE;
                mo_raise_irq(MOINT_TIMEOUT, 0);
            }
        }
        return false;
    }
}

void fmt_io(Uint32 sector_id) {
    if (fmt_mode==FMT_MODE_IDLE) {
        return;
    }
    if (fmt_mode==FMT_MODE_READ_ID) {
        mo.tracknumh = (sector_id>>16)&0xFF;
        mo.tracknuml = (sector_id>>8)&0xFF;
        mo.sector_num = sector_id&0x0F;
        mo_raise_irq(MOINT_OPER_COMPL, 0);
        return;
    }
    
    /* Compare sector ID to formatter registers */
    if (fmt_match_id(sector_id)==false) {
        return;
    }
    
    switch (fmt_mode) {
        case FMT_MODE_READ:
            if (modrv[dnum].head!=READ_HEAD) {
                abort();
            }
            fmt_read_sector(sector_id);
            break;
        case FMT_MODE_WRITE:
            if (modrv[dnum].head!=WRITE_HEAD) {
                abort();
            }
            fmt_write_sector(sector_id);
            break;
        case FMT_MODE_ERASE:
            if (modrv[dnum].head!=ERASE_HEAD) {
                abort();
            }
            fmt_erase_sector(sector_id);
            break;
        case FMT_MODE_VERIFY:
            if (modrv[dnum].head!=VERIFY_HEAD) {
                abort();
            }
            fmt_verify_sector(sector_id);
            break;
            
        default:
            break;
    }
    
    fmt_sector_done();
    
    /* Check if the operation is complete */
    if (mo.sector_count==0) {
        switch (fmt_mode) {
            case FMT_MODE_WRITE:
                mo_raise_irq(MOINT_OPER_COMPL, SECTOR_IO_DELAY);
                break;
            case FMT_MODE_VERIFY:
                mo_raise_irq(MOINT_OPER_COMPL|MOINT_ECC_DONE, 0);
            default:
                mo_raise_irq(MOINT_OPER_COMPL, 0);
                break;
        }
        fmt_mode=FMT_MODE_IDLE;
    }
}


/* I/O functions */

void fmt_read_sector(Uint32 sector_id) {
    Uint32 sector_num = get_logical_sector(sector_id);
    MOdata.size = MO_SECTORSIZE;
    
    Log_Printf(LOG_WARN, "MO disk %i: Read sector at offset %i (%i sectors remaining)",
               dnum, sector_num, mo.sector_count-1);
    
    /* seek to the position */
	fseek(modrv[dnum].dsk, sector_num*MO_SECTORSIZE, SEEK_SET);
    fread(MOdata.buf, MOdata.size, 1, modrv[dnum].dsk);
    
    dma_mo_write_memory();
    
    if (MOdata.rpos==MOdata.size) {
        MOdata.rpos=MOdata.size=0;
    } else {
        // indicate error?
        Log_Printf(LOG_WARN, "MO disk %i: Error! Incomplete DMA transfer (%i byte)",
                   dnum, MOdata.size-MOdata.rpos);
    }
}

void fmt_write_sector(Uint32 sector_id) {
    Uint32 sector_num = get_logical_sector(sector_id);
    MOdata.size = MO_SECTORSIZE;
    
    dma_mo_read_memory();
    
    Log_Printf(LOG_WARN, "MO disk %i: Write sector at offset %i (%i sectors remaining)",
               dnum, sector_num, mo.sector_count-1);
    
    if (MOdata.rpos==MOdata.size) {
        /* seek to the position */
        /* NO FILE WRITE */
        Log_Printf(LOG_WARN, "MO Warning: File write disabled!");
#if 0
        fseek(modrv[dnum].dsk, sector_num*MO_SECTORSIZE, SEEK_SET);
        fwrite(MOdata.buf, MOdata.size, 1, modrv[dnum].dsk);
#endif
        MOdata.rpos=MOdata.size=0;
    } else {
        // indicate error?
        Log_Printf(LOG_WARN, "MO disk %i: Error! Incomplete DMA transfer (%i byte)",
                   dnum, MOdata.size);
    }
}

void fmt_erase_sector(Uint32 sector_id) {
    Uint32 sector_num = get_logical_sector(sector_id);
    MOdata.size = MOdata.rpos = MO_SECTORSIZE;
    memset(MOdata.buf, 0, MOdata.size);
    
    Log_Printf(LOG_WARN, "MO disk %i: Erase sector at offset %i (%i sectors remaining)",
               dnum, sector_num, mo.sector_count-1);
    
    if (MOdata.rpos==MOdata.size) {
        /* seek to the position */
        /* NO FILE WRITE */
        Log_Printf(LOG_WARN, "MO Warning: File write disabled!");
#if 0
        fseek(modrv[dnum].dsk, sector_num*MO_SECTORSIZE, SEEK_SET);
        fwrite(MOdata.buf, MOdata.size, 1, modrv[dnum].dsk);
#endif
        MOdata.rpos=MOdata.size=0;
    }
}

void fmt_verify_sector(Uint32 sector_id) {
    Uint32 sector_num = get_logical_sector(sector_id);
    MOdata.size = MOdata.rpos = MO_SECTORSIZE;
    
    Log_Printf(LOG_WARN, "MO disk %i: Verify sector at offset %i (%i sectors remaining)",
               dnum, sector_num, mo.sector_count-1);
    
    if (MOdata.rpos==MOdata.size) {
        MOdata.rpos=MOdata.size=0;
    }
}


/* Drive commands */

#define DRV_SEK     0x0000 /* seek (last 12 bits are track position) */
#define DRV_HOS     0xA000 /* high order seek (last 4 bits are high order (<<12) track position) */
#define DRV_REC     0x1000 /* recalibrate */
#define DRV_RDS     0x2000 /* return drive status */
#define DRV_RCA     0x2200 /* return current track address */
#define DRV_RES     0x2800 /* return extended status */
#define DRV_RHS     0x2A00 /* return hardware status */
#define DRV_RGC     0x3000 /* return general config */
#define DRV_RVI     0x3F00 /* return drive version information */
#define DRV_SRH     0x4100 /* select read head */
#define DRV_SVH     0x4200 /* select verify head */
#define DRV_SWH     0x4300 /* select write head */
#define DRV_SEH     0x4400 /* select erase head */
#define DRV_SFH     0x4500 /* select RF head */
#define DRV_RID     0x5000 /* reset attn and status */
#define DRV_RJ      0x5100 /* relative jump (see below) */
#define DRV_SPM     0x5200 /* stop motor */
#define DRV_STM     0x5300 /* start motor */
#define DRV_LC      0x5400 /* lock cartridge */
#define DRV_ULC     0x5500 /* unlock cartridge */
#define DRV_EC      0x5600 /* eject */
#define DRV_SOO     0x5900 /* spiral operation on */
#define DRV_SOF     0x5A00 /* spiral operation off */
#define DRV_RSD     0x8000 /* request self-diagnostic */
#define DRV_SD      0xB000 /* send data (last 12 bits used) */

/* Relative jump:
 * bits 0 to 3: offset (signed -8 (0x8) to +7 (0x7)
 * bits 4 to 6: head select
 */

/* Head select for relative jump */
#define RJ_READ     0x10
#define RJ_VERIFY   0x20
#define RJ_WRITE    0x30
#define RJ_ERASE    0x40

/* Drive status information */

/* Disk status (returned for DRV_RDS) */
#define DS_INSERT   0x0004 /* load completed */
#define DS_RESET    0x0008 /* power on reset */
#define DS_SEEK     0x0010 /* address fault */
#define DS_CMD      0x0020 /* invalid or unimplemented command */
#define DS_INTFC    0x0040 /* interface fault */
#define DS_I_PARITY 0x0080 /* interface parity error */
#define DS_STOPPED  0x0200 /* not spinning */
#define DS_SIDE     0x0400 /* media upside down */
#define DS_SERVO    0x0800 /* servo not ready */
#define DS_POWER    0x1000 /* laser power alarm */
#define DS_WP       0x2000 /* disk write protected */
#define DS_EMPTY    0x4000 /* no disk inserted */
#define DS_BUSY     0x8000 /* execute busy */

/* Extended status (returned for DRV_RES) */
#define ES_RF       0x0002 /* RF detected */
#define ES_WR_INH   0x0008 /* write inhibit (high temperature) */
#define ES_WRITE    0x0010 /* write mode failed */
#define ES_COARSE   0x0020 /* coarse seek failed */
#define ES_TEST     0x0040 /* test write failed */
#define ES_SLEEP    0x0080 /* sleep/wakeup failed */
#define ES_LENS     0x0100 /* lens out of range */
#define ES_TRACKING 0x0200 /* tracking servo failed */
#define ES_PLL      0x0400 /* PLL failed */
#define ES_FOCUS    0x0800 /* focus failed */
#define ES_SPEED    0x1000 /* not at speed */
#define ES_STUCK    0x2000 /* disk cartridge stuck */
#define ES_ENCODER  0x4000 /* linear encoder failed */
#define ES_LOST     0x8000 /* tracing failure */

/* Hardware status (returned for DRV_RHS) */
#define HS_LASER    0x0040 /* laser power failed */
#define HS_INIT     0x0080 /* drive init failed */
#define HS_TEMP     0x0100 /* high drive temperature */
#define HS_CLAMP    0x0200 /* spindle clamp misaligned */
#define HS_STOP     0x0400 /* spindle stop timeout */
#define HS_TEMPSENS 0x0800 /* temperature sensor failed */
#define HS_LENSPOS  0x1000 /* lens position failure */
#define HS_SERVOCMD 0x2000 /* servo command failure */
#define HS_SERVOTO  0x4000 /* servo timeout failure */
#define HS_HEAD     0x8000 /* head select failure */

/* Version information (returned for DRV_RVI) */
#define VI_VERSION  0x0880

void mo_drive_cmd(void) {

    if (!modrv[dnum].connected) {
        Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Drive %i not connected.\n", dnum);
        return;
    }

    Uint16 command = (mo.csrh<<8) | mo.csrl;
    
    /* Command in progress */
    mo.intstatus &= ~MOINT_CMD_COMPL;
    
    if (no_disk()) {
        Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Drive %i: No disk inserted.\n", dnum);
        switch (command) {
            case DRV_RDS:
            case DRV_RES:
            case DRV_RHS:
            case DRV_RGC:
            case DRV_RVI:
            case DRV_RID:
            case DRV_RSD:
                break;
            default:
                modrv[dnum].dstat |= DS_EMPTY;
                mo_raise_irq(MOINT_CMD_COMPL|MOINT_ATTN, CMD_DELAY);
                return;
        }
    }
    
    if ((command&0xF000)==DRV_SEK) {
        Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Seek (%04X)\n", command);
        modrv[dnum].head_pos = (modrv[dnum].ho_head_pos&0xF000) | (command&0x0FFF);
    } else if ((command&0xF000)==DRV_SD) {
        Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Send Data (%04X)\n", command);
    } else if ((command&0xFF00)==DRV_RJ) {
        Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Relative Jump (%04X)\n", command);
        mo_jump_head(command);
    } else if ((command&0xFFF0)==DRV_HOS) {
        Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: High Order Seek (%04X)\n", command);
        modrv[dnum].ho_head_pos = (command&0xF)<<12; /* CHECK: only seek command actually moves head? */
    } else {
    
        switch (command&0xFFFF) {
            case DRV_REC:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Recalibrate (%04X)\n", command);
                mo_recalibrate();
                break;
            case DRV_RDS:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Return Drive Status (%04X)\n", command);
                modrv[dnum].status = modrv[dnum].dstat;
                break;
            case DRV_RCA:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Return Current Track Address (%04X)\n", command);
                modrv[dnum].status = modrv[dnum].head_pos; /* TODO: check if correct */
                break;
            case DRV_RES:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Return Extended Status (%04X)\n", command);
                modrv[dnum].status = modrv[dnum].estat;
                break;
            case DRV_RHS:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Return Hardware Status (%04X)\n", command);
                modrv[dnum].status = modrv[dnum].hstat;
                break;
            case DRV_RGC:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Return General Config (%04X)\n", command);
                break;
            case DRV_RVI:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Return Version Information (%04X)\n", command);
                modrv[dnum].status = VI_VERSION;
                break;
            case DRV_SRH:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Select Read Head (%04X)\n", command);
                modrv[dnum].head = READ_HEAD;
                break;
            case DRV_SVH:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Select Verify Head (%04X)\n", command);
                modrv[dnum].head = VERIFY_HEAD;
                break;
            case DRV_SWH:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Select Write Head (%04X)\n", command);
                modrv[dnum].head = WRITE_HEAD;
                break;
            case DRV_SEH:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Select Erase Head (%04X)\n", command);
                modrv[dnum].head = ERASE_HEAD;
                break;
            case DRV_SFH:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Select RF Head (%04X)\n", command);
                modrv[dnum].head = RF_HEAD;
                break;
            case DRV_RID:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Reset Attn and Status (%04X)\n", command);
                mo_reset_attn_status();
                break;
            case DRV_SPM:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Stop Spindle Motor (%04X)\n", command);
                mo_stop_spinning();
                break;
            case DRV_STM:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Start Spindle Motor (%04X)\n", command);
                mo_start_spinning();
                break;
            case DRV_LC:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Lock Cartridge (%04X)\n", command);
                break;
            case DRV_ULC:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Unlock Cartridge (%04X)\n", command);
                break;
            case DRV_EC:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Eject (%04X)\n", command);
                mo_eject_disk();
                break;
            case DRV_SOO:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Start Spiraling (%04X)\n", command);
                mo_start_spiraling();
                break;
            case DRV_SOF:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Stop Spiraling (%04X)\n", command);
                mo_stop_spiraling();
                break;
            case DRV_RSD:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Request Self-Diagnostic (%04X)\n", command);
                break;
                
            default:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Unknown command! (%04X)\n", command);
                break;
        }
    }
    
    mo_raise_irq(MOINT_CMD_COMPL, CMD_DELAY);
}


void mo_reset(void) {
    /* TODO: reset more things */
    mo.intstatus=0;
    modrv[dnum].dstat=DS_RESET;
    //modrv[dnum].spinning=false;
    //modrv[dnum].spiraling=false;
    
    if (!modrv[dnum].inserted) {
        modrv[dnum].dstat|=DS_EMPTY;
    } else if (!modrv[dnum].spinning) {
        modrv[dnum].dstat|=DS_STOPPED;
    }
    mo_raise_irq(MOINT_ATTN, 100000);
}

void mo_reset_attn_status(void) {
    modrv[dnum].dstat=modrv[dnum].estat=modrv[dnum].hstat=0;
    mo.intstatus &= ~MOINT_ATTN;
#if 1
    if (!modrv[dnum].inserted) {
        modrv[dnum].dstat|=DS_EMPTY;
    } else if (!modrv[dnum].spinning) {
        modrv[dnum].dstat|=DS_STOPPED;
    }
#endif
    /* TODO: re-enable status messages? */
}

void mo_eject_disk(void) {
    Log_Printf(LOG_WARN, "MO disk %i: Eject",dnum);
    
    File_Close(modrv[dnum].dsk);
    modrv[dnum].dsk=NULL;
    modrv[dnum].inserted=false;
    
    ConfigureParams.MO.drive[dnum].bDiskInserted=false;
    ConfigureParams.MO.drive[dnum].szImageName[0]='\0';
}

void mo_insert_disk(int drv) {
    Log_Printf(LOG_WARN, "MO disk %i: Insert %s",dnum,ConfigureParams.MO.drive[dnum].szImageName);
    modrv[drv].inserted=true;
    if (ConfigureParams.MO.drive[drv].bWriteProtected) {
        modrv[drv].dsk = File_Open(ConfigureParams.MO.drive[drv].szImageName, "r");
        modrv[drv].protected=true;
    } else {
        modrv[drv].dsk = File_Open(ConfigureParams.MO.drive[drv].szImageName, "r+");
        modrv[drv].protected=false;
    }
    
    modrv[drv].dstat|=DS_INSERT;
    mo_raise_irq(MOINT_ATTN, 0);
}

void mo_recalibrate(void) {
    modrv[dnum].head_pos = 0; /* FIXME: What is real base head position? */
    modrv[dnum].sec_offset = 0;
    modrv[dnum].spiraling = false;
}

void mo_jump_head(Uint16 command) {
    int offset = command&0x7;
    if (command&0x8) {
        offset = 8 - offset;
        modrv[dnum].head_pos-=offset;
    } else {
        modrv[dnum].head_pos+=offset;
    }
    modrv[dnum].sec_offset=0; /* CHECK: is this needed? same for seek? */
    
    switch (command&0xF0) {
        case RJ_READ:
            modrv[dnum].head=READ_HEAD;
            break;
        case RJ_VERIFY:
            modrv[dnum].head=VERIFY_HEAD;
            break;
        case RJ_WRITE:
            modrv[dnum].head=WRITE_HEAD;
            break;
        case RJ_ERASE:
            modrv[dnum].head=ERASE_HEAD;
            break;
            
        default:
            modrv[dnum].head=NO_HEAD;
            break;
    }
    Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Relative Jump: %i sectors %s (%s head)\n", offset*16,
               (command&0x8)?"back":"forward",
               (command&0xF0)==RJ_READ?"read":
               (command&0xF0)==RJ_VERIFY?"verify":
               (command&0xF0)==RJ_WRITE?"write":
               (command&0xF0)==RJ_ERASE?"erase":"unknown");
}

void mo_start_spinning(void) {
    modrv[dnum].spinning=true;
}

void mo_stop_spinning(void) {
    modrv[dnum].spinning=false;
    modrv[dnum].spiraling=false;
}

void mo_start_spiraling(void) {
    if (!modrv[0].spiraling && !modrv[1].spiraling) { /* periodic disk operation already active? */
        CycInt_AddRelativeInterrupt(SECTOR_IO_DELAY, INT_CPU_CYCLE, INTERRUPT_MO_IO);
    }
    modrv[dnum].spiraling=true;
}

void mo_stop_spiraling(void) {
    modrv[dnum].spiraling=false;
}

void mo_spiraling_operation(void) {
    if (!modrv[0].spiraling && !modrv[1].spiraling) { /* this stops periodic disk operation */
        return; /* nothing to do */
    }
    
    int i;
    for (i=0; i<2; i++) {
        if (modrv[i].spiraling) {
            
            /* If the drive is selected, connect to formatter */
            if (i==dnum) {
                fmt_io((modrv[i].head_pos<<8)|modrv[i].sec_offset);
            }
            
            /* Continue spiraling */
            modrv[i].sec_offset++;
            modrv[i].head_pos+=modrv[i].sec_offset/MO_SEC_PER_TRACK;
            modrv[i].sec_offset%=MO_SEC_PER_TRACK;
        }
    }
    CycInt_AddRelativeInterrupt(SECTOR_IO_DELAY, INT_CPU_CYCLE, INTERRUPT_MO_IO);
}

void MO_IO_Handler(void) {
    CycInt_AcknowledgeInterrupt();

    mo_spiraling_operation();
}


/* Interrupts */

void mo_interrupt(Uint8 interrupt) {
    mo.intstatus|=interrupt;
    if (mo.intstatus&mo.intmask) {
        set_interrupt(INT_DISK, SET_INT);
    }
}

void mo_raise_irq(Uint8 interrupt, Uint32 delay) {
    if (delay>0) {
        delayed_intr|=interrupt;
        CycInt_AddRelativeInterrupt(delay, INT_CPU_CYCLE, INTERRUPT_MO);
    } else {
        mo_interrupt(interrupt);
    }
}

void MO_InterruptHandler(void) {
    CycInt_AcknowledgeInterrupt();
    
    mo.intstatus |= delayed_intr;
    delayed_intr = 0;
    
    if (mo.intstatus&mo.intmask) {
        set_interrupt(INT_DISK, SET_INT);
    }
}


/* Initialize/Uninitialize MO disks */
void MO_Init(void) {
    Log_Printf(LOG_WARN, "Loading magneto-optical disks:");
    int i;
    
    for (i=0; i<2; i++) {
        /* Check if files exist. */
        if (ConfigureParams.MO.drive[i].bDriveConnected) {
            modrv[i].connected=true;
            if (ConfigureParams.MO.drive[i].bDiskInserted &&
                File_Exists(ConfigureParams.MO.drive[i].szImageName)) {
                modrv[i].inserted=true;
                if (ConfigureParams.MO.drive[i].bWriteProtected) {
                    modrv[i].dsk = File_Open(ConfigureParams.MO.drive[i].szImageName, "r");
                    modrv[i].protected=true;
                } else {
                    modrv[i].dsk = File_Open(ConfigureParams.MO.drive[i].szImageName, "r+");
                    modrv[i].protected=false;
                }
            } else {
                modrv[i].dsk = NULL;
                modrv[i].inserted=false;
            }
        } else {
            modrv[i].connected=false;
        }

        Log_Printf(LOG_WARN, "MO Disk%i: %s\n",i,ConfigureParams.MO.drive[i].szImageName);
    }
}

void MO_Uninit(void) {
    if (modrv[0].dsk)
        File_Close(modrv[0].dsk);
    if (modrv[1].dsk) {
        File_Close(modrv[1].dsk);
    }
    modrv[0].dsk = modrv[1].dsk = NULL;
    modrv[0].inserted = modrv[1].inserted = false;
}

void MO_Reset(void) {
    MO_Uninit();
    MO_Init();
}




/* ECC functions */

void mo_read_ecc(void) {
#if 0
    if (!MOdata.size) {
        MOdata.size=1296;// hack
    }
#endif
    mo_raise_irq(MOINT_ECC_DONE, SECTOR_IO_DELAY);
    //dma_mo_write_memory();
}

void mo_write_ecc(void) {
#if 0
    if (!MOdata.size) {
        MOdata.size=1024; // hack
    }
#endif
    //dma_mo_read_memory();
    mo_raise_irq(MOINT_ECC_DONE, SECTOR_IO_DELAY);
}




/* old stuff, remove later */

Uint8 ECC_buffer[1600];
Uint32 length;
Uint8 sector_position;

/* MO Drive Registers */
#define MO_INTSTATUS    4
#define MO_INTMASK      5
#define MO_CTRLR_CSR2   6
#define MO_CTRLR_CSR1   7
#define MO_COMMAND_HI   8
#define MO_COMMAND_LO   9
#define MO_INIT         12
#define MO_FORMAT       13
#define MO_MARK         14

/* MO Drive Register Constants */
#define INTSTAT_CLR     0xFC
#define INTSTAT_RESET   0x01
// controller csr 2
#define ECC_MODE        0x20
// controller csr 1
#define ECC_READ        0x80
#define ECC_WRITE       0x40

// drive commands
#define OD_SEEK         0x0000
#define OD_HOS          0xA000
#define OD_RECALIB      0x1000
#define OD_RDS          0x2000
#define OD_RCA          0x2200

#define OD_RID          0x5000

void check_ecc(void);
void compute_ecc(void);

struct {
    Uint16 track_num;
    Uint8 sector_incrnum;
    Uint8 sector_count;
    Uint8 intstatus;
    Uint8 intmask;
    Uint8 ctrlr_csr2;
    Uint8 ctrlr_csr1;
    Uint16 command;
} mo_drive;

void MOdrive_Read(void) {
    Uint8 val;
	Uint8 reg = IoAccessCurrentAddress&0x1F;
    
    switch (reg) {
        case MO_INTSTATUS:
            val = mo_drive.intstatus;
            mo_drive.intstatus |= 0x01;
            break;
            
        case MO_INTMASK:
            val = mo_drive.intmask;
            break;
            
        case MO_CTRLR_CSR2:
            val = mo_drive.ctrlr_csr2;
            break;

        case MO_CTRLR_CSR1:
            val = mo_drive.ctrlr_csr1;
            break;

        case MO_COMMAND_HI:
            val = 0x00;
            break;

        case MO_COMMAND_LO:
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
            val = 0;
            break;
    }
    
    IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = val;
    Log_Printf(LOG_MO_REG_LEVEL, "[MO Drive] read reg %d val %02x PC=%x %s at %d",reg,val,m68k_getpc(),__FILE__,__LINE__);
}


void MOdrive_Write(void) {
    Uint8 val = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
	Uint8 reg = IoAccessCurrentAddress&0x1F;
    
    Log_Printf(LOG_MO_REG_LEVEL, "[MO Drive] write reg %d val %02x PC=%x %s at %d",reg,val,m68k_getpc(),__FILE__,__LINE__);
    
    switch (reg) {
        case MO_INTSTATUS: // reg 4
            switch (val) {
                case INTSTAT_CLR:
                    mo_drive.intstatus &= 0x02;
                    mo_drive.intstatus |= 0x01;
                    break;
                    
                case INTSTAT_RESET:
                    mo_drive.intstatus |= 0x01;
                    //MOdrive_Reset();

                default:
                    break;
                    
                //mo_drive.intstatus = (mo_drive.intstatus & (~val & 0xfc)) | (val & 3);
            }
            break;
            
        case MO_INTMASK: // reg 5
            mo_drive.intmask = val;
            break;
            
        case MO_CTRLR_CSR2: // reg 6
            mo_drive.ctrlr_csr2 = val;
            break;
            
        case MO_CTRLR_CSR1: // reg 7
            mo_drive.ctrlr_csr1 = val;
            switch (mo_drive.ctrlr_csr1) {
                case ECC_WRITE:
                    //dma_memory_read(ECC_buffer, &length, CHANNEL_DISK);
                    mo_drive.intstatus |= 0xFF;
                    if (mo_drive.ctrlr_csr2&ECC_MODE)
                        check_ecc();
                    else
                        compute_ecc();
                    break;
                    
                case ECC_READ:
                    //dma_memory_write(ECC_buffer, length, CHANNEL_DISK);
                    mo_drive.intstatus |= 0xFF;
                    break;
                    
                case 0x20: // RD_STAT
                    mo_drive.intstatus |= 0x01; // set cmd complete
                    break;
                    
                default:
                    break;
            }
            break;
        case MO_COMMAND_HI:
            mo_drive.command = (val << 8)&0xFF00;
            break;
        case MO_COMMAND_LO:
            mo_drive.command |= val&0xFF;
            MOdrive_Execute_Command(mo_drive.command);
//            mo_drive.intstatus &= ~0x01; // release cmd complete
//            set_interrupt(INT_DISK, SET_INT);
            break;
            
        case MO_INIT: // reg 12
            if (val&0x80) { // sector > enable
                printf("MO Init: sector > enable\n");
            }
            if (val&0x40) { // ECC starve disable
                printf("MO Init: ECC starve disable\n");
            }
            if (val&0x20) { // ID cmp on track, not sector
                printf("MO Init: ID cmp on track not sector\n");
            }
            if (val&0x10) { // 25 MHz ECC clk for 3600 RPM
                printf("MO Init: 25 MHz ECC clk for 3600 RPM\n");
            }
            if (val&0x08) { // DMA starve enable
                printf("MO Init: DMA starve enable\n");
            }
            if (val&0x04) { // diag: generate bad parity
                printf("MO Init: diag: generate bad parity\n");
            }
            if (val&0x03) {
                printf("MO Init: %i IDs must match\n", val&0x03);
            }
            break;
            
        case MO_FORMAT: // reg 13
            break;
            
        case MO_MARK: // reg 14
            break;
            
        case 16:
        case 17:
        case 18:
        case 19:
        case 20:
        case 21:
        case 22:
            break; // flag strategy
            
        default:
            break;
    }
}


void MOdrive_Execute_Command(Uint16 command) {
    mo_drive.intstatus &= ~0x01; // release cmd complete
    switch (command) {
        case OD_SEEK:
//            set_interrupt(INT_DISK, SET_INT);
            break;
        case OD_RDS:
            break;
            
        case OD_RID:
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