#ifndef CAPS_FORM_H
#define CAPS_FORM_H

#include <caps/capsimage.h>

#if defined(__GNUC__) && !defined(__mc68000__)
#pragma pack(1)
#endif // __GNUC__

// block data descriptor for converter
struct CapsFormatBlock {
	CapsULong gapacnt;   // gap before first am count
	CapsULong gapavalue; // gap before first am value
	CapsULong gapbcnt;   // gap after first am count
	CapsULong gapbvalue; // gap after first am value
	CapsULong gapccnt;   // gap before second am count
	CapsULong gapcvalue; // gap before second am value
	CapsULong gapdcnt;   // gap after second am count
	CapsULong gapdvalue; // gap after second am value
	CapsULong blocktype; // type of block
	CapsULong track;     // track#
	CapsULong side;      // side#
	CapsULong sector;    // sector#
	CapsLong sectorlen;  // sector length in bytes
	CapsUByte *databuf;  // source data buffer
	CapsULong datavalue; // source data value if buffer is NULL
};

// track data descriptor for converter
struct CapsFormatTrack {
	CapsULong type;      // structure type
	CapsULong gapacnt;   // gap after index count
	CapsULong gapavalue; // gap after index value
	CapsULong gapbvalue; // gap before index value
	CapsUByte *trackbuf; // track buffer memory
	CapsULong tracklen;  // track buffer memory length
	CapsULong buflen;    // track buffer allocation size
	CapsULong bufreq;    // track buffer requested size
	CapsULong startpos;  // start position in buffer
	CapsLong blockcnt;   // number of blocks
	struct CapsFormatBlock *block; // block data
	CapsULong size;      // internal counter
};

#if defined(__GNUC__) && !defined(__mc68000__)
#pragma pack()
#endif // __GNUC__

// block types
enum {
	cfrmbtNA=0,  // invalid type
	cfrmbtIndex, // index 
	cfrmbtData   // data
};

#ifndef CLIB_CAPSIMAGE_PROTOS_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

CapsLong CAPSFormatDataToMFM(void *pformattrack, CapsULong flag);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // CLIB_CAPSIMAGE_PROTOS_H

#endif // CAPS_FORM_H
