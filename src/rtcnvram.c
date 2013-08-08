/*  Previous - rtcnvram.c
 
 This file is distributed under the GNU Public License, version 2 or at
 your option any later version. Read the file gpl.txt for details.
 
 Emulation of Real Time Clock including NVRAM.
 
 Old systems use MC68HC68T1 chip, new systems use MCCS1850 chip.

 */

#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "configuration.h"
#include "sysReg.h"
#include "rtcnvram.h"


#define LOG_RTC_LEVEL   LOG_WARN


/* RTC interface */
#define RTC_ADDR_WRITE  0x80
#define RTC_ADDR_CLOCK  0x20
#define RTC_ADDR_MASK   0x7F
Uint8 rtc_addr = 0;
Uint8 rtc_val = 0;
int phase = 0;
bool freeze = false;

int oldrtc_interface_io(Uint8 rtdatabit);
int newrtc_interface_io(Uint8 rtdatabit);

int rtc_interface_io(Uint8 rtdatabit) {
    switch (ConfigureParams.System.nRTC) {
        case MC68HC68T1: return oldrtc_interface_io(rtdatabit);
        case MCCS1850: return newrtc_interface_io(rtdatabit);
        default:
            Log_Printf(LOG_WARN, "[RTC] error: no I/O function for this chip!");
            return oldrtc_interface_io(rtdatabit); /* trying old chip */
    }
}

void rtc_interface_reset(void) {
    phase = 0;
    rtc_addr = 0;
    rtc_val = 0;
    freeze = false;
}


/* RTC power down request */
void newrtc_request_power_down(void);

void rtc_request_power_down(void) {
    if (ConfigureParams.System.nRTC==MCCS1850) {
        newrtc_request_power_down();
    }
}



/* --------------------- MC68HC68T1 --------------------- */

/* RTC NVRAM is located at address 0x00 to 0x1F (32 bytes),
 * time registers, alarm registers and control/status
 * registers are located at address 0x20 to 0x32.
 */

Uint8 rtc_get_clock(Uint8 addr);
void rtc_put_clock(Uint8 addr, Uint8 val);

/* All time values in RTC clock are in packed decimal format */

typedef struct {
    Uint8 sec;      /* 00 - 59 */
    Uint8 min;      /* 00 - 59 */
    Uint8 hour;     /* 01 - 12 or 00 - 24; bit 7: 1 = 12 hr, 0 = 24 hr; bit 5: 1 = pm, 0 = am */
    Uint8 wday;     /* 01 - 07; 1 = sunday */
    Uint8 mday;     /* 01 - 31 */
    Uint8 month;    /* 01 - 12; 1 = january */
    Uint8 year;     /* 00 - 99 */
} RTC_TIME;

RTC_TIME get_rtc_time(void);

typedef struct {
    Uint8 sec;      /* 00 - 59 */
    Uint8 min;      /* 00 - 59 */
    Uint8 hour;     /* 01 - 12 or 00 - 24; bit 5: 1 = pm, 0 = am in 24 hr mode */
} RTC_ALARM;


/* There are 3 status and control registers inside the chip */

/* RTC Status Register at 0x30 (r only)
 *
 * 0-0- ----    always 0 (0 in bit 7 identifies MC68HC68T1 chip)
 * -x-- ----    watchdog detected cpu failure
 * ---x ----    first time up
 * ---- x---    interrupt true (one of following interrupts is valid)
 * ---- -x--    power sense interrupt
 * ---- --x-    alarm interrupt
 * ---- ---x    clock interrupt
 */

#define RTC_CPUFAIL     0x40
#define RTC_FIRSTUP     0x10
#define RTC_INT         0x08
#define RTC_INT_SENSE   0x04
#define RTC_INT_ALARM   0x02
#define RTC_INT_CLOCK   0x01


/* RTC Clock Control Register at 0x31 (r) or 0xB1 (w)
 *
 * x--- ----    1 = start, 0 = stop counter
 * -x-- ----    1 = enable line input, 0 = enable chrystal input
 * --xx ----    chrystal select: 0 = 4.194304 MHz, 1 = 2.097152, 2 = 1.048576, 3 = 32.768 kHz
 * ---- x---    1 = line input 50 Hz, 0 = line input 60 Hz
 * ---- -xxx    clock out frequency:
 *
 * 0 = chrystal
 * 1 = chrystal/2
 * 2 = chrystal/4
 * 3 = chrystal/8
 * 4 = disable
 * 5 = 1 Hz
 * 6 = 2 Hz
 * 7 = 50/60 Hz for line operation; 64 Hz for chrystal operation
 */

#define RTC_START   0x80
#define RTC_STOP    0x00
#define RTC_LINE    0x40
#define RTC_XTAL    0x30
#define RTC_L50HZ   0x08
#define RTC_FREQ    0x07


/* RTC Interrupt Control Register at 0x32 (r) or 0xB2 (w)
 *
 * x--- ----    watchdog enable
 * -x-- ----    initiate power down
 * --x- ----    power sense
 * ---x ----    enable alarm
 * ---- xxxx    select frequency of periodic interrupt:
 *
 * 0 = diable
 * chrystal:
 * 1 = 2048 Hz, 2 = 1024 Hz, ... , C = 1 Hz
 * D = 1 per min, E = 1 per hour, F = 1 per day
 * line:
 * 6 = 50 or 60 Hz, B = 2 Hz, C = 1 Hz
 * D = 1 per min, E = 1 per hour, F = 1 per day
 */

#define RTC_WATCHDOG    0x80
#define RTC_POWERDOWN   0x40
#define RTC_PWRSENSE    0x20
#define RTC_ENABLEALRM  0x10
#define RTC_PERIODIC    0x0F

struct {
    Uint8 ram[32];      /* 0x00 - 0x1F (r), 0x80 - 0x9F (w) */
    RTC_TIME time;      /* 0x20 - 0x26 (r), 0xA0 - 0xA6 (w) */
    RTC_ALARM alarm;    /* 0xA8 - 0xAA (w) */
    Uint8 status;       /* 0x30 (r) */
    Uint8 clkctrl;      /* 0x31 (r), 0xB1 (w) */
    Uint8 intctrl;      /* 0x32 (r), 0xB2 (w) */
} rtc;


int oldrtc_interface_io(Uint8 rtdatabit) {
    
    phase++;
    
    if (phase<=8) {
        rtc_addr = (rtc_addr<<1)|(rtdatabit?1:0);
    } else {
        
        if (phase==9) {
            if (!(rtc_addr&RTC_ADDR_WRITE)) {
                if (rtc_addr&RTC_ADDR_CLOCK) {
                    rtc_val = rtc_get_clock(rtc_addr);
                } else {
                    rtc_val = rtc.ram[rtc_addr&RTC_ADDR_MASK];
                }
                
                Log_Printf(LOG_RTC_LEVEL,"[RTC] reading val $%02X from addr $%02X at PC=$%08x\n",
                           rtc_val,rtc_addr,m68k_getpc());
            }
        }
        
        if (rtc_addr&RTC_ADDR_WRITE) {
            rtc_val = (rtc_val<<1)|(rtdatabit?1:0);
        } else {
            rtdatabit = (rtc_val&(1<<(16-phase)))?1:0;
        }
        
        if (phase==16) {
            if (rtc_addr&RTC_ADDR_WRITE) {
                Log_Printf(LOG_RTC_LEVEL,"[RTC] writing val $%02X to addr $%02X at PC=$%08x\n",
                           rtc_val,rtc_addr,m68k_getpc());
                
                if (rtc_addr&RTC_ADDR_CLOCK) {
                    rtc_put_clock(rtc_addr, rtc_val);
                } else {
                    rtc.ram[rtc_addr&RTC_ADDR_MASK] = rtc_val;
                }
            }
            
            switch (rtc_addr) {
                case 0x1F:
                case 0x9F: rtc_addr = 0x00; break;
                case 0x32:
                case 0xB2: rtc_addr = 0x20; break;
                default: rtc_addr++; break;
            }
            phase-=8;
        }
    }
    
    /* RTC returns 0 or 1 */
    return rtdatabit;
}


Uint8 rtc_get_clock(Uint8 addr) {
    Uint8 val = 0x00;
    
    if (!freeze) {
        rtc.time = get_rtc_time();
        freeze = true;
    }
    
    switch (rtc_addr&RTC_ADDR_MASK) {
        case 0x20: /* seconds */
            val = rtc.time.sec; break;
        case 0x21: /* minutes */
            val = rtc.time.min; break;
        case 0x22: /* hours */
            val = rtc.time.hour; break;
        case 0x23: /* day of week (sunday = 1) */
            val = rtc.time.wday; break;
        case 0x24: /* day of month */
            val = rtc.time.mday; break;
        case 0x25: /* month */
            val = rtc.time.month; break;
        case 0x26: /* year (0 - 99) */
            val = rtc.time.year; break;
            
        case 0x30: /* status register */
            val = rtc.status;
            rtc.status &= RTC_PWRSENSE;
            break;
        case 0x31: /* clock control register */
            val = rtc.clkctrl; break;
        case 0x32: /* interrupt control register */
            val = rtc.intctrl; break;
            
        default: break;
    }
    
    return val;
}

void rtc_put_clock(Uint8 addr, Uint8 val) {
    switch (rtc_addr&RTC_ADDR_MASK) {
        case 0x20: /* seconds */
        case 0x21: /* minutes */
        case 0x22: /* hours */
        case 0x23: /* day of week (sunday = 1) */
        case 0x24: /* day of month */
        case 0x25: /* month */
        case 0x26: /* year (0 - 99) */
            break; /* we can't set our clock */
            
        case 0x28: /* alarm: seconds */
        case 0x29: /* alarm: minutes */
        case 0x2A: /* alarm: hours */
            break; /* not yet! */
            
        case 0x31: /* clock control register */
            rtc.clkctrl = val;
            break;
        case 0x32: /* interrupt control register */
            rtc.intctrl = val;
            if (rtc.intctrl&RTC_POWERDOWN) {
                Log_Printf(LOG_WARN, "[RTC] Power down!");
                M68000_Stop();
            }
            break;
            
        default: break;
    }
}

RTC_TIME get_rtc_time(void) {
    RTC_TIME rt;
    
    time_t tmp = time(NULL);
    struct tm t =*localtime(&tmp);
    
    rt.sec = (((t.tm_sec/10)%10)<<4)|(t.tm_sec%10);
    rt.min = (((t.tm_min/10)%10)<<4)|(t.tm_min%10);
    rt.hour = (((t.tm_hour/10)%10)<<4)|(t.tm_hour%10);
    rt.wday = t.tm_wday+1;
    rt.mday = (((t.tm_mday/10)%10)<<4)|(t.tm_mday%10);
    rt.month = ((((t.tm_mon+1)/10)%10)<<4)|((t.tm_mon+1)%10);
    rt.year = (((t.tm_year/10)%10)<<4)|(t.tm_year%10);
    
    return rt;
}



/* ------------------------- MCCS1850 ------------------------- */

/* RTC NVRAM (64 bytes) is located at address 0x00 to 0x1F
 * and 0x40 to 0x5F, time registers, alarm registers and 
 * control/status registers are located at address 0x20 to 0x31.
 */

Uint8 newrtc_get_clock(Uint8 addr);
void newrtc_put_clock(Uint8 addr, Uint8 val);

/* New RTC has two 32 bit counters, one for time and one for alarm */

/* There are 3 status and control registers inside the chip */

/* RTC Status Register at 0x30 (r only)
 *
 * 1--- ----    always 1 (identifies MCCS1850 chip)
 * -0-- ----    always 0
 * --x- ----    test mode status
 * ---x ----    first time up
 * ---- x---    interrupt true (one of following interrupts is valid)
 * ---- -x--    low battery interrupt
 * ---- --x-    alarm interrupt
 * ---- ---x    power down interrupt
 */

#define NRTC_NEWCHIP    0x80
#define NRTC_TMODE      0x20
#define NRTC_FIRSTUP    0x10
#define NRTC_INT        0x08
#define NRTC_INT_LBAT   0x04
#define NRTC_INT_ALARM  0x02
#define NRTC_INT_PDOWN  0x01


/* RTC Control Register at 0x31 (r) or 0xB1 (w)
 *
 * x--- ----    1 = start, 0 = stop counter
 * -x-- ----    initiate power down
 * --x- ----    enable auto restart sequence
 * ---x ----    enable alarm
 * ---- x---    alarm clear (clear alarm int bit in status)
 * ---- -x--    first time up clear (clear first up bit in status)
 * ---- --x-    enable low battery interrupting
 * ---- ---x    request power down clear (clear power down int bit in status)
 *
 * ---- xx-x    always read as 0
 */

#define NRTC_CTRL_0     0x0D

#define NRTC_START      0x80
#define NRTC_STOP       0x00
#define NRTC_POWERDOWN  0x40
#define NRTC_AR         0x20
#define NRTC_ENABLEALRM 0x10
#define NRTC_CLRALARM   0x08
#define NRTC_CLRFTU     0x04
#define NRTC_LBE        0x02
#define NRTC_CLRPDOWN   0x01


struct {
    /* --> see old chip  * 0x00 - 0x1F (r), 0x80 - 0x9F (w) */
    Uint32 timecntr;    /* 0x20 - 0x23 (r), 0xA0 - 0xA3 (w) */
    Uint32 alarmcntr;   /* 0x24 - 0x27 (r), 0xA4 - 0xA7 (w) */
    Uint8 status;       /* 0x30 (r) */
    Uint8 control;      /* 0x31 (r), 0xB1 (w) */
    Uint8 ram2[32];     /* 0x40 - 0x5F (r), 0xC0 - 0xDF (w) */
} newrtc;

#define RTC_ADDR_NEWRAM 0x40

int newrtc_interface_io(Uint8 rtdatabit) {
    
    phase++;
    
    if (phase<=8) {
        rtc_addr = (rtc_addr<<1)|(rtdatabit?1:0);
    } else {
        
        if (phase==9) {
            if (!(rtc_addr&RTC_ADDR_WRITE)) {
                if (rtc_addr&RTC_ADDR_CLOCK) {
                    rtc_val = newrtc_get_clock(rtc_addr);
                } else {
                    if (rtc_addr&RTC_ADDR_NEWRAM) {
                        rtc_val = newrtc.ram2[rtc_addr&0x1F];
                    } else {
                        rtc_val = rtc.ram[rtc_addr&0x1F];
                    }
                    
                    Log_Printf(LOG_RTC_LEVEL,"[newRTC] reading val $%02X from addr $%02X at PC=$%08x\n",
                               rtc_val,rtc_addr,m68k_getpc());
                }
            }
        }
        
        if (rtc_addr&RTC_ADDR_WRITE) {
            rtc_val = (rtc_val<<1)|(rtdatabit?1:0);
        } else {
            rtdatabit = (rtc_val&(1<<(16-phase)))?1:0;
        }
        
        if (phase==16) {
            if (rtc_addr&RTC_ADDR_WRITE) {
                Log_Printf(LOG_RTC_LEVEL,"[newRTC] writing val $%02X to addr $%02X at PC=$%08x\n",
                           rtc_val,rtc_addr,m68k_getpc());
                
                if (rtc_addr&RTC_ADDR_CLOCK) {
                    newrtc_put_clock(rtc_addr, rtc_val);
                } else {
                    if (rtc_addr&RTC_ADDR_NEWRAM) {
                        newrtc.ram2[rtc_addr&0x1F] = rtc_val;
                    } else {
                        rtc.ram[rtc_addr&0x1F] = rtc_val;
                    }
                }
            }
            
            switch (rtc_addr) {
                case 0x7F: rtc_addr = 0x00; break;
                case 0xFF: rtc_addr = 0x80; break;
                default: rtc_addr++; break;
            }
            phase-=8;
        }
    }
    
    /* RTC returns 0 or 1 */
    return rtdatabit;
}


Uint8 newrtc_get_clock(Uint8 addr) {
    Uint8 val = 0x00;
    
    newrtc.timecntr = time(NULL);
    
    switch (rtc_addr&RTC_ADDR_MASK) {
        case 0x20:
            val = (newrtc.timecntr>>24)&0xFF; break;
        case 0x21:
            val = (newrtc.timecntr>>16)&0xFF; break;
        case 0x22:
            val = (newrtc.timecntr>>8)&0xFF; break;
        case 0x23:
            val = newrtc.timecntr&0xFF; break;
            
        case 0x24:
            val = (newrtc.alarmcntr>>24)&0xFF; break;
        case 0x25:
            val = (newrtc.alarmcntr>>16)&0xFF; break;
        case 0x26:
            val = (newrtc.alarmcntr>>8)&0xFF; break;
        case 0x27:
            val = newrtc.alarmcntr&0xFF; break;

        case 0x30: /* status register */
            val = newrtc.status|NRTC_NEWCHIP;
            break;
        case 0x31: /* control register */
            val = newrtc.control&~NRTC_CTRL_0; break;
            
        default: break;
    }
    
    return val;
}

void newrtc_put_clock(Uint8 addr, Uint8 val) {
    switch (rtc_addr&RTC_ADDR_MASK) {
        case 0x20:
        case 0x21:
        case 0x22:
        case 0x23:
            break; /* not yet! */
        case 0x24:
        case 0x25:
        case 0x26:
        case 0x27:
            break; /* not yet! */
            
        case 0x31: /* control register */
            newrtc.control = val;
            if (newrtc.control&NRTC_CLRFTU) {
                newrtc.status&= ~NRTC_FIRSTUP;
            }
            if (newrtc.control&NRTC_CLRALARM) {
                newrtc.status&= ~NRTC_INT_ALARM;
            }
            if (newrtc.control&NRTC_CLRPDOWN) {
                newrtc.status&= ~NRTC_INT_PDOWN;
            }
            if (newrtc.control&NRTC_POWERDOWN) {
                Log_Printf(LOG_WARN, "[newRTC] Power down!");
                M68000_Stop();
            }
            break;
            
        default: break;
    }
}

void newrtc_request_power_down(void) {
    newrtc.status |= (NRTC_INT|NRTC_INT_PDOWN);
}


/* ---------------------- RTC NVRAM ---------------------- */

// file mon/nvram.h
// struct nvram_info {
// #define	NI_RESET	9
// 	u_int	ni_reset : 4,
// #define	SCC_ALT_CONS	0x08000000
// 		ni_alt_cons : 1,
// #define	ALLOW_EJECT	0x04000000
// 		ni_allow_eject : 1,
// 		ni_vol_r : 6,
// 		ni_brightness : 6,
// #define	HW_PWD	0x6
// 		ni_hw_pwd : 4,
// 		ni_vol_l : 6,
// 		ni_spkren : 1,
// 		ni_lowpass : 1,
// #define	BOOT_ANY	0x00000002
// 		ni_boot_any : 1,
// #define	ANY_CMD		0x00000001
// 		ni_any_cmd : 1;
// #define	NVRAM_HW_PASSWD	6
// 	u_char ni_ep[NVRAM_HW_PASSWD];
// #define	ni_enetaddr	ni_ep
// #define	ni_hw_passwd	ni_ep
// 	u_short ni_simm;		/* 4 SIMMs, 4 bits per SIMM */
// 	char ni_adobe[2];
// 	u_char ni_pot[3];
// 	u_char	ni_new_clock_chip : 1,
// 		ni_auto_poweron : 1,
// 		ni_use_console_slot : 1,	/* Console slot was set by user. */
// 		ni_console_slot : 2,		/* Preferred console dev slot>>1 */
// 		ni_use_parity_mem : 1,	/* Use parity RAM if available? */
// 		: 2;
// #define	NVRAM_BOOTCMD	12
// 	char ni_bootcmd[NVRAM_BOOTCMD];
// 	u_short ni_cksum;
// };

// #define	N_brightness	0
// #define	N_volume_l	1
// #define	N_volume_r	2

/* nominal values during self test */
// #define	BRIGHT_NOM	20
// #define	VOL_NOM		0

/* bits in ni_pot[0] */
#define POT_ON              0x01
#define EXTENDED_POT        0x02
#define LOOP_POT            0x04
#define VERBOSE_POT         0x08
#define TEST_DRAM_POT       0x10
#define BOOT_POT            0x20
#define TEST_MONITOR_POT    0x40

/* bits in byte 17 */
#define NEW_CLOCK_CHIP      0x80
#define AUTO_POWERON        0x40
#define USE_CONSOLE_SLOT    0x20
#define CONSOLE_SLOT        0x18
#define USE_PARITY_MEM      0x04

/* bits in ni_simm (rtc ram byte 10 and 11) *
 * -------- -----xxx bit 0 - 2: 1st simm: bit 1+2 define size, bit 3 defines page mode
 * -------- --xxx--- bit 3 - 5: 2nd simm: bit 1+2 define size, bit 3 defines page mode
 * -------x xx------ bit 6 - 8: 3rd simm: bit 1+2 define size, bit 3 defines page mode
 * ----xxx- -------- bit 9 -11: 4th simm: bit 1+2 define size, bit 3 defines page mode
 * xxxx---- -------- bit 12-15: defines parity, 1 bit for each simm (ignored on 68030)
 */
/* for 68030 and monochrome non-turbo systems */
#define SIMM_EMPTY          0x0
#define SIMM_16MB           0x1 /* Group of four 4 Mbyte SIMMs */
#define SIMM_4MB            0x2 /* Group of four 1 Mbyte SIMMs */
#define SIMM_1MB            0x3 /* Group of four 256 KByte SIMMs */
#define SIMM_PAGE_MODE      0x4 /* SIMM type is page mode, else nibble mode */
/* for all 68040 systems */
#define SIMM_PARITY         0x8 /* SIMMs support parity */
/* for non-turbo color systems */
#define SIMM_8MB_C          0x1 /* Pair of 4 Mbyte SIMMs */
#define SIMM_2MB_C          0x2 /* Pair of 1 Mbyte SIMMs */
#define SIMM_EMPTY2         0x3 /* reserved */
/* for turbo systems */
#define SIMM_32MB_T         0x1 /* Pair of 16 or 32 MByte SIMMs (front or back) */
#define SIMM_8MB_T          0x2 /* Pair of 4 or 8 MByte SIMMs (front or back) */
#define SIMM_2MB_T          0x3 /* Pair of 1 or 2 MByte SIMMs (front or back) */


/* RTC RAM */
Uint8 nvram_default[32]={
    0x94,0x0f,0x40,0x00, // byte 0 - 3
    0x00,0x00,0x00,0x00,0x00,0x00, // byte 4 - 9: hardware password, ethernet address (?)
    0x00,0x00, // byte 10, 11: simm type and size (4 simms, 4 bits per simm), see bits in ni_simm above
    0x00,0x00, // byte 12, 13: adobe (?)
    0x4b,0x00,0x00, // byte 14: POT, byte 15: oldest ..., byte 16: most recent selftest error code
    0x00, // byte 17: bit7:clock chip; 6:auto poweron; 5:enable console slot; 3,4:console slot; 2:parity mem
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // byte 18 - 29: boot command
    0x0F,0x13 // byte 30, 31: checksum
};

void nvram_init(void) {
    /* Reset RTC RAM to default values */
    memcpy(rtc.ram, nvram_default, sizeof(nvram_default));
    
    /* Build boot command */
    switch (ConfigureParams.Boot.nBootDevice) {
        case BOOT_ROM:
            rtc.ram[18] = 0x00;
            rtc.ram[19] = 0x00;
            break;
        case BOOT_SCSI:
            rtc.ram[18] = 's';
            rtc.ram[19] = 'd';
            break;
        case BOOT_ETHERNET:
            rtc.ram[18] = 'e';
            rtc.ram[19] = 'n';
            break;
        case BOOT_MO:
            rtc.ram[18] = 'o';
            rtc.ram[19] = 'd';
            break;
        case BOOT_FLOPPY:
            rtc.ram[18] = 'f';
            rtc.ram[19] = 'd';
            break;
            
        default: break;
    }
    
    /* Copy ethernet address from ROM to RTC RAM */
    int i;
    for (i = 0; i<6; i++) {
        rtc.ram[i+4]=NEXTRom[i+8];
    }
    
    /* Build SIMM bytes */
    Uint16 SIMMconfig = 0x0000;
    Uint8 simm[4];
    Uint8 parity = 0xF0;
    if (ConfigureParams.System.bTurbo) {
        for (i = 0; i<4; i++) {
            switch (MemBank_Size[i]>>20) {
                case 0: simm[i] = SIMM_EMPTY; parity &= ~(0x10<<i); break;
                case 2: simm[i] = SIMM_2MB_T; break;
                case 8: simm[i] = SIMM_8MB_T; break;
                case 32: simm[i] = SIMM_32MB_T; break;
                default: simm[i] = SIMM_EMPTY; break;
            }
        }
        
    } else if (ConfigureParams.System.bColor) {
        for (i = 0; i<4; i++) {
            switch (MemBank_Size[i]>>20) {
                case 0: simm[i] = SIMM_EMPTY; parity &= ~(0x10<<i); break;
                case 2: simm[i] = SIMM_2MB_C; break;
                case 8: simm[i] = SIMM_8MB_C; break;
                default: simm[i] = SIMM_EMPTY; break;
            }
        }
        
    } else {
        for (i = 0; i<4; i++) {
            switch (MemBank_Size[i]>>20) {
                case 0: simm[i] = SIMM_EMPTY; parity &= ~(0x10<<i); break;
                case 1: simm[i] = SIMM_1MB | SIMM_PAGE_MODE; break;
                case 4: simm[i] = SIMM_4MB | SIMM_PAGE_MODE; break;
                case 16: simm[i] = SIMM_16MB | SIMM_PAGE_MODE; break;
                default: simm[i] = SIMM_EMPTY | SIMM_PAGE_MODE; break;
            }
        }
    }
    
    SIMMconfig = ((parity&0xF0)<<8) | (simm[3]<<9) | (simm[2]<<6) | (simm[1]<<3) | simm[0];
    rtc.ram[10] = (SIMMconfig>>8)&0xFF;
    rtc.ram[11] = SIMMconfig&0xFF;
    
    /* Build POT byte[0] */
    rtc.ram[14] = 0x00;
    if (ConfigureParams.Boot.bEnableDRAMTest)
        rtc.ram[14] |= TEST_DRAM_POT;
    if (ConfigureParams.Boot.bEnablePot)
        rtc.ram[14] |= POT_ON;
    if (ConfigureParams.Boot.bEnableSoundTest)
        rtc.ram[14] |= TEST_MONITOR_POT;
    if (ConfigureParams.Boot.bEnableSCSITest)
        rtc.ram[14] |= EXTENDED_POT;
    if (ConfigureParams.Boot.bLoopPot)
        rtc.ram[14] |= LOOP_POT;
    if (ConfigureParams.Boot.bVerbose)
        rtc.ram[14] |= VERBOSE_POT;
    if (ConfigureParams.Boot.bExtendedPot)
        rtc.ram[14] |= BOOT_POT;
    
    /* Set clock chip bit */
    switch (ConfigureParams.System.nRTC) {
        case MCCS1850: rtc.ram[17] |= NEW_CLOCK_CHIP; break;
        case MC68HC68T1: rtc.ram[17] &= ~NEW_CLOCK_CHIP; break;
        default: break;
    }
    
    /* Re-calculate checksum */
    nvram_checksum(1);
}

void nvram_checksum(int force) {
	int sum,i;
	sum=0;
	for (i=0;i<30;i+=2) {
		sum+=(rtc.ram[i]<<8)|(rtc.ram[i+1]);
		if (sum>=0x10000) {
			sum-=0x10000;
			sum+=1;
		}
	}
    
	sum=0xFFFF-sum;
    
	if (force) {
		rtc.ram[30]=(sum&0xFF00)>>8;
		rtc.ram[31]=(sum&0xFF);
		Log_Printf(LOG_WARN,"Forcing RTC checksum to %x %x",rtc.ram[30],rtc.ram[31]);
	} else {
		Log_Printf(LOG_WARN,"Check RTC checksum to %x %x %x %x",
                   rtc.ram[30],(sum&0xFF00)>>8,
                   rtc.ram[31],(sum&0xFF));
	}
}


#if 1
static char rtc_ram_info[1024];
char * get_rtc_ram_info(void) {
    char buf[256];
    int sum;
    int i;
    int ni_vol_l,ni_vol_r,ni_brightness;
    int ni_hw_pwd,ni_spkren,ni_lowpass;
    sprintf(buf,"Rtc info:\n");
    strcpy(rtc_ram_info,buf);
    
    // struct nvram_info {
    // #define	NI_RESET	9
    // 	u_int	ni_reset : 4,
    
    sprintf(buf,"RTC RESET:x%1X ",rtc.ram[0]>>4);
    strcat(rtc_ram_info,buf);
    
    // #define	SCC_ALT_CONS	0x08000000
    // 		ni_alt_cons : 1,
    if (rtc.ram[0]&0x08) strcat(rtc_ram_info,"ALT_CONS ");
    // #define	ALLOW_EJECT	0x04000000
    // 		ni_allow_eject : 1,
    if (rtc.ram[0]&0x04) strcat(rtc_ram_info,"ALLOW_EJECT ");
    // 		ni_vol_r : 6,
    // 		ni_brightness : 6,
    // #define	HW_PWD	0x6
    // 		ni_hw_pwd : 4,
    // 		ni_vol_l : 6,
    // 		ni_spkren : 1,
    // 		ni_lowpass : 1,
    // #define	BOOT_ANY	0x00000002
    // 		ni_boot_any : 1,
    // #define	ANY_CMD		0x00000001
    // 		ni_any_cmd : 1;
    
    ni_vol_r=(((rtc.ram[0]&0x3)<<4)|((rtc.ram[1]&0xF0)>>4));
    ni_brightness=(((rtc.ram[1]&0xF)<<2)|((rtc.ram[2]&0xC0)>>6));
    ni_vol_l=((rtc.ram[2]&0x3F)<<2);
    ni_hw_pwd=(rtc.ram[3]&0xF0)>>4;
    sprintf(buf,"VOL_R:x%1X BRIGHT:x%1X HWPWD:x%1X VOL_L:x%1X",ni_vol_r,ni_brightness,ni_vol_l,ni_hw_pwd);
    strcat(rtc_ram_info,buf);
    
    if (rtc.ram[3]&0x08) strcat(rtc_ram_info,"SPK_ENABLE ");
    if (rtc.ram[3]&0x04) strcat(rtc_ram_info,"LOW_PASS ");
    if (rtc.ram[3]&0x02) strcat(rtc_ram_info,"BOOT_ANY ");
    if (rtc.ram[3]&0x01) strcat(rtc_ram_info,"ANY_CMD ");
    
    
    
    // #define	NVRAM_HW_PASSWD	6
    // 	u_char ni_ep[NVRAM_HW_PASSWD];
    
    sprintf(buf,"NVRAM_HW_PASSWD:%2X %2X %2X %2X %2X %2X ",rtc.ram[4],rtc.ram[5],rtc.ram[6],rtc.ram[7],rtc.ram[8],rtc.ram[9]);
    strcat(rtc_ram_info,buf);
    // #define	ni_enetaddr	ni_ep
    // #define	ni_hw_passwd	ni_ep
    // 	u_short ni_simm;		/* 4 SIMMs, 4 bits per SIMM */
    sprintf(buf,"SIMM:%1X %1X %1X %1X ",rtc.ram[10]>>4,rtc.ram[10]&0x0F,rtc.ram[11]>>4,rtc.ram[11]&0x0F);
    strcat(rtc_ram_info,buf);
    
    
    // 	char ni_adobe[2];
    sprintf(buf,"ADOBE:%2X %2X ",rtc.ram[12],rtc.ram[13]);
    strcat(rtc_ram_info,buf);
    
    // 	u_char ni_pot[3];
    sprintf(buf,"POT:%2X %2X %2X ",rtc.ram[14],rtc.ram[15],rtc.ram[16]);
    strcat(rtc_ram_info,buf);
    
    // 	u_char	ni_new_clock_chip : 1,
    // 		ni_auto_poweron : 1,
    // 		ni_use_console_slot : 1,	/* Console slot was set by user. */
    // 		ni_console_slot : 2,		/* Preferred console dev slot>>1 */
    // 		ni_use_parity_mem : 1,	/* Use parity RAM if available? */
    // 		: 2;
    if (rtc.ram[17]&0x80) strcat(rtc_ram_info,"NEW_CLOCK_CHIP ");
    if (rtc.ram[17]&0x40) strcat(rtc_ram_info,"AUTO_POWERON ");
    if (rtc.ram[17]&0x20) strcat(rtc_ram_info,"CONSOLE_SLOT ");
    
    sprintf(buf,"console_slot:%X ",(rtc.ram[17]&0x18)>>3);
    strcat(rtc_ram_info,buf);
    
    if (rtc.ram[17]&0x04) strcat(rtc_ram_info,"USE_PARITY ");
    
    
    strcat(rtc_ram_info,"boot_command:");
    for (i=0;i<12;i++) {
        if ((rtc.ram[18+i]>=0x20) && (rtc.ram[18+i]<=0x7F)) {
            sprintf(buf,"%c",rtc.ram[18+i]);
            strcat(rtc_ram_info,buf);
        }
    }
    
    strcat(rtc_ram_info," ");
    sprintf(buf,"CKSUM:%2X %2X ",rtc.ram[30],rtc.ram[31]);
    strcat(rtc_ram_info,buf);
    
    
    sum=0;
    for (i=0;i<30;i+=2) {
        sum+=(rtc.ram[i]<<8)|(rtc.ram[i+1]);
        if (sum>=0x10000) { sum-=0x10000;
            sum+=1;
        }
    }
    
    sum=0xFFFF-sum;
    
    sprintf(buf,"CALC_CKSUM:%04X ",sum&0xFFFF);
    strcat(rtc_ram_info,buf);
    
    // #define	NVRAM_BOOTCMD	12
    // 	char ni_bootcmd[NVRAM_BOOTCMD];
    // 	u_short ni_cksum;
    // };
    
    return rtc_ram_info;
}
#endif