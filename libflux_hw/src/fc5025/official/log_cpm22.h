struct dentry {
	uint8_t usernum; /* CP/M 1.4: 0 = Normal file
	                              0x80 = Hidden file (Removed in CP/M 2.x)
	                              0xE5 = file deleted
	                    CP/M 2.2: User number (0-15 or 0-31)
	                    CP/M 3.0: User number (0-15)
	                              Password (16-31)
	                              Directory label (0x20)
	                              Timestamp (0x21) */
	uint8_t basename[8], ext[3];
	uint8_t extent;
	uint8_t s1; /* CP/M 2.2: Unused
	               CP/M 3.0: 0 = Last record of file contains 128 bytes
	                         1-255 = Byte count (program specific)
	               CP/M 4.1: 1-128 = Bytes used in the final record */
	uint8_t s2; /* extent high byte */
	uint8_t records; 
	uint8_t blocks[16];
} __attribute__ ((__packed__));
