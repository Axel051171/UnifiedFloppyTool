/* North Star MDS double-density - single-sided (175k) and double-sided (350k)*/

#include <stdio.h>
#include <stdint.h>
#ifdef WIN32
#include <winsock.h>
#else
#include <arpa/inet.h>
#endif
#include <usb.h>
#include <string.h>
#include "phys.h"
#include "fc5025.h"
#include "endec.h"
#include "crc.h"

static int min_track(struct phys *this) {
	return 0;
}

static int max_track(struct phys *this) {
	return 34;
}

static int max_track_ds(struct phys *this) {
	/* 2 sides, 35 tracks per side, serpentine layout, exposed as
	   pseudo single sided, like TI-99/4A */
	return 69;
}

static int physical_track(struct phys *this, int track) {
	if(track>34)
		track=69-track;
	return track*2;
}

static int min_sector(struct phys *this, int track, int side) {
	return 1;
}

static int max_sector(struct phys *this, int track, int side) {
	return 10;
}

static int sector_bytes(struct phys *this, int track, int side, int sector) {
	return 512;
}

static unsigned char ns_csum(unsigned char *buf, int count) {
	unsigned char csum=0;

	while(count--) {
		csum^=*buf++;
		csum=(csum<<1)|(csum>>7);
	}
	return csum;
}

static int read_sector(struct phys *this, unsigned char *out, int pseudotrack, int dummy, int sector) {
	int side=(pseudotrack>34)?1:0;
	unsigned int xferlen=774;
	int xferlen_out;
	unsigned char raw[774], plain[515];
	struct {
		uint8_t opcode, flags, format;
		uint16_t bitcell;
		uint8_t sectorhole;
		uint8_t rdelayh; uint16_t rdelayl;
		uint8_t idam, id_pat[12], id_mask[12], dam[3];
	} __attribute__ ((__packed__)) cdb={OPCODE_READ_FLEXIBLE,side,FORMAT_MFM,htons(3333),sector,0,htons(347),1,{0,},};
	int status=0;

	status|=fc_bulk_cdb(&cdb,sizeof(cdb),4000,NULL,raw,xferlen,&xferlen_out);
	if(xferlen_out!=xferlen)
		status|=1;
	dec_mfm(plain,raw,515);
	if(plain[0]!=0xfb) /* first sync byte; ignore second */
		return 1;
	if(ns_csum(plain+2,513)!=0)
		status|=1;
	memcpy(out,plain+2,512);
	return status;
}

struct phys phys_mdsad={
	.min_track = min_track,
	.max_track = max_track,
	.num_tracks = phys_gen_num_tracks,
	.min_side = min_track,
	.max_side = min_track,
	.num_sides = phys_gen_num_sides,
	.min_sector = min_sector,
	.max_sector = max_sector,
	.num_sectors = phys_gen_num_sectors,
	.tpi = phys_gen_48tpi,
	.density = phys_gen_low_density,
	.sector_bytes = sector_bytes,
	.track_bytes = phys_gen_track_bytes,
	.physical_track = phys_gen_physical_track,
	.best_read_order = phys_gen_best_read_order,
	.read_sector = read_sector,
	.prepare = phys_gen_no_prepare
};

struct phys phys_mdsad350={
	.min_track = min_track,
	.max_track = max_track_ds,
	.num_tracks = phys_gen_num_tracks,
	.min_side = min_track,
	.max_side = min_track,
	.num_sides = phys_gen_num_sides,
	.min_sector = min_sector,
	.max_sector = max_sector,
	.num_sectors = phys_gen_num_sectors,
	.tpi = phys_gen_48tpi,
	.density = phys_gen_low_density,
	.sector_bytes = sector_bytes,
	.track_bytes = phys_gen_track_bytes,
	.physical_track = physical_track,
	.best_read_order = phys_gen_best_read_order,
	.read_sector = read_sector,
	.prepare = phys_gen_no_prepare
};
