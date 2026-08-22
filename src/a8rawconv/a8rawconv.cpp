// a8rawconv - A8 raw disk conversion utility
// Copyright (C) 2014-2020 Avery Lee
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "stdafx.h"
#include "analyze.h"
#include "compensation.h"
#include "encode.h"
#include "interleave.h"
#include "version.h"

int analyze_raw(const RawDisk& raw_disk, int selected_track);

DiskInfo g_disk;

int g_inputPathSidePos;
int g_inputPathSideWidth;
int g_inputPathSideBase;
int g_inputPathCountPos;
int g_inputPathCountWidth;

std::string g_outputPath;
bool g_showLayout;
bool g_encoding_fm = true;
bool g_encoding_mfm = true;
bool g_encoding_pcmfm = false;
bool g_encoding_amigamfm = false;
bool g_encoding_macgcr = false;
bool g_encoding_a2gcr = false;
bool g_encode_precise = false;
bool g_reverseTracks = false;
bool g_invertBit7 = false;
float g_clockPeriodAdjust = 1.0f;
bool g_layout_set = false;
int g_trackSelect = -1;
int g_trackCount = 40;
int g_trackStep = 2;
int g_sides = 1;
int g_revs = 5;
bool g_kryoflux_48tpi = false;
bool g_high_density = false;
bool g_erase_odd_tracks = false;
bool g_splice_mode = false;
InterleaveMode g_interleave = kInterleaveMode_Auto;
AnalysisMode g_analyze = kAnalysisMode_None;
PostCompensationMode g_postcomp = kPostComp_Auto;

enum InputFormat : uint8_t {
	kInputFormat_Auto,
	kInputFormat_KryoFluxStream,
	kInputFormat_SCP_Auto,
	kInputFormat_SCP_ForceSS40,
	kInputFormat_SCP_ForceDS40,
	kInputFormat_SCP_ForceSS80,
	kInputFormat_SCP_ForceDS80,
	kInputFormat_SuperCardProDirect,
	kInputFormat_Atari_ATR,
	kInputFormat_Atari_ATX,
	kInputFormat_Atari_XFD,
	kInputFormat_AppleII_DO,
	kInputFormat_AppleII_PO,
	kInputFormat_AppleII_NIB,
	kInputFormat_PC_VFD,
	kInputFormat_Amiga_ADF,
	kInputFormat_DiskScript
} g_inputFormat = kInputFormat_Auto;

enum OutputFormat : uint8_t {
	kOutputFormat_Auto,
	kOutputFormat_Atari_ATX,
	kOutputFormat_Atari_ATR,
	kOutputFormat_Atari_XFD,
	kOutputFormat_SCP_Auto,
	kOutputFormat_SCP_ForceSS40,
	kOutputFormat_SCP_ForceDS40,
	kOutputFormat_SCP_ForceSS80,
	kOutputFormat_SCP_ForceDS80,
	kOutputFormat_SCPDirect,
	kOutputFormat_AppleII_DO,
	kOutputFormat_AppleII_PO,
	kOutputFormat_AppleII_NIB,
	kOutputFormat_Mac_DSK,
	kOutputFormat_Amiga_ADF,
	kOutputFormat_PC_VFD,
} g_outputFormat = kOutputFormat_Auto;

///////////////////////////////////////////////////////////////////////////

void process_track_fm(const RawTrack& rawTrack);
void process_track_mfm(const RawTrack& rawTrack, bool decode_amiga, bool use_300rpm);
void process_track_macgcr(const RawTrack& rawTrack);
void process_track_a2gcr(const RawTrack& rawTrack);

void process_track(const RawTrack& rawTrack) {
	if (g_encoding_fm)
		process_track_fm(rawTrack);
	
	if (g_encoding_mfm)
		process_track_mfm(rawTrack, false, false);

	if (g_encoding_pcmfm)
		process_track_mfm(rawTrack, false, true);

	if (g_encoding_amigamfm)
		process_track_mfm(rawTrack, true, true);

	if (g_encoding_macgcr)
		process_track_macgcr(rawTrack);

	if (g_encoding_a2gcr)
		process_track_a2gcr(rawTrack);
}

//////////////////////////////////////////////////////////////////////////

void process_track_fm(const RawTrack& rawTrack) {
	if (rawTrack.mTransitions.size() < 2)
		return;

	// Atari disk timing produces 250,000 clocks per second at 288 RPM. We must compute the
	// effective sample rate given the actual disk rate.
	const double cells_per_rev = 250000.0 / (288.0 / 60.0) * (g_high_density ? 2 : 1);
	double scks_per_cell = rawTrack.mSamplesPerRev / cells_per_rev * g_clockPeriodAdjust;

	//printf("%.2f samples per cell\n", scks_per_cell);

	const uint32_t *samp = rawTrack.mTransitions.data();
	size_t samps_left = rawTrack.mTransitions.size() - 1;
	int time_basis = 0;
	int time_left = 0;

	int cell_len = (int)(scks_per_cell + 0.5);
	int cell_range = cell_len / 3;
	int cell_timer = 0;
	int cell_fine_adjust = 0;

	uint8_t shift_even = 0;
	uint8_t shift_odd = 0;

	std::vector<SectorParser> sectorParsers;
	uint8_t spew_data[16];
	int spew_index = 0;
	uint32_t spew_last_time = samp[0];

	for(;;) {
		while (time_left <= 0) {
			if (!samps_left)
				goto done;

			int delta = samp[1] - samp[0];

			if (g_verbosity >= 4)
				printf(" %02X %02X | %3d | %d\n", shift_even, shift_odd, delta, samp[0]);

			time_left += delta;
			time_basis = samp[1];
			++samp;
			--samps_left;
		}

		//printf("next_trans = %d, cell_timer = %d, %d transitions left\n", time_left, cell_timer, samps_left);

		// if the shift register is empty, restart shift timing at next transition
		if (!(shift_even | shift_odd)) {
			time_left = 0;
			cell_timer = cell_len;
			shift_even = 0;
			shift_odd = 1;
		} else {
			// compare time to next transition against cell length
			int trans_delta = time_left - cell_timer;

			if (trans_delta < -cell_range) {
				if (g_verbosity >= 4)
					printf(" %02X %02X | delta = %+3d | ignore\n", shift_even, shift_odd, trans_delta);
				// ignore the transition
				cell_timer -= time_left;
				continue;
			}

			std::swap(shift_even, shift_odd);
			shift_odd += shift_odd;
			
			if (trans_delta <= cell_range) {
				++shift_odd;

				if (g_verbosity >= 4)
					printf(" %02X %02X | delta = %+3d | 1\n", shift_even, shift_odd, trans_delta);

				// we have a transition in range -- clock in a 1 bit
				cell_timer = cell_len;
				time_left = 0;

				// adjust clocking by phase error
				if (trans_delta < -5)
					cell_timer -= 3;
				else if (trans_delta < -3)
					cell_timer -= 2;
				else if (trans_delta < 1)
					--cell_timer;
				else if (trans_delta > 1)
					++cell_timer;
				else if (trans_delta > 3)
					cell_timer += 2;
				else if (trans_delta > 5)
					cell_timer += 3;
			} else {
				if (g_verbosity >= 4)
					printf(" %02X %02X | delta = %+3d | 0\n", shift_even, shift_odd, trans_delta);

				// we don't have a transition in range -- clock in a 0 bit
				time_left -= cell_timer;
				cell_timer = cell_len + (cell_fine_adjust / 256);
			}

			if (g_verbosity >= 3) {
				spew_data[spew_index] = shift_odd;
				if (++spew_index == 16) {
					spew_index = 0;
					printf("%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X | %.2f\n"
						, spew_data[0]
						, spew_data[1]
						, spew_data[2]
						, spew_data[3]
						, spew_data[4]
						, spew_data[5]
						, spew_data[6]
						, spew_data[7]
						, spew_data[8]
						, spew_data[9]
						, spew_data[10]
						, spew_data[11]
						, spew_data[12]
						, spew_data[13]
						, spew_data[14]
						, spew_data[15]
						, (double)(time_basis - time_left - spew_last_time) / scks_per_cell
						);

					spew_last_time = time_basis - time_left;
				}
			}
			
			const uint32_t vsn_time = time_basis - time_left;
			for(auto it = sectorParsers.begin(); it != sectorParsers.end();) {
				if (it->Parse(vsn_time, shift_even, shift_odd))
					++it;
				else
					it = sectorParsers.erase(it);
			}

			if (shift_even == 0xC7 && shift_odd == 0xFE) {
				sectorParsers.emplace_back();
				sectorParsers.back().Init(rawTrack.mPhysTrack / g_trackStep, &rawTrack.mIndexTimes, (float)scks_per_cell, &g_disk.mPhysTracks[rawTrack.mSide][rawTrack.mPhysTrack], vsn_time);
			}
		}
	}

done:
	;
}

void process_track_mfm(const RawTrack& rawTrack, bool decode_amiga, bool use_300rpm) {
	if (rawTrack.mTransitions.size() < 2)
		return;

	const double cells_per_rev = 500000.0 / ((use_300rpm ? 300.0 : 288.0) / 60.0) * (g_high_density ? 2 : 1);
	double scks_per_cell = rawTrack.mSamplesPerRev / cells_per_rev * g_clockPeriodAdjust;

	auto transitions = rawTrack.mTransitions;

	if(0){
		size_t n = transitions.size();

		if (n > 3) {
			uint32_t t0 = transitions[0];
			uint32_t t1 = transitions[1];

//			int thresh = (int)(0.5 + 2.0 / cells_per_sample);
			int thresh = (int)(0.5 + rawTrack.mSamplesPerRev / 90000.0 * (float)(400 - rawTrack.mPhysTrack) / 400.0f);

			for(size_t i=2; i<n; ++i) {
				uint32_t t2 = transitions[i];
				int32_t t01 = (int32_t)(t1 - t0);
				int32_t t12 = (int32_t)(t2 - t1);

				int32_t delta1 = std::max<int>(0, thresh - t01) * 5 / 12;
				int32_t delta2 = std::max<int>(0, thresh - t12) * 5 / 12;

				transitions[i-1] = t1 - delta1 + delta2;

				t0 = t1;
				t1 = t2;
			}
		}
	}

	const uint32_t *samp = transitions.data();
	size_t samps_left = transitions.size() - 1;
	int time_basis = 0;
	int time_left = 0;

	int cell_len = (int)(scks_per_cell + 0.5);
	int cell_range = cell_len / 2;
	int cell_timer = 0;

	uint8_t shift_even = 0;
	uint8_t shift_odd = 0;

	std::vector<SectorParserMFM> sectorParsers;
	std::vector<SectorParserMFMAmiga> amigaSectorParsers;
	uint8_t spew_data[16];
	int spew_index = 0;
	uint32_t spew_last_time = samp[0];

	int state = 0;
	for(;;) {
		while (time_left <= 0) {
			if (!samps_left)
				goto done;

			time_left += samp[1] - samp[0];
			time_basis = samp[1];
			++samp;
			--samps_left;
		}

//		printf("next_trans = %d, cell_timer = %d, %d transitions left\n", time_left, cell_timer, samps_left);

		// if the shift register is empty, restart shift timing at next transition
		if (!(shift_even | shift_odd)) {
			time_left = 0;
			cell_timer = cell_len;
			shift_even = 0;
			shift_odd = 1;
		} else {
			// compare time to next transition against cell length
			int trans_delta = time_left - cell_timer;

			if (trans_delta < -cell_range) {
				// ignore the transition
				cell_timer -= time_left;
				continue;
			}

			std::swap(shift_even, shift_odd);
			shift_odd += shift_odd;
			
			if (trans_delta <= cell_range) {
				cell_timer = cell_len;

				// we have a transition in range -- clock in a 1 bit
				if (trans_delta < -5)
					cell_timer -= 3;
				else if (trans_delta < -3)
					cell_timer -= 2;
				else if (trans_delta < 1)
					--cell_timer;
				else if (trans_delta > 1)
					++cell_timer;
				else if (trans_delta > 3)
					cell_timer += 2;
				else if (trans_delta > 5)
					cell_timer += 3;

				shift_odd++;
				time_left = 0;
			} else {
				// we don't have a transition in range -- clock in a 0 bit
				time_left -= cell_timer;
				cell_timer = cell_len;
			}

			if (g_verbosity >= 3) {
				spew_data[spew_index] = shift_odd;
				if (++spew_index == 16) {
					spew_index = 0;
					printf("%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X | %.2f\n"
						, spew_data[0]
						, spew_data[1]
						, spew_data[2]
						, spew_data[3]
						, spew_data[4]
						, spew_data[5]
						, spew_data[6]
						, spew_data[7]
						, spew_data[8]
						, spew_data[9]
						, spew_data[10]
						, spew_data[11]
						, spew_data[12]
						, spew_data[13]
						, spew_data[14]
						, spew_data[15]
						, (double)(time_basis - time_left - spew_last_time) / scks_per_cell
						);

					spew_last_time = time_basis - time_left;
				}
			}

			const uint32_t vsn_time = time_basis - time_left;

			if (decode_amiga) {
				for(auto it = amigaSectorParsers.begin(); it != amigaSectorParsers.end();) {
					if (it->Parse(vsn_time, shift_even, shift_odd))
						++it;
					else
						it = amigaSectorParsers.erase(it);
				}
			} else {
				for(auto it = sectorParsers.begin(); it != sectorParsers.end();) {
					if (it->Parse(vsn_time, shift_even, shift_odd))
						++it;
					else
						it = sectorParsers.erase(it);
				}
			}

			// The IDAM is 0xA1 with a missing clock pulse:
			//
			// data		 0 0 0 0 0 0 0 0 1 0 1 0 0 0 0 1
			// clock	1 1 1 1 1 1 1 1 0 0 0 0 1>0<1 0

			if (state == 0) {
				if (shift_even == 0x0A && shift_odd == 0xA1)
					++state;
			} else if (state == 16) {
				if (shift_even == 0x0A && shift_odd == 0xA1) {
					++state;

					if (decode_amiga) {
						amigaSectorParsers.emplace_back();
						amigaSectorParsers.back().Init(rawTrack.mPhysTrack, rawTrack.mSide, &rawTrack.mIndexTimes, (float)scks_per_cell, &g_disk.mPhysTracks[rawTrack.mSide][rawTrack.mPhysTrack], vsn_time);
						state = 0;
					}
				} else
					state = 0;
			} else if (state == 32) {
				if (shift_even == 0x0A && shift_odd == 0xA1) {
					sectorParsers.emplace_back();
					sectorParsers.back().Init(rawTrack.mPhysTrack / g_trackStep, rawTrack.mSide, &rawTrack.mIndexTimes, (float)scks_per_cell, &g_disk.mPhysTracks[rawTrack.mSide][rawTrack.mPhysTrack], vsn_time);
				}

				state = 0;
			} else {
				++state;
			}
		}
	}

done:
	;
}

static const uint8_t kGCR6Decoder[256]={
#define IL 255
	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,
	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,
	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,
	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,
	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,
	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,
	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,
	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,
	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,

	// $90
	IL,IL,IL,IL,IL,IL, 0, 1,IL,IL, 2, 3,IL, 4, 5, 6,

	// $A0
	IL,IL,IL,IL,IL,IL, 7, 8,IL,IL, 8, 9,10,11,12,13,

	// $B0
	IL,IL,14,15,16,17,18,19,IL,20,21,22,23,24,25,26,

	// $C0
	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,27,IL,28,29,30,

	// $D0
	IL,IL,IL,31,IL,IL,32,33,IL,34,35,36,37,38,39,40,

	// $E0
	IL,IL,IL,IL,IL,41,42,43,IL,44,45,46,47,48,49,50,

	// $F0
	IL,IL,51,52,53,54,55,56,IL,57,58,59,60,61,62,63,
#undef IL
};

void process_track_macgcr(const RawTrack& rawTrack) {
	double rpm = 590.0;

	if (rawTrack.mPhysTrack < 16)
		rpm = 394.0;
	else if (rawTrack.mPhysTrack < 32)
		rpm = 429.0;
	else if (rawTrack.mPhysTrack < 48)
		rpm = 472.0;
	else if (rawTrack.mPhysTrack < 64)
		rpm = 525.0;

	// Macintosh / Unidisk bit cells are not exactly 2us, but rather 2.02ms -- due
	// to a 7.8336MHz FCLK being divided by 16.
	const double cells_per_rev = 1000000.0 / 2.02 / (rpm / 60.0);
	double scks_per_cell = rawTrack.mSamplesPerRev / cells_per_rev * g_clockPeriodAdjust;

	const uint32_t *samp = rawTrack.mTransitions.data();
	size_t samps_left = rawTrack.mTransitions.size() - 1;
	int time_left = 0;
	int time_basis = 0;

	int cell_len = (int)(scks_per_cell + 0.5);
	int cell_range = cell_len / 2;
	int cell_timer = 0;

	uint8_t shifter = 0;

	int bit_state = 0;
	int byte_state = 0;

	int sector_headers = 0;
	int data_sectors = 0;
	int good_sectors = 0;

	uint8_t buf[704];
	uint8_t decbuf[528];

	int sector = -1;
	int last_byte_time = 0;

	float sector_position = 0;
	uint32_t raw_start = 0;
	uint32_t rot_start = 0;
	uint32_t rot_end = 0;

	if (rawTrack.mTransitions.size() >= 2) {
		for(;;) {
			while (time_left <= 0) {
				if (!samps_left)
					goto done;

				time_left += samp[1] - samp[0];
				time_basis = samp[1];
				++samp;
				--samps_left;
			}

			// if the shift register is empty, restart shift timing at next transition
			if (!shifter) {
				time_left = 0;
				cell_timer = cell_len;
				shifter = 1;

				bit_state = 0;
			} else {
				// compare time to next transition against cell length
				int trans_delta = time_left - cell_timer;

				shifter += shifter;

				if (trans_delta <= cell_range) {
					cell_timer = cell_len;
					time_left = 0;

					// we have a transition in range -- clock in a 1 bit
					if (trans_delta < -5)
						cell_timer -= 3;
					else if (trans_delta < -3)
						cell_timer -= 2;
					else if (trans_delta < 1)
						--cell_timer;
					else if (trans_delta > 1)
						++cell_timer;
					else if (trans_delta > 3)
						cell_timer += 2;
					else if (trans_delta > 5)
						cell_timer += 3;

					shifter++;
				} else {
					// we don't have a transition in range -- clock in a 0 bit
					time_left -= cell_timer;
					cell_timer = cell_len;
				}

				// advance bit machine state
				if (bit_state == 0) {
					if (shifter & 0x80) {
						bit_state = 1;

						if (g_verbosity >= 3) {
							int t = time_basis - time_left;

							printf("%02X (%.2f)\n", shifter, (float)(t - last_byte_time) / (scks_per_cell * 8));
							last_byte_time = t;
						}

						// okay, we have a byte... advance the byte state machine.
						if (byte_state == 0) {			// waiting for FF
							if (shifter == 0xFF)
								byte_state = 1;
						} else if (byte_state == 1) {	// waiting for D5 in address/data mark
							if (shifter == 0xD5)
								byte_state = 2;
							else if (shifter != 0xFF)
								byte_state = 0;
						} else if (byte_state == 2) {	// waiting for AA in address/data mark
							if (shifter == 0xAA)
								byte_state = 3;
							else if (shifter == 0xFF)
								byte_state = 1;
							else
								byte_state = 0;
						} else if (byte_state == 3) {	// waiting for 96 for address mark or AD for data mark
							if (shifter == 0x96)
								byte_state = 10;
							else if (shifter == 0xAD)
								byte_state = (sector >= 0 ? 1000 : 0);
							else if (shifter == 0xFF)
								byte_state = 1;
							else
								byte_state = 0;
						} else if (byte_state >= 10 && byte_state < 15) {
							// found D5 AA 96 for address mark - read track, sector, side,
							// format, checksum bytes
							buf[byte_state - 10] = shifter;

							if (++byte_state == 15) {
								uint8_t checksum = 0;

								for(int i=0; i<5; ++i) {
									decbuf[i] = kGCR6Decoder[buf[i]];
									checksum ^= decbuf[i];
								}

								if (!checksum) {
									sector = decbuf[1];		// zero-based

									int track = decbuf[0] + ((decbuf[2] & 1) << 6);
									int side = decbuf[2] & 0x20 ? 1 : 0;

									if (track != rawTrack.mPhysTrack || side != rawTrack.mSide) {
										printf("Ignoring sector header -- track %d, side %d, sector %d is on the wrong track.\n", track, side, sector);
										goto reject;
									}

									if (g_verbosity >= 2)
										printf("Sector header %02X %02X %02X %02X %02X (checksum OK)\n", decbuf[0], decbuf[1], decbuf[2], decbuf[3], decbuf[4]);

									// find the nearest index mark
									int vsn_time = time_basis - time_left;
									auto it_index = std::upper_bound(rawTrack.mIndexTimes.begin(), rawTrack.mIndexTimes.end(), (uint32_t)vsn_time + 1);

									if (it_index == rawTrack.mIndexTimes.begin()) {
										if (g_verbosity >= 2)
											printf("Skipping track %d, sector %d before first index mark\n", rawTrack.mPhysTrack, decbuf[2]);

										goto reject;
									}

									if (it_index == rawTrack.mIndexTimes.end()) {
										if (g_verbosity >= 2)
											printf("Skipping track %d, sector %d after last index mark\n", rawTrack.mPhysTrack, decbuf[2]);
								
										goto reject;
									}

									int vsn_offset = vsn_time - *--it_index;

									rot_start = it_index[0];
									rot_end = it_index[1];

									sector_position = (float)vsn_offset / (float)(it_index[1] - it_index[0]);

									if (sector_position >= 1.0f)
										sector_position -= 1.0f;
								} else {
									if (g_verbosity >= 2)
										printf("Sector header %02X %02X %02X %02X %02X (checksum BAD)\n", decbuf[0], decbuf[1], decbuf[2], decbuf[3], decbuf[4]);

reject:
									sector = -1;
								}

								++sector_headers;
								byte_state = 0;
							}
						} else if (byte_state >= 1000 && byte_state < 1704) {
							buf[byte_state - 1000] = shifter;

							if (++byte_state == 1704) {
								do {
									// check if sector is correct
									int marked_sector = kGCR6Decoder[buf[0]];
									if (marked_sector != sector) {
										printf("Rejecting sector %d (expected sector %d)\n", marked_sector, sector);
										break;
									}

									// decode first 522 of 524 data bytes
									uint8_t checksumA = 0;
									uint8_t checksumB = 0;
									uint8_t checksumC = 0;
									uint8_t carry = 0;
									uint32_t invalid = 0;

									for(int i=0; i<175; ++i) {
										const uint8_t x0 = kGCR6Decoder[buf[i*4+0+1]];
										const uint8_t x1 = kGCR6Decoder[buf[i*4+1+1]];
										const uint8_t x2 = kGCR6Decoder[buf[i*4+2+1]];
										const uint8_t x3 = kGCR6Decoder[buf[i*4+3+1]];

										invalid += (x0 >> 7);
										invalid += (x1 >> 7);
										invalid += (x2 >> 7);
										invalid += (x3 >> 7);

										checksumC = (checksumC << 1) + (checksumC >> 7);

										uint8_t y0 = x1 + ((x0 << 2) & 0xc0);
										y0 ^= checksumC;

										uint32_t tmpSumA = (uint32_t)checksumA + y0 + (checksumC & 1);
										checksumA = (uint8_t)tmpSumA;
										carry = (uint8_t)(tmpSumA >> 8);

										uint8_t y1 = x2 + ((x0 << 4) & 0xc0);
										y1 ^= checksumA;

										uint32_t tmpSumB = (uint32_t)checksumB + y1 + carry;
										checksumB = (uint8_t)tmpSumB;
										carry = (uint8_t)(tmpSumB >> 8);

										decbuf[i*3+0] = y0;
										decbuf[i*3+1] = y1;

										if (i<174) {		// @&*(@$
											uint8_t y2 = x3 + ((x0 << 6) & 0xc0);
											y2 ^= checksumB;

											uint32_t tmpSumC = (uint32_t)checksumC + y2 + carry;
											checksumC = (uint8_t)tmpSumC;
											carry = (uint8_t)(tmpSumC >> 8);
											decbuf[i*3+2] = y2;
										}
									}

									const uint8_t z0 = kGCR6Decoder[buf[175*4+0]];
									const uint8_t z1 = kGCR6Decoder[buf[175*4+1]];
									const uint8_t z2 = kGCR6Decoder[buf[175*4+2]];
									const uint8_t z3 = kGCR6Decoder[buf[175*4+3]];
									invalid += (z0 >> 7);
									invalid += (z1 >> 7);
									invalid += (z2 >> 7);
									invalid += (z3 >> 7);

									uint8_t decCheckA = z1 + ((z0 << 2) & 0xc0);
									uint8_t decCheckB = z2 + ((z0 << 4) & 0xc0);
									uint8_t decCheckC = z3 + ((z0 << 6) & 0xc0);

									if (invalid && g_verbosity >= 2)
										printf("%u invalid GCR bytes encountered\n", invalid);

									bool checksumOK = (checksumA == decCheckA && checksumB == decCheckB && checksumC == decCheckC);

									if (g_verbosity >= 2) {
										printf("checksums: %02X %02X %02X vs. %02X %02X %02X (%s)\n"
											, checksumA
											, checksumB
											, checksumC
											, decCheckA
											, decCheckB
											, decCheckC
											, checksumOK
												? "good" : "BAD"
											);
									}

									++data_sectors;

									if (checksumOK)
										++good_sectors;

									int vsn_time = time_basis - time_left;

									auto& tracksecs = g_disk.mPhysTracks[rawTrack.mSide][rawTrack.mPhysTrack].mSectors;
									tracksecs.emplace_back();
									SectorInfo& newsec = tracksecs.back();

									memcpy(newsec.mData, decbuf, 512);

									newsec.mIndex = sector;
									newsec.mRawStart = raw_start;
									newsec.mRawEnd = vsn_time;
									newsec.mPosition = sector_position;
									newsec.mEndingPosition = (float)(vsn_time - rot_start) / (float)(rot_end - rot_start);		// FIXME
									newsec.mEndingPosition -= floorf(newsec.mEndingPosition);
									newsec.mAddressMark = 0;
									newsec.mRecordedAddressCRC = 0;
									newsec.mComputedAddressCRC = 0;
									newsec.mRecordedCRC = ((uint32_t)checksumA << 16) + ((uint32_t)checksumB << 8) + (uint32_t)checksumC;
									newsec.mComputedCRC = ((uint32_t)decCheckA << 16) + ((uint32_t)decCheckB << 8) + (uint32_t)decCheckC;
									newsec.mSectorSize = 512;
									newsec.mbMFM = false;
									newsec.mWeakOffset = -1;

									if (g_verbosity >= 1)
										printf("Decoded Mac track %2d.%d, sector %2d [pos %.3f-%.3f]\n",
											rawTrack.mPhysTrack,
											rawTrack.mSide,
											sector,
											newsec.mPosition,
											newsec.mEndingPosition);

								} while(false);

								byte_state = 1;
							}
						}
					}
				} else {
					++bit_state;

					if (bit_state == 8)
						bit_state = 0;
				}
			}
		}
	}

done:
	;

	if (g_verbosity > 0) {
		printf("%d sector headers decoded\n", sector_headers);
		printf("%d data sectors decoded\n", data_sectors);
		printf("%d good sectors decoded\n", good_sectors);
	}
}

///////////////////////////////////////////////////////////////////////////
void process_track_a2gcr(const RawTrack& rawTrack) {
	double rpm = 300.0;

	const double cells_per_rev = 250000.0 / (rpm / 60.0);
	double scks_per_cell = rawTrack.mSamplesPerRev / cells_per_rev * g_clockPeriodAdjust;

	const uint8_t logical_track = rawTrack.mPhysTrack / g_trackStep;
	auto& decTrack = g_disk.mPhysTracks[rawTrack.mSide][rawTrack.mPhysTrack];
	
	if (rawTrack.mTransitions.size() < 2)
		return;

	const uint32_t *samp = rawTrack.mTransitions.data();
	size_t samps_left = rawTrack.mTransitions.size() - 1;
	int time_left = 0;
	int time_basis = 0;

	int cell_len = (int)(scks_per_cell + 0.5);
	int cell_range = cell_len / 3;
	int cell_timer = 0;

	uint8_t shifter = 0;

	int bit_state = 0;
	int byte_state = 0;

	int sector_headers = 0;
	int data_sectors = 0;
	int good_sectors = 0;

	int sector_index = -1;
	float sector_position = 0;
	uint8_t sector_volume = 0;
	uint32_t raw_start = 0;
	uint32_t rot_start = 0;
	uint32_t rot_end = 0;

	uint8_t buf[704];
	uint8_t decbuf[528];

	for(;;) {
		while (time_left <= 0) {
			if (!samps_left)
				goto done;

			//printf("%d\n", samp[1] - samp[0]);
			time_left += samp[1] - samp[0];
			time_basis = samp[1];
			++samp;
			--samps_left;
		}

		// if the shift register is empty, restart shift timing at next transition
		if (!shifter) {
			time_left = 0;
			cell_timer = cell_len;
			shifter = 1;
			bit_state = 0;
		} else {
			// compare time to next transition against cell length
			int trans_delta = time_left - cell_timer;

			if (0 && trans_delta < -cell_range) {
				// ignore the transition
				cell_timer -= time_left;
				time_left = 0;
				continue;
			}

			shifter += shifter;
			
			if (trans_delta <= cell_range) {
				cell_timer = cell_len - trans_delta/3;
				time_left = 0;

				shifter++;
			} else {
				// we don't have a transition in range -- clock in a 0 bit
				time_left -= cell_timer;
				cell_timer = cell_len;
			}

			// advance bit machine state
			if (bit_state == 0) {
				if (shifter & 0x80) {
					bit_state = 1;

					decTrack.mGCRData.push_back(shifter);

					if (g_verbosity >= 2)
						printf("%4u  %02X\n", byte_state, shifter);

					// okay, we have a byte... advance the byte state machine.
					if (byte_state == 0) {			// waiting for FF
						raw_start = time_basis - time_left;

						if (shifter == 0xFF)
							byte_state = 1;
					} else if (byte_state == 1) {	// waiting for D5 in address/data mark
						if (shifter == 0xD5)
							byte_state = 2;
						else if (shifter != 0xFF)
							byte_state = 0;
					} else if (byte_state == 2) {	// waiting for AA in address/data mark
						if (shifter == 0xAA)
							byte_state = 3;
						else if (shifter == 0xFF)
							byte_state = 1;
						else
							byte_state = 0;
					} else if (byte_state == 3) {	// waiting for 96 for address mark or AD for data mark
						if (shifter == 0x96)
							byte_state = 10;
						else if (shifter == 0xAD) {
							if (sector_index >= 0)
								byte_state = 1000;
							else
								byte_state = 1;
						} else if (shifter == 0xFF)
							byte_state = 1;
						else
							byte_state = 0;
					} else if (byte_state >= 10 && byte_state < 18) {
						// found D5 AA 96 for address mark - read volume, track, sector, checksum
						// in 4-4 encoding
						buf[byte_state - 10] = shifter;

						if (++byte_state == 18) {
							uint8_t checksum = 0;

							for(int i=0; i<4; ++i) {
								decbuf[i] = (buf[i*2] & 0x55)*2 + (buf[i*2+1] & 0x55);
								checksum ^= decbuf[i];
							}

							byte_state = 0;
							if (!checksum) {
								// toss it if it's the wrong track number
								if (decbuf[1] != logical_track)
									continue;

								if (g_verbosity >= 1)
									printf("Sector header %02X %02X %02X %02X\n", decbuf[0], decbuf[1], decbuf[2], decbuf[3]);

								// find the nearest index mark
								int vsn_time = time_basis - time_left;
								auto it_index = std::upper_bound(rawTrack.mIndexTimes.begin(), rawTrack.mIndexTimes.end(), (uint32_t)vsn_time + 1);

								if (it_index == rawTrack.mIndexTimes.begin()) {
									if (g_verbosity >= 2)
										printf("Skipping track %d, sector %d before first index mark\n", logical_track, decbuf[2]);

									continue;
								}

								if (it_index == rawTrack.mIndexTimes.end()) {
									if (g_verbosity >= 2)
										printf("Skipping track %d, sector %d after last index mark\n", logical_track, decbuf[2]);
								
									continue;
								}

								int vsn_offset = vsn_time - *--it_index;

								rot_start = it_index[0];
								rot_end = it_index[1];

								sector_position = (float)vsn_offset / (float)(it_index[1] - it_index[0]);

								if (sector_position >= 1.0f)
									sector_position -= 1.0f;

								sector_volume = decbuf[0];
								sector_index = decbuf[2];
								++sector_headers;
							}
						}
					} else if (byte_state >= 1000 && byte_state < 1343) {
						buf[byte_state - 1000] = shifter;

						if (++byte_state == 1343) {
							int vsn_time = time_basis - time_left;
							uint8_t chksum = 0;
							uint32_t invalid = 0;

							for(int i=0; i<343; ++i) {
								const uint8_t z0 = kGCR6Decoder[buf[i]];
								invalid += (z0 >> 7);
								chksum ^= z0;

								decbuf[i] = chksum & 0x3f;
							}

							if (invalid)
								printf("%u invalid GCR bytes encountered\n", invalid);

							bool checksumOK = !chksum;

							if (!checksumOK && g_verbosity >= 1) {
								printf("(%d) Checksum mismatch! %02X\n", sector_index, chksum);
							}

							++data_sectors;

							if (checksumOK)
								++good_sectors;

							auto& secs = decTrack.mSectors;
							secs.emplace_back();
							auto& sector = secs.back();

							sector.mbMFM = false;
							sector.mAddressMark = sector_volume;
							sector.mComputedAddressCRC = 0;
							sector.mRecordedAddressCRC = 0;
							sector.mComputedCRC = 0;
							sector.mRecordedCRC = chksum;
							sector.mSectorSize = 256;
							sector.mWeakOffset = -1;
							sector.mIndex = sector_index;
							sector.mRawStart = raw_start;
							sector.mRawEnd = vsn_time;
							sector.mPosition = sector_position;
							sector.mEndingPosition = (float)(vsn_time - rot_start) / (float)(rot_end - rot_start);
							sector.mEndingPosition -= floorf(sector.mEndingPosition);

							// Decode the sector data.
							//
							// Apple II sector data uses 6-and-2 encoding to encode 256 data bytes as 342 GCR
							// bytes, plus an additional checksum byte. Decoding first involves an adjacent-XOR
							// step as part of the checksum pass (already done above). Next, the two bits from
							// the fragments are combined with 6 bits from the data payload.

							const uint8_t invert = g_invertBit7 ? 0x80 : 0x00;
							for(int i=0; i<256; ++i) {
								uint8_t c = decbuf[i + 86] << 2;
								uint8_t d;

								if (i >= 172)
									d = (decbuf[i - 172] >> 4) & 0x03;
								else if (i >= 86)
									d = (decbuf[i - 86] >> 2) & 0x03;
								else
									d = (decbuf[i] >> 0) & 0x03;

								sector.mData[i] = (c + ((d & 2) >> 1) + ((d & 1) << 1)) ^ invert;
							}

							byte_state = 1;
							sector_index = -1;
						}
					}
				}
			} else {
				++bit_state;

				if (bit_state == 8)
					bit_state = 0;
			}
		}
	}

done:
	;

	if (g_verbosity > 0) {
		printf("%d sector headers decoded\n", sector_headers);
		printf("%d data sectors decoded\n", data_sectors);
		printf("%d good sectors decoded\n", good_sectors);
	}
}

///////////////////////////////////////////////////////////////////////////

void banner() {
	puts("A8 raw disk conversion utility v" A8RC_VERSION);
	puts("Copyright (C) 2014-2023 Avery Lee, All Rights Reserved.");
	puts("Licensed under GNU General Public License, version 2 or later.");
	puts("");
}

void exit_usage() {
	puts(R"--(Usage: a8rawconv [options] input output
	
Options:
    -analyze  Analyze flux timing
            atari-fm   Calibrate for 288 RPM, 4us bit cell
            atari-mfm  Calibrate for 288 RPM, 2us bit cell
            pc-360k    Calibrate for 300 RPM, 2us bit cell
            pc-1.2m    Calibrate for 360 RPM, 1us bit cell
            pc-1.44m   Calibrate for 300 RPM, 1us bit cell
            amiga      Calibrate for 300 RPM, 2us bit cell
            apple2     Calibrate for 300 RPM, 4us bit cell
            mac        Calibrate for variable speed, 2us bit cell
    -b    Dump detailed contents of bad sectors
    -d    Decoding mode
            auto       Try both FM and MFM
            fm         Atari FM only (288 RPM single density)
            mfm        Atari MFM only (288 RPM enhanced/double density)
            a2gcr      Apple II GCR only
            macgcr     Mac GCR only
            pcmfm      PC MFM only (300/360 RPM double density)
            amiga      Amiga MFM only
    -e    Encoding mode
            ordered    Encode sectors in order with default timing (default)
            precise    Encode sectors with precise timing (may cause overlaps)
    -E    Erase odd tracks when writing a 48 TPI image to a 96 TPI drive
    -g    Set geometry (-g tracks,sides)
            40,1       Single-sided 40 tracks
            80,2       Double-sided 80 tracks
            84,2       Double-sided 84 tracks
    -H    Use high density encoding
    -i    Set sector interleave
            auto       Use best interleave, if there is no position information
            force-auto Use best interleave, overriding existing positions
            none       Use 1:1 interleave
            xf551-hs   Use XF551 high speed interleave (9:1)
    -if   Set input format:
            auto       Determine by input name extension
            atr        Read as Atari ATR disk image
            atx        Read as Atari ATX disk image
            xfd        Read as Atari XFD disk image
            kryoflux   Read as KryoFlux raw stream (specify track00.0.raw)
            scp        Read as SuperCard Pro image
            scp-ss40   Read as SuperCard Pro image, forcing single-sided, 40-track layout
            scp-ds40   Read as SuperCard Pro image, forcing double-sided, 40-track layout
            scp-ss80   Read as SuperCard Pro image, forcing single-sided, 80-track layout
            scp-ds80   Read as SuperCard Pro image, forcing double-sided, 80-track layout
            do         Read Apple II DOS 3.3 format (.do/.dsk)
            vfd        Read as PC virtual floppy image (.vfd/.flp)
            adf        Read Amiga image format
    -I    Invert decoded Apple II GCR data
    -l    Show track/sector layout map
    -of   Set output format:
            auto       Determine by output name extension
            atr        Write Atari ATR disk image format
            atx        Write Atari ATX disk image format
            xfd        Write Atari XFD disk image format
            scp        Write SuperCard Pro image format
            scp-ss40   Write SuperCard Pro image format, forcing single-sided, 40-track layout
            scp-ds40   Write SuperCard Pro image format, forcing double-sided, 40-track layout
            scp-ss80   Write SuperCard Pro image format, forcing single-sided, 80-track layout
            scp-ds80   Write SuperCard Pro image format, forcing double-sided, 80-track layout
            do         Write Apple II DOS 3.3 order format
            po         Write Apple II ProDOS order format
            macdsk     Write Macintosh 400/800K DSK image format
            adf        Write Amiga image format
            vfd        Write as PC virtual floppy image (.vfd/.flp)
    -p    Adjust clock period by percentage (50-200)
            -p 98      Use 98% of normal period (2% fast)
            -p 102     Use 102% of normal period (2% slow)
    -P    Set post-compensation mode (raw disks only)
            none       No post-compensation; do not adjust flux
            auto       Auto-select post-comp mode based on formats
            mac800k    Apply post-comp for Macintosh 800K disk
    -r    Decode backwards (used for flipped tracks)
    -revs Revolutions to use when imaging from SuperCard Pro
            -revs 2    Image 2 revolutions per track
            -revs 5    Image 5 revolutions per track (default, max)
    -S    Use splice mode when reading/writing directly to SCP device
    -t    Restrict processing to single track
            -t 4       Process only track 4
    -tpi  Override track density for Kryoflux stream image sets
            -tpi 48    40 track (48 TPI) set
            -tpi 96    80 track (96 TPI) set (default)
            -tpi 135   80 track (135 TPI set (synonym for -tpi 96)
    -v    Verbose output
    -vv   Very verbose output: sector-level debugging
    -vvv  Extremely verbose output: bit level debugging
    -vvvv Ridiculously verbose output: flux level debugging
    
Direct SCP read/write is possible using the following path:
    scp[drive]:(48tpi|96tpi|135tpi)[:override_port_path]
    
Example: scp0:96tpi to use a 96 TPI drive 0.
)--");

	exit(1);
}

void exit_argerr() {
	puts("Use -h or -? for help.");
	exit(1);
}

void parse_args(int argc, char **argv) {
	bool allow_switches = true;

	banner();

	if (argc) {
		--argc;
		++argv;
	}

	if (!argc) {
		exit_usage();
	}

	bool autoDecoder = true;

	while(argc--) {
		const char *arg = *argv++;

		if (allow_switches && *arg == '-') {
			const char *sw = arg+1;

			if (!strcmp(sw, "-")) {
				allow_switches = false;
			} else if (!strcmp(sw, "h") || !strcmp(sw, "?")) {
				exit_usage();
			} else if (!strcmp(sw, "analyze")) {
				if (!argc--) {
					printf("Missing argument for -analyze switch.\n");
					exit_argerr();
				}

				arg = *argv++;
				if (!strcmp(arg, "atari-fm"))
					g_analyze = kAnalysisMode_Atari_FM;
				else if (!strcmp(arg, "atari-mfm"))
					g_analyze = kAnalysisMode_Atari_MFM;
				else if (!strcmp(arg, "pc-360k"))
					g_analyze = kAnalysisMode_PC_360K;
				else if (!strcmp(arg, "pc-1.2m"))
					g_analyze = kAnalysisMode_PC_1_2M;
				else if (!strcmp(arg, "pc-1.44m"))
					g_analyze = kAnalysisMode_PC_1_44M;
				else if (!strcmp(arg, "amiga"))
					g_analyze = kAnalysisMode_Amiga;
				else if (!strcmp(arg, "apple2"))
					g_analyze = kAnalysisMode_AppleII;
				else if (!strcmp(arg, "mac"))
					g_analyze = kAnalysisMode_Mac;
				else {
					printf("Unsupported analysis mode: %s.\n", arg);
					exit_argerr();
				}
			} else if (!strcmp(sw, "b")) {
				g_dumpBadSectors = true;
			} else if (!strcmp(sw, "l")) {
				g_showLayout = true;
			} else if (!strcmp(sw, "d")) {
				if (!argc--) {
					printf("Missing argument for -d switch.\n");
					exit_argerr();
				}

				autoDecoder = false;

				g_encoding_fm = false;
				g_encoding_mfm = false;
				g_encoding_macgcr = false;
				g_encoding_a2gcr = false;
				g_encoding_pcmfm = false;
				g_encoding_amigamfm = false;

				arg = *argv++;
				if (!strcmp(arg, "auto")) {
					autoDecoder = true;
				} else if (!strcmp(arg, "fm")) {
					g_encoding_fm = true;
				} else if (!strcmp(arg, "mfm")) {
					g_encoding_mfm = true;
				} else if (!strcmp(arg, "pcmfm")) {
					g_encoding_pcmfm = true;
				} else if (!strcmp(arg, "a2gcr")) {
					g_encoding_a2gcr = true;
					g_trackCount = 35;
				} else if (!strcmp(arg, "macgcr")) {
					g_encoding_macgcr = true;
				} else {
					printf("Unsupported decoding mode: %s.\n", arg);
					exit_argerr();
				}
			} else if (!strcmp(sw, "e")) {
				if (!argc--) {
					printf("Missing argument for -e switch.\n");
					exit_argerr();
				}

				arg = *argv++;
				if (!strcmp(arg, "ordered")) {
					g_encode_precise = false;
				} else if (!strcmp(arg, "precise")) {
					g_encode_precise = true;
				} else {
					printf("Unsupported encoding mode: %s.\n", arg);
					exit_argerr();
				}
			} else if (!strcmp(sw, "g")) {
				if (!argc--) {
					printf("Missing argument for -g switch.\n");
					exit_argerr();
				}

				arg = *argv++;

				int tracks, sides;
				char dummy;
				if (2 != sscanf(arg, "%d,%d%c", &tracks, &sides, &dummy) || tracks < 1 || tracks > 84 || sides < 1 || sides > 2) {
					printf("Invalid geometry: %s. Must be of form: tracks,sides.\n", arg);
					exit_argerr();
				}
				
				g_trackCount = tracks;
				g_trackStep = tracks > 41 ? 1 : 2;
				g_sides = sides;
				g_layout_set = true;
			} else if (!strcmp(sw, "H")) {
				g_high_density = true;
			} else if (!strcmp(sw, "i")) {
				if (!argc--) {
					printf("Missing argument for -i switch.\n");
					exit_argerr();
				}

				arg = *argv++;
				if (!strcmp(arg, "auto"))
					g_interleave = kInterleaveMode_Auto;
				else if (!strcmp(arg, "force-auto"))
					g_interleave = kInterleaveMode_ForceAuto;
				else if (!strcmp(arg, "none"))
					g_interleave = kInterleaveMode_None;
				else if (!strcmp(arg, "xf551-hs"))
					g_interleave = kInterleaveMode_XF551_DD_HS;
				else {
					printf("Unsupported interleave mode: %s.\n", arg);
					exit_argerr();
				}
			} else if (!strcmp(sw, "if")) {
				if (!argc--) {
					printf("Missing argument for -if switch.\n");
					exit_argerr();
				}

				arg = *argv++;
				if (!strcmp(arg, "auto"))
					g_inputFormat = kInputFormat_Auto;
				else if (!strcmp(arg, "kryoflux"))
					g_inputFormat = kInputFormat_KryoFluxStream;
				else if (!strcmp(arg, "scp"))
					g_inputFormat = kInputFormat_SCP_Auto;
				else if (!strcmp(arg, "scp-ss40"))
					g_inputFormat = kInputFormat_SCP_ForceSS40;
				else if (!strcmp(arg, "scp-ds40"))
					g_inputFormat = kInputFormat_SCP_ForceDS40;
				else if (!strcmp(arg, "scp-ss80"))
					g_inputFormat = kInputFormat_SCP_ForceSS80;
				else if (!strcmp(arg, "scp-ds80"))
					g_inputFormat = kInputFormat_SCP_ForceDS80;
				else if (!strcmp(arg, "atr"))
					g_inputFormat = kInputFormat_Atari_ATR;
				else if (!strcmp(arg, "atx"))
					g_inputFormat = kInputFormat_Atari_ATX;
				else if (!strcmp(arg, "xfd"))
					g_inputFormat = kInputFormat_Atari_XFD;
				else if (!strcmp(arg, "do"))
					g_inputFormat = kInputFormat_AppleII_DO;
				else if (!strcmp(arg, "po"))
					g_inputFormat = kInputFormat_AppleII_PO;
				else if (!strcmp(arg, "nib"))
					g_inputFormat = kInputFormat_AppleII_NIB;
				else if (!strcmp(arg, "vfd"))
					g_inputFormat = kInputFormat_PC_VFD;
				else if (!strcmp(arg, "diskscript"))
					g_inputFormat = kInputFormat_DiskScript;
				else {
					printf("Unsupported input format type: %s.\n", arg);
					exit_argerr();
				}
			} else if (!strcmp(sw, "of")) {
				if (!argc--) {
					printf("Missing argument for -of switch.\n");
					exit_argerr();
				}

				arg = *argv++;
				if (!strcmp(arg, "auto"))
					g_outputFormat = kOutputFormat_Auto;
				else if (!strcmp(arg, "atx"))
					g_outputFormat = kOutputFormat_Atari_ATX;
				else if (!strcmp(arg, "atr"))
					g_outputFormat = kOutputFormat_Atari_ATR;
				else if (!strcmp(arg, "xfd"))
					g_outputFormat = kOutputFormat_Atari_XFD;
				else if (!strcmp(arg, "scp"))
					g_outputFormat = kOutputFormat_SCP_Auto;
				else if (!strcmp(arg, "scp-ss40"))
					g_outputFormat = kOutputFormat_SCP_ForceSS40;
				else if (!strcmp(arg, "scp-ds40"))
					g_outputFormat = kOutputFormat_SCP_ForceDS40;
				else if (!strcmp(arg, "scp-ss80"))
					g_outputFormat = kOutputFormat_SCP_ForceSS80;
				else if (!strcmp(arg, "scp-ds80"))
					g_outputFormat = kOutputFormat_SCP_ForceDS80;
				else if (!strcmp(arg, "do"))
					g_outputFormat = kOutputFormat_AppleII_DO;
				else if (!strcmp(arg, "po"))
					g_outputFormat = kOutputFormat_AppleII_PO;
				else if (!strcmp(arg, "macdsk"))
					g_outputFormat = kOutputFormat_Mac_DSK;
				else if (!strcmp(arg, "vfd"))
					g_outputFormat = kOutputFormat_PC_VFD;
				else if (!strcmp(arg, "nib"))
					g_outputFormat = kOutputFormat_AppleII_NIB;
				else {
					printf("Unsupported output format type: %s.\n", arg);
					exit_argerr();
				}
			} else if (!strcmp(sw, "p")) {
				if (!argc--) {
					printf("Missing argument for -p switch.\n");
					exit_argerr();
				}

				arg = *argv++;

				char dummy;
				float period;
				if (1 != sscanf(arg, "%g%c", &period, &dummy)
					|| !(period >= 50.0f && period <= 200.0f))
				{
					printf("Invalid period adjustment: %s\n", arg);
					exit_argerr();
				}

				g_clockPeriodAdjust = period / 100.0f;
			} else if (!strcmp(sw, "P")) {
				if (!argc--) {
					printf("Missing argument for -P switch.\n");
					exit_argerr();
				}

				arg = *argv++;

				if (!strcmp(arg, "none"))
					g_postcomp = kPostComp_None;
				else if (!strcmp(arg, "auto"))
					g_postcomp = kPostComp_Auto;
				else if (!strcmp(arg, "mac800k"))
					g_postcomp = kPostComp_Mac800K;
				else {
					printf("Unsupported post-compensation type: %s.\n", arg);
					exit_argerr();
				}
			} else if (!strcmp(sw, "v")) {
				g_verbosity = 1;
			} else if (!strcmp(sw, "vv")) {
				g_verbosity = 2;
			} else if (!strcmp(sw, "vvv")) {
				g_verbosity = 3;
			} else if (!strcmp(sw, "vvvv")) {
				g_verbosity = 4;
			} else if (!strcmp(sw, "r")) {
				g_reverseTracks = true;
			} else if (!strcmp(sw, "revs")) {
				if (!argc--) {
					printf("Missing argument for -revs switch.\n");
					exit_argerr();
				}

				arg = *argv++;

				char dummy;
				unsigned revs;
				if (1 != sscanf(arg, "%u%c", &revs, &dummy) || revs < 1 || revs > 5)
				{
					printf("Invalid revolution count: %s\n", arg);
					exit_argerr();
				}

				g_revs = revs;
			} else if (!strcmp(sw, "t")) {
				if (!argc--) {
					printf("Missing argument for -t switch.\n");
					exit_argerr();
				}

				arg = *argv++;

				char dummy;
				unsigned track;
				if (1 != sscanf(arg, "%u%c", &track, &dummy) || track > 79)
				{
					printf("Invalid track number: %s\n", arg);
					exit_argerr();
				}

				g_trackSelect = track;
			} else if (!strcmp(sw, "tpi")) {
				if (!argc--) {
					puts("Missing argument for -tpi switch.\n");
					exit_argerr();
				}

				arg = *argv++;
				char dummy;
				unsigned tpi;
				if (1 != sscanf(arg, "%u%c", &tpi, &dummy) || (tpi != 48 && tpi != 96 && tpi != 135))
				{
					printf("Invalid tracks per inch: %s. Must be 48, 96, or 135.\n", arg);
					exit_argerr();
				}

				g_kryoflux_48tpi = (tpi == 48);
			} else if (!strcmp(sw, "I")) {
				g_invertBit7 = true;
			} else if (!strcmp(sw, "E")) {
				g_erase_odd_tracks = true;
			} else if (!strcmp(sw, "S")) {
				g_splice_mode = true;
			} else {
				printf("Unknown switch: %s\n", arg);
				exit_argerr();
			}
		} else {
			if (g_inputPath.empty()) {
				if (!*arg) {
					printf("Invalid input path.\n");
					exit_argerr();
				}

				g_inputPath = arg;
			} else if (g_outputPath.empty()) {
				if (!*arg) {
					printf("Invalid output path.\n");
					exit_argerr();
				}

				g_outputPath = arg;
			} else {
				printf("Extraneous argument: %s\n", arg);
				exit_argerr();
			}
		}
	}

	if (g_inputPath.empty()) {
		printf("Missing input path.\n");
		exit_argerr();
	}

	if (g_outputPath.empty() && !g_analyze) {
		printf("Missing output path.\n");
		exit_argerr();
	}

	if (g_inputFormat == kInputFormat_Auto) {
		if (g_inputPath.compare(0, 5, "scp0:") == 0
			|| g_inputPath.compare(0, 5, "scp1:") == 0)
		{
			g_inputFormat = kInputFormat_SuperCardProDirect;
		} else {
			const char *extptr = strrchr(g_inputPath.c_str(), '.');

			if (extptr) {
				std::string ext(extptr+1);

				std::transform(ext.begin(), ext.end(), ext.begin(), [](char c) { return tolower((unsigned char)c); });

				if (ext == "raw")
					g_inputFormat = kInputFormat_KryoFluxStream;
				else if (ext == "scp")
					g_inputFormat = kInputFormat_SCP_Auto;
				else if (ext == "atr")
					g_inputFormat = kInputFormat_Atari_ATR;
				else if (ext == "atx")
					g_inputFormat = kInputFormat_Atari_ATX;
				else if (ext == "xfd")
					g_inputFormat = kInputFormat_Atari_XFD;
				else if (ext == "do")
					g_inputFormat = kInputFormat_AppleII_DO;
				else if (ext == "po")
					g_inputFormat = kInputFormat_AppleII_PO;
				else if (ext == "nib")
					g_inputFormat = kInputFormat_AppleII_NIB;
				else if (ext == "vfd")
					g_inputFormat = kInputFormat_PC_VFD;
				else if (ext == "flp")
					g_inputFormat = kInputFormat_PC_VFD;
				else if (ext == "adf")
					g_inputFormat = kInputFormat_Amiga_ADF;
				else if (ext == "diskscript")
					g_inputFormat = kInputFormat_DiskScript;
			}
		}

		if (g_inputFormat == kInputFormat_Auto) {
			printf("Unable to determine input format from input path: %s. Use -if to override input format.\n", g_inputPath.c_str());
			exit_usage();
		}
	}

	if (g_inputFormat == kInputFormat_KryoFluxStream) {
		// attempt to identify track counter in KryoFlux stream filename
		const char *fn = g_inputPath.c_str();
		const char *s = strrchr(fn, '.');

		if (s) {
			g_inputPathSideWidth = 0;
			while(s != fn && s[-1] >= '0' && s[-1] <= '9') {
				--s;
				++g_inputPathSideWidth;
			}
			g_inputPathSidePos = (int)(s - fn);
			
			if (s != fn && s[-1] == '.') {
				--s;

				g_inputPathCountWidth = 0;
				while(s != fn && s[-1] == '0') {
					++g_inputPathCountWidth;
					--s;
				}

				if (s != fn && (s[-1] < '0' || s[-1] > '9'))
					g_inputPathCountPos = (int)(s - fn);
			}
		}

		if (!g_inputPathCountPos || !g_inputPathCountWidth || g_inputPathCountWidth > 10 || !g_inputPathSideWidth || g_inputPathSideWidth > 4) {
			printf("Unable to determine filename pattern for KryoFlux raw track streams. Expected pattern: track00.0.raw.\n");
			exit_usage();
		}

		g_inputPathSideBase = atoi(std::string(&fn[g_inputPathSidePos], g_inputPathSideWidth).c_str());
	}

	if (g_outputFormat == kOutputFormat_Auto && !g_analyze) {
		if (g_outputPath.compare(0, 5, "scp0:") == 0
			|| g_outputPath.compare(0, 5, "scp1:") == 0)
		{
			g_outputFormat = kOutputFormat_SCPDirect;
		} else {
			const char *extptr = strrchr(g_outputPath.c_str(), '.');

			if (extptr) {
				std::string ext(extptr+1);

				std::transform(ext.begin(), ext.end(), ext.begin(), [](char c) { return tolower((unsigned char)c); });

				if (ext == "atr") {
					g_outputFormat = kOutputFormat_Atari_ATR;
				} else if (ext == "atx") {
					g_outputFormat = kOutputFormat_Atari_ATX;
				} else if (ext == "xfd") {
					g_outputFormat = kOutputFormat_Atari_XFD;
				} else if (ext == "scp") {
					g_outputFormat = kOutputFormat_SCP_Auto;
				} else if (ext == "dsk") {
					g_outputFormat = !autoDecoder && g_encoding_macgcr ? kOutputFormat_Mac_DSK : kOutputFormat_AppleII_DO;
				} else if (ext == "do") {
					g_outputFormat = kOutputFormat_AppleII_DO;
				} else if (ext == "po") {
					g_outputFormat = kOutputFormat_AppleII_PO;
				} else if (ext == "nib") {
					g_outputFormat = kOutputFormat_AppleII_NIB;
				} else if (ext == "vfd") {
					g_outputFormat = kOutputFormat_PC_VFD;
				} else if (ext == "flp") {
					g_outputFormat = kOutputFormat_PC_VFD;
				} else if (ext == "adf") {
					g_outputFormat = kOutputFormat_Amiga_ADF;
				}
			}
		}

		if (g_outputFormat == kOutputFormat_Auto) {
			printf("Unable to determine output format from output path: %s. Use -of to override output format.\n", g_outputPath.c_str());
			exit_usage();
		}
	}

	if (autoDecoder) {
		g_encoding_fm = false;
		g_encoding_mfm = false;
		g_encoding_pcmfm = false;
		g_encoding_macgcr = false;
		g_encoding_a2gcr = false;

		switch(g_outputFormat) {
			case kOutputFormat_AppleII_DO:
			case kOutputFormat_AppleII_PO:
			case kOutputFormat_AppleII_NIB:
				g_encoding_a2gcr = true;
				break;

			case kOutputFormat_Mac_DSK:
				g_encoding_macgcr = true;
				break;

			case kOutputFormat_PC_VFD:
				g_encoding_pcmfm = true;
				break;

			case kOutputFormat_Amiga_ADF:
				g_encoding_amigamfm = true;
				break;

			default:
				g_encoding_fm = true;
				g_encoding_mfm = true;
				break;
		}
	}

	if (!g_layout_set) {
		switch(g_outputFormat) {
			case kOutputFormat_AppleII_DO:
			case kOutputFormat_AppleII_PO:
			case kOutputFormat_AppleII_NIB:
				g_trackCount = 35;
				break;

			case kOutputFormat_Mac_DSK:
				g_trackCount = 80;
				g_trackStep = 1;
				g_sides = 2;
				break;

			case kOutputFormat_PC_VFD:
				g_trackCount = 80;
				g_trackStep = 1;
				g_sides = 2;
				break;

			case kOutputFormat_Amiga_ADF:
				g_trackCount = 80;
				g_trackStep = 1;
				g_sides = 2;
				break;

			default:
				break;
		}
	}
}

void show_layout() {
	char trackbuf[73];

	DiskInfo tempDisk(g_disk);

	// write tracks
	for(int i=0; i<tempDisk.mTrackCount; ++i) {
		if (g_trackSelect >= 0 && g_trackSelect != i)
			continue;

		for(int side=0; side<tempDisk.mSideCount; ++side) {
			TrackInfo& track_info = tempDisk.mPhysTracks[side][i * tempDisk.mTrackStep];
			const int num_raw_secs = (int)track_info.mSectors.size();

			// sort sectors by angular position
			std::vector<SectorInfo *> secptrs(num_raw_secs);
			for(int j=0; j<num_raw_secs; ++j)
				secptrs[j] = &track_info.mSectors[j];

			sift_sectors(track_info, i, secptrs);

			memset(trackbuf, ' ', 68);
			memset(trackbuf+68, 0, 5);

			for(const SectorInfo *sec_ptr : secptrs) {
				int xpos = (unsigned)(sec_ptr->mPosition * 68);
				if (sec_ptr->mIndex >= 10)
					trackbuf[xpos++] = '0' + sec_ptr->mIndex / 10;

				trackbuf[xpos++] = '0' + sec_ptr->mIndex % 10;
			};

			if (tempDisk.mSideCount > 1)
				printf("%2d.%d (%2d) | %s\n", i, side, (int)secptrs.size(), trackbuf);
			else
				printf("%2d (%2d) | %s\n", i, (int)secptrs.size(), trackbuf);
		}
	}
}

int main(int argc, char **argv) {
	parse_args(argc, argv);

	bool src_raw = false;
	bool src_gcr = false;
	bool dst_raw = false;

	switch(g_outputFormat) {
		case kOutputFormat_SCP_Auto:
		case kOutputFormat_SCP_ForceSS40:
		case kOutputFormat_SCP_ForceDS40:
		case kOutputFormat_SCP_ForceSS80:
		case kOutputFormat_SCP_ForceDS80:
		case kOutputFormat_SCPDirect:
			dst_raw = true;
			break;
	}

	RawDisk raw_disk;

	switch(g_inputFormat) {
		case kInputFormat_KryoFluxStream:
			raw_disk.mSideCount = g_sides;
			kf_read(raw_disk, g_trackCount, g_trackStep, g_inputPath.c_str(), g_inputPathSidePos, g_inputPathSideWidth, g_inputPathSideBase, g_inputPathCountPos, g_inputPathCountWidth, g_trackSelect, g_kryoflux_48tpi);
			src_raw = true;
			break;

		case kInputFormat_SCP_Auto:
			scp_read(raw_disk, g_inputPath.c_str(), g_trackSelect, 0, 0, g_layout_set ? g_trackCount : 0, g_layout_set ? g_sides : 0);
			src_raw = true;
			break;

		case kInputFormat_SCP_ForceSS40:
			scp_read(raw_disk, g_inputPath.c_str(), g_trackSelect, 48, 1, g_layout_set ? g_trackCount : 0, g_layout_set ? g_sides : 0);
			src_raw = true;
			break;

		case kInputFormat_SCP_ForceDS40:
			scp_read(raw_disk, g_inputPath.c_str(), g_trackSelect, 48, 2, g_layout_set ? g_trackCount : 0, g_layout_set ? g_sides : 0);
			src_raw = true;
			break;

		case kInputFormat_SCP_ForceSS80:
			scp_read(raw_disk, g_inputPath.c_str(), g_trackSelect, 96, 1, g_layout_set ? g_trackCount : 0, g_layout_set ? g_sides : 0);
			src_raw = true;
			break;

		case kInputFormat_SCP_ForceDS80:
			scp_read(raw_disk, g_inputPath.c_str(), g_trackSelect, 96, 2, g_layout_set ? g_trackCount : 0, g_layout_set ? g_sides : 0);
			src_raw = true;
			break;

		case kInputFormat_SuperCardProDirect:
			raw_disk.mTrackCount = g_trackCount;
			raw_disk.mTrackStep = g_trackStep;
			raw_disk.mSideCount = g_sides;
			scp_direct_read(raw_disk, g_inputPath.c_str(), g_trackSelect, g_revs, g_high_density, g_splice_mode);
			src_raw = true;
			break;

		case kInputFormat_Atari_ATR:
			read_atr(g_disk, g_inputPath.c_str(), g_trackSelect);
			break;

		case kInputFormat_Atari_ATX:
			read_atx(g_disk, g_inputPath.c_str(), g_trackSelect);
			break;

		case kInputFormat_Atari_XFD:
			read_xfd(g_disk, g_inputPath.c_str(), g_trackSelect);
			break;

		case kInputFormat_DiskScript:
			script_read(raw_disk, g_inputPath.c_str(), g_trackSelect);
			src_raw = true;
			break;

		case kInputFormat_AppleII_DO:
			read_apple2_dsk(g_disk, g_inputPath.c_str(), g_trackSelect, false);
			src_gcr = true;
			break;

		case kInputFormat_AppleII_PO:
			read_apple2_dsk(g_disk, g_inputPath.c_str(), g_trackSelect, true);
			src_gcr = true;
			break;

		case kInputFormat_AppleII_NIB:
			read_apple2_nib(raw_disk, g_inputPath.c_str(), g_trackSelect);
			src_gcr = true;
			src_raw = true;
			break;

		case kInputFormat_PC_VFD:
			read_vfd(g_disk, g_inputPath.c_str(), g_trackSelect);
			break;

		case kInputFormat_Amiga_ADF:
			read_adf(g_disk, g_inputPath.c_str(), g_trackSelect);
			break;
	}

	// apply post-compensation if we have a raw disk
	if (src_raw) {
		if (g_postcomp == kPostComp_Auto) {
			if (g_encoding_macgcr || g_analyze == kAnalysisMode_Mac)
				g_postcomp = kPostComp_Mac800K;
			else
				g_postcomp = kPostComp_None;
		}

		postcomp_disk(raw_disk, g_postcomp);
	}

	// sync the global params with the actual disk geometry we got, and run analysis if enabled
	if (src_raw) {
		A8RC_RT_ASSERT(raw_disk.mTrackCount * raw_disk.mTrackStep <= raw_disk.kMaxPhysTracks);

		if (g_analyze)
			return analyze_raw(raw_disk, g_trackSelect, g_analyze);

		// if the layout hasn't been forced and the destination isn't also raw, then use the
		// min of what we expect and what we have
		if (!dst_raw) {
			// raw -> decoded -- use min of geometry and raw disk
			g_trackCount = std::min<int>(g_trackCount, raw_disk.mTrackCount);
			raw_disk.mTrackCount = g_trackCount;

			g_trackStep = raw_disk.mTrackStep;

			g_sides = std::min<int>(g_sides, raw_disk.mSideCount);
			raw_disk.mSideCount = g_sides;
		} else {
			// raw -> raw -- use source geometry
			g_trackCount = g_disk.mTrackCount;
			g_trackStep = g_disk.mTrackStep;
			g_sides = g_disk.mSideCount;
		}
	} else {
		g_trackCount = g_disk.mTrackCount;
		g_trackStep = g_disk.mTrackStep;
		g_sides = g_disk.mSideCount;

		// If we loaded a decoded disk, apply interleave to the sectors now if any
		// are missing positions or if we are forcing a particular interleave.
		update_disk_interleave(g_disk, g_interleave);
	}

	// Figure out if we need a splice point for the destination format
	bool dst_spliced = false;
	switch(g_outputFormat) {
		case kOutputFormat_Atari_ATX:
		case kOutputFormat_Atari_ATR:
		case kOutputFormat_Atari_XFD:
		case kOutputFormat_SCP_Auto:
		case kOutputFormat_SCP_ForceSS40:
		case kOutputFormat_SCP_ForceDS40:
		case kOutputFormat_SCP_ForceSS80:
		case kOutputFormat_SCP_ForceDS80:
		case kOutputFormat_AppleII_DO:
		case kOutputFormat_AppleII_PO:
		case kOutputFormat_AppleII_NIB:
		case kOutputFormat_PC_VFD:
		case kOutputFormat_Amiga_ADF:
			dst_spliced = false;
			break;

		case kOutputFormat_SCPDirect:
			dst_spliced = true;
			break;
	}

	if (src_raw && g_reverseTracks)
		reverse_tracks(raw_disk);

	// Check if we are going from raw or decoded source.
	if (src_raw) {
		// Raw -- if the destination is decoded then we need
		// to decode tracks. If the destination is raw but requires splice points,
		// then we may need to decode the tracks if we didn't already have splice
		// points.
		if (!dst_raw || dst_spliced) {
			g_disk.mTrackCount = raw_disk.mTrackCount;
			g_disk.mTrackStep = raw_disk.mTrackStep;
			g_disk.mSideCount = raw_disk.mSideCount;

			for(int i=0; i<raw_disk.mTrackCount; ++i) {
				if (g_trackSelect >= 0 && g_trackSelect != i)
					continue;

				for(int side=0; side < raw_disk.mSideCount; ++side) {
					RawTrack& raw_track = raw_disk.mPhysTracks[side][i * raw_disk.mTrackStep];

					// if we just need splice points and we already have them, skip this
					// track
					if (dst_raw && raw_track.mSpliceStart >= 0)
						continue;

					process_track(raw_track);
				}
			}

			if (dst_spliced)
				find_splice_points(raw_disk, g_disk);
		}
	} else if (!src_raw) {
		// Decoded -- if the destination is raw, then we need to encode tracks.
		if (dst_raw)
			encode_disk(raw_disk, g_disk, g_clockPeriodAdjust, g_trackSelect, src_gcr, g_encode_precise);
	}

	if (g_showLayout && (!src_raw || !dst_raw))
		show_layout();

	switch(g_outputFormat) {
		case kOutputFormat_Atari_ATX:
			write_atx(g_outputPath.c_str(), g_disk, g_trackSelect);
			break;

		case kOutputFormat_Atari_ATR:
			write_atr(g_outputPath.c_str(), g_disk, g_trackSelect);
			break;

		case kOutputFormat_Atari_XFD:
			write_xfd(g_outputPath.c_str(), g_disk, g_trackSelect);
			break;

		case kOutputFormat_SCP_Auto:
			scp_write(raw_disk, g_outputPath.c_str(), g_trackSelect, 0, 0);
			break;

		case kOutputFormat_SCP_ForceSS40:
			scp_write(raw_disk, g_outputPath.c_str(), g_trackSelect, 48, 1);
			break;

		case kOutputFormat_SCP_ForceDS40:
			scp_write(raw_disk, g_outputPath.c_str(), g_trackSelect, 48, 2);
			break;

		case kOutputFormat_SCP_ForceSS80:
			scp_write(raw_disk, g_outputPath.c_str(), g_trackSelect, 96, 1);
			break;

		case kOutputFormat_SCP_ForceDS80:
			scp_write(raw_disk, g_outputPath.c_str(), g_trackSelect, 96, 2);
			break;

		case kOutputFormat_SCPDirect:
			if (g_erase_odd_tracks && raw_disk.mTrackStep == 2) {
				for(int side=0; side<raw_disk.mSideCount; ++side) {
					for(int i=0; i<raw_disk.mTrackCount; ++i) {
						RawTrack& odd_track = raw_disk.mPhysTracks[side][i*2+1];

						odd_track.mIndexTimes.clear();
						odd_track.mTransitions.clear();
					}
				}
				raw_disk.mTrackStep = 1;
				raw_disk.mTrackCount *= 2;
			}
			scp_direct_write(raw_disk, g_outputPath.c_str(), g_trackSelect, g_high_density, g_splice_mode);
			break;

		case kOutputFormat_AppleII_DO:
			write_apple2_dsk(g_outputPath.c_str(), g_disk, g_trackSelect, false, false);
			break;

		case kOutputFormat_AppleII_PO:
			write_apple2_dsk(g_outputPath.c_str(), g_disk, g_trackSelect, true, false);
			break;

		case kOutputFormat_Mac_DSK:
			write_apple2_dsk(g_outputPath.c_str(), g_disk, g_trackSelect, false, true);
			break;

		case kOutputFormat_AppleII_NIB:
			write_apple2_nib(g_outputPath.c_str(), g_disk, g_trackSelect);
			break;

		case kOutputFormat_PC_VFD:
			write_vfd(g_outputPath.c_str(), g_disk, g_trackSelect);
			break;

		case kOutputFormat_Amiga_ADF:
			write_adf(g_outputPath.c_str(), g_disk, g_trackSelect);
			break;
	}

	return 0;
}
