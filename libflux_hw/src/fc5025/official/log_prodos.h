struct file_entry {
	uint8_t stnl; /* storage_type and name_length */
	uint8_t file_name[15];
	uint8_t file_type;
	uint16_t key_pointer, blocks_used;
	uint8_t eof_low;
	uint16_t eof_high;
	uint32_t creation;
	uint8_t version, min_version;
	uint8_t access;
	uint16_t aux_type;
	uint32_t last_mod;
	uint16_t header_pointer;
} __attribute__ ((__packed__));

struct directory_block {
	uint16_t prev, next;
	uint8_t stnl; /* storage_type and name_length */
	uint8_t file_name[15];
	uint8_t reserved[8];
	uint32_t creation;
	uint8_t version, min_version;
	uint8_t access;
	uint8_t entry_length, entries_per_block;
	uint16_t file_count;
	union {
		struct {
			uint16_t bit_map_pointer, total_blocks;
		} __attribute__ ((__packed__)) root;
		struct {
			uint16_t parent_pointer;
			uint8_t parent_entry_number, parent_entry_length;
		} __attribute__ ((__packed__)) sub;
	} u;
} __attribute__ ((__packed__));
