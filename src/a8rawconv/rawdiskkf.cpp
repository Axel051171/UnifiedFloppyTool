#include "stdafx.h"

class MemStream {
public:
	MemStream(const uint8_t *s, const uint8_t *t) : mpSrc(s), mpSrcEnd(t), mpSrcBegin(s) {}

	int get() {
		return mpSrc != mpSrcEnd ? *mpSrc++ : -1;
	}

	int pos() const { return (int)(mpSrc - mpSrcBegin); }

private:
	const uint8_t *mpSrc;
	const uint8_t *const mpSrcEnd;
	const uint8_t *const mpSrcBegin;
};

void kf_read_track(RawTrack& rawTrack, int track, int side, const char *path) {
	// KryoFlux system constants (defaults)
	const double mck = 18432000.0 * 73.0 / 14.0 / 2.0;
	double sck = mck / 2.0;
	double ick = mck / 16.0;

	std::vector<uint32_t> indexmarks;
	std::vector<uint32_t> indexstreamposes;
	std::vector<uint32_t> indextimers;

	typedef std::pair<uint32_t, uint32_t> streamtime_t;
	std::vector<streamtime_t> streamtimes;
	
	if (g_verbosity >= 1)
		printf("Reading KryoFlux track stream: %s\n", path);

	FILE *f = fopen(path, "rb");
	if (!f)
		fatalf("Unable to open input track stream: %s.\n", path);

	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	if (len > 500*1024*1024 || (unsigned long)len != size_t(len))
		fatalf("Stream too long: %ld bytes\n", len);

	std::vector<uint8_t> rawstream((size_t)len);
	if (fseek(f, 0, SEEK_SET) || 1 != fread(rawstream.data(), (size_t)len, 1, f))
		fatalf("Error reading from stream: %s\n", path);
	fclose(f);

	// getc() is painfully slow in VS2010
	MemStream ms(rawstream.data(), rawstream.data() + rawstream.size());

	int t = 0;
	uint32_t oobSize = 0;
	for(;;) {
		// Index marks are placed according to stream buffer positions, which do NOT include
		// out of band (OOB) blocks. Therefore, we have to subtract the OOB block sizes to
		// account for this.
		streamtimes.push_back(streamtime_t(ms.pos() - oobSize, t));

		int c = ms.get();

		if (c < 0)
			break;

//		printf("%d | %d %d\n", t, ftell(f)-1, c);

		if (c < 8) {
			int delay = c << 8;

			c = ms.get();
			if (c < 0)
				fatal("Incomplete cell");

			delay += c;

			t += delay;
			rawTrack.mTransitions.push_back(t);
		} else if (c == 8) {
		} else if (c == 9) {
			c = ms.get();
			if (c < 0)
				fatal("Incomplete Nop2");
		} else if (c == 10) {
			for(int i=0; i<2; ++i) {
				c = ms.get();
				if (c < 0)
					fatal("Incomplete Nop3");
			}
		} else if (c == 11) {
			t += 0x10000;
		} else if (c == 12) {
			c = ms.get();
			if (c < 0)
				fatal("Incomplete Value16");
			t += c << 8;

			c = ms.get();
			if (c < 0)
				fatal("Incomplete Value16");
			t += c;
			rawTrack.mTransitions.push_back(t);
		} else if (c == 13) {
			int oobPos = ms.pos();
			int oobType = ms.get();
			if (oobType < 0)
				fatal("Incomplete OOB block");

			// end is a special block with no length
			if (oobType == 13)
				break;

			c = ms.get();
			if (c < 0)
				fatal("Incomplete OOB block");

			int oobLen = c;

			c = ms.get();
			if (c < 0)
				fatal("Incomplete OOB block");

			oobLen += c << 8;

			oobSize += oobLen + 3;

			//printf("Received %u bytes of OOB data (type %u) (offset %u)\n", oobLen, oobType, ftell(f));

			std::vector<uint8_t> oobData(oobLen);

			for(int i=0; i<oobLen; ++i) {
				c = ms.get();
				if (c < 0)
					fatal("Incomplete OOB block");

				oobData[i] = (uint8_t)c;
			}

			if (oobType == 2) {
				if (oobLen != 12)
					fatal("Invalid index mark OOB block");

				const uint32_t raw_index_time = read_u32(&oobData[8]);
				const uint32_t raw_streampos = read_u32(&oobData[0]);
				const uint32_t raw_timer = read_u32(&oobData[4]);

				indexmarks.push_back(raw_index_time);
				rawTrack.mIndexTimes.push_back(raw_timer);
				indexstreamposes.push_back(raw_streampos);
				indextimers.push_back(raw_timer);

				if (g_verbosity >= 2)
					printf("Index mark: oobpos=%u, streampos=%u, streamtime=%u, timer=%u, systime=%u\n", oobPos, raw_streampos, t, raw_timer, raw_index_time);
			} else if (oobType == 4) {
				oobData.push_back(0);

				// read sample clock (sck) and index clock (ick) values from stream
				const char *sckref = strstr((const char *)oobData.data(), "sck=");
				if (sckref) {
					double adj_sck = strtod(sckref, nullptr);

					if (adj_sck >= sck * 0.90 && adj_sck < sck * 1.10)
						sck = adj_sck;
				}

				const char *ickref = strstr((const char *)oobData.data(), "ick=");
				if (ickref) {
					double adj_ick = strtod(ickref, nullptr);

					if (adj_ick >= ick * 0.90 && adj_ick < ick * 1.10)
						ick = adj_ick;
				}
			}
		} else {
			t += c;
			rawTrack.mTransitions.push_back(t);
		}
	}

	// go back and just all of the index mark times
	for(size_t i=0, n=rawTrack.mIndexTimes.size(); i<n; ++i) {
		auto it = std::lower_bound(streamtimes.begin(), streamtimes.end(), streamtime_t(indexstreamposes[i], 0),
			[](const streamtime_t& x, const streamtime_t& y) { return x.first < y.first; });

		uint32_t prev_sck = 0;

		if (it != streamtimes.begin()) {
			--it;
			prev_sck = it->second;
		}

		rawTrack.mIndexTimes[i] += prev_sck + indextimers[i];

		if (i && g_verbosity >= 2)
			printf("  Rotation time: %u scks\n", rawTrack.mIndexTimes[i] - rawTrack.mIndexTimes[i-1]);
	}

	//printf("%u transitions read\n", (unsigned)transitions.size());

	if (indexmarks.size() < 2) {
		fatal("Less than two index marks read -- need at least one full disk revolution.");
	}

	// compute disk RPM
	double icks_per_rev = (double)(int32_t)(indexmarks.back() - indexmarks.front()) / (double)(indexmarks.size() - 1);
	double rpm = ick / icks_per_rev * 60.0;
	double samples_per_rev = icks_per_rev * sck / ick;

	if (g_verbosity >= 1) {
		printf("  %u index marks found\n", (unsigned)indexmarks.size());
		printf("  Rotational rate: %.2f RPM (%.0f samples per revolution)\n", rpm, samples_per_rev);
	}

	rawTrack.mSide = side;
	rawTrack.mSamplesPerRev = (float)samples_per_rev;
	rawTrack.mSpliceStart = -1;
	rawTrack.mSpliceEnd = -1;
}

void kf_read(RawDisk& raw_disk, int trackcount, int trackstep, const char *basepath, int sidepos, int sidewidth, int sidebase, int countpos, int countwidth, int trackselect, bool use_48tpi) {
	printf("Reading KryoFlux track stream set (%u TPI)...\n", !use_48tpi || trackcount > 40 ? 96 : 48);

	if (use_48tpi && trackstep < 2)
		fatalf("Cannot use a 48tpi stream set with 96tpi geometry.\n");

	const int image_track_step = !use_48tpi && trackstep > 1 ? 2 : 1;

	for(int i=0; i<trackcount; ++i) {
		if (trackselect >= 0 && i != trackselect)
			continue;

		for(int side=0; side<raw_disk.mSideCount; ++side) {
			std::string track_filename(g_inputPath);

			char buf[64];
			sprintf(buf, "%0*u", countwidth, i * image_track_step);
			track_filename.replace(countpos, countwidth, buf);

			sprintf(buf, "%0*u", sidewidth, side + sidebase);
			track_filename.replace(sidepos, sidewidth, buf);

			kf_read_track(raw_disk.mPhysTracks[side][i * raw_disk.mTrackStep], i, side, track_filename.c_str());
		}
	}
}
