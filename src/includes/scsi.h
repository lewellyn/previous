/* SCSI Bus and Disk emulation */


/* Opcodes */
/* The following are multi-sector transfers with seek implied */
#define HD_VERIFY_TRACK    0x05               /* Verify track */
#define HD_FORMAT_TRACK    0x06               /* Format track */
#define HD_READ_SECTOR     0x08               /* Read sector */
#define HD_READ_SECTOR1    0x28               /* Read sector (class 1) */
#define HD_WRITE_SECTOR    0x0A               /* Write sector */
#define HD_WRITE_SECTOR1   0x2A               /* Write sector (class 1) */

/* other codes */
#define HD_TEST_UNIT_RDY   0x00               /* Test unit ready */
#define HD_FORMAT_DRIVE    0x04               /* Format the whole drive */
#define HD_SEEK            0x0B               /* Seek */
#define HD_CORRECTION      0x0D               /* Correction */
#define HD_INQUIRY         0x12               /* Inquiry */
#define HD_MODESELECT      0x15               /* Mode select */
#define HD_MODESENSE       0x1A               /* Mode sense */
#define HD_REQ_SENSE       0x03               /* Request sense */
#define HD_SHIP            0x1B               /* Ship drive */
#define HD_READ_CAPACITY1  0x25               /* Read capacity (class 1) */

/* Status codes */
#define HD_STATUS_OK       0x00               /* OK */
#define HD_STATUS_ERROR    0x02               /* Check Condition */
#define HD_STATUS_CONDMET  0x04               /* Condition Met */
#define HD_STATUS_BUSY     0x08               /* Busy */

/* Error codes for REQUEST SENSE: */
#define HD_REQSENS_OK       0x00              /* OK return status */
#define HD_REQSENS_NOSECTOR 0x01              /* No index or sector */
#define HD_REQSENS_WRITEERR 0x03              /* Write fault */
#define HD_REQSENS_OPCODE   0x20              /* Opcode not supported */
#define HD_REQSENS_INVADDR  0x21              /* Invalid block address */
#define HD_REQSENS_INVARG   0x24              /* Invalid argument */
#define HD_REQSENS_NODRIVE  0x25              /* Invalid drive */


/* ----------- NEW ---------- */
/* Status Codes */
#define STAT_GOOD           0x00
#define STAT_CHECK_COND     0x02
#define STAT_COND_MET       0x04
#define STAT_BUSY           0x08
#define STAT_INTERMEDIATE   0x10
#define STAT_INTER_COND_MET 0x14
#define STAT_RESERV_CONFL   0x18

/* Messages */
#define MSG_COMPLETE        0x00
#define MSG_SAVE_PTRS       0x02
#define MSG_RESTORE_PTRS    0x03
#define MSG_DISCONNECT      0x04
#define MSG_INITIATOR_ERR   0x05
#define MSG_ABORT           0x06
#define MSG_MSG_REJECT      0x07
#define MSG_NOP             0x08
#define MSG_PARITY_ERR      0x09
#define MSG_LINK_CMD_CMPLT  0x0A
#define MSG_LNKCMDCMPLTFLAG 0x0B
#define MSG_DEVICE_RESET    0x0C

#define MSG_IDENTIFY_MASK   0x80
#define MSG_ID_DISCONN      0x40
#define MSG_LUNMASK         0x07

/* Sense Keys */
#define SENSE_NOSENSE       0x00
#define SENSE_RECOVERED     0x01
#define SENSE_NOTREADY      0x02
#define SENSE_MEDIA         0x03
#define SENSE_HARDWARE      0x04
#define SENSE_ILLEGAL_REQ   0x05
#define SENSE_UNIT_ATN      0x06
#define SENSE_DATAPROTECT   0x07
#define SENSE_ABORTED_CMD   0x0B
#define SENSE_VOL_OVERFLOW  0x0D
#define SENSE_MISCOMPARE    0x0E


/* Command Descriptor Block */
#define SCSI_CDB_MAX_SIZE 12

/* Mode Pages */
#define MODEPAGE_MAX_SIZE 24
typedef struct {
    Uint8 current[MODEPAGE_MAX_SIZE];
    Uint8 changeable[MODEPAGE_MAX_SIZE];
    Uint8 modepage[MODEPAGE_MAX_SIZE]; // default values
    Uint8 saved[MODEPAGE_MAX_SIZE];
    Uint8 pagesize;
} MODEPAGE;

/* This buffer temporarily stores data to be written to memory or disk */
#define SCSI_BUFFER_SIZE 65536

struct {
    Uint8 buffer[SCSI_BUFFER_SIZE];
    Uint32 size;
    Uint32 rpos; /* actual read pointer */
} SCSIdata;

int SCSI_fill_data_buffer(void *buf, Uint32 size, bool disk);
void SCSI_WriteFromBuffer(Uint8 *buf, Uint32 size);


struct {
    Uint8 target;
//    Uint8 phase;
} SCSIbus;


/* SCSI phase */
Uint8 scsi_phase;

void SCSI_Init(void);
void SCSI_Uninit(void);
void SCSI_Reset(void);

Uint8 SCSIdisk_Send_Status(void);
Uint8 SCSIdisk_Send_Message(void);
void SCSIdisk_Send_Data(void);
void SCSIdisk_Receive_Data(void);
bool SCSIdisk_Select(Uint8 target);
void SCSIdisk_Receive_Command(Uint8 *commandbuf, Uint8 identify);


/* Helpers */
int SCSI_GetTransferLength(Uint8 opcode, Uint8 *cdb);
unsigned long SCSI_GetOffset(Uint8 opcode, Uint8 *cdb);
int SCSI_GetCount(Uint8 opcode, Uint8 *cdb);
MODEPAGE SCSI_GetModePage(Uint8 pagecode);


void SCSI_Emulate_Command(Uint8 opcode, Uint8 *cdb);

/* SCSI Commands */
void SCSI_Inquiry (Uint8 *cdb);
void SCSI_StartStop(Uint8 *cdb);
void SCSI_TestUnitReady(Uint8 *cdb);
void SCSI_ReadCapacity(Uint8 *cdb);
void SCSI_ReadSector(Uint8 *cdb);
void SCSI_WriteSector(Uint8 *cdb);
void SCSI_RequestSense(Uint8 *cdb);
void SCSI_ModeSense(Uint8 *cdb);
