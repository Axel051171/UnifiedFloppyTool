#ifndef CAPS_CAPSIMAGE_H
#define CAPS_CAPSIMAGE_H

#ifdef AMIGA
#include <exec/types.h>
typedef UBYTE CapsUByte;
typedef LONG  CapsLong;
typedef ULONG CapsULong;
#else
#include <stdint.h>
typedef uint8_t  CapsUByte;
typedef int32_t  CapsLong;
typedef uint32_t CapsULong;
#endif // AMIGA

#define CAPS_FILEEXT "ipf"
#define CAPS_FILEPFX ".ipf"
#define CAPS_NAME "capsimage.device"

#define CAPS_UNITS 10

// Flags provided for locking, in order:
//  0: re-align data as index synced recording
//  1: decode track to word aligned size
//  2: generate cell density for variable density tracks
//  3: generate density for automatically sized cells
//  4: generate density for unformatted cells
//  5: generate unformatted data
//  6: generate unformatted data, that changes each revolution
//  7: directly use source memory buffer supplied with LockImageMemory
//  8: flakey/weak data is created on one revolution, updated with each lock
//  9: ...Info.type holds the expected structure type
// 10: alternate density map as fractions
// 11: overlap position is in bits
// 12: tracklen is in bits, and the track buffer is bit sized
// 13: track overlap or weak data is never updated, just initialized
// 14: set weak bit generator seed value
#define DI_LOCK_INDEX    (1L<<0)
#define DI_LOCK_ALIGN    (1L<<1)
#define DI_LOCK_DENVAR   (1L<<2)
#define DI_LOCK_DENAUTO  (1L<<3)
#define DI_LOCK_DENNOISE (1L<<4)
#define DI_LOCK_NOISE    (1L<<5)
#define DI_LOCK_NOISEREV (1L<<6)
#define DI_LOCK_MEMREF   (1L<<7)
#define DI_LOCK_UPDATEFD (1L<<8)
#define DI_LOCK_TYPE     (1L<<9)
#define DI_LOCK_DENALT   (1L<<10)
#define DI_LOCK_OVLBIT   (1L<<11)
#define DI_LOCK_TRKBIT   (1L<<12)
#define DI_LOCK_NOUPDATE (1L<<13)
#define DI_LOCK_SETWSEED (1L<<14)

#define CAPS_MAXPLATFORM 4
#define CAPS_MTRS 5

#define CTIT_FLAG_FLAKEY (1L<<31)
#define CTIT_MASK_TYPE 0xff

#if defined(__GNUC__) && !defined(__mc68000__)
#pragma pack(1)
#endif // __GNUC__

// decoded caps date.time
struct CapsDateTimeExt {
	CapsULong year;
	CapsULong month;
	CapsULong day;
	CapsULong hour;
	CapsULong min;
	CapsULong sec;
	CapsULong tick;
};


// library version information block
struct CapsVersionInfo {
	CapsULong type;     // library type
	CapsULong release;  // release ID
	CapsULong revision; // revision ID
	CapsULong flag;     // supported flags
};


// disk image information block
struct CapsImageInfo {
	CapsULong type;        // image type
	CapsULong release;     // release ID
	CapsULong revision;    // release revision ID
	CapsULong mincylinder; // lowest cylinder number
	CapsULong maxcylinder; // highest cylinder number
	CapsULong minhead;     // lowest head number
	CapsULong maxhead;     // highest head number
	struct CapsDateTimeExt crdt; // image creation date.time
	CapsULong platform[CAPS_MAXPLATFORM]; // intended platform(s)
};

// disk track information block
struct CapsTrackInfo {
	CapsULong type;       // track type
	CapsULong cylinder;   // cylinder#
	CapsULong head;       // head#
	CapsULong sectorcnt;  // available sectors
	CapsULong sectorsize; // sector size
	CapsULong trackcnt;   // track variant count
	CapsUByte *trackbuf;  // track buffer memory
	CapsULong tracklen;   // track buffer memory length
	CapsUByte *trackdata[CAPS_MTRS]; // track data pointer if available
	CapsULong tracksize[CAPS_MTRS]; // track data size
	CapsULong timelen;    // timing buffer length
	CapsULong *timebuf;   // timing buffer
};

// disk track information block
struct CapsTrackInfoT1 {
	CapsULong type;       // track type
	CapsULong cylinder;   // cylinder#
	CapsULong head;       // head#
	CapsULong sectorcnt;  // available sectors
	CapsULong sectorsize; // sector size
	CapsUByte *trackbuf;  // track buffer memory
	CapsULong tracklen;   // track buffer memory length
	CapsULong timelen;    // timing buffer length
	CapsULong *timebuf;   // timing buffer
	CapsLong overlap;     // overlap position
};

// disk track information block
struct CapsTrackInfoT2 {
	CapsULong type;       // track type
	CapsULong cylinder;   // cylinder#
	CapsULong head;       // head#
	CapsULong sectorcnt;  // available sectors
	CapsULong sectorsize; // sector size, unused
	CapsUByte *trackbuf;  // track buffer memory
	CapsULong tracklen;   // track buffer memory length
	CapsULong timelen;    // timing buffer length
	CapsULong *timebuf;   // timing buffer
	CapsLong overlap;     // overlap position
	CapsULong startbit;   // start position of the decoding
	CapsULong wseed;      // weak bit generator data
	CapsULong weakcnt;    // number of weak data areas
};

// disk sector information block
struct CapsSectorInfo {
	CapsULong descdatasize; // data size in bits from IPF descriptor
	CapsULong descgapsize;  // gap size in bits from IPF descriptor
	CapsULong datasize;     // data size in bits from decoder
	CapsULong gapsize;      // gap size in bits from decoder
	CapsULong datastart;    // data start position in bits from decoder
	CapsULong gapstart;     // gap start position in bits from decoder
	CapsULong gapsizews0;   // gap size before write splice
	CapsULong gapsizews1;   // gap size after write splice
	CapsULong gapws0mode;   // gap size mode before write splice
	CapsULong gapws1mode;   // gap size mode after write splice
	CapsULong celltype;     // bitcell type
	CapsULong enctype;      // encoder type
};

// disk data information block
struct CapsDataInfo {
	CapsULong type;  // data type
	CapsULong start; // start position
	CapsULong size;  // size in bits
};

#if defined(__GNUC__) && !defined(__mc68000__)
#pragma pack()
#endif // __GNUC__

// CapsImageInfo.image type
enum {
	ciitNA=0, // invalid image type
	ciitFDD   // floppy disk
};

// CapsImageInfo.platform IDs, not about configuration, but intended use
enum {
	ciipNA=0, // invalid platform (dummy entry)
	ciipAmiga,
	ciipAtariST,
	ciipPC,
	ciipAmstradCPC,
	ciipSpectrum,
	ciipSamCoupe,
	ciipArchimedes,
	ciipC64,
	ciipAtari8
};

// CapsTrackInfo.track type
enum {
	ctitNA=0,  // invalid type
	ctitNoise, // cells are unformatted (random size)
	ctitAuto,  // automatic cell size, according to track size
	ctitVar    // variable density
};

// CapsSectorInfo.bitcell type
enum {
	csicNA=0, // invalid cell type
	csic2us   // 2us cells
};

// CapsSectorInfo.encoder type
enum {
	csieNA=0, // undefined encoder
	csieMFM,  // MFM
	csieRaw   // no encoder used, test data only
};

// CapsSectorInfo.gap size mode
enum {
	csiegmFixed=0,  // fixed size, can't be changed
	csiegmAuto,     // size can be changed, resize information calculated automatically
	csiegmResize    // size can be changed, resize information is scripted
};

// CapsDataInfo.data type
enum {
	cditNA=0, // undefined
	cditWeak  // weak bits
};

// CAPSGetInfo inftype
enum {
	cgiitNA=0,   // illegal
	cgiitSector, // CapsSectorInfo
	cgiitWeak    // CapsDataInfo, weak bits
};

// image error status
enum {
	imgeOk=0,
	imgeUnsupported,
	imgeGeneric,
	imgeOutOfRange,
	imgeReadOnly,
	imgeOpen,
	imgeType,
	imgeShort,
	imgeTrackHeader,
	imgeTrackStream,
	imgeTrackData,
	imgeDensityHeader,
	imgeDensityStream,
	imgeDensityData,
	imgeIncompatible,
	imgeUnsupportedType,
	imgeBadBlockType,
	imgeBadBlockSize,
	imgeBadDataStart,
	imgeBufferShort
};

#ifndef CLIB_CAPSIMAGE_PROTOS_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

CapsLong CAPSInit(void);
CapsLong CAPSExit(void);
CapsLong CAPSAddImage(void);
CapsLong CAPSRemImage(CapsLong id);
CapsLong CAPSLockImage(CapsLong id, char *name);
CapsLong CAPSLockImageMemory(CapsLong id, CapsUByte *buffer, CapsULong length, CapsULong flag);
CapsLong CAPSUnlockImage(CapsLong id);
CapsLong CAPSLoadImage(CapsLong id, CapsULong flag);
CapsLong CAPSGetImageInfo(struct CapsImageInfo *pi, CapsLong id);
CapsLong CAPSLockTrack(void *ptrackinfo, CapsLong id, CapsULong cylinder, CapsULong head, CapsULong flag);
CapsLong CAPSUnlockTrack(CapsLong id, CapsULong cylinder, CapsULong head);
CapsLong CAPSUnlockAllTracks(CapsLong id);
char *CAPSGetPlatformName(CapsULong pid);
CapsLong CAPSGetVersionInfo(void *pversioninfo, CapsULong flag);
CapsLong CAPSGetInfo(void *pinfo, CapsLong id, CapsULong cylinder, CapsULong head, CapsULong inftype, CapsULong infid);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // CLIB_CAPSIMAGE_PROTOS_H

#endif // CAPS_CAPSIMAGE_H
