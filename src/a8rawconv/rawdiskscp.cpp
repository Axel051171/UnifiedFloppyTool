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
#include "version.h"
#include "os.h"

struct SCPFileHeader {
	uint8_t		mSignature[3];
	uint8_t		mVersion;
	uint8_t		mDiskType;
	uint8_t		mNumRevs;
	uint8_t		mStartTrack;
	uint8_t		mEndTrack;
	uint8_t		mFlags;
	uint8_t		mBitCellEncoding;
	uint8_t		mSides;
	uint8_t		mReserved;
	uint32_t	mChecksum;
	uint32_t	mTrackOffsets[168];
};

static_assert(sizeof(SCPFileHeader) == 0x2B0, "");

struct SCPTrackRevolution {
	uint32_t	mTimeDuration;
	uint32_t	mDataLength;
	uint32_t	mDataOffset;
};

struct SCPTrackHeader {
	uint8_t		mSignature[3];
	uint8_t		mTrackNumber;
};

struct SCPExtensionFooter {
	static constexpr uint8_t kSignature[] = {
		(uint8_t)'F',
		(uint8_t)'P',
		(uint8_t)'C',
		(uint8_t)'S',
	};

	uint32_t	mDriveManufacturerOffset;
	uint32_t	mDriveModelOffset;
	uint32_t	mDriveSerialNumberOffset;
	uint32_t	mUserNameOffset;
	uint32_t	mApplicationNameOffset;
	uint32_t	mUserCommentsOffset;
	uint64_t	mCreationTime;
	uint64_t	mModificationTime;
	uint8_t		mApplicationVersion;
	uint8_t		mSuperCardHardwareVersion;
	uint8_t		mSuperCardSoftwareVersion;
	uint8_t		mFooterRevision = 0x16;			// v1.6 footer version (per spec)
	uint8_t		mSignature[4] {
		kSignature[0],
		kSignature[1],
		kSignature[2],
		kSignature[3]
	};
};

void scp_read(RawDisk& raw_disk, const char *path, int selected_track, int forced_tpi, int forced_side_step, int forced_tracks, int forced_sides) {
	printf("Reading SuperCard Pro image: %s\n", path);

	FILE *fi = fopen(g_inputPath.c_str(), "rb");
	if (!fi)
		fatalf("Unable to open input file: %s.\n", path);

	SCPFileHeader fileHeader = {0};
	if (1 != fread(&fileHeader, sizeof(fileHeader), 1, fi))
		fatal_read();

	if (memcmp(fileHeader.mSignature, "SCP", 3))
		fatalf("Input file does not start with correct SCP image signature.\n");

	// check if extended mode is enabled, and if so, re-read the TDH from the extended
	// location (absolute position 0x80)
	if (fileHeader.mFlags & 0x40) {
		memset(fileHeader.mTrackOffsets, 0, sizeof fileHeader.mTrackOffsets);
		if (fseek(fi, 0x80, SEEK_SET) || 1 != fread(fileHeader.mTrackOffsets, sizeof fileHeader.mTrackOffsets, 1, fi))
			fatal("Unable to read extended header from input file.");
	}

	// The 96 tpi flag in the disk image is not useful, it indicates the TPI of
	// the imaging drive rather than the track organization in the file. Unfortunately
	// this makes it hard to determine the physical tracks associated with each
	// track entry. One heuristic we can use: if the disk image stores substantially
	// more than 40 tracks/side, we can assume it is a 96 tpi drive as no 48 tpi
	// drive is going to be reading/writing track 70. Note that the start/end
	// track range is in terms of tracks per disk and not tracks per side.

	const bool tpi96 = forced_tpi >= 96 || (!forced_tpi && fileHeader.mEndTrack >= 59*2);
	const bool double_sided = (fileHeader.mSides != 1);

	// SCP images always have track entries reserved for two sides even if the
	// image is single-sided.
	const bool image_double_sided = forced_side_step != 1;
	const int rawdisk_track_step = tpi96 ? 1 : 2;
	const int image_track_step = ((tpi96 ? 2 : 1) * rawdisk_track_step)/2 * (image_double_sided ? 2 : 1);

	if (g_verbosity >= 1)
		printf("SCP: Image is %d tpi, %s-sided\n", tpi96 ? 96 : 48, double_sided ? "double" : "single");

	if (fileHeader.mNumRevs <= 1) {
		printf(
			"Warning: Only one disk revolution found in image. Atari disks are not\n"
			"         index aligned and require at least two revolutions.\n");
	}

	const int tracks_to_read = forced_tracks ? forced_tracks : std::min<int>(RawDisk::kMaxPhysTracks, (fileHeader.mEndTrack + 1) / image_track_step);
	const int sides_to_read = forced_sides ? forced_sides : image_double_sided ? 2 : 1;

	raw_disk.mTrackCount = tracks_to_read;
	raw_disk.mSideCount = sides_to_read;
	raw_disk.mTrackStep = rawdisk_track_step;

	// set synthesized flag if the original image was marked as normalized
	raw_disk.mSynthesized = (fileHeader.mFlags & 0x08) != 0;

	std::vector<uint16_t> data_buf;
	for(int i=0; i<tracks_to_read; ++i) {
		if (selected_track >= 0 && i != selected_track)
			continue;

		for(int side=0; side<sides_to_read; ++side) {
			const int image_track = i * image_track_step + side;

			const uint32_t track_offset = fileHeader.mTrackOffsets[image_track];
			if (!track_offset)
				continue;

			if (fseek(fi, track_offset, SEEK_SET))
				fatalf("Unable to read track %d from input file.", i);

			SCPTrackHeader track_hdr = {};
			if (1 != fread(&track_hdr, sizeof(track_hdr), 1, fi))
				fatalf("Unable to read track %d from input file.", i);

			if (memcmp(track_hdr.mSignature, "TRK", 3))
				fatalf("SCP raw track %d has broken header at %08x with incorrect signature.", image_track, track_offset);

			std::vector<SCPTrackRevolution> revs(fileHeader.mNumRevs);
			if (1 != fread(revs.data(), sizeof(SCPTrackRevolution)*fileHeader.mNumRevs, 1, fi))
				fatalf("Unable to read track %d from input file.", i);

			// initialize raw track parameters
			RawTrack& raw_track = raw_disk.mPhysTracks[side][i * rawdisk_track_step];
			raw_track.mIndexTimes.push_back(0);

			// compute average revolution time
			uint32_t total_rev_time = 0;
			for(auto it = revs.begin(), itEnd = revs.end(); it != itEnd; ++it) {
				const auto& rev = *it;

				total_rev_time += rev.mTimeDuration;
				raw_track.mIndexTimes.push_back(total_rev_time);
			};

			raw_track.mSamplesPerRev = (float)total_rev_time / (float)fileHeader.mNumRevs;
			raw_track.mSpliceStart = -1;
			raw_track.mSpliceEnd = -1;

			if (g_verbosity >= 1)
				printf("Track %d: %.2f RPM\n", i, 60.0 / (raw_track.mSamplesPerRev * 0.000000025));

			// parse out flux transitions for each rev
			uint32_t time = 0;

			for(auto it = revs.begin(), itEnd = revs.end(); it != itEnd; ++it) {
				const auto& rev = *it;

				if (rev.mDataLength > 0x1000000)
					fatalf("SCP raw track %u at %08X has an excessively long sample list.\n", image_track, track_offset);

				data_buf.resize(rev.mDataLength);

				if (rev.mDataLength) {
					if (fseek(fi, track_offset + rev.mDataOffset, SEEK_SET) < 0
						|| 1 != fread(data_buf.data(), rev.mDataLength * 2, 1, fi))
						fatalf("Unable to read track %d from input file.", i);

					raw_track.mTransitions.reserve(data_buf.size());

					for(const auto offset_be : data_buf) {
						// need to byte-swap
						uint32_t offset = ((offset_be << 8) & 0xff00) + ((offset_be >> 8) & 0xff);

						if (offset) {
							time += offset;
							raw_track.mTransitions.push_back(time);
						} else
							time += 0x10000;
					}
				}
			}
		}
	}

	fclose(fi);
};

void scp_write(const RawDisk& raw_disk, const char *path, int selected_track, int forced_tpi, int forced_side_step) {
	int maxrevs = 5;

	// go through all tracks we'll be writing, and find out the max number of revs we have
	for(int i=0; i<40; ++i) {
		if (selected_track >= 0 && selected_track != i)
			continue;

		int revs = (int)raw_disk.mPhysTracks[0][i * 2].mIndexTimes.size() - 1;

		if (revs > 0 && revs < maxrevs)
			maxrevs = revs;
	}
	
	const int tracks_to_write = raw_disk.mTrackCount;
	const int sides_to_write = raw_disk.mSideCount;

	printf("Writing %s-sided SuperCard Pro %u-track image with %u revolutions: %s\n", sides_to_write > 1 ? "double" : "single", tracks_to_write, maxrevs, path);

	FILE *fo = fopen(path, "wb");
	if (!fo)
		fatalf("Cannot open %s for write\n", path);
	
	const bool source_96tpi = raw_disk.mTrackStep == 1;
	const bool source_double_sided = sides_to_write > 1;
	const bool write_96tpi = source_96tpi || forced_tpi >= 96;

	// if the side step isn't forced, use double stepping either if he source is double sided or
	// the source is 40 track, as 40 track single sided isn't normally a thing
	const bool image_double_sided = (!forced_side_step && !source_96tpi) || source_double_sided || forced_side_step != 1;

	if (forced_tpi && forced_tpi < 96 && source_96tpi)
		fatal("Cannot write a 96tpi disk to a 48tpi SCP image.");

	if (forced_side_step == 1 && source_double_sided)
		fatal("Cannot write a double-sided image with single-side stepping.\n");

	const int image_track_step = (write_96tpi && !source_96tpi ? 2 : 1) * (image_double_sided ? 2 : 1);

	SCPFileHeader filehdr = {0};

	filehdr.mSignature[0] = 'S';
	filehdr.mSignature[1] = 'C';
	filehdr.mSignature[2] = 'P';
	filehdr.mVersion = 0x00;	// now using footer

	// SCP unfortunately uses the disk type field to determine track layout, so we vary it based on the density
	// of the tracks. For 96/135 tpi, use the 720K other format for (80/2); otherwise, use the Atari format (40/2).
	filehdr.mDiskType = write_96tpi ? 0x84 : 0x10;		// Other 720K / Atari 800

	filehdr.mNumRevs = maxrevs;
	filehdr.mStartTrack = 0;
	filehdr.mEndTrack = tracks_to_write * image_track_step - 1;

	// Index, 96 TPI, 360 RPM, read only, has footer
	const bool use_360rpm = true;

	filehdr.mFlags = use_360rpm ? 0x36 : 0x34;

	// set normalized flag if source was synthesized or modified
	if (raw_disk.mSynthesized)
		filehdr.mFlags |= 0x08;

	filehdr.mBitCellEncoding = 0;
	filehdr.mSides = 0;
	filehdr.mChecksum = 0;

	// write out initial header
	fwrite(&filehdr, sizeof filehdr, 1, fo);

	for(int i=0; i<tracks_to_write; ++i) {
		if (selected_track >= 0 && selected_track != i)
			continue;

		for(int side=0; side<sides_to_write; ++side) {
			const auto& track_info = raw_disk.mPhysTracks[side][i * raw_disk.mTrackStep];

			// skip track if it is empty
			if (track_info.mIndexTimes.size() < (size_t)maxrevs + 1)
				continue;

			const int image_track = i * image_track_step + side;

			// rescale all transitions and index times
			// we need 25ns samples and a 360 RPM rotational speed
			const double sample_scale = (use_360rpm ? 40000000.0 / 6.0 : 40000000.0 / 5.0) / (double)track_info.mSamplesPerRev;

			std::vector<uint32_t> new_samples;
			const auto& transitions = track_info.mTransitions;
		
			new_samples.reserve(transitions.size());
			for(std::vector<uint32_t>::const_iterator it = std::lower_bound(transitions.begin(), transitions.end(), track_info.mIndexTimes.front()),
				itEnd = std::upper_bound(it, transitions.end(), track_info.mIndexTimes[maxrevs]); it != itEnd; ++it) {
				new_samples.push_back((uint32_t)((double)*it * sample_scale + 0.5));
			}

			std::vector<uint32_t> new_index_marks;
			for(auto it = track_info.mIndexTimes.begin(), itEnd = track_info.mIndexTimes.end(); it != itEnd; ++it) {
				new_index_marks.push_back((uint32_t)((double)*it * sample_scale + 0.5));
			}

			// encode all samples
			std::vector<uint8_t> bitdata;
			std::vector<uint32_t> new_sample_offsets;

			if (new_samples.size() > 1) {
				uint32_t error = 0;
				uint32_t last_time = new_index_marks.front();

				for(auto it = new_samples.begin(), itEnd = new_samples.end(); it != itEnd; ++it) {
					uint32_t delay = *it - last_time;
					last_time = *it;

					// we can't encode a sample of 0, so we need to push such transitions
					if (delay < error + 1) {		// delay - error < 1
						error += 1 - delay;
						delay = 1;
					}

					// encode
					new_sample_offsets.push_back((uint32_t)bitdata.size());

					while(delay >= 0x10000) {
						bitdata.push_back(0);
						bitdata.push_back(0);
						delay -= 0x10000;
					}

					bitdata.push_back((uint8_t)(delay >> 8));
					bitdata.push_back((uint8_t)(delay >> 0));
				}
			}

			if (bitdata.size() & 2) {
				bitdata.push_back(0);
				bitdata.push_back(0);
			}

			new_sample_offsets.push_back((uint32_t)bitdata.size());

			uint32_t track_data[16] = {0};

			memcpy(&track_data[0], "TRK", 3);
			((uint8_t *)track_data)[3] = image_track;

			for(int j=0; j<maxrevs; ++j) {
				// index time
				track_data[3*j + 1] = new_index_marks[j + 1] - new_index_marks[j];

				// track data offset
				auto sample_start = std::lower_bound(new_samples.begin(), new_samples.end(), new_index_marks[j]);
				auto sample_end = std::lower_bound(new_samples.begin(), new_samples.end(), new_index_marks[j+1]);
				uint32_t data_start = new_sample_offsets[sample_start - new_samples.begin()];
				uint32_t data_end = new_sample_offsets[sample_end - new_samples.begin()];

				track_data[3*j + 3] = sizeof(track_data) + data_start;

				// track length (in encoded values)
				track_data[3*j + 2] = (data_end - data_start) >> 1;
			}

			// update checksum
			filehdr.mChecksum += ComputeByteSum(track_data, sizeof track_data);
			filehdr.mChecksum += ComputeByteSum(bitdata.data(), bitdata.size());

			// set track offset and update checksum
			uint32_t track_offset = (uint32_t)ftell(fo);
			filehdr.mTrackOffsets[image_track] = track_offset;

			// write it out
			fwrite(track_data, sizeof track_data, 1, fo);
			fwrite(bitdata.data(), bitdata.size(), 1, fo);
		}
	}

	// Write timestamp.
	//
	// This is written as the SCP software as (date) space (time), where (date) and (time)
	// are formatted in the local region encoding, and with no length or terminating null.
	// This is... not ideal. To bring some sanity, we always encode local time with the US date
	// and time format, and emit at least one null between the timestamp and the footer
	// data to prevent the first byte of a footer string from being misinterpreted.
	const auto scp_timestamp = get_localtime_scp_us();
	const size_t scp_timestamp_len = scp_timestamp.size() + 1;
	fwrite(scp_timestamp.c_str(), scp_timestamp_len, 1, fo);
	filehdr.mChecksum += ComputeByteSum(scp_timestamp.c_str(), scp_timestamp_len);

	// set up footer
	SCPExtensionFooter footer {};

	footer.mCreationTime = get_time64();
	footer.mModificationTime = get_time64();

	// write application name
	static const char kAppName[] = A8RC_NAME_AND_VERSION;
	footer.mApplicationNameOffset = (uint32_t)ftell(fo);

	uint16_t nameLen = sizeof(kAppName) - 1;
	write_u16(nameLen, fo);
	filehdr.mChecksum += ComputeByteSum(&nameLen, sizeof nameLen);

	fwrite(kAppName, sizeof kAppName, 1, fo);
	filehdr.mChecksum += ComputeByteSum(kAppName, sizeof kAppName);

	// write footer
	fwrite(&footer, sizeof footer, 1, fo);
	filehdr.mChecksum += ComputeByteSum(&footer, sizeof footer);

	// rewrite file header
	filehdr.mChecksum += ComputeByteSum(filehdr.mTrackOffsets, sizeof filehdr.mTrackOffsets);

	fseek(fo, 0, SEEK_SET);
	fwrite(&filehdr, sizeof filehdr, 1, fo);
	fclose(fo);
}
