#ifndef UFT_FORMATS_AMIGA_EXT_HUNK_H
#define UFT_FORMATS_AMIGA_EXT_HUNK_H


typedef struct hunk_header_
{
	uint32_t HUNK_ID;
	uint32_t RESIDENTLIBLIST;
	uint32_t NB_OF_HUNKS;
	uint32_t FIRST_HUNK;
	uint32_t LAST_HUNK;
	uint32_t HUNKSIZE[];
}hunk_header;

#endif /* UFT_FORMATS_AMIGA_EXT_HUNK_H */
