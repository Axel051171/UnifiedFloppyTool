#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cassert>

#include <string.h>

#include "image_base.h"

typedef struct {
	uint8_t HEADERSIGNATURE[8];		// "HXCPICFE"
	uint8_t formatrevision;			// Revision 0
	uint8_t number_of_track;		// Number of track in the file
	uint8_t number_of_side;			// Number of valid side (Not used by the emulator)
	uint8_t track_encoding;			// Track Encoding mode
	// (Used for the write support - Please see the list above)
	uint16_t bitRate;				// Bitrate in Kbit/s. Ex : 250=250,000bits/s
	// Max value : 500
	uint16_t floppyRPM;				// Rotation per minute (Not used by the emulator)
	uint8_t floppyinterfacemode;	// Floppy interface mode. (Please see the list above.)
	uint8_t dnu;					// Free
	uint16_t track_list_offset;		// Offset of the track list LUT in block of 512 bytes
	// (Ex: 1=0x200)
	uint8_t write_allowed;			// The Floppy image is write protected ?
	uint8_t single_step;			// 0xFF : Single Step ? 0x00 Double Step mode
	uint8_t track0s0_altencoding;	// 0x00 : Use an alternate track_encoding for track 0 Side 0
	uint8_t track0s0_encoding;		// alternate track_encoding for track 0 Side 0
	uint8_t track0s1_altencoding;	// 0x00 : Use an alternate track_encoding for track 0 Side 1
	uint8_t track0s1_encoding;		// alternate track_encoding for track 0 Side 1
} hfe_header;

// floppyinterfacemode
enum floppyinterfacemode_t {
	IBMPC_DD_FLOPPYMODE				= 0x00,
	IBMPC_HD_FLOPPYMODE				= 0x01,
	ATARIST_DD_FLOPPYMODE			= 0x02,
	ATARIST_HD_FLOPPYMODE			= 0x03,
	AMIGA_DD_FLOPPYMODE				= 0x04,
	AMIGA_HD_FLOPPYMODE				= 0x05,
	CPC_DD_FLOPPYMODE				= 0x06,
	GENERIC_SHUGGART_DD_FLOPPYMODE	= 0x07,
	IBMPC_ED_FLOPPYMODE				= 0x08,
	MSX2_DD_FLOPPYMODE				= 0x09,
	C64_DD_FLOPPYMODE				= 0x0A,
	EMU_SHUGART_FLOPPYMODE			= 0x0B,
	S950_DD_FLOPPYMODE				= 0x0C,
	S950_HD_FLOPPYMODE				= 0x0D,
	DISABLE_FLOPPYMODE				= 0xFE
};

// track_encoding
enum track_encoding_t {
	ISOIBM_MFM_ENCODING				= 0x00,
	AMIGA_MFM_ENCODING				= 0x01,
	ISOIBM_FM_ENCODING				= 0x02,
	EMU_FM_ENCODING					= 0x03,
	UNKNOWN_ENCODING				= 0xFF
};

typedef struct
{
	uint16_t offset;		// Offset of the track data in block of 512 bytes (Ex: 2=0x400)
	uint16_t track_len;		// Length of the track data in byte.
} hfe_track;

class disk_image_hfe : public disk_image {
public:
	disk_image_hfe(void) : disk_image() {}

	void read(const std::string file_name) override;
	void write(const std::string file_name) const override;

    disk_image_hfe& operator=(disk_image &image)
    {
        m_base_prop = image.get_property();
        m_track_data = image.get_track_data_all();
        return *this;
    }
};
