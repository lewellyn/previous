/* SCSI Bus and Disk emulation */

#include "main.h"
#include "ioMem.h"
#include "ioMemTables.h"
#include "video.h"
#include "configuration.h"
#include "sysdeps.h"
#include "m68000.h"
#include "sysReg.h"
#include "statusbar.h"
#include "scsi.h"

typedef struct {
    int readCount;    /* count of number of command bytes written */
    unsigned char target;
    unsigned char opcode;
    bool extended;
    
    int byteCount;             /* count of number of command bytes written */
    unsigned char command[10];
    short int returnCode;      /* return code from the HDC operation */
} SCSICOMMAND;

/* HDC globals */
SCSICOMMAND SCSICommandBlock;
int nPartitions = 0;
unsigned long hdSize = 0;
short int HDCSectorCount;
bool bAcsiEmuOn = false;

static FILE *hd_image_file = NULL;
static Uint32 nLastBlockAddr;
static bool bSetLastBlockAddr;
static Uint8 nLastError;


#define COMMAND_ReadInt16(a, i) (((unsigned) a[i] << 8) | a[i + 1])

/* SCSI output buffer */
//#define MAX_OUTBUF_SIZE 1024
//Uint8 scsi_outbuf[MAX_OUTBUF_SIZE];


/* INQUIRY response data */
static unsigned char inquiry_bytes[] =
{
	0x00,             /* 0: device type: 0x00 = direct access device, 0x05 = cd-rom, 0x07 = mo-disk */
	0x00,             /* 1: &0x7F - device type qulifier 0x00 unsupported, &0x80 - rmb: 0x00 = nonremovable, 0x80 = removable */
	0x01,             /* 2: ANSI SCSI standard (first release) compliant */
    0x01,             /* 3: Restponse format (format of following data): 0x01 SCSI-1 compliant */
	0x31,             /* 4: additional length of the following data */
    0x00, 0x00,       /* 5,6: reserved */
    0x1C,             /* 7: RelAdr=0, Wbus32=0, Wbus16=0, Sync=1, Linked=1, RSVD=1, CmdQue=0, SftRe=0 */
	'P','r','e','v','i','o','u','s',        /*  8-15: Vendor ASCII */
	'H','D','D',' ',' ',' ',' ',' ',        /* 16-23: Model ASCII */
    ' ',' ',' ',' ',' ',' ',' ',' ',        /* 24-31: Blank space ASCII */
    '0','0','0','0','0','0','0','1',        /* 32-39: Revision ASCII */
	'0','0','0','0','0','0','0','0',        /* 40-47: Serial Number ASCII */
    ' ',' ',' ',' ',' ',' '                 /* 48-53: Blank space ASCII */
};




void scsi_command_analyzer(Uint8 commandbuf[], int size) {
    memcpy(SCSICommandBlock.command, commandbuf, size);
    SCSICommandBlock.opcode = SCSICommandBlock.command[1];
    Log_Printf(LOG_WARN, "SCSI command: Length = %i, Opcode = $%02x\n", size, SCSICommandBlock.opcode);
    SCSI_Emulate_Command();
}

void SCSI_Emulate_Command(void)
{
    
	switch(SCSICommandBlock.opcode)
	{
            
        case HD_TEST_UNIT_RDY:
            Log_Printf(LOG_WARN, "SCSI command: Test unit ready\n");
//            HDC_Cmd_TestUnitReady();
            break;
            
        case HD_READ_CAPACITY1:
            Log_Printf(LOG_WARN, "SCSI command: Read capacity\n");
//            HDC_Cmd_ReadCapacity();
            break;
            
        case HD_READ_SECTOR:
        case HD_READ_SECTOR1:
            Log_Printf(LOG_WARN, "SCSI command: Read sector\n");
//            HDC_Cmd_ReadSector();
            break;
            
        case HD_WRITE_SECTOR:
        case HD_WRITE_SECTOR1:
            Log_Printf(LOG_WARN, "SCSI command: Write sector\n");
//            HDC_Cmd_WriteSector();
            break;
            
        case HD_INQUIRY:
            Log_Printf(LOG_WARN, "SCSI command: Inquiry\n");
            SCSI_Inquiry();
//            HDC_Cmd_Inquiry();
            break;
            
        case HD_SEEK:
            Log_Printf(LOG_WARN, "SCSI command: Seek\n");
//            HDC_Cmd_Seek();
            break;
            
        case HD_SHIP:
            Log_Printf(LOG_WARN, "SCSI command: Ship\n");
            SCSICommandBlock.returnCode = 0xFF;
//            FDC_AcknowledgeInterrupt();
            break;
            
        case HD_REQ_SENSE:
            Log_Printf(LOG_WARN, "SCSI command: Request sense\n");
//            HDC_Cmd_RequestSense();
            break;
            
        case HD_MODESELECT:
            Log_Printf(LOG_WARN, "MODE SELECT call not implemented yet.\n");
            SCSICommandBlock.returnCode = HD_STATUS_OK;
            nLastError = HD_REQSENS_OK;
            bSetLastBlockAddr = false;
//            FDC_SetDMAStatus(false);
//            FDC_AcknowledgeInterrupt();
            break;
            
        case HD_MODESENSE:
            Log_Printf(LOG_WARN, "SCSI command: Mode sense\n");
//            HDC_Cmd_ModeSense();
            break;
            
        case HD_FORMAT_DRIVE:
            Log_Printf(LOG_WARN, "SCSI command: Format drive\n");
//            HDC_Cmd_FormatDrive();
            break;
            
            /* as of yet unsupported commands */
        case HD_VERIFY_TRACK:
        case HD_FORMAT_TRACK:
        case HD_CORRECTION:
            
        default:
            Log_Printf(LOG_WARN, "Unknown Command\n");
            SCSICommandBlock.returnCode = HD_STATUS_ERROR;
            nLastError = HD_REQSENS_OPCODE;
            bSetLastBlockAddr = false;
//            FDC_AcknowledgeInterrupt();
            break;
	}
    
	/* Update the led each time a command is processed */
	Statusbar_EnableHDLed();
}



static int SCSI_get_return_length(void)
{
	return SCSICommandBlock.opcode < 0x20?
    // class 0
    SCSICommandBlock.command[5] :
    // class1
    COMMAND_ReadInt16(SCSICommandBlock.command, 8);
}



/* SCSI Commands */


void SCSI_Inquiry (void) {  //conversion in progress
    Uint32 nDmaAddr;
	int return_length = SCSI_get_return_length();
    Log_Printf(LOG_WARN, "return length: %d", return_length);
    Uint8 scsi_outbuf[return_length];
    
    memcpy(scsi_outbuf, inquiry_bytes, return_length);
    
    Log_Printf(LOG_WARN, "Inquiry Data: %c,%c,%c,%c,%c,%c,%c,%c\n",scsi_outbuf[8],scsi_outbuf[9],scsi_outbuf[10],scsi_outbuf[11],scsi_outbuf[12],scsi_outbuf[13],scsi_outbuf[14],scsi_outbuf[15]);
    
    copy_to_scsidma_buffer(scsi_outbuf, return_length);
    
//	nDmaAddr = FDC_GetDMAAddress();
    
#ifdef HDC_VERBOSE
	fprintf(stderr,"HDC: Inquiry, %i bytes to 0x%x.\n", count, nDmaAddr);
#endif
    
	if (return_length > (int)sizeof(inquiry_bytes))
		return_length = sizeof(inquiry_bytes);
    
//	inquiry_bytes[4] = return_length - 8;
    
//	if (NEXTMemory_SafeCopy(nDmaAddr, inquiry_bytes, count, "HDC DMA inquiry"))
		SCSICommandBlock.returnCode = HD_STATUS_OK;
//	else
//		SCSICommandBlock.returnCode = HD_STATUS_ERROR;
    
//	FDC_WriteDMAAddress(nDmaAddr + count);
    
//	FDC_SetDMAStatus(false);              /* no DMA error */
//	FDC_AcknowledgeInterrupt();
	nLastError = HD_REQSENS_OK;
	bSetLastBlockAddr = false;
}
