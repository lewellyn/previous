/*  Previous - mo.c
 
 This file is distributed under the GNU Public License, version 2 or at
 your option any later version. Read the file gpl.txt for details.
 
 Canon Magneto-optical Disk Drive and NeXT MO Controller Emulation.
  
 NeXT Optical Storage Processor uses Reed-Solomon algorithm for error correction.
 It has 2 128 byte internal buffers and uses double-buffering to perform error correction.
 
 Dummy to pass POT.
 
 */

#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "configuration.h"
#include "mo.h"
#include "sysReg.h"
#include "dma.h"

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

#define MOINT_CMD_COMPL 0x01


/* Functions */
void mo_formatter_cmd(void);
void mo_drive_cmd(void);

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
}

void MO_IntStatus_Write(void) {
    mo.intstatus &= ~(IoMem[IoAccessCurrentAddress & IO_SEG_MASK]);
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Interrupt status write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
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

void mo_formatter_cmd(void) {
    
    /* Command in progress */
    mo.intstatus &= ~MOINT_CMD_COMPL;
    
    switch (mo.ctrlr_csr1) {
        case FMT_RESET:
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: Reset (%02X)\n", mo.ctrlr_csr1);
            break;
        case FMT_ECC_READ:
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: ECC Read (%02X)\n", mo.ctrlr_csr1);
            break;
        case FMT_ECC_WRITE:
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: ECC Write (%02X)\n", mo.ctrlr_csr1);
            break;
        case FMT_RD_STAT:
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: Read Status (%02X)\n", mo.ctrlr_csr1);
            break;
        case FMT_ID_READ:
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: ID Read (%02X)\n", mo.ctrlr_csr1);
            break;
        case FMT_VERIFY:
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: Verify (%02X)\n", mo.ctrlr_csr1);
            break;
        case FMT_ERASE:
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: Erase (%02X)\n", mo.ctrlr_csr1);
            break;
        case FMT_READ:
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: Read (%02X)\n", mo.ctrlr_csr1);
            break;
        case FMT_WRITE:
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Formatter command: Write (%02X)\n", mo.ctrlr_csr1);
            break;
            
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

            
        default:
            Log_Printf(LOG_WARN,"[MO] Formatter command: Unknown command! (%02X)\n", mo.ctrlr_csr1);
            break;
    }
    
    CycInt_AddRelativeInterrupt(100, INT_CPU_CYCLE, INTERRUPT_MO);
}

/* Drive commands */
/* Note: track number = ((HOS & 0x000F) << 12) | (SEK & 0x0FFF) */

#define DRV_SEK     0x0000 /* seek (last 12 bits are track position) */
#define DRV_HOS     0xA000 /* high order seek (last 4 bits are high order (<<12) track position) */
#define DRV_REC     0x1000 /* recalibrate */
#define DRV_RDS     0x2000 /* return drive status */
#define DRV_RCA     0x2200 /* return current track address */
#define DRV_RES     0x2800 /* return extended status */
#define DRV_RHS     0x2A00 /* return hardware status */
#define DRV_RGC     0x3000
#define DRV_RVI     0x3F00 /* return drive version information */
#define DRV_SRH     0x4100 /* select read head */
#define DRV_SVH     0x4200 /* select verify head */
#define DRV_SWH     0x4300 /* select write head */
#define DRV_SEH     0x4400 /* select erase head */
#define DRV_SFH     0x4500
#define DRV_RID     0x5000 /* spin up */
#define DRV_RJ      0x5100 /* relative jump (last 4 bits are jump offset) */
#define DRV_SPM     0x5200 /* stop motor */
#define DRV_STM     0x5300 /* start motor */
#define DRV_LC      0x5400
#define DRV_ULC     0x5500
#define DRV_EC      0x5600 /* eject */
#define DRV_SOO     0x5900 /* start spiraling? */
#define DRV_SOF     0x5A00 /* stop spiraling? */
#define DRV_RSD     0x8000
#define DRV_SD      0xB000


/* Drive status information */

/* Disk status (returned for DRV_RDS) */
#define DS_INSERT   0x0002
#define DS_RESET    0x0004
#define DS_SEEK     0x0008
#define DS_CMD      0x0010
#define DS_INTFC    0x0020 /* interface */
#define DS_PARITY   0x0040
#define DS_STOPPED  0x0100
#define DS_SIDE     0x0200 /* cartridge is upside down */
#define DS_SERVO    0x0400
#define DS_POWER    0x0800
#define DS_WP       0x1000 /* write protected */
#define DS_EMPTY    0x2000 /* no cartridge inserted */
#define DS_BUSY     0x4000

/* Extended status (returned for DRV_RES) */
#define ES_RF       0x0001
#define ES_WRITE    0x0008
#define ES_COARSE   0x0010
#define ES_TEST     0x0020
#define ES_SLEEP    0x0040
#define ES_LENS     0x0080
#define ES_TRACKING 0x0100
#define ES_PLL      0x0200
#define ES_FOCUS    0x0400
#define ES_SPEED    0x0800
#define ES_STUCK    0x1000
#define ES_ENCODER  0x2000
#define ES_LOST     0x4000

/* Hardware status (returned for DRV_RHS) */
#define HS_LASER    0x0040
#define HS_INIT     0x0080
#define HS_TEMP     0x0100
#define HS_CLAMP    0x0200
#define HS_STOP     0x0400
#define HS_TEMPSENS 0x0800
#define HS_LENSPOS  0x1000
#define HS_SERVOCMD 0x2000
#define HS_SERVOTO  0x4000 /* servo timeout */
#define HS_HEAD     0x8000

/* Version information (returned for DRV_RVI) */
#define VI_VERSION  0x0000 /* TODO: find out correct version */

void mo_drive_cmd(void) {
    Uint16 command = (mo.csrh<<8) | mo.csrl;
    
    /* Command in progress */
    mo.intstatus &= ~MOINT_CMD_COMPL;
    
    if ((command&0xF000)==DRV_SEK) {
        Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Seek (%04X)\n", command);
    } else if ((command&0xF000)==DRV_HOS) {
        Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: High Order Seek (%04X)\n", command);
    } else {
        
        switch (command&0xFF00) {
            case DRV_REC:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Recalibrate (%04X)\n", command);
                break;
            case DRV_RDS:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Return Drive Status (%04X)\n", command);
                break;
            case DRV_RCA:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Return Current Track Address (%04X)\n", command);
                break;
            case DRV_RES:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Return Extended Status (%04X)\n", command);
                break;
            case DRV_RHS:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Return Hardware Status (%04X)\n", command);
                break;
            case DRV_RGC:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: RGC (%04X)\n", command);
                break;
            case DRV_RVI:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Return Version Information (%04X)\n", command);
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
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: SFH (%04X)\n", command);
                break;
            case DRV_RID:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Spin Up (%04X)\n", command);
                break;
            case DRV_RJ:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Relative Jump (%04X)\n", command);
                break;
            case DRV_SPM:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Stop Motor (%04X)\n", command);
                break;
            case DRV_STM:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Start Motor (%04X)\n", command);
                break;
            case DRV_LC:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: LC (%04X)\n", command);
                break;
            case DRV_ULC:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: ULC (%04X)\n", command);
                break;
            case DRV_EC:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Eject (%04X)\n", command);
                break;
            case DRV_SOO:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Start Spiraling (%04X)\n", command);
                break;
            case DRV_SOF:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Stop Spiraling (%04X)\n", command);
                break;
            case DRV_RSD:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: RSD (%04X)\n", command);
                break;
            case DRV_SD:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: SD (%04X)\n", command);
                break;
                
            default:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Unknown command! (%04X)\n", command);
                break;
        }
    }
    
    mo.csrl = mo.csrh = 0x00; /* indicate no error, TODO: check error codes */
        
    CycInt_AddRelativeInterrupt(100, INT_CPU_CYCLE, INTERRUPT_MO);
}

void MO_InterruptHandler(void) {
    CycInt_AcknowledgeInterrupt();

    mo.intstatus |= MOINT_CMD_COMPL;
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