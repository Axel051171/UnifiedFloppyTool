/* Atari 810 format */

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
#include "crc.h"

static int min_track(struct phys *this) {
	return 0;
}

static int max_track(struct phys *this) {
	return 39;
}

static int min_sector(struct phys *this, int track, int side) {
	return 1;
}

static int max_sector(struct phys *this, int track, int side) {
	return 18;
}

static int sector_bytes(struct phys *this, int track, int side, int sector) {
	return 128;
}

void flip_bits(unsigned char *out, unsigned char *in, int count) {
	while(count--)
		*out++=*in++^0xff;
}

static int read_sector(struct phys *this, unsigned char *out, int track, int side, int sector) {
	unsigned char id_field_raw[]={0xfe,track,0,sector,0,0,0};
	unsigned int xferlen=260; /* 256 nibbles, checksum */
	int xferlen_out;
	unsigned char raw[260], plain[131]={0xfb,0,};
	struct {
		uint8_t opcode, flags, format;
		uint16_t bitcell;
		uint8_t sectorhole;
		uint8_t rdelayh; uint16_t rdelayl;
		uint8_t idam, id_pat[12], id_mask[12], dam[3];
	} __attribute__ ((__packed__)) cdb={OPCODE_READ_FLEXIBLE,READ_FLAG_ORUN_RECOV,FORMAT_FM,htons(6400),0,0,0,0x7e,{0,},{0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff},{0x6f,0,0}};
	int status=0;

	crc_append(id_field_raw,5);
	status|=enc_fm(cdb.id_pat,id_field_raw+1,6);
	status|=fc_bulk_cdb(&cdb,sizeof(cdb),4000,NULL,raw,xferlen,&xferlen_out);
	if(xferlen_out!=xferlen)
		status|=1;
	status|=dec_fm(plain+1,raw,130);
	if(crc_block(plain,131)!=0)
		status|=1;
	flip_bits(out,plain+1,128);
	return status;
}

static void best_read_order(struct phys *this, struct sector_list *out, int track, int side) {
	struct sector_list sector_list[]={{1,0},{5,512},{9,1024},{13,1536},{17,2048},{4,384},{8,896},{12,1408},{16,1920},{3,256},{7,768},{11,1280},{15,1792},{2,128},{6,640},{10,1152},{14,1664},{18,2176}};

	memcpy(out,sector_list,sizeof(sector_list));
}

struct phys phys_atari810={
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
	.prepare = phys_gen_no_prepare
};
