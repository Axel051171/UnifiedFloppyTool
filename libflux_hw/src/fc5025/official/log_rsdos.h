struct dentry {
	uint8_t basename[8], ext[3]; /* basename[0]=0 -> deleted file
	                                basename[0]=0xff -> end of directory */
	uint8_t type;
	uint8_t ascii; /* 0=binary, 0xff=ascii */
	uint8_t starting_granule;
	uint16_t last_sec_bytes;
	uint8_t padding[16];
} __attribute__ ((__packed__));
