/*  Previous - kms.c
 
 This file is distributed under the GNU Public License, version 2 or at
 your option any later version. Read the file gpl.txt for details.
 
 Keyboard, Mouse and Sound logic Emulation.
 
 In real hardware this logic is located in the NeXT Megapixel Display 
 or Soundbox
 
 */

#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "configuration.h"
#include "kms.h"
#include "sysReg.h"
#include "dma.h"

#define LOG_KMS_LEVEL LOG_WARN
#define IO_SEG_MASK	0x1FFFF


struct {
    struct {
        Uint8 snd_dma;
        Uint8 km;
        Uint8 transmit;
        Uint8 cmd;
    } status;
    
    Uint32 data;
    Uint32 km_data;
} kms;


/* KMS control and status register (0x0200E000) 
 *
 * x--- ---- ---- ---- ---- ---- ---- ----  sound out enable (r/w)
 * -x-- ---- ---- ---- ---- ---- ---- ----  sound output request (r)
 * --x- ---- ---- ---- ---- ---- ---- ----  sound output underrun detected (r/w)
 * ---- x--- ---- ---- ---- ---- ---- ----  sound in enable (r/w)
 * ---- -x-- ---- ---- ---- ---- ---- ----  sound input request (r)
 * ---- --x- ---- ---- ---- ---- ---- ----  sound input overrun detected (r/w)
 *
 * ---- ---- x--- ---- ---- ---- ---- ----  keyboard interrupt (r)
 * ---- ---- -x-- ---- ---- ---- ---- ----  keyboard data received (r)
 * ---- ---- --x- ---- ---- ---- ---- ----  keyboard data overrun detected (r/w)
 * ---- ---- ---x ---- ---- ---- ---- ----  non-maskable interrupt received (tilde and left or right cmd key) (r/w)
 * ---- ---- ---- x--- ---- ---- ---- ----  kms interrupt (r)
 * ---- ---- ---- -x-- ---- ---- ---- ----  kms data received (r)
 * ---- ---- ---- --x- ---- ---- ---- ----  kms data overrun detected (r/w)
 *
 * ---- ---- ---- ---- x--- ---- ---- ----  dma sound out transmit pending (r)
 * ---- ---- ---- ---- -x-- ---- ---- ----  dma sound out transmit in progress (r)
 * ---- ---- ---- ---- --x- ---- ---- ----  cpu data transmit pending (r)
 * ---- ---- ---- ---- ---x ---- ---- ----  cpu data transmit in progress (r)
 * ---- ---- ---- ---- ---- x--- ---- ----  rtx_pend ???
 * ---- ---- ---- ---- ---- -x-- ---- ----  rtx ???
 * ---- ---- ---- ---- ---- --x- ---- ----  kms enable (return from reset state) (r/w)
 * ---- ---- ---- ---- ---- ---x ---- ----  loop back transmitter data (r/w)
 *
 * ---- ---- ---- ---- ---- ---- xxxx xxxx  command to append on kms data (r/w)
 *
 * ---x ---x ---- ---x ---- ---- ---- ----  zero bits
 */


#define SNDOUT_DMA_ENABLE   0x80
#define SNDOUT_DMA_REQUEST  0x40
#define SNDOUT_DMA_UNDERRUN 0x20
#define SNDIN_DMA_ENABLE    0x08
#define SNDIN_DMA_REQUEST   0x04
#define SNDIN_DMA_OVERRUN   0x02

#define KBD_INT             0x80
#define KBD_RECEIVED        0x40
#define KBD_OVERRUN         0x20
#define NMI_RECEIVED        0x10
#define KMS_INT             0x08
#define KMS_RECEIVED        0x04
#define KMS_OVERRUN         0x02

#define TX_DMA_PENDING      0x80
#define TX_DMA              0x40
#define TX_CPU_PENDING      0x20
#define TX_CPU              0x10
#define RTX_PEND            0x08
#define RTX                 0x04
#define KMS_ENABLE          0x02
#define TX_LOOP             0x01


/* KMS commands */
#define KMSCMD_RESET    0xFF
#define KMSCMD_ASNDOUT  0xC7    /* analog sound out */
#define KMSCMD_KMREG    0xC5    /* access keyboard or mouse register */
#define KMSCMD_CTRLOUT  0xC4    /* access volume control logic */
#define KMSCMD_VOLCTRL  0xC2    /* simplified access to volume control */

void KMS_Ctrl_Snd_Write(void) {
    Uint8 val = IoMem[IoAccessCurrentAddress&IO_SEG_MASK];
    
    kms.status.snd_dma &= ~(SNDOUT_DMA_ENABLE|SNDIN_DMA_ENABLE);
    kms.status.snd_dma |= (val&(SNDOUT_DMA_ENABLE|SNDIN_DMA_ENABLE));
    
    if (val&SNDOUT_DMA_UNDERRUN) {
        kms.status.snd_dma &= ~(SNDOUT_DMA_UNDERRUN|SNDOUT_DMA_REQUEST);
        set_interrupt(INT_SOUND_OVRUN, RELEASE_INT);
    }
    if (val&SNDIN_DMA_OVERRUN) {
        kms.status.snd_dma &= ~(SNDIN_DMA_OVERRUN|SNDIN_DMA_REQUEST);
        set_interrupt(INT_SOUND_OVRUN, RELEASE_INT);
    }
}

void KMS_Stat_Snd_Read(void) {
    IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = kms.status.snd_dma;
}

void KMS_Ctrl_KM_Write(void) {
    Uint8 val = IoMem[IoAccessCurrentAddress&IO_SEG_MASK];
    
    if (val&KBD_OVERRUN) {
        kms.status.km &= ~(KBD_RECEIVED|KBD_OVERRUN);
        set_interrupt(INT_KEYMOUSE, RELEASE_INT);
    }
    if (val&NMI_RECEIVED) {
        kms.status.km &= ~NMI_RECEIVED;
        set_interrupt(INT_NMI, RELEASE_INT);
    }
    if (val&KMS_OVERRUN) {
        kms.status.km &= ~(KMS_RECEIVED|KMS_OVERRUN);
        set_interrupt(INT_MONITOR, RELEASE_INT);
    }
}

void KMS_Stat_KM_Read(void) {
    IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = kms.status.km;
}

void KMS_Ctrl_TX_Write(void) {
    Uint8 val = IoMem[IoAccessCurrentAddress&IO_SEG_MASK];
    
    kms.status.transmit &= ~(KMS_ENABLE|TX_LOOP);
    kms.status.transmit |= (val&(KMS_ENABLE|TX_LOOP));
}

void KMS_Stat_TX_Read(void) {
    IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = kms.status.transmit;
}

void KMS_Ctrl_Cmd_Write(void) {
    Uint8 val = IoMem[IoAccessCurrentAddress&IO_SEG_MASK];
    
    if (val==0xC5) {
        kms.status.km |= KBD_RECEIVED;
    }
    kms.status.cmd = val;
}

void KMS_Stat_Cmd_Read(void) {
    IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = kms.status.cmd;
}


/* KMS data register (0x0200E004) */

void KMS_Data_Write(void) {
    kms.data = IoMem_ReadLong(IoAccessCurrentAddress&IO_SEG_MASK);
}

void KMS_Data_Read(void) {
    IoMem_WriteLong(IoAccessCurrentAddress&IO_SEG_MASK, kms.data);
}


/* KMS keyboard and mouse data register (0x0200E008) *
 *
 * x--- ---- ---- ---- ---- ---- ---- ----  always 0
 * -x-- ---- ---- ---- ---- ---- ---- ----  1 = no response error, 0 = normal event
 * --x- ---- ---- ---- ---- ---- ---- ----  1 = user poll, 0 = internal poll
 * ---x ---- ---- ---- ---- ---- ---- ----  1 = invalid/master, 0 = valid/slave (user/internal)
 * ---- xxxx ---- ---- ---- ---- ---- ----  1 = mouse, 0 = keyboard
 * ---- ---- xxxx xxxx ---- ---- ---- ----  chip revision: 0 = old, 1 = new, 2 = digital
 *
 * Mouse data:
 * ---- ---- ---- ---- xxxx xxx- ---- ----  mouse y
 * ---- ---- ---- ---- ---- ---x ---- ----  right button up (1) or down (0)
 * ---- ---- ---- ---- ---- ---- xxxx xxx-  mouse x
 * ---- ---- ---- ---- ---- ---- ---- ---x  left button up (1) or down (0)
 *
 * Keyboard data:
 * ---- ---- ---- ---- x--- ---- ---- ----  valid (1) or invalid (0)
 * ---- ---- ---- ---- -x-- ---- ---- ----  right alt
 * ---- ---- ---- ---- --x- ---- ---- ----  left alt
 * ---- ---- ---- ---- ---x ---- ---- ----  right command
 * ---- ---- ---- ---- ---- x--- ---- ----  left command
 * ---- ---- ---- ---- ---- -x-- ---- ----  right shift
 * ---- ---- ---- ---- ---- --x- ---- ----  left shift
 * ---- ---- ---- ---- ---- ---x ---- ----  control
 * ---- ---- ---- ---- ---- ---- x--- ----  key up (1) or down (0)
 * ---- ---- ---- ---- ---- ---- -xxx xxxx  keycode 
 */

#define NO_RESPONSE_ERR 0x40000000
#define USER_POLL       0x20000000
#define INVALID         0x10000000
#define MASTER          0x10000000

#define DEVICE_MOUSE    0x08000000

#define MOUSE_Y         0x0000FE00
#define MOUSE_RIGT_UP   0x00000100
#define MOUSE_X         0x000000FE
#define MOUSE_LEFT_UP   0x00000001

#define KBD_KEY_VALID   0x00008000
#define KBD_MOD_MASK    0x00007F00
#define KBD_KEY_UP      0x00000080
#define KBD_KEY_MASK    0x0000007F

void KMS_KM_Data_Read(void) {
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, kms.km_data);
    
    kms.status.km &= ~(KBD_RECEIVED|KBD_INT);
    set_interrupt(INT_KEYMOUSE, RELEASE_INT);
}

void kms_keydown(Uint8 modkeys, Uint8 keycode) {
    if ((keycode==0x26)&&(modkeys&0x18)) { /* backquote and one or both command keys */
        set_interrupt(INT_NMI, SET_INT);
    }
    
    if ((keycode==0x25)&&((modkeys&0x24)==0x24)) { /* asterisk and left alt and left command key */
        /* keyboard reset: now to? */
    }
    
    if (keycode==0x58) { /* Power key */
        set_interrupt(INT_POWER, SET_INT); /* TODO: how to release? */
    }
    
    kms.km_data = USER_POLL;
    kms.km_data |= (modkeys<<8)|keycode|KBD_KEY_VALID;
    
    if (kms.status.km &KBD_RECEIVED) {
        kms.status.km |= KBD_OVERRUN;
    }
    kms.status.km |= (KBD_RECEIVED|KBD_INT);
    set_interrupt(INT_KEYMOUSE, SET_INT);
}

void kms_keyup(Uint8 modkeys, Uint8 keycode) {
    kms.km_data = USER_POLL;
    kms.km_data |= (modkeys<<8)|keycode|KBD_KEY_VALID|KBD_KEY_UP;
}
