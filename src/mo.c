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
    Uint8 sector_incrnum;
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
    
    Uint32 head_pos;
    Uint32 ho_head_pos;
    
    FILE* dsk;
} modrv[2];

int dnum;

//Uint16 mo_status = 0;
//Uint16 mo_dstat = 0;
//Uint16 mo_estat = 0;
//Uint16 mo_hstat = 0;


/* Interrupt status */
#define MOINT_CMD_COMPL     0x01
#define MOINT_GPO           0x02
#define MOINT_OPER_COMPL    0x04
#define MOINT_ECC_DONE      0x08
#define MOINT_TIMEOUT       0x10
#define MOINT_READ_FAULT    0x20
#define MOINT_PARITY_ERR    0x40
#define MOINT_DATA_ERR      0x80

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
#define MO_TRACK_OFFSET     4149
#define MO_SECTORSIZE       1024


/* Functions */
void mo_formatter_cmd(void);
void mo_drive_cmd(void);

void mo_read_disk(void);
void mo_write_disk(void);
void mo_erase_disk(void);
void mo_verify_disk(void);
void mo_eject_disk(void);
void mo_read_ecc(void);
void mo_write_ecc(void);

void mo_jump_head(Uint16 command);

void MO_Init(void);
void MO_Uninit(void);

/* Experimental */
//int drv_num = 0;
//FILE* mo_disk[2];
//Uint32 head_pos;
//Uint32 ho_head_pos;
Uint8 delayed_intr = 0;
void mo_raise_irq(Uint8 interrupt);

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
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.sector_incrnum;
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Sector increment read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_SectorIncr_Write(void) {
    mo.sector_incrnum=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Sector increment write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
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
    /* temporary hack */
    //mo.intstatus = 0x00;
    //set_interrupt(INT_DISK, RELEASE_INT);
}

void MO_IntStatus_Write(void) {
    mo.intstatus &= ~(IoMem[IoAccessCurrentAddress & IO_SEG_MASK]);
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Interrupt status write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    /* TODO: check if correct */
    if (!(mo.intstatus&mo.intmask)||(mo.intstatus&mo.intmask)==MOINT_CMD_COMPL) {
        set_interrupt(INT_DISK, RELEASE_INT);
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
    
    dnum = (mo.ctrlr_csr2&MOCSR2_DRIVE_SEL);
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
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Format at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
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

#define FMT_SPINUP      0xF0
#define FMT_EJECT       0xF1
#define FMT_SEEK        0xF2
#define FMT_SPIRAL_OFF  0xF3
#define FMT_RESPIN      0xF4
#define FMT_TEST        0xF5
#define FMT_EJECT_NOW   0xF6

void mo_formatter_cmd(void) { /* TODO: commands can be combined! (read|eccread)*/
    
    if (modrv[dnum].dsk==NULL) { /* FIXME: Add support for second drive */
        mo.intstatus &= ~MOINT_CMD_COMPL;
        return;
    }
    
    /* Command in progress */
    mo.intstatus &= ~MOINT_CMD_COMPL;
    
    switch (mo.ctrlr_csr1) {
        case FMT_RESET:
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: Reset (%02X)\n", mo.ctrlr_csr1);
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
            break;
        case FMT_VERIFY:
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: Verify (%02X)\n", mo.ctrlr_csr1);
            mo_verify_disk();
            break;
        case FMT_ERASE:
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: Erase (%02X)\n", mo.ctrlr_csr1);
            mo_erase_disk();
            break;
        case FMT_READ:
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: Read (%02X)\n", mo.ctrlr_csr1);
            mo_read_disk();
            break;
        case FMT_WRITE:
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: Write (%02X)\n", mo.ctrlr_csr1);
            mo_write_disk();
            break;
        /* Combined commands */
        case (FMT_READ|FMT_ECC_READ):
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: Read using ECC (%02X)\n", mo.ctrlr_csr1);
            mo_read_disk();
            mo_read_ecc();
            break;
        case (FMT_WRITE|FMT_ECC_WRITE):
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: Write using ECC (%02X)\n", mo.ctrlr_csr1);
            mo_write_disk();
            mo_write_ecc();
            break;
            
#if 0
        case FMT_SPINUP:
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: Spin Up (%02X)\n", mo.ctrlr_csr1);
            break;
        case FMT_EJECT:
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: Eject (%02X)\n", mo.ctrlr_csr1);
            break;
        case FMT_SEEK:
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: Seek (%02X)\n", mo.ctrlr_csr1);
            break;
        case FMT_SPIRAL_OFF:
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: Spiral Off (%02X)\n", mo.ctrlr_csr1);
            break;
        case FMT_RESPIN:
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: Respin (%02X)\n", mo.ctrlr_csr1);
            break;
        case FMT_TEST:
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: Test (%02X)\n", mo.ctrlr_csr1);
            break;
        case FMT_EJECT_NOW:
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: Eject now (%02X)\n", mo.ctrlr_csr1);
            break;
#endif
        default:
            Log_Printf(LOG_WARN,"[MO] Formatter command: Unknown command! (%02X)\n", mo.ctrlr_csr1);
            abort();
            break;
    }
    
    mo_raise_irq(MOINT_CMD_COMPL);
    CycInt_AddRelativeInterrupt(10000, INT_CPU_CYCLE, INTERRUPT_MO);
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

    if (modrv[dnum].dsk==NULL) { /* FIXME: Add support for second drive */
        mo.intstatus &= ~MOINT_CMD_COMPL;
        return;
    }

    Uint16 command = (mo.csrh<<8) | mo.csrl;
    
    /* Command in progress */
    mo.intstatus &= ~MOINT_CMD_COMPL;
    
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
                break;
            case DRV_SVH:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Select Verify Head (%04X)\n", command);
                break;
            case DRV_SWH:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Select Write Head (%04X)\n", command);
                break;
            case DRV_SEH:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Select Erase Head (%04X)\n", command);
                break;
            case DRV_SFH:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Select RF Head (%04X)\n", command);
                break;
            case DRV_RID:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Reset Attn and Status (%04X)\n", command);
                break;
            case DRV_SPM:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Stop Spindle Motor (%04X)\n", command);
                break;
            case DRV_STM:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Start Spindle Motor (%04X)\n", command);
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
                break;
            case DRV_SOF:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Stop Spiraling (%04X)\n", command);
                break;
            case DRV_RSD:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Request Self-Diagnostic (%04X)\n", command);
                break;
                
            default:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Unknown command! (%04X)\n", command);
                break;
        }
    }
    
    mo_raise_irq(MOINT_CMD_COMPL);
    CycInt_AddRelativeInterrupt(10000, INT_CPU_CYCLE, INTERRUPT_MO);
}


/* Interrupts */
#define INTDELAY_CMD    100
#define INTDELAY_OPER   2000
#define INTDELAY_ECC    3000
#define INTDELAY_OTHER  4000

void mo_raise_irq(Uint8 interrupt) {

    delayed_intr|=interrupt;
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
        /* Check if files exist. Present dialog to re-select missing files. */
        if (File_Exists(ConfigureParams.MO.drive[i].szImageName) &&
            ConfigureParams.MO.drive[i].bDriveConnected &&
            ConfigureParams.MO.drive[i].bDiskInserted) {
            //nFileSize[target] = File_Length(ConfigureParams.SCSI.target[target].szImageName);
            if (ConfigureParams.MO.drive[i].bWriteProtected) {
                modrv[i].dsk = File_Open(ConfigureParams.MO.drive[i].szImageName, "r");
            } else {
                modrv[i].dsk = File_Open(ConfigureParams.MO.drive[i].szImageName, "r+");
            }
        } else {
            //nFileSize[target] = 0;
            modrv[i].dsk=NULL;
            modrv[i].dstat=DS_EMPTY;
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
}

void MO_Reset(void) {
    MO_Uninit();
    MO_Init();
}


/* Head functions */

void mo_jump_head(Uint16 command) {
    int offset = command&0x7;
    if (command&0x8) {
        offset = 8 - offset;
        modrv[dnum].head_pos-=offset;
    } else {
        modrv[dnum].head_pos+=offset;
    }
    Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Relative Jump: %i sectors %s (%s head)\n", offset*16,
               (command&0x8)?"back":"forward",
               (command&0xF0)==RJ_READ?"read":
               (command&0xF0)==RJ_VERIFY?"verify":
               (command&0xF0)==RJ_WRITE?"write":
               (command&0xF0)==RJ_ERASE?"erase":"unknown");
}

void mo_move_head_start(void) {
    modrv[dnum].head_pos+=mo.sector_incrnum/MO_SEC_PER_TRACK;
    modrv[dnum].head_pos+=(mo.sector_incrnum%MO_SEC_PER_TRACK)?1:0;
}

void mo_move_head_end(void) {
    Uint32 offset = (mo.sector_incrnum%MO_SEC_PER_TRACK)+mo.sector_count;
    modrv[dnum].head_pos+=(offset-1)/MO_SEC_PER_TRACK;
}


/* Helpers */

Uint32 mo_get_sector(void) {
    Sint32 tracknum = modrv[dnum].head_pos;
#if 1
    tracknum-=MO_TRACK_OFFSET;
#endif
    return (tracknum*MO_SEC_PER_TRACK)+mo.sector_incrnum;
}


/* I/O functions */

void mo_read_disk(void) {
    MOdata.size = mo.sector_count*MO_SECTORSIZE;
    if (mo.sector_count==0) /* 0 is maximum size (256 sectors) */
        MOdata.size=0x100*MO_SECTORSIZE;

    Uint32 sector_num = mo_get_sector();

    mo_move_head_start();

    Log_Printf(LOG_WARN, "MO disk %i: Read %i sector(s) at offset %i",
               dnum, MOdata.size/MO_SECTORSIZE, sector_num);
    
	/* seek to the position */
	fseek(modrv[dnum].dsk, sector_num*MO_SECTORSIZE, SEEK_SET);
    fread(MOdata.buf, MOdata.size, 1, modrv[dnum].dsk);

    printf("%c%c%c%c\n",MOdata.buf[0],MOdata.buf[1],MOdata.buf[2],MOdata.buf[3]);
#if 1 // experimental, needs to be timed with DMA!
    mo_raise_irq(MOINT_OPER_COMPL);
    CycInt_AddRelativeInterrupt(10000, INT_CPU_CYCLE, INTERRUPT_MO);
#endif
    dma_mo_write_memory();
}
void mo_read_done(void) {
    mo_move_head_end();
    mo.sector_count = 0;
    
    //mo_raise_irq(MOINT_OPER_COMPL);
    //CycInt_AddRelativeInterrupt(10000, INT_CPU_CYCLE, INTERRUPT_MO);
}


void mo_write_disk(void) {
    MOdata.size = mo.sector_count*MO_SECTORSIZE;
    if (mo.sector_count==0) /* 0 is maximum size (256 sectors) */
        MOdata.size=0x100*MO_SECTORSIZE;
    dma_mo_read_memory();
}
void mo_write_done(void) {
    Uint32 sector_num = mo_get_sector();
    
    mo_move_head_start();
    
    
    
    Log_Printf(LOG_WARN, "MO disk %i: Write %i sector(s) at offset %i",
               dnum, MOdata.size/MO_SECTORSIZE, sector_num);
    
    
    /* NO FILE WRITE */
    Log_Printf(LOG_WARN, "MO Warning: File write disabled!");
#if 0
    return; // just to be sure
	/* seek to the position */
	fseek(modrv[dnum].dsk, sector_num*MO_SECTORSIZE, SEEK_SET);
    fwrite(MOdata.buf, MOdata.size, 1, modrv[dnum].dsk);
#endif
    printf("%c%c%c%c\n",MOdata.buf[0],MOdata.buf[1],MOdata.buf[2],MOdata.buf[3]);
    
    mo_move_head_end();
    mo.sector_count = 0;
    
    mo_raise_irq(MOINT_OPER_COMPL);
    CycInt_AddRelativeInterrupt(10000, INT_CPU_CYCLE, INTERRUPT_MO);
}

void mo_erase_disk(void) {
    Uint32 datasize = mo.sector_count*MO_SECTORSIZE;
    Uint32 sector_num = mo_get_sector();
    
    mo_move_head_start();

    Log_Printf(LOG_WARN, "MO disk %i: Erase %i sector(s) at offset %i",
               dnum, datasize/MO_SECTORSIZE, sector_num);
    
    mo_move_head_end();
    mo.sector_count = 0;
    
    mo_raise_irq(MOINT_OPER_COMPL);
}

void mo_verify_disk(void) {
    Uint32 datasize = mo.sector_count*MO_SECTORSIZE;
    Uint32 sector_num = mo_get_sector();
    
    mo_move_head_start();

    Log_Printf(LOG_WARN, "MO disk %i: Verify %i sector(s) at offset %i",
               dnum, datasize/MO_SECTORSIZE, sector_num);
    
    mo_move_head_end();
    mo.sector_count = 0;
    
    mo_raise_irq(MOINT_OPER_COMPL);
    mo_raise_irq(MOINT_ECC_DONE); /* TODO: check! */
}

void mo_eject_disk(void) {
    Log_Printf(LOG_WARN, "MO disk %i: Eject",dnum);
    
    File_Close(modrv[dnum].dsk);
    modrv[dnum].dsk=NULL;
    
    ConfigureParams.MO.drive[dnum].bDiskInserted=false;
    ConfigureParams.MO.drive[dnum].szImageName[0]='\0';
}


/* EXPERIMENTAL */
void mo_dma_done(bool write) {
    Log_Printf(LOG_WARN, "[MO] DMA transfer done: MO residual bytes: %i",MOdata.size-MOdata.rpos);
    
    if (MOdata.size == MOdata.rpos) { /* Transfer done */
        if (write) {
            mo_read_done();
        } else {
            mo_write_done();
        }
        MOdata.size=MOdata.rpos=0;
    }
}


/* ECC functions */

void mo_read_ecc(void) {
#if 0
    if (!MOdata.size) {
        MOdata.size=1296;// hack
    }
#endif
    mo_raise_irq(MOINT_ECC_DONE);
    dma_mo_write_memory();
}

void mo_write_ecc(void) {
#if 0
    if (!MOdata.size) {
        MOdata.size=1024; // hack
    }
#endif
    dma_mo_read_memory();
    mo_raise_irq(MOINT_ECC_DONE);
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