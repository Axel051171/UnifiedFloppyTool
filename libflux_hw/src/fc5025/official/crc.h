void crc_init(void);
void crc_add(unsigned char c);
void crc_add_block(unsigned char *buf, int count);
unsigned short crc_get(void);
unsigned short crc_block(unsigned char *buf, int count);
void crc_append(unsigned char *buf, int count);
