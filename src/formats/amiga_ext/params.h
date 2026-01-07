#ifndef UFT_FORMATS_AMIGA_EXT_PARAMS_H
#define UFT_FORMATS_AMIGA_EXT_PARAMS_H


typedef struct ldtable_
{
	uint32_t startoffset;
	uint32_t lenght;
} ldtable;

typedef struct params_
{
	uint32_t SysBase;
	uint32_t ioreq;
	uint32_t exec_size;
	uint32_t exec_checksum;
	uint32_t total_blocks_size;
	ldtable  blocktable[4];
} params;

#endif /* UFT_FORMATS_AMIGA_EXT_PARAMS_H */
