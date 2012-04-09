/*  Previous - scc.c
 
 This file is distributed under the GNU Public License, version 2 or at
 your option any later version. Read the file gpl.txt for details.
 
 Serial Communication Controller (Zilog 8530) Emulation.
 
 Based on MESS source code.
 
 Port to Previous incomplete. Hacked to pass power-on test --> see SCC_Reset()
 
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

void updateirqs(void);
void resetchannel(int ch);
void initchannel(int ch);

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
    Uint8 reg;
    Uint8 data;
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


void SCC_Read(void) {
    bool data;
        
    if (IoAccessCurrentAddress&0x1)
        ch = A;
    else
        ch = B;
    
    if (IoAccessCurrentAddress&0x2)
        data = true;
    else
        data = false;
    
    if (data) {
        IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = channel[ch].data;
        Log_Printf(LOG_SCC_LEVEL, "SCC %c, Data read: %02x\n", ch == A?'A':'B', channel[ch].data);
    } else {
        IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = channel[ch].wreg[channel[ch].reg];
        Log_Printf(LOG_SCC_LEVEL, "SCC %c, Reg%i read: %02x\n", ch == A?'A':'B', channel[ch].reg, channel[ch].wreg[channel[ch].reg]);
        
        regnum[ch] = -1;
    }
}


void SCC_Write(void) {
    bool data;
    
    if (IoAccessCurrentAddress&0x1)
        ch = A;
    else
        ch = B;
    
    if (IoAccessCurrentAddress&0x2)
        data = true;
    else
        data = false;
    
    if (data) {
        channel[ch].data = IoMem[IoAccessCurrentAddress&IO_SEG_MASK];
        Log_Printf(LOG_SCC_LEVEL, "SCC %c, Data write: %02x\n", ch == A?'A':'B', channel[ch].data);
        channel[ch].wreg[0] = 0x04|0x01; // Tx buffer empty | Rx Character Available
        return;
    }
    
    if (regnum[ch] < 0) {
        channel[ch].reg = regnum[ch] = IoMem[IoAccessCurrentAddress&IO_SEG_MASK];
        return;
    } else if (regnum[ch] >= 0) {
        channel[ch].reg = regnum[ch];
        channel[ch].wreg[channel[ch].reg] = IoMem[IoAccessCurrentAddress&IO_SEG_MASK];
        Log_Printf(LOG_SCC_LEVEL, "SCC %c, Reg%i write: %02x\n", ch == A?'A':'B', channel[ch].reg, channel[ch].wreg[channel[ch].reg]);
    
        switch (channel[ch].reg) {
            case 0:
                switch ((channel[ch].wreg[channel[ch].reg]>>3) & 7) {
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
                channel[ch].extIRQEnable = (channel[ch].wreg[channel[ch].reg]&1);
                channel[ch].txIRQEnable = (channel[ch].wreg[channel[ch].reg]&2)?1:0;
                channel[ch].rxIRQEnable = ((channel[ch].wreg[channel[ch].reg]>>3)&3);
                updateirqs();
                
                if (channel[ch].wreg[channel[ch].reg]&0x40 && channel[ch].wreg[channel[ch].reg]&0x80) {
                    channel[ch].data = *dma_memory_read(1, CHANNEL_SCC);
                    channel[ch].wreg[0] = 0x01; // Rx Character Available
                }
                break;
                
            case 2: // IRQ vector
                IRQV = channel[ch].wreg[channel[ch].reg];
                break;
                
            case 3: // Rx parameters and controls
                channel[ch].rxEnable = channel[ch].wreg[channel[ch].reg]&1;
                channel[ch].syncHunt = (channel[ch].wreg[channel[ch].reg]&0x10)?1:0;
                break;
                
            case 5: // Tx parameters and controls
                channel[ch].rxEnable = channel[ch].wreg[channel[ch].reg]&8;
                if (channel[ch].txEnable)
                    channel[ch].wreg[0] |= 0x04; // Tx empty
                
            case 4: // Tx/Rx misc parameters and modes
            case 6: // sync chars/SDLC address field
            case 7: // sync char/SDLC flag
                break;
                
            case 9: // master IRQ control
                MasterIRQEnable = (channel[ch].wreg[channel[ch].reg]&8)?1:0;
                updateirqs();
                
                // channel reset command
                switch ((channel[ch].wreg[channel[ch].reg]>>6)&3) {
                    case 0: break; // do nothing
                    case 1: resetchannel(0); break; // reset channel B
                    case 2: resetchannel(1); break; // reset channel A
                    case 3: // force h/w reset (entire chip)
                        SCC_Reset();
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
                if (channel[ch].wreg[channel[ch].reg]&0x01) // baud rate generator enable?
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
		Log_Printf(LOG_SCC_LEVEL, "SCC8530 IRQ status => %d\n", irqstat);

        if (irqstat) {
            set_interrupt(INT_SCC, SET_INT);
            Log_Printf(LOG_SCC_LEVEL, "SCC: Raise IRQ");
        }else{
            set_interrupt(INT_SCC, RELEASE_INT);
            Log_Printf(LOG_SCC_LEVEL, "SCC: Lower IRQ");
        }
        
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

void SCC_Reset(void) {
    Log_Printf(LOG_WARN, "SCC: Device Reset (Hacked!)");
    IRQType = IRQ_NONE;
    MasterIRQEnable = 0;
    IRQV = 0;
    
    initchannel(0);
    initchannel(1);
    resetchannel(0);
    resetchannel(1);
    
    regnum[0] = -1;
    regnum[1] = -1;
    
    /*--- Hack to pass power-on test ---*/
    channel[0].wreg[0] = 0xFF;
    channel[1].wreg[0] = 0xFF;
}