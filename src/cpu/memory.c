 /*
  * UAE - The Un*x Amiga Emulator - CPU core
  *
  * Memory management
  *
  * (c) 1995 Bernd Schmidt
  *
  * Adaptation to Hatari by Thomas Huth
  *
  * This file is distributed under the GNU Public License, version 2 or at
  * your option any later version. Read the file gpl.txt for details.
  */
const char Memory_fileid[] = "Hatari memory.c : " __DATE__ " " __TIME__;

#include "config.h"
#include "sysdeps.h"
#include "hatari-glue.h"
#include "maccess.h"
#include "memory.h"

#include "main.h"
#include "ioMem.h"
#include "reset.h"
#include "nextMemory.h"
#include "m68000.h"
#include "configuration.h"

#include "newcpu.h"


/* Set illegal_mem to 1 for debug output: */
#define illegal_mem 1


/*
 approximative next memory map (source netbsd/next68k file cpu.h)
 
 EPROM 128k (>cube 040)
 0x00000000-0x0001FFFF
 mirrored at
 0x01000000-0x0101FFFF
 
 RAM (16x4=64Mb max)
 0x04000000-0x07FFFFFF (0x047FFFFF for 8Mb)
 
 device
 0x02000000-0x0201BFFF
 mirrored at
 0x02100000-0x0211BFFF
 
 SCREEN
 0x0B000000-0x0B03A7FF
 
 */
#define NEXT_EPROM_START 	0x00000000
#define NEXT_EPROM2_START 	0x01000000
#define ROMmem_mask			0x0001FFFF
#define	ROMmem_size			0x00020000
#define NEXT_EPROM_SIZE		0x00020000

// ram is flat?
#define NEXT_RAM_START   	0x04000000
#define NEXT_RAM_SPACE		0x40000000
// #define NEXT_RAM_SIZE		0x007FE000
#define NEXT_RAM_SIZE		0x08000000

uae_u32	NEXTmem_size; // unused
#define NEXTmem_mask		0x00FFFFFF
// for a mono screen
#define NEXT_SCREEN			0x0B000000
#define NEXT_SCREEN_SIZE	0x00040000
#define NEXTvideo_size NEXT_SCREEN_SIZE
#define NEXTvideo_mask		0x0003FFFF
uae_u8  NEXTVideo[256*1024];



#define IOmem_mask 			0x0001FFFF
#define	IOmem_size			0x0001C000
#define NEXT_IO_START   	0x02000000
#define NEXT_IO2_START   	0x02100000
#define NEXT_IO_SIZE		0x00020000

#define NEXT_BMAP_START		0x020C0000
#define NEXT_BMAP_SIZE		0x10000
#define	NEXTbmap_size		NEXT_BMAP_SIZE
#define	NEXTbmap_mask		0x0000FFFF
uae_u8  NEXTbmap[NEXT_BMAP_SIZE];

#define NEXT_X06_START		0x06000000
#define NEXT_X06_SIZE		0x10000
#define	NEXTx06_size		NEXT_X06_SIZE
#define	NEXTx06_mask		0x0000FFFF
uae_u8  NEXTx06[NEXT_X06_SIZE];


#ifdef SAVE_MEMORY_BANKS
addrbank *mem_banks[65536];
#else
addrbank mem_banks[65536];
#endif

#ifdef NO_INLINE_MEMORY_ACCESS
__inline__ uae_u32 longget (uaecptr addr)
{
    return call_mem_get_func (get_mem_bank (addr).lget, addr);
}
__inline__ uae_u32 wordget (uaecptr addr)
{
    return call_mem_get_func (get_mem_bank (addr).wget, addr);
}
__inline__ uae_u32 byteget (uaecptr addr)
{
    return call_mem_get_func (get_mem_bank (addr).bget, addr);
}
__inline__ void longput (uaecptr addr, uae_u32 l)
{
    call_mem_put_func (get_mem_bank (addr).lput, addr, l);
}
__inline__ void wordput (uaecptr addr, uae_u32 w)
{
    call_mem_put_func (get_mem_bank (addr).wput, addr, w);
}
__inline__ void byteput (uaecptr addr, uae_u32 b)
{
    call_mem_put_func (get_mem_bank (addr).bput, addr, b);
}
#endif


/* Some prototypes: */
extern void SDL_Quit(void);
static int NEXTmem_check (uaecptr addr, uae_u32 size) REGPARAM;
static uae_u8 *NEXTmem_xlate (uaecptr addr) REGPARAM;

uae_u8 ce_banktype[65536];
uae_u8 ce_cachable[65536];


/* A dummy bank that only contains zeros */

static uae_u32 dummy_lget(uaecptr addr)
{
    if (illegal_mem)
	write_log ("Illegal lget at %08lx\n", (long)addr);

    return 0;
}

static uae_u32 dummy_wget(uaecptr addr)
{
    if (illegal_mem)
	write_log ("Illegal wget at %08lx\n", (long)addr);

    return 0;
}

static uae_u32 dummy_bget(uaecptr addr)
{
    if (illegal_mem)
	write_log ("Illegal bget at %08lx\n", (long)addr);

    return 0;
}

static void dummy_lput(uaecptr addr, uae_u32 l)
{
    if (illegal_mem)
	write_log ("Illegal lput at %08lx\n", (long)addr);
}

static void dummy_wput(uaecptr addr, uae_u32 w)
{
    if (illegal_mem)
	write_log ("Illegal wput at %08lx\n", (long)addr);
}

static void dummy_bput(uaecptr addr, uae_u32 b)
{
    if (illegal_mem)
	write_log ("Illegal bput at %08lx\n", (long)addr);
}

static int dummy_check(uaecptr addr, uae_u32 size)
{
    if (illegal_mem)
	write_log ("Illegal check at %08lx\n", (long)addr);

    return 0;
}

static uae_u8 *dummy_xlate(uaecptr addr)
{
    write_log("Your Atari program just did something terribly stupid:"
              " dummy_xlate($%x)\n", addr);
    /*Reset_Warm();*/
 //       return NEXTmem_xlate(addr);  /* So we don't crash. */
}


/* **** This memory bank only generates bus errors **** */

static uae_u32 BusErrMem_lget(uaecptr addr)
{
    if (illegal_mem)
	write_log ("Bus error lget at %08lx\n", (long)addr);

    M68000_BusError(addr, 1);
    return 0;
}

static uae_u32 BusErrMem_wget(uaecptr addr)
{
    if (illegal_mem)
	write_log ("Bus error wget at %08lx\n", (long)addr);

    M68000_BusError(addr, 1);
    return 0;
}

static uae_u32 BusErrMem_bget(uaecptr addr)
{
    if (illegal_mem)
	write_log ("Bus error bget at %08lx\n", (long)addr);

    M68000_BusError(addr, 1);
    return 0;
}

static void BusErrMem_lput(uaecptr addr, uae_u32 l)
{
    if (illegal_mem)
	write_log ("Bus error lput at %08lx\n", (long)addr);

    M68000_BusError(addr, 0);
}

static void BusErrMem_wput(uaecptr addr, uae_u32 w)
{
    if (illegal_mem)
	write_log ("Bus error wput at %08lx\n", (long)addr);

    M68000_BusError(addr, 0);
}

static void BusErrMem_bput(uaecptr addr, uae_u32 b)
{
    if (illegal_mem)
	write_log ("Bus error bput at %08lx\n", (long)addr);

    M68000_BusError(addr, 0);
}

static int BusErrMem_check(uaecptr addr, uae_u32 size)
{
    if (illegal_mem)
	write_log ("Bus error check at %08lx\n", (long)addr);

    return 0;
}

static uae_u8 *BusErrMem_xlate (uaecptr addr)
{
    write_log("Your NeXT program just did something terribly stupid:"
              " BusErrMem_xlate($%x)\n", addr);

    /*M68000_BusError(addr);*/
    return NEXTmem_xlate(addr);  /* So we don't crash. */
}


/* **** NEXT RAM memory **** */

static uae_u32 NEXTmem_lget(uaecptr addr)
{
    addr &= NEXTmem_mask;
    return do_get_mem_long(NEXTRam + addr);
}

static uae_u32 NEXTmem_wget(uaecptr addr)
{
    addr &= NEXTmem_mask;
    return do_get_mem_word(NEXTRam + addr);
}

static uae_u32 NEXTmem_bget(uaecptr addr)
{
    addr &= NEXTmem_mask;
    return NEXTRam[addr];
}

static void NEXTmem_lput(uaecptr addr, uae_u32 l)
{
    addr &= NEXTmem_mask;
    do_put_mem_long(NEXTRam + addr, l);
}

static void NEXTmem_wput(uaecptr addr, uae_u32 w)
{
    addr &= NEXTmem_mask;
    do_put_mem_word(NEXTRam + addr, w);
}

static void NEXTmem_bput(uaecptr addr, uae_u32 b)
{
    addr &= NEXTmem_mask;
    NEXTRam[addr] = b;
}

static int NEXTmem_check(uaecptr addr, uae_u32 size)
{
    addr &= NEXTmem_mask;
    return (addr + size) < NEXT_RAM_SIZE;
}

static uae_u8 *NEXTmem_xlate(uaecptr addr)
{
    addr &= NEXTmem_mask;
    return NEXTRam + addr;
}

/* bank2 */

static uae_u32 NEXTmem2_lget(uaecptr addr)
{
    addr &= NEXTmem_mask;
    return do_get_mem_long(NEXTRam + 0x00400000 + addr);
}

static uae_u32 NEXTmem2_wget(uaecptr addr)
{
    addr &= NEXTmem_mask;
    return do_get_mem_word(NEXTRam + 0x00400000 + addr);
}

static uae_u32 NEXTmem2_bget(uaecptr addr)
{
    addr &= NEXTmem_mask;
    return NEXTRam[addr+0x00400000];
}

static void NEXTmem2_lput(uaecptr addr, uae_u32 l)
{
    addr &= NEXTmem_mask;
    do_put_mem_long(NEXTRam + addr + 0x00400000, l);
}

static void NEXTmem2_wput(uaecptr addr, uae_u32 w)
{
    addr &= NEXTmem_mask;
    do_put_mem_word(NEXTRam + addr + 0x00400000, w);
}

static void NEXTmem2_bput(uaecptr addr, uae_u32 b)
{
    addr &= NEXTmem_mask;
    NEXTRam[addr+0x00400000] = b;
}

static int NEXTmem2_check(uaecptr addr, uae_u32 size)
{
    addr &= NEXTmem_mask;
    return (addr + size) <= 0x003FFFFF;
}

static uae_u8 *NEXTmem2_xlate(uaecptr addr)
{
    addr &= NEXTmem_mask;
    return NEXTRam + addr + 0x00400000;
}


/* **** NEXT VRAM memory **** */

static uae_u32 NEXTvideo_lget(uaecptr addr)
{
    addr &= NEXTvideo_mask;
    return do_get_mem_long(NEXTVideo + addr);
}

static uae_u32 NEXTvideo_wget(uaecptr addr)
{
    addr &= NEXTvideo_mask;
    return do_get_mem_word(NEXTVideo + addr);
}

static uae_u32 NEXTvideo_bget(uaecptr addr)
{
    addr &= NEXTvideo_mask;
    return NEXTVideo[addr];
}

static void NEXTvideo_lput(uaecptr addr, uae_u32 l)
{
    addr &= NEXTvideo_mask;
    do_put_mem_long(NEXTVideo + addr, l);
}

static void NEXTvideo_wput(uaecptr addr, uae_u32 w)
{
    addr &= NEXTvideo_mask;
    do_put_mem_word(NEXTVideo + addr, w);
}

static void NEXTvideo_bput(uaecptr addr, uae_u32 b)
{
    addr &= NEXTvideo_mask;
    NEXTVideo[addr] = b;
}

static int NEXTvideo_check(uaecptr addr, uae_u32 size)
{
    addr &= NEXTvideo_mask;
    return (addr + size) <= NEXTvideo_size;
}

static uae_u8 *NEXTvideo_xlate(uaecptr addr)
{
    addr &= NEXTvideo_mask;
    return (uae_u8*)NEXTVideo + addr;
}

/* **** NEXT BMAP memory **** */

static uae_u32 NEXTbmap_lget(uaecptr addr)
{
	write_log ("bmap lget at %08lx PC=%08x\n", (long)addr,m68k_getpc());
    addr &= NEXTbmap_mask;
    return do_get_mem_long(NEXTbmap + addr);
}

static uae_u32 NEXTbmap_wget(uaecptr addr)
{
	write_log ("bmap wget at %08lx PC=%08x\n", (long)addr,m68k_getpc());
    addr &= NEXTbmap_mask;
    return do_get_mem_word(NEXTbmap + addr);
}

static uae_u32 NEXTbmap_bget(uaecptr addr)
{
	write_log ("bmap bget at %08lx PC=%08x\n", (long)addr,m68k_getpc());
    addr &= NEXTbmap_mask;
    return NEXTbmap[addr];
}

static void NEXTbmap_lput(uaecptr addr, uae_u32 l)
{
	write_log ("bmap lput at %08lx val=%x PC=%08x\n", (long)addr,l,m68k_getpc());
    addr &= NEXTbmap_mask;
    do_put_mem_long(NEXTbmap + addr, l);
}

static void NEXTbmap_wput(uaecptr addr, uae_u32 w)
{
	write_log ("bmap wput at %08lx val=%x PC=%08x\n", (long)addr,w,m68k_getpc());
    addr &= NEXTbmap_mask;
    do_put_mem_word(NEXTbmap + addr, w);
}

static void NEXTbmap_bput(uaecptr addr, uae_u32 b)
{
	write_log ("bmap bput at %08lx val=%x PC=%08x\n", (long)addr,b,m68k_getpc());
    addr &= NEXTbmap_mask;
    NEXTbmap[addr] = b;
}

static int NEXTbmap_check(uaecptr addr, uae_u32 size)
{
    addr &= NEXTbmap_mask;
    return (addr + size) <= NEXTbmap_size;
}

static uae_u8 *NEXTbmap_xlate(uaecptr addr)
{
    addr &= NEXTbmap_mask;
    return (uae_u8*)NEXTbmap + addr;
}

/* **** NEXT x06 memory **** */

static uae_u32 NEXTx06_lget(uaecptr addr)
{
	write_log ("x06 lget at %08lx PC=%08x\n", (long)addr,m68k_getpc());
    addr &= NEXTx06_mask;
    //    return do_get_mem_long(NEXTx06 + addr);
	return 0xFFFFFFFF;
}

static uae_u32 NEXTx06_wget(uaecptr addr)
{
	write_log ("x06 wget at %08lx PC=%08x\n", (long)addr,m68k_getpc());
    addr &= NEXTx06_mask;
    //    return do_get_mem_word(NEXTx06 + addr);
	return 0xFFFF;
}

static uae_u32 NEXTx06_bget(uaecptr addr)
{
	write_log ("x06 bget at %08lx PC=%08x\n", (long)addr,m68k_getpc());
    addr &= NEXTx06_mask;
    //    return NEXTx06[addr];
	return 0xFF;
}

static void NEXTx06_lput(uaecptr addr, uae_u32 l)
{
	write_log ("x06 lput at %08lx val=%x PC=%08x\n", (long)addr,l,m68k_getpc());
    addr &= NEXTx06_mask;
    do_put_mem_long(NEXTx06 + addr, l);
}

static void NEXTx06_wput(uaecptr addr, uae_u32 w)
{
	write_log ("x06 wput at %08lx val=%x PC=%08x\n", (long)addr,w,m68k_getpc());
    addr &= NEXTx06_mask;
    do_put_mem_word(NEXTx06 + addr, w);
}

static void NEXTx06_bput(uaecptr addr, uae_u32 b)
{
	write_log ("x06 bput at %08lx val=%x PC=%08x\n", (long)addr,b,m68k_getpc());
    addr &= NEXTx06_mask;
    NEXTx06[addr] = b;
}

static int NEXTx06_check(uaecptr addr, uae_u32 size)
{
    addr &= NEXTx06_mask;
    return (addr + size) <= NEXTx06_size;
}

static uae_u8 *NEXTx06_xlate(uaecptr addr)
{
    addr &= NEXTx06_mask;
    return (uae_u8*)NEXTx06 + addr;
}


/*
 * **** Void memory ****
 * lots of free space in next's full 32bits memory map
 * Reading always returns the same value and writing does nothing at all.
 */

static uae_u32 VoidMem_lget(uaecptr addr)
{
    return 0;
}

static uae_u32 VoidMem_wget(uaecptr addr)
{
    return 0;
}

static uae_u32 VoidMem_bget(uaecptr addr)
{
    return 0;
}

static void VoidMem_lput(uaecptr addr, uae_u32 l)
{
}

static void VoidMem_wput(uaecptr addr, uae_u32 w)
{
}

static void VoidMem_bput (uaecptr addr, uae_u32 b)
{
}

static int VoidMem_check(uaecptr addr, uae_u32 size)
{
    if (illegal_mem)
        write_log ("Void memory check at %08lx\n", (long)addr);
    
    return 0;
}

static uae_u8 *VoidMem_xlate (uaecptr addr)
{
    write_log("Your Next program just did something terribly stupid:"
              " VoidMem_xlate($%x)\n", addr);
    
    return NEXTmem_xlate(addr);  /* So we don't crash. */
}


/* **** ROM memory **** */

uae_u8 *ROMmemory;

static uae_u32 ROMmem_lget(uaecptr addr)
{
    addr &= ROMmem_mask;
    return do_get_mem_long(ROMmemory + addr);
}

static uae_u32 ROMmem_wget(uaecptr addr)
{
    addr &= ROMmem_mask;
    return do_get_mem_word(ROMmemory + addr);
}

static uae_u32 ROMmem_bget(uaecptr addr)
{
    addr &= ROMmem_mask;
    return ROMmemory[addr];
}

static void ROMmem_lput(uaecptr addr, uae_u32 b)
{
    if (illegal_mem)
        write_log ("Illegal ROMmem lput at %08lx\n", (long)addr);
    
    M68000_BusError(addr, 0);
}

static void ROMmem_wput(uaecptr addr, uae_u32 b)
{
    if (illegal_mem)
        write_log ("Illegal ROMmem wput at %08lx\n", (long)addr);
    
    M68000_BusError(addr, 0);
}

static void ROMmem_bput(uaecptr addr, uae_u32 b)
{
    if (illegal_mem)
        write_log ("Illegal ROMmem bput at %08lx\n", (long)addr);
    
    M68000_BusError(addr, 0);
}

static int ROMmem_check(uaecptr addr, uae_u32 size)
{
    addr &= ROMmem_mask;
    return (addr + size) <= ROMmem_size;
}

static uae_u8 *ROMmem_xlate(uaecptr addr)
{
    addr &= ROMmem_mask;
    return ROMmemory + addr;
}

/* Hardware IO memory */
/* see also ioMem.c */

uae_u8 *IOmemory;

static int IOmem_check(uaecptr addr, uae_u32 size)
{
    addr &= IOmem_mask;
    return (addr + size) <= IOmem_size;
}

static uae_u8 *IOmem_xlate(uaecptr addr)
{
    addr &= IOmem_mask;
    return IOmemory + addr;
}



/* **** Address banks **** */

static addrbank dummy_bank =
{
    dummy_lget, dummy_wget, dummy_bget,
    dummy_lput, dummy_wput, dummy_bput,
    dummy_xlate, dummy_check, NULL, NULL,
    dummy_lget, dummy_wget, ABFLAG_NONE
//	dummy_lgeti, dummy_wgeti, ABFLAG_NONE
};

static addrbank BusErrMem_bank =
{
    BusErrMem_lget, BusErrMem_wget, BusErrMem_bget,
    BusErrMem_lput, BusErrMem_wput, BusErrMem_bput,
    BusErrMem_xlate, BusErrMem_check, NULL, "BusError memory",
    BusErrMem_lget, BusErrMem_wget, ABFLAG_NONE
};

static addrbank NEXTmem_bank =
{
    NEXTmem_lget, NEXTmem_wget, NEXTmem_bget,
    NEXTmem_lput, NEXTmem_wput, NEXTmem_bput,
    NEXTmem_xlate, NEXTmem_check, NULL, "NEXT memory",
    NEXTmem_lget, NEXTmem_wget, ABFLAG_RAM

};

static addrbank NEXTmem_bank2 =
{
    NEXTmem2_lget, NEXTmem2_wget, NEXTmem2_bget,
    NEXTmem2_lput, NEXTmem2_wput, NEXTmem2_bput,
    NEXTmem2_xlate, NEXTmem2_check, NULL, "NEXT memory",
    NEXTmem2_lget, NEXTmem2_wget, ABFLAG_RAM
};

static addrbank VoidMem_bank =
{
    VoidMem_lget, VoidMem_wget, VoidMem_bget,
    VoidMem_lput, VoidMem_wput, VoidMem_bput,
    VoidMem_xlate, VoidMem_check, NULL, "Void memory",
    VoidMem_lget, VoidMem_wget, ABFLAG_NONE
};

static addrbank Video_bank =
{
    NEXTvideo_lget, NEXTvideo_wget, NEXTvideo_bget,
    NEXTvideo_lput, NEXTvideo_wput, NEXTvideo_bput,
    NEXTvideo_xlate, NEXTvideo_check, NULL, "Video memory",
    NEXTvideo_lget, NEXTvideo_wget, ABFLAG_RAM
};

static addrbank bmap_bank =
{
    NEXTbmap_lget, NEXTbmap_wget, NEXTbmap_bget,
    NEXTbmap_lput, NEXTbmap_wput, NEXTbmap_bput,
    NEXTbmap_xlate, NEXTbmap_check, NULL, "bmap memory",
    NEXTbmap_lget, NEXTbmap_wget, ABFLAG_RAM
};

static addrbank x06_bank =
{
    NEXTx06_lget, NEXTx06_wget, NEXTx06_bget,
    NEXTx06_lput, NEXTx06_wput, NEXTx06_bput,
    NEXTx06_xlate, NEXTx06_check, NULL, "x06 memory",
    NEXTx06_lget, NEXTx06_wget, ABFLAG_RAM
};

static addrbank ROMmem_bank =
{
    ROMmem_lget, ROMmem_wget, ROMmem_bget,
    ROMmem_lput, ROMmem_wput, ROMmem_bput,
    ROMmem_xlate, ROMmem_check, NULL, "ROM memory",
    ROMmem_lget, ROMmem_wget, ABFLAG_ROM
};

static addrbank IOmem_bank =
{
    IoMem_lget, IoMem_wget, IoMem_bget,
    IoMem_lput, IoMem_wput, IoMem_bput,
    IOmem_xlate, IOmem_check, NULL, "IO memory",
    IoMem_lget, IoMem_wget, ABFLAG_RAM
};



static void init_mem_banks (void)
{
    int i;
    for (i = 0; i < 65536; i++)
        put_mem_bank (i<<16, &dummy_bank);
}


/*
 * Initialize the memory banks
 */
void memory_init(uae_u32 nNewNEXTMemSize)
{
    NEXTmem_size = (nNewNEXTMemSize + 65535) & 0xFFFF0000;
    
    write_log("memory_init: NEXTmem_size=$%x (not used yet)\n",
              nNewNEXTMemSize);
    
	/* fill every 65536 bank with dummy */
    init_mem_banks(); 
    
    // map_banks(&BusErrMem_bank,NEXT_RAM_START>>16,NEXT_RAM_SPACE>>16);
    
    map_banks(&NEXTmem_bank, NEXT_RAM_START>>16, NEXT_RAM_SIZE >> 16);
    
    // map_banks(&NEXTmem_bank2, NEXT_RAM_START2>>16, NEXT_RAM_SIZE2 >> 16);
    
    
    map_banks(&Video_bank, NEXT_SCREEN>>16, NEXT_SCREEN_SIZE >> 16);
    map_banks(&Video_bank, NEXT_SCREEN>>16, NEXT_SCREEN_SIZE >> 16);
    map_banks(&Video_bank, NEXT_SCREEN>>16, NEXT_SCREEN_SIZE >> 16);
    map_banks(&Video_bank, NEXT_SCREEN>>16, NEXT_SCREEN_SIZE >> 16);
    
    map_banks(&ROMmem_bank, NEXT_EPROM_START >> 16, NEXT_EPROM_SIZE>>16);
    map_banks(&ROMmem_bank, NEXT_EPROM2_START >> 16, NEXT_EPROM_SIZE>>16);
    
    
    map_banks(&IOmem_bank, NEXT_IO_START >> 16, NEXT_IO_SIZE>>16);
    map_banks(&IOmem_bank, NEXT_IO2_START >> 16, NEXT_IO_SIZE>>16);
    //	map_banks(&VoidMem_bank, NEXT_IO_START >> 16, NEXT_IO_SIZE>>16);
    //    map_banks(&VoidMem_bank, NEXT_IO2_START >> 16, NEXT_IO_SIZE>>16);
    
    map_banks(&bmap_bank, NEXT_BMAP_START >> 16, NEXT_BMAP_SIZE>>16);
    //    map_banks(&x06_bank, NEXT_X06_START >> 16, NEXT_X06_SIZE>>16);
    //    map_banks(&x06_bank, 0x07000000 >> 16, NEXT_X06_SIZE>>16);
    
	ROMmemory=NEXTRom;
	IOmemory=NEXTIo;
	{
		FILE* fin;
		int ret;
	// if processor is 68030, loads a cube rom
        if(ConfigureParams.System.nCpuLevel == 3)
            fin=fopen(ConfigureParams.Rom.szRom030FileName,"rb");
        else
            fin=fopen(ConfigureParams.Rom.szRom040FileName,"rb");

		ret=fread(ROMmemory,1,0x20000,fin);
        
		write_log("Read ROM %d\n",ret);
		fclose(fin);
	}
	
	{
		int i;
		for (i=0;i<sizeof(NEXTVideo);i++) NEXTVideo[i]=0xAA;
		for (i=0;i<sizeof(NEXTRam);i++) NEXTRam[i]=0xAA;
	}
    
}


/*
 * Uninitialize the memory banks.
 */
void memory_uninit (void)
{
}


void map_banks (addrbank *bank, int start, int size)
{
    int bnr;
    unsigned long int hioffs = 0, endhioffs = 0x100;
    
	for (bnr = start; bnr < start + size; bnr++)
	    put_mem_bank (bnr << 16, bank);
	return;
}

void memory_hardreset (void)
{
}
