#ifndef f_SCP_H
#define f_SCP_H

void scp_init(const char *path);
void scp_shutdown();
bool scp_read_status();
uint8_t scp_compute_checksum(const uint8_t *src, size_t len);
bool scp_send_command(const void *buf, uint32_t len);
bool scp_select_drive(bool driveB, bool select);
bool scp_select_side(bool side2);
bool scp_select_density(bool hidensity);
bool scp_motor(bool drive, bool enabled);
bool scp_seek0();
bool scp_seek(int track);
bool scp_mem_read(void *data, uint32_t offset, uint32_t len);
bool scp_mem_write(const void *data, uint32_t offset, uint32_t len);
bool scp_erase(bool rpm360);
bool scp_track_read(bool rpm360, uint8_t revs, bool prefer_8bit, bool splice);
bool scp_track_getreadinfo(uint32_t data[10]);
bool scp_track_write(bool rpm360, uint32_t bitCellCount, bool splice, bool erase);

#endif
