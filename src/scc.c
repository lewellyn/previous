/*  Previous - scc.c
 
 This file is distributed under the GNU Public License, version 2 or at
 your option any later version. Read the file gpl.txt for details.
 
 Serial Communication Controller (Zilog 8530) Emulation.
 
 Based on MESS source code.
 
 Port to Previous incomplete.
 
 */

#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "configuration.h"
#include "scc.h"
#include "sysReg.h"
#include "dma.h"

#define LOG_SCC_LEVEL LOG_WARN
#define IO_SEG_MASK	0x1FFFF

Uint8 test[1];

void updateirqs(void);
void resetchannel(int ch);
void initchannel(int ch);
void device_reset(void);

/* Variables */
int MasterIRQEnable;
int lastIRQStat;
typedef enum {
    IRQ_NONE,
    IRQ_B_TX,
    IRQ_A_TX,
    IRQ_B_EXT,
    IRQ_A_EXT
} IRQ_TYPES;

IRQ_TYPES IRQType;

typedef struct {
    Uint8 rreg[16];
    Uint8 wreg[16];
    bool txIRQEnable;
    bool txIRQPending;
    bool extIRQEnable;
    bool extIRQPending;
    bool rxIRQEnable;
    bool rxIRQPending;
    bool rxEnable;
    bool txEnable;
    bool syncHunt;
    bool txUnderrun;
} SCC_CHANNEL;

SCC_CHANNEL channel[2];

int IRQV;

enum {
    B = 0,
    A = 1
} ch;


Sint8 regnum[2];
Sint8 rregnum[2];

void SCC_Reset(void) {
    regnum[0] = -1;
    regnum[1] = -1;
    
    rregnum[0] = -1;
    rregnum[1] = -1;
}

void SCC_Read(void) {
    Uint8 reg;
    Log_Printf(LOG_WARN, "SCC Read at %08x", IoAccessCurrentAddress);
    
    if (IoAccessCurrentAddress&0x1)
        ch = A;
    else
        ch = B;
            
    if (rregnum[ch] < 0) {
        IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = reg = rregnum[ch] = 1;
        return;
    } else if (rregnum[ch] >= 0) {
        reg = rregnum[ch];
        IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = channel[ch].rreg[reg];
        printf("SCC %c, Reg%i read: %02x\n", ch == A?'A':'B', reg, channel[ch].rreg[reg]);
        
        rregnum[ch] = -1;
    }
}

void SCC_Write(void) {
    Uint8 reg;
    
    if (IoAccessCurrentAddress&0x1)
        ch = A;
    else
        ch = B;
    
    if (regnum[ch] < 0) {
        reg = regnum[ch] = IoMem[IoAccessCurrentAddress&IO_SEG_MASK];
        return;
    } else if (regnum[ch] >= 0) {
        reg = regnum[ch];
        channel[ch].wreg[reg] = IoMem[IoAccessCurrentAddress&IO_SEG_MASK];
        printf("SCC %c, Reg%i write: %02x\n", ch == A?'A':'B', reg, channel[ch].wreg[reg]);
    
        switch (reg) {
            case 0:
                switch ((channel[ch].wreg[reg]>>3) & 7) {
                    case 1: break; // select high registers (handled elsewhere)
                    case 2: channel[0].txIRQPending = false; break; // reset external and status IRQs
                    case 5: updateirqs(); break; // ack Tx IRQ
                    case 0: // nothing
                    case 3: // send SDLC abort
                    case 4: // enable IRQ on next Rx byte
                    case 6: // reset errors
                    case 7: // reset highest IUS
                        break; // not handled
                }
                break;
                
            case 1: // Tx/Rx IRQ and data transfer mode definition
                channel[ch].extIRQEnable = (channel[ch].wreg[reg]&1);
                channel[ch].txIRQEnable = (channel[ch].wreg[reg]&2)?1:0;
                channel[ch].rxIRQEnable = ((channel[ch].wreg[reg]>>3)&3);
                updateirqs();
                
                if (channel[ch].wreg[reg]&0x40 && channel[ch].wreg[reg]&0x80)
                    channel[ch].rreg[reg]= *dma_memory_read(1, CHANNEL_SCC);
                break;
                
            case 2: // IRQ vector
                IRQV = channel[ch].wreg[reg];
                break;
                
            case 3: // Rx parameters and controls
                channel[ch].rxEnable = channel[ch].wreg[reg]&1;
                channel[ch].syncHunt = (channel[ch].wreg[reg]&0x10)?1:0;
                break;
                
            case 5: // Tx parameters and controls
                channel[ch].rxEnable = channel[ch].wreg[reg]&8;
                if (channel[ch].txEnable)
                    channel[ch].rreg[0] |= 0x40; // Tx empty
                
            case 4: // Tx/Rx misc parameters and modes
            case 6: // sync chars/SDLC address field
            case 7: // sync char/SDLC flag
                break;
                
            case 9: // master IRQ control
                MasterIRQEnable = (channel[ch].wreg[reg]&8)?1:0;
                updateirqs();
                
                // channel reset command
                switch ((channel[ch].wreg[reg]>>6)&3) {
                    case 0: break; // do nothing
                    case 1: resetchannel(0); break; // reset channel B
                    case 2: resetchannel(1); break; // reset channel A
                    case 3: // force h/w reset (entire chip)
                        device_reset();
                        updateirqs();
                        break;
                }
                break;
                
            case 10: // misc transmitter/receiver control bits
            case 11: // clock mode control
            case 12: // lower byte of baud rate gen
            case 13: // upper byte of baud rate gen
                break;
                
            case 14: // misc control bits
                if (channel[ch].wreg[reg]&0x01) // baud rate generator enable?
                {} // later
                break;
                
            case 15: // later
                break;
        }
        
        regnum[ch] = -1;
    }
}



/* Functions */

void updateirqs(void)
{
	int irqstat;
    
	irqstat = 0;
	if (MasterIRQEnable)
	{
		if ((channel[0].txIRQEnable) && (channel[0].txIRQPending))
		{
			IRQType = IRQ_B_TX;
			irqstat = 1;
		}
		else if ((channel[1].txIRQEnable) && (channel[1].txIRQPending))
		{
			IRQType = IRQ_A_TX;
			irqstat = 1;
		}
		else if ((channel[0].extIRQEnable) && (channel[0].extIRQPending))
		{
			IRQType = IRQ_B_EXT;
			irqstat = 1;
		}
		else if ((channel[1].extIRQEnable) && (channel[1].extIRQPending))
		{
			IRQType = IRQ_A_EXT;
			irqstat = 1;
		}
	}
	else
	{
		IRQType = IRQ_NONE;
	}
    
    //  printf("SCC: irqstat %d, last %d\n", irqstat, lastIRQStat);
    //  printf("ch0: en %d pd %d  ch1: en %d pd %d\n", channel[0].txIRQEnable, channel[0].txIRQPending, channel[1].txIRQEnable, channel[1].txIRQPending);
    
	// don't spam the driver with unnecessary transitions
	if (irqstat != lastIRQStat)
	{
		lastIRQStat = irqstat;
        
		// tell the driver the new IRQ line status if possible
		printf("SCC8530 IRQ status => %d\n", irqstat);

        set_interrupt(INT_SCC, SET_INT);
//		if(!intrq_cb.isnull())
//			intrq_cb(irqstat);
	}
}


void resetchannel(int ch)
{
//	emu_timer *timersave = channel[ch].baudtimer;
    
	memset(&channel[ch], 0, sizeof(SCC_CHANNEL));
    
	channel[ch].txUnderrun = 1;
//	channel[ch].baudtimer = timersave;
    
//	channel[ch].baudtimer->adjust(attotime::never, ch);
}

void initchannel(int ch)
{
	channel[ch].syncHunt = 1;
}

void device_reset(void) {
    IRQType = IRQ_NONE;
    MasterIRQEnable = 0;
    IRQV = 0;
    
    initchannel(0);
    initchannel(1);
    resetchannel(0);
    resetchannel(1);
    
    regnum[0] = -1;
    regnum[1] = -1;
    
    rregnum[0] = -1;
    rregnum[1] = -1;
}