int rtc_interface_io(Uint8 rtdatabit);
void rtc_interface_reset(void);

void nvram_init(void);
void rtc_checksum(int force);