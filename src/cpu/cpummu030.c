/* Emulation of MC68030 MMU
 * This code has been written for Previous - a NeXT Computer emulator
 *
 * This file is distributed under the GNU Public License.
 *
 *
 * Release notes:
 * 01-09-2012: First release (Andreas Grabher)
 *
 *
 * Known Problems:
 * - The MMU code is not properly connected to the CPU code
 * - MMU configuration exceptions are missing
 *
 *
 * TODO list:
 * - Once MMU is properly connected to CPU, fix function code handling:
 *   In PFLUSH functions (mmu030_flush_atc_page_fc, mmu030_flush_atc_fc)
 *   1. How to apply fc_mask?
 *   2. How to compare fc in ATC entry with fc to flush (==, &)?
 *   Check all other function code handling!
 * - Check if read-modify-write operations are correctly detected for
 *   handling transparent access (see TT matching functions)
 * - Do MMU configuration exceptions and F-line unimplemented instruction
 *   exceptions (see decode_tc, decode_rp, etc)
 * - If possible, test mmu030_table_search with all kinds of translations
 *   (early termination, invalid descriptors, bus errors, indirect
 *   descriptors, PTEST in different levels, etc).
 * - Check which bits of an ATC entry should be set and which should be
 *   un-set, if an invalid translation occurs.
 * - Handle cache inhibit bit when accessing ATC entries
 */

#include "compat.h"
#include "sysconfig.h"
#include "sysdeps.h"
#include "hatari-glue.h"
#include "options_cpu.h"
#include "events.h"
#include "custom.h"
#include "maccess.h"
#include "memory.h"
#include "newcpu.h"
#include "main.h"
#include "cpummu.h"
#include "cpu_prefetch.h"
#include "main.h"
#include "m68000.h"
#include "reset.h"
#include "cycInt.h"
#include "mfp.h"
#include "cart.h"
#include "dialog.h"
#include "screen.h"
#include "video.h"
#include "options.h"
#include "log.h"
#include "debugui.h"
#include "debugcpu.h"


#define MMUOP_DEBUG 1
#define MMU030_ATC_DBG_MSG 1
#define MMU030_REG_DBG_MSG 1

/* for debugging messages */
char table_letter[4] = {'A','B','C','D'};


/* ATC struct */
#define ATC030_NUM_ENTRIES  22

typedef struct {
    struct {
        uaecptr addr;
        bool modified;
        bool write_protect;
        bool cache_inhibit;
        bool bus_error;
    } physical;
    
    struct {
        uaecptr addr;
        uae_u32 fc;
        bool valid;
    } logical;
    /* history bit */
    int mru;
} MMU030_ATC_LINE;


/* MMU struct for 68030 */
struct {
    
    /* Translation tables */
    struct {
        struct {
            uae_u32 mask;
            uae_u8 shift;
        } table[4];
        
        struct {
            uae_u32 mask;
            uae_u8 size;
        } page;
        
        uae_u8 init_shift;
        uae_u8 last_table;
    } translation;
    
    /* Transparent translation */
    struct {
        TT_info tt0;
        TT_info tt1;
    } transparent;
    
    /* Address translation cache */
    MMU030_ATC_LINE atc[ATC030_NUM_ENTRIES];
    
    /* Condition */
    bool enabled;
    uae_u16 status;
} mmu030;



/* MMU Status Register
 *
 * ---x ---x x-xx x---
 * reserved (all 0)
 *
 * x--- ---- ---- ----
 * bus error
 *
 * -x-- ---- ---- ----
 * limit violation
 *
 * --x- ---- ---- ----
 * supervisor only
 *
 * ---- x--- ---- ----
 * write protected
 *
 * ---- -x-- ---- ----
 * invalid
 *
 * ---- --x- ---- ----
 * modified
 *
 * ---- ---- -x-- ----
 * transparent access
 *
 * ---- ---- ---- -xxx
 * number of levels (number of tables accessed during search)
 *
 */

#define MMUSR_BUS_ERROR         0x8000
#define MMUSR_LIMIT_VIOLATION   0x4000
#define MMUSR_SUPER_VIOLATION   0x2000
#define MMUSR_WRITE_PROTECTED   0x0800
#define MMUSR_INVALID           0x0400
#define MMUSR_MODIFIED          0x0200
#define MMUSR_TRANSP_ACCESS     0x0040
#define MMUSR_NUM_LEVELS_MASK   0x0007


/* This function builds the function code from
 * the parameters super and data */
/* TODO: check function code handling! */
uae_u8 mmu030_get_fc(bool super, bool data) {
    return (super ? 4 : 0) | (data ? 1 : 2);
}


/* -- MMU instructions -- */
static const TCHAR *mmu30regs[] = { "TCR", "", "SRP", "CRP", "", "", "", "" };


void mmu_op30_pmove (uaecptr pc, uae_u32 opcode, uae_u16 next, uaecptr extra)
{
	int preg = (next >> 10) & 31;
	int rw = (next >> 9) & 1;
	int fd = (next >> 8) & 1;
	const TCHAR *reg = NULL;
	uae_u32 otc = tc_030;
	int siz;
    
	switch (preg)
	{
        case 0x10: // TC
            reg = "TC";
            siz = 4;
            if (rw)
                x_put_long (extra, tc_030);
            else {
                tc_030 = x_get_long (extra);
                mmu030_decode_tc(tc_030);
            }
            break;
        case 0x12: // SRP
            reg = "SRP";
            siz = 8;
            if (rw) {
                x_put_long (extra, srp_030 >> 32);
                x_put_long (extra + 4, srp_030);
            } else {
                srp_030 = (uae_u64)x_get_long (extra) << 32;
                srp_030 |= x_get_long (extra + 4);
                mmu030_decode_rp(srp_030);
            }
            break;
        case 0x13: // CRP
            reg = "CRP";
            siz = 8;
            if (rw) {
                x_put_long (extra, crp_030 >> 32);
                x_put_long (extra + 4, crp_030);
            } else {
                crp_030 = (uae_u64)x_get_long (extra) << 32;
                crp_030 |= x_get_long (extra + 4);
                mmu030_decode_rp(crp_030);
            }
            break;
        case 0x18: // MMUSR
            reg = "MMUSR";
            siz = 2;
            if (rw)
                x_put_word (extra, mmusr_030);
            else
                mmusr_030 = x_get_word (extra);
            break;
        case 0x02: // TT0
            reg = "TT0";
            siz = 4;
            if (rw)
                x_put_long (extra, tt0_030);
            else {
                tt0_030 = x_get_long (extra);
                mmu030.transparent.tt0 = mmu030_decode_tt(tt0_030);
            }
            break;
        case 0x03: // TT1
            reg = "TT1";
            siz = 4;
            if (rw)
                x_put_long (extra, tt1_030);
            else {
                tt1_030 = x_get_long (extra);
                mmu030.transparent.tt1 = mmu030_decode_tt(tt1_030);
            }
            break;
	}
    
	if (!reg) {
		write_log ("Bad PMOVE at %08x\n",m68k_getpc());
		op_illg (opcode);
		return;
	}
    
    if (!fd && !rw && !(preg==0x18)) {
        write_log("PMOVE: Flush ATC\n");
        mmu030_flush_atc_all();
    }
    
#if MMUOP_DEBUG > 0
	{
		uae_u32 val;
		if (siz == 8) {
			uae_u32 val2 = x_get_long (extra);
			val = x_get_long (extra + 4);
			if (rw)
				write_log ("PMOVE %s,%08X%08X", reg, val2, val);
			else
				write_log ("PMOVE %08X%08X,%s", val2, val, reg);
		} else {
			if (siz == 4)
				val = x_get_long (extra);
			else
				val = x_get_word (extra);
			if (rw)
				write_log ("PMOVE %s,%08X", reg, val);
			else
				write_log ("PMOVE %08X,%s", val, reg);
		}
		write_log (" PC=%08X\n", pc);
	}
#endif
    
}

void mmu_op30_ptest (uaecptr pc, uae_u32 opcode, uae_u16 next, uaecptr extra)
{
#if MMUOP_DEBUG > 0
	TCHAR tmp[10];
    
	tmp[0] = 0;
	if ((next >> 8) & 1)
		_stprintf (tmp, ",A%d", (next >> 4) & 15);
	write_log ("PTEST%c %02X,%08X,#%X%s PC=%08X\n",
               ((next >> 9) & 1) ? 'W' : 'R', (next & 15), extra, (next >> 10) & 7, tmp, pc);
#endif
	mmusr_030 = 0;
    
    int level = (next&0x1C00)>>10;
    int rw = (next >> 9) & 1;
    int a = (next >> 8) & 1;
    int areg = (next&0xE0)>>4;
    uae_u32 fc = mmu_op30_helper_get_fc(next);
        
    bool write = rw ? false : true;
    bool super = (fc&0x4) ? true : false;
    bool data = (fc&0x1) ? true : false;

    uae_u32 ret = 0;
    
    /* TODO: implement this - datasheet says:
     * "When the instruction specifies an address translation cache search
     *  with an address register operand, the MC68030 takes an F-line
     *  unimplemented instruction exception."
     */
    if (!level && a) { /* correct ? */
        /* TODO: F-line unimplemented instruction exception */
    }
    
    if (!level) {
        mmu030_ptest_atc_search(extra, fc, write);
    } else {
        ret = mmu030_table_search(extra, super, data, write, level);
        if (a) {
            m68k_areg (regs, areg) = ret;
        }
    }
    mmusr_030 = mmu030.status;
    
    
    write_log("PTEST: %08X, fc = %i, level = %i, ",
              extra, fc, level);
    if (a) {
        write_log("return descriptor to register A%i\n", areg);
    } else {
        write_log("do not return descriptor\n");
    }
    write_log("PTEST status: %04X, B = %i, L = %i, S = %i, W = %i, I = %i, M = %i, T = %i, N = %i\n",
              mmusr_030, (mmusr_030&MMUSR_BUS_ERROR)?1:0, (mmusr_030&MMUSR_LIMIT_VIOLATION)?1:0,
              (mmusr_030&MMUSR_SUPER_VIOLATION)?1:0, (mmusr_030&MMUSR_WRITE_PROTECTED)?1:0,
              (mmusr_030&MMUSR_INVALID)?1:0, (mmusr_030&MMUSR_MODIFIED)?1:0,
              (mmusr_030&MMUSR_TRANSP_ACCESS)?1:0, mmusr_030&MMUSR_NUM_LEVELS_MASK);
}

void mmu_op30_pload (uaecptr pc, uae_u32 opcode, uae_u16 next, uaecptr extra)
{
#if MMUOP_DEBUG > 0
    write_log ("PLOAD%c PC=%08X\n", ((next >> 9) & 1) ? 'W' : 'R', pc);
#endif
    
    int rw = (next >> 9) & 1;
    uae_u32 fc = mmu_op30_helper_get_fc(next);
    
    bool write = rw ? false : true;
    bool super = (fc&0x4) ? true : false;
    bool data = (fc&0x1) ? true : false;

    write_log ("PLOAD%c: Create ATC entry for %08X, FC = %i\n", write?'W':'R', extra, fc);

    mmu030_flush_atc_page(extra);
    mmu030_table_search(extra, super, data, write, 0);
}

void mmu_op30_pflush (uaecptr pc, uae_u32 opcode, uae_u16 next, uaecptr extra)
{
    write_log ("PFLUSH PC=%08X, NEXT=%04X, EXTRA=%08X\n", pc, next, extra);

    uae_u16 mode = (next&0x1C00)>>10;
    uae_u32 fc_mask = (uae_u32)(next&0x00E0)>>5;
    uae_u32 fc = mmu_op30_helper_get_fc(next);
        
    switch (mode) {
        case 0x1:
            write_log("PFLUSH: Flush all entries\n");
            mmu030_flush_atc_all();
            break;
        case 0x4:
            write_log("PFLUSH: Flush by function code only\n");
            write_log("PFLUSH: function code = %02X, mask = %02X\n", fc, fc_mask);
            mmu030_flush_atc_fc(fc&fc_mask);
            break;
        case 0x6:
            write_log("PFLUSH: Flush by function code and effective address\n");
            write_log("PFLUSH: function code = %02X, mask = %02X, addr = %08X\n", fc, fc_mask, extra);
            mmu030_flush_atc_page_fc(extra, fc&fc_mask);
            break;
            
        default:
            write_log("PFLUSH ERROR: bad mode! (%i)\n",mode);
            break;
    }
}

/* -- Helper function for MMU instructions -- */
uae_u32 mmu_op30_helper_get_fc(uae_u16 next) {
    switch (next&0x0018) {
        case 0x0010:
            return (next&0x7);
        case 0x0008:
            return (m68k_dreg(regs, next&0x7)&0x7);
        case 0x0000:
            if (next&1) {
                return regs.dfc;
            } else {
                return regs.sfc;
            }
        default:
            write_log("MMU_OP30 ERROR: bad fc source! (%04X)\n",next&0x0018);
            return 0;
    }
}


/* -- ATC flushing functions -- */

/* This function flushes ATC entries depending on their function code */
void mmu030_flush_atc_fc(uae_u8 function_code) {
    int i;
    for (i=0; i<ATC030_NUM_ENTRIES; i++) {
        if (((mmu030.atc[i].logical.fc&function_code)==function_code) && /* TODO: check this */
            mmu030.atc[i].logical.valid) {
            mmu030.atc[i].logical.valid = false;
            write_log("ATC: Flushing %08X\n", mmu030.atc[i].physical.addr);
        }
    }
}

/* This function flushes ATC entries depending on their logical address
 * and their function code */
void mmu030_flush_atc_page_fc(uaecptr logical_addr, uae_u8 function_code) {
    int i;
    for (i=0; i<ATC030_NUM_ENTRIES; i++) {
        if (((mmu030.atc[i].logical.fc&function_code)==function_code) && /* TODO: check this */
            (mmu030.atc[i].logical.addr == logical_addr) &&
            mmu030.atc[i].logical.valid) {
            mmu030.atc[i].logical.valid = false;
            write_log("ATC: Flushing %08X\n", mmu030.atc[i].physical.addr);
        }
    }
}

/* This function flushes ATC entries depending on their logical address */
void mmu030_flush_atc_page(uaecptr logical_addr) {
    int i;
    for (i=0; i<ATC030_NUM_ENTRIES; i++) {
        if ((mmu030.atc[i].logical.addr == logical_addr) &&
            mmu030.atc[i].logical.valid) {
            mmu030.atc[i].logical.valid = false;
            write_log("ATC: Flushing %08X\n", mmu030.atc[i].physical.addr);
        }
    }
}

/* This function flushes all ATC entries */
void mmu030_flush_atc_all(void) {
    write_log("ATC: Flushing all entries\n");
    int i;
    for (i=0; i<ATC030_NUM_ENTRIES; i++) {
        mmu030.atc[i].logical.valid = false;
    }
}


/* Transparent Translation Registers (TT0 and TT1)
 *
 * ---- ---- ---- ---- -xxx x--- x--- x---
 * reserved, must be 0
 *
 * ---- ---- ---- ---- ---- ---- ---- -xxx
 * function code mask (FC bits to be ignored)
 *
 * ---- ---- ---- ---- ---- ---- -xxx ----
 * function code base (FC value for transparent block)
 *
 * ---- ---- ---- ---- ---- ---x ---- ----
 * 0 = r/w field used, 1 = read and write is transparently translated
 *
 * ---- ---- ---- ---- ---- --x- ---- ----
 * r/w field: 0 = write ..., 1 = read access transparent
 *
 * ---- ---- ---- ---- ---- -x-- ---- ----
 * cache inhibit: 0 = caching allowed, 1 = caching inhibited
 *
 * ---- ---- ---- ---- x--- ---- ---- ----
 * 0 = transparent translation enabled disabled, 1 = enabled
 *
 * ---- ---- xxxx xxxx ---- ---- ---- ----
 * logical address mask
 *
 * xxxx xxxx ---- ---- ---- ---- ---- ----
 * logical address base
 *
 */


#define TT_FC_MASK      0x00000003
#define TT_FC_BASE      0x00000030
#define TT_RWM          0x00000100
#define TT_RW           0x00000200
#define TT_CI           0x00000400
#define TT_ENABLE       0x00008000

#define TT_ADDR_MASK    0x00FF0000
#define TT_ADDR_BASE    0xFF000000

/* TT comparision results */
#define TT_NO_MATCH	0x1
#define TT_OK_MATCH	0x2
#define TT_NO_READ  0x4
#define TT_NO_WRITE	0x8

TT_info mmu030_decode_tt(uae_u32 TT) {
    
    TT_info ret;
    
    uae_u8 tt_fc_mask = TT & TT_FC_MASK;
    uae_u8 tt_fc_base = (TT & TT_FC_BASE) >> 4;
    
    uae_u8 fc_translate_transparent = tt_fc_base | tt_fc_mask;
    
    uae_u32 tt_addr_mask = (TT & TT_ADDR_MASK) << 8;
    uae_u32 tt_addr_base = TT & TT_ADDR_BASE;
    
    uae_u32 translate_transparent = tt_addr_base | tt_addr_mask | 0x00FFFFFF;
    
#if MMU030_REG_DBG_MSG /* enable or disable debugging messages */
    write_log("\n");
    write_log("TRANSPARENT TRANSLATION: %08X\n", TT);
    write_log("\n");
    
    write_log("TT: transparent translation %s\n", (TT&TT_ENABLE) ? "enabled" : "disabled");
    write_log("TT: caching %s\n", (TT&TT_CI) ? "inhibited" : "enabled");
    write_log("TT: read-modify-write ");
    if (TT&TT_RWM) {
        write_log("enabled\n");
    } else {
        write_log("disabled (%s only)\n", (TT&TT_RW) ? "read" : "write");
    }
    write_log("\n");
    write_log("TT: function code mask: %i\n", tt_fc_mask);
    write_log("TT: function code base: %i\n", tt_fc_base);
    write_log("\n");
    write_log("TT: address mask: %08X\n", tt_addr_mask);
    write_log("TT: address base: %08X\n", tt_addr_base);
    write_log("\n");
    write_log("TT: translate via MMU if address&%08X or fc&%02X\n",
              ~translate_transparent, ~fc_translate_transparent);
    write_log("\n");
#endif
    
    ret.tt_fcmask = ~fc_translate_transparent;
    ret.tt_addrmask = ~translate_transparent;   /* use like: translate transparent if:
                                                 * addr & (~translate_transparent) == 0 */
    return ret;
}

/* This function compares the address with both transparent
 * translation registers and returns the result */
int mmu030_match_ttr(uaecptr addr, bool super, bool data, bool write)
{
    int tt0, tt1;

    bool cache_inhibit = false; /* TODO: pass to memory access function */
    
    tt0 = mmu030_do_match_ttr(tt0_030, mmu030.transparent.tt0, addr, super, data, write);
    if (tt0&TT_OK_MATCH) {
        cache_inhibit = (tt0_030&TT_CI) ? true : false;
    }
    tt1 = mmu030_do_match_ttr(tt1_030, mmu030.transparent.tt1, addr, super, data, write);
    if (tt1&TT_OK_MATCH) {
        if (!cache_inhibit) {
            cache_inhibit = (tt1_030&TT_CI) ? true : false;
        }
    }
    
    return (tt0|tt1);
}

/* This function checks if an address matches a transparent
 * translation register */

/* FIXME:
 * If !(tt&TT_RMW) neither the read nor the write portion
 * of a read-modify-write cycle is transparently translated! */

int mmu030_do_match_ttr(uae_u32 tt, TT_info masks, uaecptr addr, bool super, bool data, bool write)
{
	if (tt & TT_ENABLE)	{	/* transparent translation enabled */

        uae_u8 fc = mmu030_get_fc(super, data);
        
        /* Compare function code with mask */
#if 0
        if (!((fc&masks.tt_fcmask)&0x7)) {
#endif
            /* Compare address with mask */
            if (!(addr&masks.tt_addrmask)) {
                if (tt&TT_RWM) {  /* r/w field disabled */
                    return TT_OK_MATCH;
                } else {
                    if (tt&TT_RW) { /* read access transparent */
                        return write ? TT_NO_WRITE : TT_OK_MATCH;
                    } else {        /* write access transparent */
                        return write ? TT_OK_MATCH : TT_NO_READ; /* TODO: check this! */
                    }
                }
            }
		}
#if 0
	}
#endif
	return TT_NO_MATCH;
}



/* Translation Control Register:
 *
 * x--- ---- ---- ---- ---- ---- ---- ----
 * translation: 1 = enable, 0 = disable
 *
 * ---- --x- ---- ---- ---- ---- ---- ----
 * supervisor root: 1 = enable, 0 = disable
 *
 * ---- ---x ---- ---- ---- ---- ---- ----
 * function code lookup: 1 = enable, 0 = disable
 *
 * ---- ---- xxxx ---- ---- ---- ---- ----
 * page size:
 * 1000 = 256 bytes
 * 1001 = 512 bytes
 * 1010 =  1 kB
 * 1011 =  2 kB
 * 1100 =  4 kB
 * 1101 =  8 kB
 * 1110 = 16 kB
 * 1111 = 32 kB
 *
 * ---- ---- ---- xxxx ---- ---- ---- ----
 * initial shift
 *
 * ---- ---- ---- ---- xxxx ---- ---- ----
 * number of bits for table index A
 *
 * ---- ---- ---- ---- ---- xxxx ---- ----
 * number of bits for table index B
 *
 * ---- ---- ---- ---- ---- ---- xxxx ----
 * number of bits for table index C
 *
 * ---- ---- ---- ---- ---- ----- ---- xxxx
 * number of bits for table index D
 *
 */


#define TC_ENABLE_TRANSLATION   0x80000000
#define TC_ENABLE_SUPERVISOR    0x02000000
#define TC_ENABLE_FCL           0x01000000

#define TC_PS_MASK              0x00F00000
#define TC_IS_MASK              0x000F0000

#define TC_TIA_MASK             0x0000F000
#define TC_TIB_MASK             0x00000F00
#define TC_TIC_MASK             0x000000F0
#define TC_TID_MASK             0x0000000F


void mmu030_decode_tc(uae_u32 TC) {
        
    /* Set MMU condition */    
    if (TC & TC_ENABLE_TRANSLATION) {
        mmu030.enabled = true;
        write_log("MMU enabled\n");
    } else {
        mmu030.enabled = false;
        write_log("MMU disabled\n");
        return;
    }
    
    /* Note: 0 = Table A, 1 = Table B, 2 = Table C, 3 = Table D */
    int i, j;
    uae_u8 TI_bits[4] = {0,0,0,0};

    /* Reset variables before extracting new values from TC */
    for (i = 0; i < 4; i++) {
        mmu030.translation.table[i].mask = 0;
        mmu030.translation.table[i].shift = 0;
    }
    mmu030.translation.page.mask = 0;
    
    
    /* Extract initial shift and page size values from TC register */
    mmu030.translation.page.size = (TC & TC_PS_MASK) >> 20;
    mmu030.translation.init_shift = (TC & TC_IS_MASK) >> 16;
    
    if (mmu030.translation.page.size<8) {
        write_log("MMU Error: Bad value in TC register! (bad page size: %i byte)\n",
                  1<<mmu030.translation.page.size);
        /* TODO: perform MMU configuration exception */
    }
    /* Build the page mask */
    for (i = 0; i < mmu030.translation.page.size; i++) {
        mmu030.translation.page.mask |= (1<<i);
    }

    
    /* Calculate masks and shifts for later extracting table indices
     * from logical addresses using: index = (addr&mask)>>shift */
    
    /* Get number of bits for each table index */
    for (i = 0; i < 4; i++) {
        j = (3-i)*4;
        TI_bits[i] = (TC >> j) & 0xF;
    }

    /* Calculate masks and shifts for each table */
    mmu030.translation.last_table = 0;
    uae_u8 shift = 32 - mmu030.translation.init_shift;
    for (i = 0; (i < 4) && TI_bits[i]; i++) {
        /* Get the shift */
        shift -= TI_bits[i];
        mmu030.translation.table[i].shift = shift;
        /* Build the mask */
        for (j = 0; j < TI_bits[i]; j++) {
            mmu030.translation.table[i].mask |= (1<<(mmu030.translation.table[i].shift + j));
        }
        /* Update until reaching the last table */
        mmu030.translation.last_table = i;
    }
    
#if MMU030_REG_DBG_MSG
    /* At least one table has to be defined using at least
     * 1 bit for the index. At least 2 bits are necessary 
     * if there is no second table. If these conditions are
     * not met, it will automatically lead to a sum <32
     * and cause an exception (see below). */
    if (!TI_bits[0]) {
        write_log("MMU Error: Bad value in TC register! (no first table index defined)\n");
    } else if ((TI_bits[0]<2) && !TI_bits[1]) {
        write_log("MMU Error: Bad value in TC register! (no second table index defined and)\n");
        write_log("MMU Error: Bad value in TC register! (only 1 bit for first table index)\n");
    }
#endif
    
    /* TI fields are summed up until a zero field is reached (see above
     * loop). The sum of all TI field values plus page size and initial
     * shift has to be 32: IS + PS + TIA + TIB + TIC + TID = 32 */
    if ((shift-mmu030.translation.page.size)!=0) {
        write_log("MMU Error: Bad value in TC register! (bad sum)\n");
        /* TODO: perform MMU configuration exception */
    }
    
#if MMU030_REG_DBG_MSG /* enable or disable debugging output */
    write_log("\n");
    write_log("TRANSLATION CONTROL: %08X\n", TC);
    write_log("\n");
    write_log("TC: translation %s\n", (TC&TC_ENABLE_TRANSLATION ? "enabled" : "disabled"));
    write_log("TC: supervisor root pointer %s\n", (TC&TC_ENABLE_SUPERVISOR ? "enabled" : "disabled"));
    write_log("TC: function code lookup %s\n", (TC&TC_ENABLE_FCL ? "enabled" : "disabled"));
    write_log("\n");
    
    write_log("TC: Initial Shift: %i\n", mmu030.translation.init_shift);
    write_log("TC: Page Size: %i byte\n", (1<<mmu030.translation.page.size));
    write_log("\n");
    
    for (i = 0; i <= mmu030.translation.last_table; i++) {
        write_log("TC: Table %c: mask = %08X, shift = %i\n", table_letter[i], mmu030.translation.table[i].mask, mmu030.translation.table[i].shift);
    }

    write_log("TC: Page:    mask = %08X\n", mmu030.translation.page.mask);
    write_log("\n");

    write_log("TC: Last Table: %c\n", table_letter[mmu030.translation.last_table]);
    write_log("\n");
#endif
}



/* Root Pointer Registers (SRP and CRP)
 *
 * ---- ---- ---- ---- xxxx xxxx xxxx xx-- ---- ---- ---- ---- ---- ---- ---- xxxx
 * reserved, must be 0
 *
 * ---- ---- ---- ---- ---- ---- ---- ---- xxxx xxxx xxxx xxxx xxxx xxxx xxxx ----
 * table A address
 *
 * ---- ---- ---- ---- ---- ---- ---- --xx ---- ---- ---- ---- ---- ---- ---- ----
 * descriptor type
 *
 * -xxx xxxx xxxx xxxx ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
 * limit
 *
 * x--- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
 * 0 = upper limit, 1 = lower limit
 *
 */


#define RP_ADDR_MASK    (UVAL64(0x00000000FFFFFFF0))
#define RP_DESCR_MASK   (UVAL64(0x0000000300000000))
#define RP_LIMIT_MASK   (UVAL64(0x7FFF000000000000))
#define RP_LOWER_MASK   (UVAL64(0x8000000000000000))

void mmu030_decode_rp(uae_u64 RP) {
    
    uae_u8 descriptor_type = (RP & RP_DESCR_MASK) >> 32;
    if (!descriptor_type) { /* If descriptor type is invalid */
        /* TODO: perform an MMU configuration exception */
    }

#if MMU030_REG_DBG_MSG /* enable or disable debugging output */
    uae_u32 table_limit = (RP & RP_LIMIT_MASK) >> 48;
    uae_u32 first_addr = (RP & RP_ADDR_MASK);
    
    write_log("\n");
    write_log("ROOT POINTER: %08X%08X\n", (uae_u32)(RP>>32)&0xFFFFFFFF, (uae_u32)(RP&0xFFFFFFFF));
    write_log("\n");
    
    write_log("RP: descriptor type = %i ", descriptor_type);
    switch (descriptor_type) {
        case 0:
            write_log("(invalid descriptor)\n");
            break;
        case 1:
            write_log("(early termination page descriptor)\n");
            break;
        case 2:
            write_log("(valid 4 byte descriptor)\n");
            break;
        case 3:
            write_log("(valid 8 byte descriptor)\n");
            break;
    }
    
    write_log("RP: %s limit = %i\n", (RP&RP_LOWER_MASK) ? "lower" : "upper", table_limit);
    
    write_log("RP: first table address = %08X\n", first_addr);
    write_log("\n");
#endif
}



/* Descriptors */

#define DESCR_TYPE_MASK         0x00000003

#define DESCR_TYPE_INVALID      0 /* all tables */

#define DESCR_TYPE_EARLY_TERM   1 /* all but lowest level table */
#define DESCR_TYPE_PAGE         1 /* only lowest level table */
#define DESCR_TYPE_VALID4       2 /* all but lowest level table */
#define DESCR_TYPE_INDIRECT4    2 /* only lowest level table */
#define DESCR_TYPE_VALID8       3 /* all but lowest level table */
#define DESCR_TYPE_INDIRECT8    3 /* only lowest level table */

#define DESCR_TYPE_VALID_MASK       0x2 /* all but lowest level table */
#define DESCR_TYPE_INDIRECT_MASK    0x2 /* only lowest level table */


/* Short format (4 byte):
 *
 * ---- ---- ---- ---- ---- ---- ---- --xx
 * descriptor type:
 * 0 = invalid
 * 1 = page descriptor (early termination)
 * 2 = valid (4 byte)
 * 3 = valid (8 byte)
 *
 *
 * table descriptor:
 * ---- ---- ---- ---- ---- ---- ---- -x--
 * write protect
 *
 * ---- ---- ---- ---- ---- ---- ---- x---
 * update
 *
 * xxxx xxxx xxxx xxxx xxxx xxxx xxxx ----
 * table address
 *
 *
 * (early termination) page descriptor:
 * ---- ---- ---- ---- ---- ---- ---- -x--
 * write protect
 *
 * ---- ---- ---- ---- ---- ---- ---- x---
 * update
 *
 * ---- ---- ---- ---- ---- ---- ---x ----
 * modified
 *
 * ---- ---- ---- ---- ---- ---- -x-- ----
 * cache inhibit
 *
 * ---- ---- ---- ---- ---- ---- x-x- ----
 * reserved (must be 0)
 *
 * xxxx xxxx xxxx xxxx xxxx xxxx ---- ----
 * page address
 *
 *
 * indirect descriptor:
 * xxxx xxxx xxxx xxxx xxxx xxxx xxxx xx--
 * descriptor address
 *
 */

#define DESCR_WP       0x00000004
#define DESCR_U        0x00000008
#define DESCR_M        0x00000010 /* only last level table */
#define DESCR_CI       0x00000040 /* only last level table */

#define DESCR_TD_ADDR_MASK 0xFFFFFFF0
#define DESCR_PD_ADDR_MASK 0xFFFFFF00
#define DESCR_ID_ADDR_MASK 0xFFFFFFFC


/* Long format (8 byte):
 *
 * ---- ---- ---- ---- ---- ---- ---- --xx | ---- ---- ---- ---- ---- ---- ---- ----
 * descriptor type:
 * 0 = invalid
 * 1 = page descriptor (early termination)
 * 2 = valid (4 byte)
 * 3 = valid (8 byte)
 *
 *
 * table desctriptor:
 * ---- ---- ---- ---- ---- ---- ---- -x-- | ---- ---- ---- ---- ---- ---- ---- ----
 * write protect
 *
 * ---- ---- ---- ---- ---- ---- ---- x--- | ---- ---- ---- ---- ---- ---- ---- ----
 * update
 *
 * ---- ---- ---- ---- ---- ---- xxxx ---- | ---- ---- ---- ---- ---- ---- ---- ----
 * reserved (must be 0)
 *
 * ---- ---- ---- ---- ---- ---x ---- ---- | ---- ---- ---- ---- ---- ---- ---- ----
 * supervisor
 *
 * ---- ---- ---- ---- xxxx xxx- ---- ---- | ---- ---- ---- ---- ---- ---- ---- ----
 * reserved (must be 1111 110)
 *
 * -xxx xxxx xxxx xxxx ---- ---- ---- ---- | ---- ---- ---- ---- ---- ---- ---- ----
 * limit
 *
 * x--- ---- ---- ---- ---- ---- ---- ---- | ---- ---- ---- ---- ---- ---- ---- ----
 * 0 = upper limit, 1 = lower limit
 *
 * ---- ---- ---- ---- ---- ---- ---- ---- | xxxx xxxx xxxx xxxx xxxx xxxx xxxx ----
 * table address
 *
 *
 * (early termination) page descriptor:
 * ---- ---- ---- ---- ---- ---- ---- -x-- | ---- ---- ---- ---- ---- ---- ---- ----
 * write protect
 *
 * ---- ---- ---- ---- ---- ---- ---- x--- | ---- ---- ---- ---- ---- ---- ---- ----
 * update
 *
 * ---- ---- ---- ---- ---- ---- ---x ---- | ---- ---- ---- ---- ---- ---- ---- ----
 * modified
 *
 * ---- ---- ---- ---- ---- ---- -x-- ---- | ---- ---- ---- ---- ---- ---- ---- ----
 * cache inhibit
 *
 * ---- ---- ---- ---- ---- ---x ---- ---- | ---- ---- ---- ---- ---- ---- ---- ----
 * supervisor
 *
 * ---- ---- ---- ---- ---- ---- x-x- ---- | ---- ---- ---- ---- ---- ---- ---- ----
 * reserved (must be 0)
 *
 * ---- ---- ---- ---- xxxx xxx- ---- ---- | ---- ---- ---- ---- ---- ---- ---- ----
 * reserved (must be 1111 110)
 *
 * -xxx xxxx xxxx xxxx ---- ---- ---- ---- | ---- ---- ---- ---- ---- ---- ---- ----
 * limit (only used with early termination page decriptor)
 *
 * x--- ---- ---- ---- ---- ---- ---- ---- | ---- ---- ---- ---- ---- ---- ---- ----
 * 0 = upper limit, 1 = lower limit (only used with early termination page descriptor)
 *
 * ---- ---- ---- ---- ---- ---- ---- ---- | xxxx xxxx xxxx xxxx xxxx xxxx ---- ----
 * page address
 *
 *
 * indirect descriptor:
 * ---- ---- ---- ---- ---- ---- ---- ---- | xxxx xxxx xxxx xxxx xxxx xxxx xxxx xx--
 * descriptor address
 *
 */

/* only for long descriptors */
#define DESCR_S        0x00000100

#define DESCR_LIMIT_MASK   0x7FFF0000
#define DESCR_LOWER_MASK   0x80000000



/* This functions searches through the translation tables. It can be used 
 * for PTEST (levels 1 to 7). Using level 0 creates an ATC entry. */

uae_u32 mmu030_table_search(uaecptr addr, bool super, bool data, bool write, int level) {
    /* During table walk up to 7 different descriptors are used:
     * root pointer, descriptors fetched from function code lookup table,
     * tables A, B, C and D and one indirect descriptor */
    uae_u32 descr[7][2];
    uae_u32 descr_type;
    uaecptr table_addr = 0;
    uaecptr page_addr = 0;
    uaecptr indirect_addr = 0;
    uae_u32 fc = mmu030_get_fc(super, data);
    uae_u32 table_index = 0;
    uae_u32 limit = 0;
    bool write_protect = false;
    bool cache_inhibit = false;
    bool descr_modified = false;
    
    mmu030.status = 0; /* Reset status */
    
    /* Initial values for condition variables.
     * Note: Root pointer is long descriptor. */
    int addr_position = 1;
    int next_size = 0;
    int descr_size = 8;
    int descr_num = 0;
    bool early_termination = false;
    int t,i;
    
    /* Clear bus error flag, so we only detect our own bus errors.
     * Restore bus error flag, before returning from this function!
     */
    bool restore_be = (regs.spcflags&SPCFLAG_BUSERROR) ? true : false;
    regs.spcflags &= ~SPCFLAG_BUSERROR;
    
    /* Use super user root pointer if enabled in TC register and access is in
     * super user mode, else use cpu root pointer. */
    if ((tc_030&TC_ENABLE_SUPERVISOR) && super) {
        descr[descr_num][0] = (srp_030>>32)&0xFFFFFFFF;
        descr[descr_num][1] = srp_030&0xFFFFFFFF;
        write_log("Supervisor Root Pointer: %08X%08X\n",descr[descr_num][0],descr[descr_num][1]);
    } else {
        descr[0][0] = (crp_030>>32)&0xFFFFFFFF;
        descr[0][1] = crp_030&0xFFFFFFFF;
        write_log("CPU Root Pointer: %08X%08X\n",descr[descr_num][0],descr[descr_num][1]);
    }
    
#define RP_ZERO_BITS 0x0000FFFC /* These bits in upper longword of RP must be 0 */
    
    if (descr[descr_num][0]&RP_ZERO_BITS) {
        write_log("MMU Warning: Root pointer reserved bits are non-zero!\n");
        descr[descr_num][0] &= (~RP_ZERO_BITS);
    }
    
    /* Check descriptor type of root pointer */
    descr_type = descr[descr_num][0]&DESCR_TYPE_MASK;
    switch (descr_type) {
        case DESCR_TYPE_INVALID:
            write_log("Fatal error: Root pointer is invalid descriptor!\n");
            goto stop_search;
        case DESCR_TYPE_EARLY_TERM:
            write_log("Root pointer is early termination page descriptor.\n");
            early_termination = true;
            t = 0; /* Use index of table A for limit check */
            goto handle_page_descriptor;
        case DESCR_TYPE_VALID4:
            next_size = 4;
            break;
        case DESCR_TYPE_VALID8:
            next_size = 8;
            break;
    }
    
    /* If function code lookup is enabled in TC register use function code as
     * index for top level table, limit check not required */
    
    if (tc_030&TC_ENABLE_FCL) {
        write_log("Function code lookup enabled, FC = %i\n", fc);

        addr_position = (descr_size==4) ? 0 : 1;
        table_addr = descr[descr_num][addr_position]&DESCR_TD_ADDR_MASK;
        table_index = fc; /* table index is function code */
        write_log("Table FCL at %08X: index = %i, ",table_addr,table_index);
        
        /* Fetch next descriptor */
        descr_num++;
        if (next_size==4) {
            descr[descr_num][0] = phys_get_long(table_addr+(table_index*next_size));
            write_log("Next descriptor: %08X\n",descr[descr_num][0]);
        } else {
            descr[descr_num][0] = phys_get_long(table_addr+(table_index*next_size));
            descr[descr_num][1] = phys_get_long(table_addr+(table_index*next_size)+4);
            write_log("Next descriptor: %08X%08X\n",descr[descr_num][0],descr[descr_num][1]);
        }
        /* Check if a bus error occured */
        if (regs.spcflags&SPCFLAG_BUSERROR) {
            goto bus_error_read;
        }
        descr_size = next_size;
        
        /* Check descriptor type */
        descr_type = descr[descr_num][0]&DESCR_TYPE_MASK;
        switch (descr_type) {
            case DESCR_TYPE_INVALID:
                write_log("Invalid descriptor!\n");
                /* stop table walk */
                mmu030.status |= MMUSR_INVALID;
                goto stop_search;
            case DESCR_TYPE_EARLY_TERM:
                write_log("Early termination page descriptor!\n");
                early_termination = true;
                t = 0; /* Use index of table A for limit check */
                goto handle_page_descriptor;
            case DESCR_TYPE_VALID4:
                next_size = 4;
                break;
            case DESCR_TYPE_VALID8:
                next_size = 8;
                break;
        }
    }
    
    
    /* Upper level tables */
    for (t = 0; t <= mmu030.translation.last_table; t++) {
        if (descr_num) { /* if not root pointer */
            /* Set the updated bit */
            if (!level && !(descr[descr_num][0]&DESCR_U) && !(mmu030.status&MMUSR_SUPER_VIOLATION)) {
                descr[descr_num][0] |= DESCR_U;
                phys_put_long(table_addr+(table_index*next_size), descr[descr_num][0]);
                /* check if a bus error occured */
                if (regs.spcflags&SPCFLAG_BUSERROR) {
                    goto bus_error_write;
                }
            }
            /* Update status bits */
            if (descr_size==8) {
                if (descr[descr_num][0]&DESCR_S)
                    mmu030.status |= super ? 0 : MMUSR_SUPER_VIOLATION;
            }
            if (descr[descr_num][0]&DESCR_WP) {
                mmu030.status |= (descr[descr_num][0]&DESCR_WP) ? MMUSR_WRITE_PROTECTED : 0;
                write_protect = true;
            }
            
            /* Check if ptest level is reached */
            if (level && (level==descr_num)) {
                goto stop_search;
            }
        }
        
        addr_position = (descr_size==4) ? 0 : 1;
        table_addr = descr[descr_num][addr_position]&DESCR_TD_ADDR_MASK;
        table_index = (addr&mmu030.translation.table[t].mask)>>mmu030.translation.table[t].shift;
        write_log("Table %c at %08X: index = %i, ",table_letter[t],table_addr,table_index);
        
        /* Perform limit check */
        if (descr_size==8) {
            limit = (descr[descr_num][0]&DESCR_LIMIT_MASK)>>16;
            if ((descr[descr_num][0]&DESCR_LOWER_MASK) && (table_index<limit)) {
                mmu030.status |= (MMUSR_LIMIT_VIOLATION|MMUSR_INVALID);
                write_log("limit violation (lower limit %i)\n",limit);
                goto stop_search;
            } else if (table_index>limit) {
                mmu030.status |= (MMUSR_LIMIT_VIOLATION|MMUSR_INVALID);
                write_log("limit violation (upper limit %i)\n",limit);
                goto stop_search;
            }
        }
        
        /* Fetch next descriptor */
        descr_num++;
        if (next_size==4) {
            descr[descr_num][0] = phys_get_long(table_addr+(table_index*next_size));
            write_log("Next descriptor: %08X\n",descr[descr_num][0]);
        } else {
            descr[descr_num][0] = phys_get_long(table_addr+(table_index*next_size));
            descr[descr_num][1] = phys_get_long(table_addr+(table_index*next_size)+4);
            write_log("Next descriptor: %08X%08X\n",descr[descr_num][0],descr[descr_num][1]);
        }
        /* Check if a bus error occured */
        if (regs.spcflags&SPCFLAG_BUSERROR) {
            goto bus_error_read;
        }
        descr_size = next_size;
                
        /* Check descriptor type */
        descr_type = descr[descr_num][0]&DESCR_TYPE_MASK;
        switch (descr_type) {
            case DESCR_TYPE_INVALID:
                write_log("Invalid descriptor!\n");
                /* stop table walk */
                mmu030.status |= MMUSR_INVALID;
                goto stop_search;
            case DESCR_TYPE_EARLY_TERM:
                /* go to last level table handling code */
                if (t<mmu030.translation.last_table) {
                    write_log("Early termination page descriptor!\n");
                    early_termination = true;
                    t++; /* Use index of next table for limit check */
                }
                goto handle_page_descriptor;
            case DESCR_TYPE_VALID4:
                next_size = 4;
                break;
            case DESCR_TYPE_VALID8:
                next_size = 8;
                break;
        }
    }
    
    
    /* Handle indirect descriptor */
    
    /* Check if ptest level is reached */
    if (level && (level==descr_num)) {
        goto stop_search;
    }
    
    addr_position = (descr_size==4) ? 0 : 1;
    indirect_addr = descr[descr_num][addr_position]&DESCR_ID_ADDR_MASK;
    write_log("Page indirect descriptor at %08X: ",indirect_addr);
    
    /* Fetch indirect descriptor */
    descr_num++;
    if (next_size==4) {
        descr[descr_num][0] = phys_get_long(indirect_addr);
        write_log("descr = %08X\n",descr[descr_num][0]);
    } else {
        descr[descr_num][0] = phys_get_long(indirect_addr);
        descr[descr_num][1] = phys_get_long(indirect_addr+4);
        write_log("descr = %08X%08X",descr[descr_num][0],descr[descr_num][1]);
    }
    /* Check if a bus error occured */
    if (regs.spcflags&SPCFLAG_BUSERROR) {
        goto bus_error_read;
    }
    descr_size = next_size;
    
    /* Check descriptor type, only page descriptor is valid */
    descr_type = descr[descr_num][0]&DESCR_TYPE_MASK;
    if (descr_type!=DESCR_TYPE_PAGE) {
        mmu030.status |= MMUSR_INVALID;
        goto stop_search;
    }
            
handle_page_descriptor:
    
    if (descr_num) { /* if not root pointer */
        if (!level && !(mmu030.status&MMUSR_SUPER_VIOLATION)) {
            /* set modified bit */
            if (!(descr[descr_num][0]&DESCR_M) && write && !(mmu030.status&MMUSR_WRITE_PROTECTED)) {
                descr[descr_num][0] |= DESCR_M;
                descr_modified = true;
            }
            /* set updated bit */
            if (!(descr[descr_num][0]&DESCR_U)) {
                descr[descr_num][0] |= DESCR_U;
                descr_modified = true;
            }
            /* write modified descriptor if neccessary */
            if (descr_modified) {
                if (descr_type&DESCR_TYPE_INDIRECT_MASK) {
                    phys_put_long(indirect_addr, descr[descr_num][0]);
                } else {
                    phys_put_long(table_addr+(table_index*descr_size), descr[descr_num][0]);
                }
                if (regs.spcflags&SPCFLAG_BUSERROR) {
                    goto bus_error_write;
                }
            }
        }
        
        if ((descr_size==8) && (descr[descr_num][0]&DESCR_S)) {
            mmu030.status |= super ? 0 : MMUSR_SUPER_VIOLATION;
        }
        
        /* check if caching is inhibited */
        cache_inhibit = descr[descr_num][0]&DESCR_CI ? true : false;
        
        /* check write protection */
        if (descr[descr_num][0]&DESCR_WP) {
            mmu030.status |= (descr[descr_num][0]&DESCR_WP) ? MMUSR_WRITE_PROTECTED : 0;
            write_protect = true;
        }
        /* TODO: check if this is handled at correct point (maybe before updating descr?) */
        mmu030.status |= (descr[descr_num][0]&DESCR_M) ? MMUSR_MODIFIED : 0;
    }
    
    /* Check limit using next index field of logical address.
     * Limit is only checked on early termination. If we are
     * still at root pointer level, only check limit, if FCL
     * is disabled. */
    if (early_termination && (descr_size==8)) {
        if (descr_num || !(tc_030&TC_ENABLE_FCL)) {
            table_index = (addr&mmu030.translation.table[t].mask)>>mmu030.translation.table[t].shift;
            limit = (descr[descr_num][0]&DESCR_LIMIT_MASK)>>16;
            if ((descr[descr_num][0]&DESCR_LOWER_MASK) && (table_index<limit)) {
                mmu030.status |= (MMUSR_LIMIT_VIOLATION|MMUSR_INVALID);
                goto stop_search;
            } else if (table_index>limit) {
                mmu030.status |= (MMUSR_LIMIT_VIOLATION|MMUSR_INVALID);
                goto stop_search;
            }
        }
    }
    
    /* Get page address */
    addr_position = (descr_size==4) ? 0 : 1;
    page_addr = descr[descr_num][addr_position]&DESCR_PD_ADDR_MASK;
    write_log("Page at %08X\n",page_addr);

    goto stop_search;
    
bus_error_write:
    mmu030.status |= (MMUSR_BUS_ERROR|MMUSR_INVALID);
    write_log("Bus error while writing updated descriptor!\n");
    goto stop_search;
    
bus_error_read:
    mmu030.status |= (MMUSR_BUS_ERROR|MMUSR_INVALID);
    write_log("Bus error while fetching descriptor!\n");
    descr_num--;
    
stop_search:    
    /* check if we have to handle ptest */
    if (level) {
        if (mmu030.status&MMUSR_INVALID) {
            /* these bits are undefined, if the I bit is set: */
            mmu030.status &= ~(MMUSR_WRITE_PROTECTED|MMUSR_MODIFIED|MMUSR_SUPER_VIOLATION);
        }
        mmu030.status = (mmu030.status&~MMUSR_NUM_LEVELS_MASK) | descr_num;
        regs.spcflags |= restore_be ? SPCFLAG_BUSERROR : 0;
        /* If root pointer is page descriptor (descr_num 0), return 0 */
        return descr_num ? descr[descr_num][0] : 0;
    }
    
    /* Find an ATC entry to replace */
    /* Search for invalid entry */
    for (i=0; i<ATC030_NUM_ENTRIES; i++) {
        if (!mmu030.atc[i].logical.valid) {
            break;
        }
    }
    /* If there are no invalid entries, replace first entry
     * with history bit not set */
    if (i == ATC030_NUM_ENTRIES) {
        for (i=0; i<ATC030_NUM_ENTRIES; i++) {
            if (!mmu030.atc[i].mru) {
                break;
            }
        }
        write_log("ATC is full. Replacing entry %i\n", i);
    }
    mmu030_atc_handle_history_bit(i);
    
    /* Create ATC entry */
    mmu030.atc[i].logical.addr = addr&0xFFFFFF00; /* field is only 24 bit */
    mmu030.atc[i].logical.fc = fc;
    mmu030.atc[i].logical.valid = true;
    mmu030.atc[i].physical.addr = page_addr; /* already masked to 24 bit */
    if ((mmu030.status&MMUSR_INVALID) || (mmu030.status&MMUSR_SUPER_VIOLATION)) {
        mmu030.atc[i].physical.bus_error = true;
    } else {
        mmu030.atc[i].physical.bus_error = false;
    }
    if (write && !(mmu030.status&MMUSR_SUPER_VIOLATION) && !(mmu030.status&MMUSR_LIMIT_VIOLATION)) {
        mmu030.atc[i].physical.modified = true;
    } else {
        mmu030.atc[i].physical.modified = false;
    }
    mmu030.atc[i].physical.cache_inhibit = cache_inhibit;
    mmu030.atc[i].physical.write_protect = write_protect;
    
    write_log("ATC create entry(%i): logical = %08X, physical = %08X, FC = %i\n", i,
              mmu030.atc[i].logical.addr, mmu030.atc[i].physical.addr,
              mmu030.atc[i].logical.fc);
    write_log("ATC create entry(%i): B = %i, CI = %i, WP = %i, M = %i\n", i,
              mmu030.atc[i].physical.bus_error?1:0,
              mmu030.atc[i].physical.cache_inhibit?1:0,
              mmu030.atc[i].physical.write_protect?1:0,
              mmu030.atc[i].physical.modified?1:0);
    
    regs.spcflags |= restore_be ? SPCFLAG_BUSERROR : 0;
    return 0;
}

/* This function is used for PTEST level 0. */
void mmu030_ptest_atc_search(uaecptr logical_addr, uae_u32 function_code, bool write) {
    int i;
    mmu030.status = 0;
    
    bool super = (function_code&0x4) ? true : false;
    bool data = (function_code&0x1) ? true : false;
        
    if (mmu030_match_ttr(logical_addr, super, data, write)&TT_OK_MATCH) {
        mmu030.status |= MMUSR_TRANSP_ACCESS;
        return;
    }
    
    for (i = 0; i < ATC030_NUM_ENTRIES; i++) {
        if (((mmu030.atc[i].logical.fc&function_code)==function_code) &&
            (mmu030.atc[i].logical.addr == logical_addr) &&
            mmu030.atc[i].logical.valid) {
            break;
        }
    }
    
    if (i==ATC030_NUM_ENTRIES) {
        mmu030.status |= MMUSR_INVALID;
        return;
    }
    
    mmu030.status |= mmu030.atc[i].physical.bus_error ? (MMUSR_BUS_ERROR|MMUSR_INVALID) : 0;
    mmu030.status |= mmu030.atc[i].physical.write_protect ? MMUSR_WRITE_PROTECTED : 0;
    mmu030.status |= mmu030.atc[i].physical.modified ? MMUSR_MODIFIED : 0;
    if (mmu030.status&MMUSR_INVALID) {
        mmu030.status &= ~(MMUSR_WRITE_PROTECTED|MMUSR_MODIFIED);
    }
}



/* Address Translation Cache
 *
 * The ATC uses a pseudo-least-recently-used algorithm to keep track of
 * least recently used entries. They are replaced if the cache is full.
 * An internal history-bit (MRU-bit) is used to identify these entries.
 * If an entry is accessed, its history-bit is set to 1. If after that
 * there are no more entries with zero-bits, all other history-bits are
 * set to 0. When no more invalid entries are in the ATC, the first entry
 * with a zero-bit is replaced.
 *
 *
 * Logical Portion (28 bit):
 * oooo ---- xxxx xxxx xxxx xxxx xxxx xxxx
 * logical address (most significant 24 bit)
 *
 * oooo -xxx ---- ---- ---- ---- ---- ----
 * function code
 *
 * oooo x--- ---- ---- ---- ---- ---- ----
 * valid
 *
 *
 * Physical Portion (28 bit):
 * oooo ---- xxxx xxxx xxxx xxxx xxxx xxxx
 * physical address
 *
 * oooo ---x ---- ---- ---- ---- ---- ----
 * modified
 *
 * oooo --x- ---- ---- ---- ---- ---- ----
 * write protect
 *
 * oooo -x-- ---- ---- ---- ---- ---- ----
 * cache inhibit
 *
 * oooo x--- ---- ---- ---- ---- ---- ----
 * bus error
 *
 */

#define ATC030_MASK         0x0FFFFFFF
#define ATC030_ADDR_MASK    0x00FFFFFF /* after masking shift 8 (<< 8) */

#define ATC030_LOG_FC   0x07000000
#define ATC030_LOG_V    0x08000000

#define ATC030_PHYS_M   0x01000000
#define ATC030_PHYS_WP  0x02000000
#define ATC030_PHYS_CI  0x04000000
#define ATC030_PHYS_BE  0x08000000



void mmu030_put_long_atc(uaecptr addr, uae_u32 val, int l, bool super, bool data) {
    uae_u32 page_index = addr & mmu030.translation.page.mask;
    uae_u32 addr_mask = ~mmu030.translation.page.mask;
    
    uae_u32 physical_addr = mmu030.atc[l].physical.addr&addr_mask;
#if MMU030_ATC_DBG_MSG
    write_log("ATC match(%i): page addr = %08X, index = %08X (lput %08X)\n",
              l, physical_addr, page_index, val);
#endif
    physical_addr += page_index;
    
    if (mmu030.atc[l].physical.bus_error || mmu030.atc[l].physical.write_protect) {
        M68000_BusError(physical_addr, 0);
        return;
    }

    phys_put_long(physical_addr, val);
}

void mmu030_put_word_atc(uaecptr addr, uae_u16 val, int l, bool super, bool data) {
    uae_u32 page_index = addr & mmu030.translation.page.mask;
    uae_u32 addr_mask = ~mmu030.translation.page.mask;
    
    uae_u32 physical_addr = mmu030.atc[l].physical.addr&addr_mask;
#if MMU030_ATC_DBG_MSG
    write_log("ATC match(%i): page addr = %08X, index = %08X (wput %04X)\n",
              l, physical_addr, page_index, val);
#endif
    physical_addr += page_index;
    
    if (mmu030.atc[l].physical.bus_error || mmu030.atc[l].physical.write_protect) {
        M68000_BusError(physical_addr, 0);
        return;
    }

    phys_put_word(physical_addr, val);
}

void mmu030_put_byte_atc(uaecptr addr, uae_u8 val, int l, bool super, bool data) {
    uae_u32 page_index = addr & mmu030.translation.page.mask;
    uae_u32 addr_mask = ~mmu030.translation.page.mask;
    
    uae_u32 physical_addr = mmu030.atc[l].physical.addr&addr_mask;
#if MMU030_ATC_DBG_MSG
    write_log("ATC match(%i): page addr = %08X, index = %08X (bput %02X)\n",
              l, physical_addr, page_index, val);
#endif
    physical_addr += page_index;
    
    if (mmu030.atc[l].physical.bus_error || mmu030.atc[l].physical.write_protect) {
        M68000_BusError(physical_addr, 0);
        return;
    }

    phys_put_byte(physical_addr, val);
}

uae_u32 mmu030_get_long_atc(uaecptr addr, int l, bool super, bool data) {
    uae_u32 page_index = addr & mmu030.translation.page.mask;
    uae_u32 addr_mask = ~mmu030.translation.page.mask;
    
    uae_u32 physical_addr = mmu030.atc[l].physical.addr&addr_mask;
#if MMU030_ATC_DBG_MSG
    write_log("ATC match(%i): page addr = %08X, index = %08X (lget %08X)\n", l,
              physical_addr, page_index, phys_get_long(physical_addr+page_index));
#endif
    physical_addr += page_index;
    
    if (mmu030.atc[l].physical.bus_error) {
        M68000_BusError(physical_addr, 1);
        return 0;
    }

    return phys_get_long(physical_addr);
}

uae_u16 mmu030_get_word_atc(uaecptr addr, int l, bool super, bool data) {
    uae_u32 page_index = addr & mmu030.translation.page.mask;
    uae_u32 addr_mask = ~mmu030.translation.page.mask;
    
    uae_u32 physical_addr = mmu030.atc[l].physical.addr&addr_mask;
#if MMU030_ATC_DBG_MSG
    write_log("ATC match(%i): page addr = %08X, index = %08X (wget %04X)\n", l,
              physical_addr, page_index, phys_get_word(physical_addr+page_index));
#endif
    physical_addr += page_index;
    
    if (mmu030.atc[l].physical.bus_error) {
        M68000_BusError(physical_addr, 1);
        return 0;
    }
    
    return phys_get_word(physical_addr);
}

uae_u8 mmu030_get_byte_atc(uaecptr addr, int l, bool super, bool data) {
    uae_u32 page_index = addr & mmu030.translation.page.mask;
    uae_u32 addr_mask = ~mmu030.translation.page.mask;
    
    uae_u32 physical_addr = mmu030.atc[l].physical.addr&addr_mask;
#if MMU030_ATC_DBG_MSG
    write_log("ATC match(%i): page addr = %08X, index = %08X (bget %02X)\n", l,
              physical_addr, page_index, phys_get_byte(physical_addr+page_index));
#endif
    physical_addr += page_index;
    
    if (mmu030.atc[l].physical.bus_error) {
        M68000_BusError(physical_addr, 1);
        return 0;
    }

    return phys_get_byte(physical_addr);
}


/* This function checks if a certain logical address is in the ATC 
 * by comparing the logical address and function code to the values
 * stored in the ATC entries. If a matching entry is found it sets
 * the history bit and returns the cache index of the entry. */
int mmu030_logical_is_in_atc(uaecptr addr, bool super, bool data, bool write) {
    uaecptr physical_addr = 0;
    uaecptr logical_addr = 0;
    uae_u32 addr_mask = ~mmu030.translation.page.mask;
    uae_u32 page_index = addr & mmu030.translation.page.mask;
    uae_u32 fc = mmu030_get_fc(super, data);
    
    int i, j;
    for (i=0; i<ATC030_NUM_ENTRIES; i++) {
        logical_addr = mmu030.atc[i].logical.addr;
        /* If actual address matches address in ATC */
        if ((addr&addr_mask)==(logical_addr&addr_mask) &&
            ((mmu030.atc[i].logical.fc&fc)==fc) && /* TODO: check this! */
            mmu030.atc[i].logical.valid) {
            /* If M bit is set or access is read, return true
             * else invalidate entry */
            if (mmu030.atc[i].physical.modified || !write) {
                /* Maintain history bit */
                mmu030_atc_handle_history_bit(i);
                return i;
            } else {
                mmu030.atc[i].logical.valid = false;
            }
        }
    }
    return ATC030_NUM_ENTRIES;
}

void mmu030_atc_handle_history_bit(int entry_num) {
    int j;
    mmu030.atc[entry_num].mru = 1;
    for (j=0; j<ATC030_NUM_ENTRIES; j++) {
        if (!mmu030.atc[j].mru)
            break;
    }
    /* If there are no more zero-bits, reset all */
    if (j==ATC030_NUM_ENTRIES) {
        for (j=0; j<ATC030_NUM_ENTRIES; j++) {
            mmu030.atc[j].mru = 0;
        }
        mmu030.atc[entry_num].mru = 1;
        write_log("ATC: No more history zero-bits. Reset all.\n");
    }
}


/* Memory access functions:
 * If the address matches one of the transparent translation registers
 * use it directly as physical address, else check ATC for the
 * logical address. If the logical address is not resident in the ATC
 * create a new ATC entry and then look up the physical address. 
 */

void mmu030_put_long(uaecptr addr, uae_u32 val, bool data, int size) {
    
	//                                        addr,super,write
	if ((!mmu030.enabled) || (mmu030_match_ttr(addr,regs.s != 0,data,true)&TT_OK_MATCH)
//        || ((regs.dfc&7)==7) /* not sure about this, TODO: check! */
        ) {
		phys_put_long(addr,val);
		return;
    }

    int atc_line_num = mmu030_logical_is_in_atc(addr, regs.s != 0, data, true);

    if (atc_line_num<ATC030_NUM_ENTRIES) {
        mmu030_put_long_atc(addr, val, atc_line_num, regs.s != 0, data);
    } else {
        mmu030_table_search(addr,regs.s!=0,data,true,0);
        mmu030_put_long_atc(addr, val, mmu030_logical_is_in_atc(addr,regs.s!=0,data,true), regs.s != 0, data);
    }
}

void mmu030_put_word(uaecptr addr, uae_u16 val, bool data, int size) {
    
	//                                        addr,super,write
	if ((!mmu030.enabled) || (mmu030_match_ttr(addr,regs.s != 0,data,true)&TT_OK_MATCH)
        //        || ((regs.dfc&7)==7) /* not sure about this, TODO: check! */
        ) {
		phys_put_word(addr,val);
		return;
    }
    
    int atc_line_num = mmu030_logical_is_in_atc(addr, regs.s != 0, data, true);
    
    if (atc_line_num<ATC030_NUM_ENTRIES) {
        mmu030_put_word_atc(addr, val, atc_line_num, regs.s != 0, data);
    } else {
        mmu030_table_search(addr, regs.s != 0, data, true, 0);
        mmu030_put_word_atc(addr, val, mmu030_logical_is_in_atc(addr,regs.s!=0,data,true), regs.s != 0, data);
    }
}

void mmu030_put_byte(uaecptr addr, uae_u8 val, bool data, int size) {
    
	//                                        addr,super,write
	if ((!mmu030.enabled) || (mmu030_match_ttr(addr,regs.s != 0,data,true)&TT_OK_MATCH)
        //        || ((regs.dfc&7)==7) /* not sure about this, TODO: check! */
        ) {
		phys_put_byte(addr,val);
		return;
    }
    
    int atc_line_num = mmu030_logical_is_in_atc(addr, regs.s != 0, data, true);

    if (atc_line_num<ATC030_NUM_ENTRIES) {
        mmu030_put_byte_atc(addr, val, atc_line_num, regs.s != 0, data);
    } else {
        mmu030_table_search(addr, regs.s != 0, data, true, 0);
        mmu030_put_byte_atc(addr, val, mmu030_logical_is_in_atc(addr,regs.s!=0,data,true), regs.s != 0, data);
    }
}

uae_u32 mmu030_get_long(uaecptr addr, bool data, int size) {
    
	//                                        addr,super,write
	if ((!mmu030.enabled) || (mmu030_match_ttr(addr,regs.s != 0,data,false)&TT_OK_MATCH)
        //        || ((regs.dfc&7)==7) /* not sure about this, TODO: check! */
        ) {
		return phys_get_long(addr);
    }
    
    int atc_line_num = mmu030_logical_is_in_atc(addr, regs.s != 0, data, false);

    if (atc_line_num<ATC030_NUM_ENTRIES) {
        return mmu030_get_long_atc(addr, atc_line_num, regs.s != 0, data);
    } else {
        mmu030_table_search(addr, regs.s != 0, data, false, 0);
        return mmu030_get_long_atc(addr, mmu030_logical_is_in_atc(addr,regs.s!=0,data,false), regs.s != 0, data);
    }
}

uae_u16 mmu030_get_word(uaecptr addr, bool data, int size) {
    
	//                                        addr,super,write
	if ((!mmu030.enabled) || (mmu030_match_ttr(addr,regs.s != 0,data,false)&TT_OK_MATCH)
        //        || ((regs.dfc&7)==7) /* not sure about this, TODO: check! */
        ) {
		return phys_get_word(addr);
    }
    
    int atc_line_num = mmu030_logical_is_in_atc(addr, regs.s != 0, data, false);

    if (atc_line_num<ATC030_NUM_ENTRIES) {
        return mmu030_get_word_atc(addr, atc_line_num, regs.s != 0, data);
    } else {
        mmu030_table_search(addr, regs.s != 0, data, false, 0);
        return mmu030_get_word_atc(addr, mmu030_logical_is_in_atc(addr,regs.s!=0,data,false), regs.s != 0, data);
    }
}

uae_u8 mmu030_get_byte(uaecptr addr, bool data, int size) {
    
	//                                        addr,super,write
	if ((!mmu030.enabled) || (mmu030_match_ttr(addr,regs.s != 0,data,false)&TT_OK_MATCH)
        //        || ((regs.dfc&7)==7) /* not sure about this, TODO: check! */
        ) {
		return phys_get_byte(addr);
    }
    
    int atc_line_num = mmu030_logical_is_in_atc(addr, regs.s != 0, data, false);

    if (atc_line_num<ATC030_NUM_ENTRIES) {
        return mmu030_get_byte_atc(addr, atc_line_num, regs.s != 0, data);
    } else {
        mmu030_table_search(addr, regs.s != 0, data, false, 0);
        return mmu030_get_byte_atc(addr, mmu030_logical_is_in_atc(addr,regs.s!=0,data,false), regs.s != 0, data);
    }
}
