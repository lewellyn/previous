/* NeXT DMA Emulation */


enum next_dma_chan {
    NEXTDMA_FD,
    NEXTDMA_ENRX,
    NEXTDMA_ENTX,
    NEXTDMA_SCSI,
    NEXTDMA_SCC,
    NEXTDMA_SND
};

void DMA_SCSI_CSR_Read(void);
void DMA_SCSI_CSR_Write(void);

void DMA_SCSI_Saved_Next_Read(void);
void DMA_SCSI_Saved_Next_Write(void);
void DMA_SCSI_Saved_Limit_Read(void);
void DMA_SCSI_Saved_Limit_Write(void);
void DMA_SCSI_Saved_Start_Read(void);
void DMA_SCSI_Saved_Start_Write(void);
void DMA_SCSI_Saved_Stop_Read(void);
void DMA_SCSI_Saved_Stop_Write(void);

void DMA_SCSI_Next_Read(void);
void DMA_SCSI_Next_Write(void);
void DMA_SCSI_Limit_Read(void);
void DMA_SCSI_Limit_Write(void);
void DMA_SCSI_Start_Read(void);
void DMA_SCSI_Start_Write(void);
void DMA_SCSI_Stop_Read(void);
void DMA_SCSI_Stop_Write(void);

void DMA_SCSI_Init_Read(void);
void DMA_SCSI_Init_Write(void);
void DMA_SCSI_Size_Read(void);
void DMA_SCSI_Size_Write(void);


void nextdma_write(Uint8 *buf, int size, int type);