#include "stdafx.h"
#include "scp.h"

bool g_scpDirDriveB;
bool g_scpDirRPM360;
int g_scpDirTrackStep;
uint32_t g_scpDirRotTicks;

void scp_direct_init(const char *path, bool require_96tpi, bool high_density) {
	if (0 == strncmp(path, "scp0:", 5)) {
		g_scpDirDriveB = false;
	} else if (0 == strncmp(path, "scp1:", 5)) {
		g_scpDirDriveB = true;
	} else {
		fatalf("Unrecognized SCP direct path: %s\n", path);
	}

	// decode TPI
	if (0 == strncmp(path+5, "48tpi", 5)) {
		g_scpDirTrackStep = 1;

		if (require_96tpi)
			fatalf("Cannot use a 48 TPI drive to read or write a 96 TPI image.\n");
	} else if (0 == strncmp(path+5, "96tpi", 5) || 0 == strncmp(path+5, "135tpi", 6)) {
		g_scpDirTrackStep = 2;
	} else {
		fatalf("Bad SCP direct path: %s. Missing 48tpi/96tpi/135tpi identifier (e.g. scp0:48tpi).\n", path);
	}

	// check for override path
	const char *override_path = strchr(path + 5, ':');

	if (override_path)
		++override_path;

	scp_init(override_path);

	// turn off other drive's motor and deselect it
	scp_select_drive(g_scpDirDriveB, false);
	scp_select_drive(!g_scpDirDriveB, true);
	scp_motor(!g_scpDirDriveB, false);
	scp_select_drive(!g_scpDirDriveB, false);

	// read a track to determine RPM
	printf("Detecting drive RPM...");
	scp_select_drive(g_scpDirDriveB, true);
	scp_select_density(high_density);
	scp_select_side(false);
	scp_motor(g_scpDirDriveB, true);

	(void)scp_track_read(false, 1, false, false);

	uint32_t readinfo[10];
	scp_track_getreadinfo(readinfo);

	// 300 RPM is 8M ticks, 360 RPM is 6.67M ticks... so split down the middle
	uint32_t avgtime = readinfo[0];

	g_scpDirRPM360 = (avgtime < 7333333);
	g_scpDirRotTicks = avgtime;

	printf(" %u RPM (actual %.2f RPM)\n", g_scpDirRPM360 ? 360 : 300, 40000000.0 * 60.0f / (double)avgtime);
}

void scp_direct_shutdown() {
	// turn off motor and deselect drive
	scp_motor(g_scpDirDriveB, false);
	scp_select_drive(g_scpDirDriveB, false);
	scp_shutdown();
}

void scp_direct_read(RawDisk& raw_disk, const char *path, int selected_track, int revs, bool high_density, bool splice) {
	if (splice && revs < 2)
		fatal("Error: Splice mode requires at least two revolutions.\n");

	scp_direct_init(path, raw_disk.mTrackStep == 1, high_density);
	scp_seek0();

	const int phys_track_step = (g_scpDirTrackStep * raw_disk.mTrackStep) / 2;

	bool use_8bit = false;

	for(int i=0; i<raw_disk.mTrackCount; ++i) {
		if (selected_track >= 0 && i != selected_track)
			continue;

		for(int side=0; side<raw_disk.mSideCount; ++side) {
			if (raw_disk.mSideCount > 1)
				printf("Reading track %u, side %u (%d revs)\n", i, side, revs);
			else
				printf("Reading track %u (%d revs)\n", i, revs);

			scp_seek(i * phys_track_step);
			scp_select_side(side);
			use_8bit = scp_track_read(g_scpDirRPM360, revs, use_8bit, splice);

			uint32_t trkinfo[10];
			scp_track_getreadinfo(trkinfo);

			uint32_t totallen = 0;
			
			for(int rev=0; rev<revs; ++rev)
				totallen += trkinfo[rev*2 + 1];

			if (totallen > (use_8bit ? 524288U : 262144U))
				fatalf("Error: SCP reported too many bitcells: %u (exceeds 512K memory)", totallen);

			std::vector<uint8_t> bitcells8;
			std::vector<uint16_t> bitcells16;

			if (use_8bit) {
				bitcells8.resize(totallen);
				scp_mem_read(bitcells8.data(), 0, totallen);
			} else {
				bitcells16.resize(totallen);
				scp_mem_read(bitcells16.data(), 0, totallen*2);
			}

			RawTrack& raw_track = raw_disk.mPhysTracks[side][i * raw_disk.mTrackStep];

			uint32_t time_sum = 0;
			for(int rev = 0; rev < revs; ++rev)
				time_sum += trkinfo[2*rev];

			raw_track.mSamplesPerRev = (float)(time_sum) / (float)revs;
			raw_track.mSpliceStart = -1;
			raw_track.mSpliceEnd = -1;

			raw_track.mIndexTimes.resize(revs + 1, 0);

			for(int i=0; i<revs; ++i)
				raw_track.mIndexTimes[i+1] = raw_track.mIndexTimes[i] + trkinfo[i*2];

			uint32_t last_time = 0;

			if (use_8bit) {
				for(size_t i=0; i<totallen; ++i) {
					uint8_t sample = bitcells8[i];

					if (!sample)
						last_time += 0x100;
					else {
						last_time += sample;

						raw_track.mTransitions.push_back(last_time);
					}
				}
			} else {
				for(size_t i=0; i<totallen; ++i) {
					uint16_t sample = swizzle_u16_from_be(bitcells16[i]);

					if (!sample)
						last_time += 0x10000;
					else {
						last_time += sample;

						raw_track.mTransitions.push_back(last_time);
					}
				}
			}
		}
	}

	scp_seek0();
	scp_direct_shutdown();
}

void scp_direct_write(const RawDisk& raw_disk, const char *path, int selected_track, bool high_density, bool splice) {
	scp_direct_init(path, raw_disk.mTrackStep == 1, high_density);
	scp_seek0();
	
	const int phys_track_step = (g_scpDirTrackStep * raw_disk.mTrackStep) / 2;

	for(int i=0; i<raw_disk.mTrackCount; ++i) {
		if (selected_track >= 0 && i != selected_track)
			continue;

		scp_seek(i * phys_track_step);

		for(int side = 0; side < raw_disk.mSideCount; ++side) {
			// check if we have at least two revs on this track
			const RawTrack& raw_track = raw_disk.mPhysTracks[side][i * raw_disk.mTrackStep];

			scp_select_side(side);

			if (raw_track.mIndexTimes.size() < 3) {
erase_track:
				// empty track -- wipe the track
				std::vector<uint16_t> emptyTrack((g_scpDirRotTicks >> 16) + 1, 0);

				printf("Erasing track %u (%u revs)\n", i, (uint32_t)raw_track.mIndexTimes.size());

				scp_mem_write(emptyTrack.data(), 0, (uint32_t)emptyTrack.size() * 2);
				scp_track_write(g_scpDirRPM360, (uint32_t)emptyTrack.size(), splice, false);
				continue;
			}

			// determine splice points
			const uint32_t idx1 = raw_track.mIndexTimes[0];
			const uint32_t idx2 = raw_track.mIndexTimes[1];
			uint32_t splice_start = idx1;
			uint32_t splice_end = idx2;

			if (raw_track.mSpliceStart >= 0) {
				splice_start = raw_track.mSpliceStart;
				splice_end = raw_track.mSpliceEnd;
			}

			// back off the splice end point by 1% to avoid track overrun
			//splice_end -= (splice_end - splice_start) / 50;

			// extract transitions between splice points
			auto it1 = std::lower_bound(raw_track.mTransitions.begin(), raw_track.mTransitions.end(), splice_start);
			auto it2 = std::upper_bound(it1, raw_track.mTransitions.end(), splice_end);

			// Encode leader time -- we need to delay from the index mark to the splice
			// start. This needs to be at least a few dozen bits (~5K ticks) as the initially
			// written bits tend to read/write as garbage.
			double rot_delay = (double)(splice_start - idx1) / (double)(idx2 - idx1);
			rot_delay -= floor(rot_delay);

			uint32_t rot_delay_ticks = splice ? 5000 : (uint32_t)(0.5 + rot_delay * (double)g_scpDirRotTicks);

			if (rot_delay_ticks < 5000)
				rot_delay_ticks += g_scpDirRotTicks;

			std::vector<uint16_t> transitions;
			while(rot_delay_ticks >= 0x10000) {
				transitions.push_back(0);
				rot_delay_ticks -= 0x10000;
			}

			if (rot_delay_ticks)
				transitions.push_back(swizzle_u16_to_be(rot_delay_ticks));
			else
				transitions.push_back(swizzle_u16_to_be(1));

			// Rescale transitions from splice start to splice stop.
			if (it1 == it2) {
				// Uh oh... we don't have any transitions. Well, just erase the track.
				goto erase_track;
			}
		
			double tick_scale = (double)g_scpDirRotTicks / raw_track.mSamplesPerRev;
			uint32_t last_time = splice_start;
			for(auto it = it1; it != it2; ++it) {
				uint32_t cur_time = (uint32_t)(0.5 + (*it) * tick_scale);
				uint32_t delay = cur_time >= last_time ? cur_time - last_time : 0;
				last_time = cur_time;

				if (!delay) {
					delay = 1;
					++last_time;
				}

				while(delay >= 0x10000) {
					delay -= 0x10000;
					transitions.push_back(0);
				}

				if (delay)
					transitions.push_back(swizzle_u16_to_be(delay));
				else {
					// Uh oh... we can't actually write this transition time exactly because
					// it's a multiple of 64K. Write one tick more and adjust the time base
					// for the next transition.
					transitions.push_back(swizzle_u16_to_be(1));
					++last_time;
				}
			}

			if (transitions.size() > 262144)
				fatalf("Cannot write track: exceeds 512K memory limit.\n", i);

			unsigned track_bytes = (unsigned)(transitions.size() * 2);
			if (splice) {
				if (raw_disk.mSideCount > 1)
					printf("Writing track %u.%u: %u bytes, splice\n", i, side, track_bytes);
				else
					printf("Writing track %u: %u bytes, splice\n", i, track_bytes);
			} else {
				if (raw_disk.mSideCount > 1)
					printf("Writing track %u.%u: %u bytes, delay %.3f\n", i, side, track_bytes, rot_delay);
				else
					printf("Writing track %u: %u bytes, delay %.3f\n", i, track_bytes, rot_delay);
			}

			scp_mem_write(transitions.data(), 0, (uint32_t)(transitions.size() * 2));
			scp_track_write(g_scpDirRPM360, (uint32_t)transitions.size(), splice, false);
		}
	}

	scp_seek0();
	scp_direct_shutdown();
}
