int init_amiga_fdc(unsigned char drive);
void shutdown_amiga_fdc(void);
unsigned char readsector(unsigned char sectornum,unsigned char * data,unsigned char invalidate_cache);
unsigned char writesector(unsigned char sectornum,unsigned char * data);
int jumptotrack(unsigned char t);
int test_drive(int drive);

#define L_INDIAN(var) (((var&0x000000FF)<<24) |((var&0x0000FF00)<<8) |((var&0x00FF0000)>>8) |((var&0xFF000000)>>24))
