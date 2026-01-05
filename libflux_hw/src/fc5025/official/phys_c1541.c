/* Commodore 1541 format */

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

static uint16_t volume;

static int min_track(struct phys *this) {
	return 1;
}

static int max_track(struct phys *this) {
	return 35;
}

static int min_sector(struct phys *this, int track, int side) {
	return 0;
}

static int max_sector(struct phys *this, int track, int side) {
	static int sector_num[]={
		0,
		20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 
		18, 18, 18, 18, 18, 18, 18,
		17, 17, 17, 17, 17, 17, 
		16, 16, 16, 16, 16, 
	};

	return sector_num[track];
}

static int sector_bytes(struct phys *this, int track, int side, int sector) {
	return 256;
}

static int cbm_bitcell(int track) {
	if(track<18)
		return 2708;
	if(track<25)
		return 2917;
	if(track<31)
		return 3125;
	return 3333;
}

static unsigned char xor_csum(unsigned char *buf, int bytes) {
	unsigned char c=0;

	while(bytes--)
		c^=*(buf++);

	return c;
}

static int read_sector(struct phys *this, unsigned char *out, int track, int side, int sector) {
	unsigned char csum=track^sector^(volume>>8)^(volume&0xff);
	unsigned char id_field_raw[8]={0x08,csum,sector,track,volume>>8,volume&0xff,0,0};
	unsigned int xferlen=325;
	int xferlen_out;
	unsigned char raw[325], plain[260];
	struct {
		uint8_t opcode, flags, format;
		uint16_t bitcell;
		uint8_t sectorhole;
		uint8_t rdelayh; uint16_t rdelayl;
		uint8_t idam, id_pat[12], id_mask[12], dam[3];
	} __attribute__ ((__packed__)) cdb={OPCODE_READ_FLEXIBLE,READ_FLAG_ORUN_RECOV,FORMAT_COMMODORE_GCR,htons(cbm_bitcell(track)),0,0,0,0xff,{0,},{0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0,0,0,0},{0xff,0,0}};
	int status=0;

	status|=enc_54gcr(cdb.id_pat,id_field_raw,8);
	cdb.id_pat[8]=0; cdb.id_pat[9]=0;
	status|=fc_bulk_cdb(&cdb,sizeof(cdb),4000,NULL,raw,xferlen,&xferlen_out);
	status|=dec_54gcr(plain,raw,260);
	if(plain[257]!=xor_csum(plain+1,256))
		status|=1;
	if(plain[0]!=0x07) /* block type */
		status|=1;
	memcpy(out,plain+1,256);
	return status;
}

static int prepare(struct phys *this) {
	unsigned char gcr[10], plain[4];
	int status=0;

	status|=fc_SEEK_abs(this->physical_track(this,18));
	status|=fc_READ_ID(gcr,10,0,FORMAT_COMMODORE_GCR,cbm_bitcell(18),0x52,0x40,0);
	status|=dec_54gcr(plain,gcr+5,4);
	volume=(plain[0]<<8)|plain[1];
	return status;
}

struct phys phys_c1541={
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
	.prepare = prepare
};

