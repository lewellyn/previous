/*  Previous - iethernet.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

   Network adapter for the NEXT. 

   constants and struct netBSD mb8795reg.h file

*/

#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "configuration.h"
#include "esp.h"
#include "sysReg.h"
#include "dma.h"
#include "scsi.h"
#include "ethernet.h"
#include "cycInt.h"

#define LOG_EN_LEVEL        LOG_WARN
#define LOG_EN_REG_LEVEL    LOG_WARN
#define IO_SEG_MASK	0x1FFFF


struct {
    Uint8 tx_status;
    Uint8 tx_mask;
    Uint8 tx_mode;
    Uint8 rx_status;
    Uint8 rx_mask;
    Uint8 rx_mode;
    Uint8 reset;
    
    Uint8 mac_addr[6];
} enet;


#define TXSTAT_READY        0x80    /* r */
#define TXSTAT_NET_BUSY     0x40    /* r */
#define TXSTAT_TX_RECVD     0x20    /* r */
#define TXSTAT_SHORTED      0x10    /* r */
#define TXSTAT_UNDERFLOW    0x08    /* rw */
#define TXSTAT_COLL         0x04    /* rw */
#define TXSTAT_16COLLS      0x02    /* rw */
#define TXSTAT_PAR_ERR      0x01    /* rw */

#define TXMASK_PKT_RDY      0x80
#define TXMASK_TX_RECVD     0x20
#define TXMASK_UNDERFLOW    0x08
#define TXMASK_COLL         0x04
#define TXMASK_16COLLS      0x02
#define TXMASK_PAR_ERR      0x01

#define RXSTAT_PKT_OK       0x80    /* rw */
#define RXSTAT_RESET_PKT    0x10    /* r */
#define RXSTAT_SHORT_PKT    0x08    /* rw */
#define RXSTAT_ALIGN_ERR    0x04    /* rw */
#define RXSTAT_CRC_ERR      0x02    /* rw */
#define RXSTAT_OVERFLOW     0x01    /* rw */

#define RXMASK_PKT_OK       0x80
#define RXMASK_RESET_PKT    0x10
#define RXMASK_SHORT_PKT    0x80
#define RXMASK_ALIGN_ERR    0x40
#define RXMASK_CRC_ERR      0x20
#define RXMASK_OVERFLOW     0x10

#define TXMODE_COLL_ATMPT   0xF0    /* r */
#define TXMODE_IGNORE_PAR   0x08    /* rw */
#define TXMODE_TM           0x04    /* rw */
#define TXMODE_DIS_LOOP     0x02    /* rw */
#define TXMODE_DIS_CONTNT   0x01    /* rw */

#define RXMODE_TEST_CRC     0x80
#define RXMODE_ADDR_SIZE    0x10
#define RXMODE_ENA_SHORT    0x08
#define RXMODE_ENA_RST      0x04
#define RXMODE_MATCH_MODE   0x03

#define RX_NOPACKETS        0   // Accept no packets
#define RX_LIMITED          1   // Accept broadcast/limited
#define RX_NORMAL           2   // Accept broadcast/multicast
#define RX_PROMISCUOUS      3   // Accept all packets

#define EN_RESET            0x80    /* w */


void enet_reset(void);


void EN_TX_Status_Read(void) { // 0x02006000
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = enet.tx_status;
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Transmitter status read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_TX_Status_Write(void) {
    Uint8 val=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Transmitter status write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    enet.tx_status&=~(val&0x0F);
    
    if ((enet.tx_status&enet.tx_mask&0x0F)==0 || (enet.tx_status&enet.tx_mask&0x0F)==TXSTAT_READY) {
        set_interrupt(INT_EN_TX, RELEASE_INT);
    }
}

void EN_TX_Mask_Read(void) { // 0x02006001
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = enet.tx_mask&0xAF;
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Transmitter masks read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_TX_Mask_Write(void) {
    enet.tx_mask=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Transmitter masks write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    
    if ((enet.tx_status&enet.tx_mask&0x0F)==0 || (enet.tx_status&enet.tx_mask&0x0F)==TXSTAT_READY) {
        set_interrupt(INT_EN_TX, RELEASE_INT);
    }
}

void EN_RX_Status_Read(void) { // 0x02006002
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = enet.rx_status;
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Receiver status read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_RX_Status_Write(void) {
    Uint8 val=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Receiver status write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    enet.rx_status&=~(val&0x8F);
    
    if ((enet.rx_status&enet.rx_mask&0x8F)==0) {
        set_interrupt(INT_EN_RX, RELEASE_INT);
    }
}

void EN_RX_Mask_Read(void) { // 0x02006003
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = enet.rx_mask&0x9F;
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Receiver masks read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_RX_Mask_Write(void) {
    enet.rx_mask=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Receiver masks write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    
    if ((enet.rx_status&enet.rx_mask&0x8F)==0) {
        set_interrupt(INT_EN_RX, RELEASE_INT);
    }
}

void EN_TX_Mode_Read(void) { // 0x02006004
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = enet.tx_mode;
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Transmitter mode read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_TX_Mode_Write(void) {
    enet.tx_mode=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Transmitter mode write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_RX_Mode_Read(void) { // 0x02006005
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = enet.rx_mode;
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Receiver mode read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_RX_Mode_Write(void) {
    enet.rx_mode=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Receiver mode write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_Reset_Write(void) { // 0x02006006
    enet.reset=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Reset write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    if (enet.reset&EN_RESET) {
        enet_reset();
    }
}

void EN_NodeID0_Read(void) { // 0x02006008
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = enet.mac_addr[0];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] MAC byte 0 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_NodeID0_Write(void) {
    enet.mac_addr[0]=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] MAC byte 0 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_NodeID1_Read(void) { // 0x02006009
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = enet.mac_addr[1];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] MAC byte 1 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_NodeID1_Write(void) {
    enet.mac_addr[1]=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] MAC byte 1 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_NodeID2_Read(void) { // 0x0200600a
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = enet.mac_addr[2];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] MAC byte 2 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_NodeID2_Write(void) {
    enet.mac_addr[2]=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] MAC byte 2 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_NodeID3_Read(void) { // 0x0200600b
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = enet.mac_addr[3];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] MAC byte 3 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_NodeID3_Write(void) {
    enet.mac_addr[3]=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] MAC byte 3 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_NodeID4_Read(void) { // 0x0200600c
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = enet.mac_addr[4];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] MAC byte 4 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_NodeID4_Write(void) {
    enet.mac_addr[4]=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] MAC byte 4 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_NodeID5_Read(void) { // 0x0200600d
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = enet.mac_addr[5];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] MAC byte 5 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_NodeID5_Write(void) {
    enet.mac_addr[5]=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] MAC byte 5 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_CounterLo_Read(void) { // 0x02006007
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = ((enet_tx_buffer.limit-enet_tx_buffer.size)*8)&0xFF; /* FIXME: counter value */
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Receiver mode read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_CounterHi_Read(void) { // 0x0200600f
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = (((enet_tx_buffer.limit-enet_tx_buffer.size)*8)>>8)&0x3F; /* FIXME: counter value */
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Receiver mode read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void enet_tx_interrupt(Uint8 intr) {
    enet.tx_status|=intr;
    if (enet.tx_status&enet.tx_mask) {
        set_interrupt(INT_EN_TX, SET_INT);
    }
}

void enet_rx_interrupt(Uint8 intr) {
    enet.rx_status|=intr;
    if (enet.rx_status&enet.rx_mask) {
        set_interrupt(INT_EN_RX, SET_INT);
    }
}

void enet_reset(void) {
    enet.tx_status=TXSTAT_READY;
}

/* Functions to find out if we are intended to receive a packet */
bool recv_multicast(Uint8 *packet) {
    if (packet[0]&0x01)
        return true;
    else
        return false;
}

bool recv_local_multicast(Uint8 *packet) {
    if (packet[0]&0x01 &&
        (packet[0]&0xFE) == enet.mac_addr[0] &&
        packet[1] == enet.mac_addr[1] &&
        packet[2] == enet.mac_addr[2])
        return true;
    else
        return false;
}

bool recv_me(Uint8 *packet) {
    if (packet[0] == enet.mac_addr[0] &&
        packet[1] == enet.mac_addr[1] &&
        packet[2] == enet.mac_addr[2] &&
        packet[3] == enet.mac_addr[3] &&
        packet[4] == enet.mac_addr[4] &&
        (packet[5] == enet.mac_addr[5] || (enet.rx_mode&RXMODE_ADDR_SIZE)))
        return true;
    else
        return false;
}

bool recv_broadcast(Uint8 *packet) {
    if (packet[0] == 0xFF &&
        packet[1] == 0xFF &&
        packet[2] == 0xFF &&
        packet[3] == 0xFF &&
        packet[4] == 0xFF &&
        packet[5] == 0xFF)
        return true;
    else
        return false;
}

bool enet_packet_for_me(Uint8 *packet) {
    switch (enet.rx_mode&RXMODE_MATCH_MODE) {
        case RX_NOPACKETS:
            return false;
            
        case RX_NORMAL:
            if (recv_broadcast(packet) || recv_me(packet) || recv_local_multicast(packet))
                return true;
            else
                return false;
            
        case RX_LIMITED:
            if (recv_broadcast(packet) || recv_me(packet) || recv_multicast(packet))
                return true;
            else
                return false;
            
        case RX_PROMISCUOUS:
            return true;
            
        default: return false;
    }
}

void print_buf(Uint8 *buf, Uint32 size) {
    int i;
    for (i=0; i<size; i++) {
        if (i==14 || (i-14)%16==0) {
            printf("\n");
        }
        printf("%02X ",buf[i]);
    }
    printf("\n");
}

/* Ethernet periodic check */
#define ENET_IO_DELAY   50000

void ENET_IO_Handler(void) {
    CycInt_AcknowledgeInterrupt();
    
    /* Receive packet */
    
    /* TODO: Receive from real network! */
    if (enet_rx_buffer.size>0) {
        if (enet_packet_for_me(enet_rx_buffer.data)) {
            Log_Printf(LOG_EN_LEVEL, "[EN] Receiving packet from %02X:%02X:%02X:%02X:%02X:%02X",
                       enet_rx_buffer.data[6], enet_rx_buffer.data[7], enet_rx_buffer.data[8],
                       enet_rx_buffer.data[9], enet_rx_buffer.data[10], enet_rx_buffer.data[11]);
            print_buf(enet_rx_buffer.data, enet_rx_buffer.size);
            if (enet_rx_buffer.size<64 && !(enet.rx_mode&RXMODE_ENA_SHORT)) {
                Log_Printf(LOG_EN_LEVEL, "[EN] Received packet is short (%i byte)",enet_rx_buffer.size);
                enet_rx_interrupt(RXSTAT_SHORT_PKT);
            } else {
                enet_rx_interrupt(RXSTAT_PKT_OK);
            }

            dma_enet_write_memory();
            enet_rx_buffer.size=0;
        }
    }
    
    /* Send packet */
    if (enet.tx_status&TXSTAT_READY) {
        dma_enet_read_memory();
        if (enet_tx_buffer.size>0) {
            Log_Printf(LOG_EN_LEVEL, "[EN] Sending packet to %02X:%02X:%02X:%02X:%02X:%02X",
                       enet_tx_buffer.data[0], enet_tx_buffer.data[1], enet_tx_buffer.data[2],
                       enet_tx_buffer.data[3], enet_tx_buffer.data[4], enet_tx_buffer.data[5]);
            //print_buf(enet_tx_buffer.data, enet_tx_buffer.size);
            if (enet.tx_mode&TXMODE_DIS_LOOP) {
                /* TODO: Send to real network! */
                enet_tx_buffer.size=0;
            } else {
                /* Loop back */
                memcpy(enet_rx_buffer.data, enet_tx_buffer.data, enet_tx_buffer.size);
                enet_rx_buffer.size=enet_rx_buffer.limit=enet_tx_buffer.size;
                enet_tx_buffer.size=0;
                enet_tx_interrupt(TXSTAT_TX_RECVD);
            }
        }
    }
    
    CycInt_AddRelativeInterrupt(ENET_IO_DELAY, INT_CPU_CYCLE, INTERRUPT_ENET_IO);
}

void Init_Ethernet(void) {
    Log_Printf(LOG_WARN, "Starting Ethernet Transmitter/Receiver");
    CycInt_AddRelativeInterrupt(ENET_IO_DELAY, INT_CPU_CYCLE, INTERRUPT_ENET_IO);
}

void Ethernet_Reset(void) {
    /* Periodic interrupt was reset by CycInt_Reset, so we only need to re-start */
    Init_Ethernet();
    
    enet_rx_buffer.size=enet_tx_buffer.size=0;
    enet_rx_buffer.limit=enet_tx_buffer.limit=2048;
}




/* ------------------------------ old stuff --> remove later ------------------------------ */
void Ethernet_Read(void);
void Ethernet_Write(void);


void Ethernet_Transmit(void);
void Ethernet_Receive(Uint8 *packet, Uint32 size);

bool Packet_Receiver_Me(Uint8 *packet);
bool Me(Uint8 *packet);
bool Multicast(Uint8 *packet);
bool Local_Multicast(Uint8 *packet);
bool Broadcast(Uint8 *packet);
/* Function Prototypes */
void EnTx_Lower_IRQ(void);
void EnRx_Lower_IRQ(void);
void EnTx_Raise_IRQ(void);
void EnRx_Raise_IRQ(void);


/* Ethernet Regisers */
#define EN_TXSTAT   0
#define EN_TXMASK   1
#define EN_RXSTAT   2
#define EN_RXMASK   3
#define EN_TXMODE   4
#define EN_RXMODE   5


/* Ethernet Register Constants */
#define TXSTAT_RDY          0x80    // Ready for Packet

#define RXSTAT_OK           0x80    // Packet received is correct

#define TXMODE_NOLOOP       0x02    // Loop back control disabled

#define RXMODE_ADDRSIZE     0x10    // Reduces NODE match to 5 chars
#define RXMODE_NOPACKETS    0x00    // Accept no packets
#define RXMODE_LIMITED      0x01    // Accept Broadcast/limited
#define RXMODE_NORMAL       0x02    // Accept Broadcast/multicasts
#define RXMODE_PROMISCUOUS  0x03    // Accept all packets
#define RXMODE_RESETENABLE  0x04    // One for reset packet enable

#define RESET_VAL           0x80    // Generate Reset

typedef struct {
    Uint8 tx_status;
    Uint8 tx_mask;
    Uint8 tx_mode;
    Uint8 rx_status;
    Uint8 rx_mask;
    Uint8 rx_mode;
} ETHERNET_CONTROLLER;

ETHERNET_CONTROLLER ethernet;

Uint8 ethernet_buffer[1600];


Uint8 MACaddress[6];

void Ethernet_Read(void) {
	Uint8 reg = IoAccessCurrentAddress&0x0F;
	Uint8 val = 0;
    
	switch (reg) {
		case EN_TXSTAT:
			val = ethernet.tx_status;
			break;
        case EN_TXMASK:
            val = ethernet.tx_mask;
            break;
        case EN_RXSTAT:
            val = ethernet.rx_status;
            break;
        case EN_RXMASK:
            val = ethernet.rx_mask;
            break;
        case EN_TXMODE:
            val = ethernet.tx_mode;
            break;
        case EN_RXMODE:
            val = ethernet.rx_mode;
            break;
        case EN_RESET:
            break;
            
        /* Read MAC Address */
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
            val = MACaddress[reg-8];
            break;
        default:
            break;
	}
    IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = val;
    
    Log_Printf(LOG_EN_LEVEL, "[Ethernet] read reg %d val %02x PC=%x %s at %d",reg,val,m68k_getpc(),__FILE__,__LINE__);
}

void Ethernet_Write(void) {
	Uint8 val = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
	Uint8 reg = IoAccessCurrentAddress&0xF;
    
    Log_Printf(LOG_EN_LEVEL, "[Ethernet] write reg %d val %02x PC=%x %s at %d",reg,val,m68k_getpc(),__FILE__,__LINE__);
    
    switch (reg) {
        case EN_TXSTAT:
            if (val == 0xFF)
                ethernet.tx_status = 0x80; // ? temp hack
            else
                ethernet.tx_status = val; //ethernet.tx_status & (0xF0 | ~val); ?
//            EnTx_Raise_IRQ(); // check irq
			break;
        case EN_TXMASK:
            ethernet.tx_mask = val & 0xAF;
//            EnTx_Raise_IRQ(); // check irq
            break;
        case EN_RXSTAT:
            if (val == 0xFF)
                ethernet.rx_status = 0x00;
            else
                ethernet.rx_status = val; //ethernet.rx_status & (0x07 | ~val); ?
//            EnRx_Raise_IRQ(); // check irq
            break;
        case EN_RXMASK:
            ethernet.rx_mask = val & 0x9F;
//            EnRx_Raise_IRQ(); // check irq
            break;
        case EN_TXMODE:
            ethernet.tx_mode = val;
            break;
        case EN_RXMODE:
            ethernet.rx_mode = val;
            break;
        case EN_RESET:
            if (val&RESET_VAL)
                Ethernet_Reset();
            break;

        /* Write MAC Address */
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
            MACaddress[reg-8] = val;
            break;
            
        default:
            break;
    }
}

#if 0
void Ethernet_Reset(void) {
    ethernet.tx_status = TXSTAT_RDY;
    ethernet.tx_mask = 0x00;
    ethernet.tx_mode = 0x00;
    ethernet.rx_status = 0x00;
    ethernet.rx_mask = 0x00;
    ethernet.rx_mode = 0x00;
    
    EnTx_Lower_IRQ();
    EnRx_Lower_IRQ();
    
    // txlen = rxlen = txcount = 0;
    // set_promisc(true);
    // start_send();
}
#endif
void Ethernet_Transmit(void) {
    Uint32 size = 0;
    //dma_memory_read(ethernet_buffer, &size, CHANNEL_EN_TX);
    ethernet.tx_status = TXSTAT_RDY;
    EnTx_Raise_IRQ();
    
    Log_Printf(LOG_EN_LEVEL, "Ethernet: Send packet to %02X:%02X:%02X:%02X:%02X:%02X",
               ethernet_buffer[0], ethernet_buffer[1], ethernet_buffer[2],
               ethernet_buffer[3], ethernet_buffer[4], ethernet_buffer[5]);
    
    if (!(ethernet.tx_mode&TXMODE_NOLOOP)) { // if loop back is not disabled
        Ethernet_Receive(ethernet_buffer, size); // loop back
    }
    
    // TODO: send packet to real network
}

void Ethernet_Receive(Uint8 *packet, Uint32 size) {
    if (Packet_Receiver_Me(packet)) {
        Log_Printf(LOG_EN_LEVEL, "Ethernet: Receive packet from %02X:%02X:%02X:%02X:%02X:%02X",
                   packet[6], packet[7], packet[8], packet[9], packet[10], packet[11]);
                   
        //dma_memory_write(packet, size, CHANNEL_EN_RX); // loop back for experiment
        ethernet.rx_status |= RXSTAT_OK; // packet received is correct
        EnRx_Raise_IRQ();
    }
}


void EnTx_Lower_IRQ(void) {
    set_interrupt(INT_EN_TX, RELEASE_INT);
}

void EnRx_Lower_IRQ(void) {
    set_interrupt(INT_EN_RX, RELEASE_INT);
}

void EnTx_Raise_IRQ(void) {
    set_interrupt(INT_EN_TX, SET_INT);
}

void EnRx_Raise_IRQ(void) {
    set_interrupt(INT_EN_RX, SET_INT);
}


/* Functions to find out if we are intended to receive a packet */

bool Packet_Receiver_Me(Uint8 *packet) {
    switch (ethernet.rx_mode&0x03) {
        case RXMODE_NOPACKETS:
            return false;
            
        case RXMODE_NORMAL:
            if (Broadcast(packet) || Me(packet) || Local_Multicast(packet))
                return true;
            else
                return false;
            
        case RXMODE_LIMITED:
            if (Broadcast(packet) || Me(packet) || Multicast(packet))
                return true;
            else
                return false;
            
        case RXMODE_PROMISCUOUS:
            return true;
            
        default: return false;
    }
}

bool Multicast(Uint8 *packet) {
    if (packet[0]&0x01)
        return true;
    else 
        return false;
}

bool Local_Multicast(Uint8 *packet) {
    if (packet[0]&0x01 &&
        (packet[0]&0xFE) == MACaddress[0] //&&
        //        packet[1] == MACaddress[1] &&
        //        packet[2] == MACaddress[2]
        )
        return true;
    else 
        return false;
}

bool Me(Uint8 *packet) {
    if (packet[0] == MACaddress[0] &&
        //        packet[1] == MACaddress[1] &&
        //        packet[2] == MACaddress[2] &&
        //        packet[3] == MACaddress[3] &&
        packet[4] == MACaddress[4] &&
        (packet[5] == MACaddress[5] || (ethernet.rx_mode&RXMODE_ADDRSIZE)))
        return true;
    else 
        return false;
}

bool Broadcast(Uint8 *packet) {
    if (packet[0] == 0xFF &&
        //        packet[1] == 0xFF &&
        //        packet[2] == 0xFF &&
        //        packet[3] == 0xFF &&
        packet[4] == 0xFF &&
        packet[5] == 0xFF)
        return true;
    else 
        return false;
}
