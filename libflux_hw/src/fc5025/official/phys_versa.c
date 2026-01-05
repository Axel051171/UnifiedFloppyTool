/* Motorola VersaDOS - uses ECMA-78 track format 1 */

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
	return 79;
}

static int min_side(struct phys *this) {
	return 0;
}

static int max_side(struct phys *this) {
	return 1;
}

static int min_sector(struct phys *this, int track, int side) {
	return 1;
}

static int max_sector(struct phys *this, int track, int side) {
	return 16;
}

static int sector_bytes(struct phys *this, int track, int side, int sector) {
	if(track==0 && side==0)
		return 128;
	return 256;
}

static int read_sector_fm(struct phys *this, unsigned char *out, int track, int side, int sector) {
	unsigned char id_field_raw[]={0xfe,track,side,sector,0,0,0};
	unsigned int xferlen=260; /* 256 nibbles, checksum */
	int xferlen_out;
	unsigned char raw[260], plain[131]={0xfb,0,};
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
	status|=dec_fm(plain+1,raw,130);
	if(crc_block(plain,131)!=0)
		status|=1;
	memcpy(out,plain+1,128);
	return status;
}

static int read_sector_mfm(struct phys *this, unsigned char *out, int track, int side, int sector) {
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

static int read_sector(struct phys *this, unsigned char *out, int track, int side, int sector) {
	if(track==0 && side==0) {
		return read_sector_fm(this,out,track,side,sector);
	} else {
		return read_sector_mfm(this,out,track,side,sector);
	}
}

struct phys phys_versa={
	.min_track = min_track,
	.max_track = max_track,
	.num_tracks = phys_gen_num_tracks,
	.min_side = min_side,
	.max_side = max_side,
	.num_sides = phys_gen_num_sides,
	.min_sector = min_sector,
	.max_sector = max_sector,
	.num_sectors = phys_gen_num_sectors,
	.tpi = phys_gen_96tpi,
	.density = phys_gen_low_density,
	.sector_bytes = sector_bytes,
	.track_bytes = phys_gen_track_bytes,
	.physical_track = phys_gen_physical_track,
	.best_read_order = phys_gen_best_read_order,
	.read_sector = read_sector,
	.prepare = phys_gen_no_prepare
};
