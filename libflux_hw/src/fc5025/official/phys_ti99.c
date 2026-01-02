/* TI-99 series formats */

/* single-sided, single-density, 90k
   double-sided, single-density, 180k
   single-sided, double-density - not supported (no sample disk)
   double-sided, double-density, 360k */

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
	return 39;
}

static int max_track_ds(struct phys *this) {
	/* TI-99/4A logical sector ordering starts with side 0, tracks 0
	   through 39, and then continues with side 1, tracks 39 through 0.
	   To get the correct sector ordering, we pretend the disk is
	   single-sided and use pseudo track numbers 40 to 79 to represent
	   the tracks on side 1. */
	return 79;
}

static int physical_track(struct phys *this, int track) {
	if(track>39)
		track=79-track;
	return track*2;
}

static int min_sector(struct phys *this, int track, int side) {
	return 0;
}

static int max_sector_is_8(struct phys *this, int track, int side) {
	return 8;
}

static int max_sector_is_17(struct phys *this, int track, int side) {
	return 17;
}

static int sector_bytes(struct phys *this, int track, int side, int sector) {
	return 256;
}

static int read_sector_fm(struct phys *this, unsigned char *out, int pseudotrack, int dummy, int sector) {
	int side=(pseudotrack>39)?1:0;
	int track=(pseudotrack>39)?(79-pseudotrack):pseudotrack;
	unsigned char id_field_raw[]={0xfe,track,side,sector,1,0,0};
	unsigned int xferlen=516; /* 512 nibbles, checksum */
	int xferlen_out;
	unsigned char raw[516], plain[259]={0xfb,0,};
	struct {
		uint8_t opcode, flags, format;
		uint16_t bitcell;
		uint8_t sectorhole;
		uint8_t rdelayh; uint16_t rdelayl;
		uint8_t idam, id_pat[12], id_mask[12], dam[3];
	} __attribute__ ((__packed__)) cdb={OPCODE_READ_FLEXIBLE,READ_FLAG_ORUN_RECOV|side,FORMAT_FM,htons(6667),0,0,0,0x7e,{0,},{0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff},{0x6f,0,0}};
	int status=0;

	crc_append(id_field_raw,5);
	status|=enc_fm(cdb.id_pat,id_field_raw+1,6);
	status|=fc_bulk_cdb(&cdb,sizeof(cdb),4000,NULL,raw,xferlen,&xferlen_out);
	if(xferlen_out!=xferlen)
		status|=1;
	status|=dec_fm(plain+1,raw,258);
	if(crc_block(plain,259)!=0)
		status|=1;
	memcpy(out,plain+1,256);
	return status;
}

static int read_sector_mfm(struct phys *this, unsigned char *out, int pseudotrack, int dummy, int sector) {
	int side=(pseudotrack>39)?1:0;
	int track=(pseudotrack>39)?(79-pseudotrack):pseudotrack;
	unsigned char id_field_raw[]={0xa1,0xa1,0xa1,0xfe,track,side,sector,1,0,0};
	unsigned int xferlen=385;
	int xferlen_out;
	unsigned char raw[386]={0xf9,0,}, plain[262]={0xa1,0xa1,0xa1,0,};
	struct {
		uint8_t opcode, flags, format;
		uint16_t bitcell;
		uint8_t sectorhole;
		uint8_t rdelayh; uint16_t rdelayl;
		uint8_t idam, id_pat[12], id_mask[12], dam[3];
	} __attribute__ ((__packed__)) cdb={OPCODE_READ_FLEXIBLE,READ_FLAG_ORUN_RECOV|side,FORMAT_MFM,htons(3333),0,0,0,0xfe,{0,},{0,},{0xf9,0,0}};
	int status=0;

	crc_append(id_field_raw,8);
	enc_mfm(cdb.id_pat,cdb.id_mask,id_field_raw+4,6);
	status|=fc_bulk_cdb(&cdb,sizeof(cdb),4000,NULL,raw+1,xferlen,&xferlen_out);
	if(xferlen_out!=xferlen)
		status|=1;
	dec_mfm(plain+3,raw,259);
	if(plain[3]!=0xfb) /* DAM */
		status|=1;
	if(crc_block(plain,262)!=0)
		status|=1;
	memcpy(out,plain+4,256);
	return status;
}

static void best_read_order_9sectors(struct phys *this, struct sector_list *out, int track, int side) {
	struct sector_list sector_list[]={{0,0},{5,1280},{1,256},{6,1536},{2,512},{7,1792},{3,768},{8,2048},{4,1024}};

	memcpy(out,sector_list,sizeof(sector_list));
}

static void best_read_order_18sectors(struct phys *this, struct sector_list *out, int track, int side) {
	struct sector_list sector_list[]={{0,0},{5,1280},{1,256},{6,1536},{2,512},{7,1792},{3,768},{8,2048},{4,1024},{9,2304},{14,3584},{10,2560},{15,3840},{11,2816},{16,4096},{12,3072},{17,4352},{13,3328}};

	memcpy(out,sector_list,sizeof(sector_list));
}

struct phys phys_ti99={
	.min_track = min_track,
	.max_track = max_track,
	.num_tracks = phys_gen_num_tracks,
	.min_side = min_track,
	.max_side = min_track,
	.num_sides = phys_gen_num_sides,
	.min_sector = min_sector,
	.max_sector = max_sector_is_8,
	.num_sectors = phys_gen_num_sectors,
	.tpi = phys_gen_48tpi,
	.density = phys_gen_low_density,
	.sector_bytes = sector_bytes,
	.track_bytes = phys_gen_track_bytes,
	.physical_track = phys_gen_physical_track,
	.best_read_order = best_read_order_9sectors,
	.read_sector = read_sector_fm,
	.prepare = phys_gen_no_prepare
};

struct phys phys_ti99ds180={
	.min_track = min_track,
	.max_track = max_track_ds,
	.num_tracks = phys_gen_num_tracks,
	.min_side = min_track,
	.max_side = min_track,
	.num_sides = phys_gen_num_sides,
	.min_sector = min_sector,
	.max_sector = max_sector_is_8,
	.num_sectors = phys_gen_num_sectors,
	.tpi = phys_gen_48tpi,
	.density = phys_gen_low_density,
	.sector_bytes = sector_bytes,
	.track_bytes = phys_gen_track_bytes,
	.physical_track = physical_track,
	.best_read_order = best_read_order_9sectors,
	.read_sector = read_sector_fm,
	.prepare = phys_gen_no_prepare
};

struct phys phys_ti99ds360={
	.min_track = min_track,
	.max_track = max_track_ds,
	.num_tracks = phys_gen_num_tracks,
	.min_side = min_track,
	.max_side = min_track,
	.num_sides = phys_gen_num_sides,
	.min_sector = min_sector,
	.max_sector = max_sector_is_17,
	.num_sectors = phys_gen_num_sectors,
	.tpi = phys_gen_48tpi,
	.density = phys_gen_low_density,
	.sector_bytes = sector_bytes,
	.track_bytes = phys_gen_track_bytes,
	.physical_track = physical_track,
	.best_read_order = best_read_order_18sectors,
	.read_sector = read_sector_mfm,
	.prepare = phys_gen_no_prepare
};
