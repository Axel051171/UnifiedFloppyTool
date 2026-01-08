#ifndef CAPS_FDC_H
#define CAPS_FDC_H

#include <caps/capsimage.h>

// drive defaults
#define CAPSDRIVE_35DD_RPM 300
#define CAPSDRIVE_35DD_HST 83

// drive attributes
// 0: true disk inserted (if not inserted it is write protected)
// 1: true disk write protected
// 2: true motor on
#define CAPSDRIVE_DA_IN (1L<<0)
#define CAPSDRIVE_DA_WP (1L<<1)
#define CAPSDRIVE_DA_MO (1L<<2)

// index pulse only available if disk is inserted and motor is running
#define CAPSDRIVE_DA_IPMASK (CAPSDRIVE_DA_IN|CAPSDRIVE_DA_MO)

// fdc output lines
// 0: drq line state
// 1: intrq line state
// 2: forced interrupt, internal
// 3: motor line state
// 4: direction line state
// 5: interrupt on index pulse
// 6: set drq, internal
#define CAPSFDC_LO_DRQ    (1L<<0)
#define CAPSFDC_LO_INTRQ  (1L<<1)
#define CAPSFDC_LO_INTFRC (1L<<2)
#define CAPSFDC_LO_MO     (1L<<3)
#define CAPSFDC_LO_DIRC   (1L<<4)
#define CAPSFDC_LO_INTIP  (1L<<5)
#define CAPSFDC_LO_DRQSET (1L<<6)

// fdc status lines
// 0: busy
// 1: index pulse/drq
// 2: track0/lost data
// 3: crc error
// 4: record not found
// 5: spin-up/record type
// 6: write protect
// 7: motor on
#define CAPSFDC_SR_BUSY   (1L<<0)
#define CAPSFDC_SR_IP_DRQ (1L<<1)
#define CAPSFDC_SR_TR0_LD (1L<<2)
#define CAPSFDC_SR_CRCERR (1L<<3)
#define CAPSFDC_SR_RNF    (1L<<4)
#define CAPSFDC_SR_SU_RT  (1L<<5)
#define CAPSFDC_SR_WP     (1L<<6)
#define CAPSFDC_SR_MO     (1L<<7)

// flags to clear before new command
#define CAPSFDC_SR_NCCLR (CAPSFDC_SR_SU_RT|CAPSFDC_SR_RNF|CAPSFDC_SR_CRCERR|CAPSFDC_SR_BUSY)

// flags to set before new command
#define CAPSFDC_SR_NCSET CAPSFDC_SR_BUSY

// type1 mask, index/track0/spin-up
// type2 read mask, drq/lost data/record type/wp=0
// type2 write mask, drq/lost data/record type
#define CAPSFDC_SM_TYPE1 0x00
#define CAPSFDC_SM_TYPE2R (CAPSFDC_SR_IP_DRQ|CAPSFDC_SR_TR0_LD|CAPSFDC_SR_SU_RT|CAPSFDC_SR_WP)
#define CAPSFDC_SM_TYPE2W (CAPSFDC_SR_IP_DRQ|CAPSFDC_SR_TR0_LD|CAPSFDC_SR_SU_RT)

// end request flags
// 0: command complete
// 1: request to stop execution at current cycle; can be bitwise set in a callback
#define CAPSFDC_ER_COMEND (1L<<0)
#define CAPSFDC_ER_REQEND (1L<<1)

// am info flags
//  0: AM detector enabled
//  1: CRC logic enabled
//  2: CRC logic active after first mark found
//  3: data being assembled in DSR is the last byte of AM
//  4: data being assembled in DSR is an A1 mark
//  5: AM in decoder, valid for 1 cell
//  6: A1 mark in decoder, valid for 1 cell
//  7: C2 mark in decoder, valid for 1 cell
//  8: DSR holds a complete byte, valid until DSR has been changed
//  9: DSR synced to AM, valid until DSR has been changed, next DSRREADY is a data byte
// 10: DSR synced to A1 mark
#define CAPSFDC_AI_AMDETENABLE (1L<<0)
#define CAPSFDC_AI_CRCENABLE   (1L<<1)
#define CAPSFDC_AI_CRCACTIVE   (1L<<2)
#define CAPSFDC_AI_AMACTIVE    (1L<<3)
#define CAPSFDC_AI_MA1ACTIVE   (1L<<4)
#define CAPSFDC_AI_AMFOUND     (1L<<5)
#define CAPSFDC_AI_MARKA1      (1L<<6)
#define CAPSFDC_AI_MARKC2      (1L<<7)
#define CAPSFDC_AI_DSRREADY    (1L<<8)
#define CAPSFDC_AI_DSRAM       (1L<<9)
#define CAPSFDC_AI_DSRMA1      (1L<<10)

#if defined(__GNUC__) && !defined(__mc68000__)
#pragma pack(1)
#endif // __GNUC__

// drive state
struct CapsDrive {
	CapsULong type;      // structure size
	CapsULong rpm;       // drive rpm
	CapsLong maxtrack;   // track with hard stop (head can't move)
	CapsLong track;      // actual track
	CapsLong buftrack;   // track# in buffer
	CapsLong side;       // actual side used for processing
	CapsLong bufside;    // side# in buffer
	CapsLong newside;    // side to select after processing
	CapsULong diskattr;  // disk attributes
	CapsULong idistance; // distance from index in clock cycles
	CapsULong clockrev;  // clock cycles per revolution
	CapsLong clockip;    // clock cycles for index pulse hold
	CapsLong ipcnt;      // index pulse clock counter, <0 init, 0 stopped
	CapsULong ttype;     // track type
	CapsUByte *trackbuf; // track buffer memory
	CapsULong *timebuf;  // timing buffer
	CapsULong tracklen;  // track buffer memory length
	CapsLong overlap;    // overlap position
	CapsLong trackbits;  // used track size
	CapsLong ovlmin;     // overlap first bit position
	CapsLong ovlmax;     // overlap last bit position
	CapsLong ovlcnt;     // overlay bit count
	CapsLong ovlact;     // active overlay phase
	CapsLong nact;       // active noise phase
	CapsULong nseed;     // noise generator seed
	void *userptr;       // free to use pointer for the host application
	CapsULong userdata;  // free to use data for the host application
};

struct CapsFdc;
#ifdef __SASC
typedef void (* __asm CapsFdcHook)(register __a0 struct CapsFdc *pfdc, register __d0 CapsULong state);
#else
typedef void (*CapsFdcHook)(struct CapsFdc *pfdc, CapsULong state);
#endif // __SASC

// fdc state
struct CapsFdc {
	CapsULong type;         // structure size
	CapsULong model;        // fdc type
	CapsULong endrequest;   // non-zero value ends command
	CapsULong clockact;     // clock cycles completed
	CapsULong clockreq;     // requested clock cycles to complete
	CapsULong clockfrq;     // clock frequency
	CapsULong addressmask;  // valid address lines
	CapsULong dataline;     // data bus
	CapsULong datamask;     // valid data lines
	CapsULong lineout;      // output lines
	CapsULong runmode;      // run mode
	CapsULong runstate;     // local run state in a command
	CapsULong r_st0;        // status0 register
	CapsULong r_st1;        // status1 register
	CapsULong r_stm;        // status mask register (1 bits select st1)
	CapsULong r_command;    // command register
	CapsULong r_track;      // track register
	CapsULong r_sector;     // sector register
	CapsULong r_data;       // data register
	CapsULong seclenmask;   // sector length mask
	CapsULong seclen;       // sector length
	CapsULong crc;          // crc holder
	CapsULong crccnt;       // crc bit counter
	CapsULong amdecode;     // am detector decoder/shifter
	CapsULong aminfo;       // am info
	CapsULong amisigmask;   // enabled am info signal bits
	CapsLong amdatadelay;   // am data delay clock
	CapsLong amdataskip;    // am data skip clock
	CapsLong ammarkdist;    // am invalid distance from last mark in bitcells or 0 if valid
	CapsLong ammarktype;    // am last mark type
	CapsULong dsr;          // data shift register
	CapsLong dsrcnt;        // dsr bit counter
	CapsLong datalock;      // data lock bit position, <0 not locked
	CapsULong datamode;     // data access mode
	CapsULong datacycle;    // clock cycle remainder of actual bit
	CapsULong dataphase;    // data access phase
	CapsULong datapcnt;     // data phase counter
	CapsLong indexcount;    // index pulse counter
	CapsLong indexlimit;    // index pulse abort point
	CapsLong readlimit;     // read abort point
	CapsLong verifylimit;   // verify abort point
	CapsLong spinupcnt;     // counter for spin-up status
	CapsLong spinuplimit;   // spin-up point
	CapsLong idlecnt;       // counter for idle
	CapsLong idlelimit;     // idle point
	CapsULong clockcnt;     // clock counter
	CapsULong steptime[4];  // stepping rates us
	CapsULong clockstep[4]; // clock cycles for stepping rates
	CapsULong hstime;       // head settling delay us
	CapsULong clockhs;      // clock cycles for head settling
	CapsULong iptime;       // index pulse hold us
	CapsULong updatetime;   // update delay between short operations us
	CapsULong clockupdate;  // clock cycles for update delay
	CapsLong drivecnt;      // number of drives, 0: no drive attached
	CapsLong drivemax;      // number of active drives, 0: no drive attached
	CapsLong drivenew;      // drive to select after processing, <0 invalid
	CapsLong drivesel;      // drive selected for processing, <0 invalid
	CapsLong driveact;      // drive used for processing, <0 invalid
	struct CapsDrive *driveprc; // drive processed, helper
	struct CapsDrive *drive; // available drives
	CapsFdcHook cbirq;      // irq line change callback
	CapsFdcHook cbdrq;      // drq line change callback
	CapsFdcHook cbtrk;      // track change callback
	void *userptr;          // free to use pointer for the host application
	CapsULong userdata;     // free to use data for the host application
};

#if defined(__GNUC__) && !defined(__mc68000__)
#pragma pack()
#endif // __GNUC__

// emulator info
enum {
	cfdciNA=0,       // invalid
	cfdciSize_Fdc,   // size required for FDC structure
	cfdciSize_Drive, // size required for Drive structure
	cfdciR_Command,  // command register
	cfdciR_ST,       // status register
	cfdciR_Track,    // track register
	cfdciR_Sector,   // sector register
	cfdciR_Data,     // data register
};

// fdc models
enum {
	cfdcmNA=0,   // invalid fdc model
	cfdcmWD1772  // WD1772
};

// run modes
enum {
	cfdcrmNop=0,  // no-operation
	cfdcrmIdle,   // idle/wait loop
	cfdcrmType1,  // type 1 loop
	cfdcrmType2R, // type 2 read loop
	cfdcrmType2W, // type 2 write loop
	cfdcrmType3R, // type 3 read loop
	cfdcrmType3W, // type 3 write loop
	cfdcrmType3A, // type 3 address loop
	cfdcrmType4   // type 4 loop
};

// data modes
enum {
	cfdcdmNoline=0, // no line input
	cfdcdmNoise,    // noise input
	cfdcdmData,     // data input
	cfdcdmDMap      // data with density map
};

#ifndef CLIB_CAPSIMAGE_PROTOS_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

CapsULong CAPSFdcGetInfo(CapsLong iid, struct CapsFdc *pc, CapsLong ext);
CapsLong CAPSFdcInit(struct CapsFdc *pc);
void CAPSFdcReset(struct CapsFdc *pc);
void CAPSFdcEmulate(struct CapsFdc *pc, CapsULong cyclecnt);
CapsULong CAPSFdcRead(struct CapsFdc *pc, CapsULong address);
void CAPSFdcWrite(struct CapsFdc *pc, CapsULong address, CapsULong data);
CapsLong CAPSFdcInvalidateTrack(struct CapsFdc *pc, CapsLong drive);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // CLIB_CAPSIMAGE_PROTOS_H

#endif // CAPS_FDC_H
