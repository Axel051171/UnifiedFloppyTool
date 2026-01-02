/* Apple 13-sector format, used by DOS 3.2 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#ifdef WIN32
#include <winsock.h>
#else
#include <arpa/inet.h>
#endif
#include <usb.h>
#include "phys.h"
#include "fc5025.h"
#include "endec.h"

static unsigned char volume;

static int min_track(struct phys *this) {
	return 0;
}

static int max_track(struct phys *this) {
	return 34;
}

static int min_sector(struct phys *this, int track, int side) {
	return 0;
}

static int max_sector(struct phys *this, int track, int side) {
	return 12;
}

static int sector_bytes(struct phys *this, int track, int side, int sector) {
	return 256;
}

static int read_sector(struct phys *this, unsigned char *out, int track, int side, int sector) {
	unsigned char csum=volume^track^sector;
	unsigned char id_field_raw[4]={volume,track,sector,csum};
	unsigned int xferlen=413; /* 410 nibbles, checksum, 2 of 3 epilog */
	int xferlen_out;
	unsigned char raw[413];
	struct {
		uint8_t opcode, flags, format;
		uint16_t bitcell;
		uint8_t sectorhole;
		uint8_t rdelayh; uint16_t rdelayl;
		uint8_t idam, id_pat[12], id_mask[12], dam[3];
	} __attribute__ ((__packed__)) cdb={OPCODE_READ_FLEXIBLE,READ_FLAG_ORUN_RECOV,FORMAT_APPLE_GCR,htons(3266),0,0,0,0xff,{0xd5,0xaa,0xb5,0,0,0,0,0,0,0,0,0xde},{0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff},{0xd5,0xaa,0xad}};
	int status=0;

	status|=enc_4and4(cdb.id_pat+3,id_field_raw,4);
	status|=fc_bulk_cdb(&cdb,sizeof(cdb),4000,NULL,raw,xferlen,&xferlen_out);
	if(xferlen_out!=xferlen)
		status|=1;
	status|=dec_5and3(out,raw,256);
	return status;
}

static void best_read_order(struct phys *this, struct sector_list *out, int track, int side) {
	struct sector_list sector_list[]={{0,0},{7,1792},{1,256},{8,2048},{2,512},{9,2304},{3,768},{10,2560},{4,1024},{11,2816},{5,1280},{12,3072},{6,1536}};

	memcpy(out,sector_list,sizeof(sector_list));
}

static int prepare(struct phys *this) {
	unsigned char buf[2];
	int status=0;

	status|=fc_READ_ID(buf,2,0,FORMAT_APPLE_GCR,3266,0xd5,0xaa,0xb5);
	status|=dec_4and4(&volume,buf,1);
	return status;
}

struct phys phys_apple32={
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
	.best_read_order = best_read_order,
	.read_sector = read_sector,
	.prepare = prepare
};
