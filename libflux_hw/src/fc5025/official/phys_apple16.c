/* Apple 16-sector format, used by DOS 3.3 and ProDOS */

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
	return 15;
}

static int sector_bytes(struct phys *this, int track, int side, int sector) {
	return 256;
}

static int read_sector_direct(struct phys *this, unsigned char *out, int track, int side, int sector) {
	unsigned char csum=volume^track^sector;
	unsigned char id_field_raw[4]={volume,track,sector,csum};
	unsigned int xferlen=345; /* 342 nibbles, checksum, 2 of 3 epilog */
	int xferlen_out;
	unsigned char raw[345];
	struct {
		uint8_t opcode, flags, format;
		uint16_t bitcell;
		uint8_t sectorhole;
		uint8_t rdelayh; uint16_t rdelayl;
		uint8_t idam, id_pat[12], id_mask[12], dam[3];
	} __attribute__ ((__packed__)) cdb={OPCODE_READ_FLEXIBLE,READ_FLAG_ORUN_RECOV,FORMAT_APPLE_GCR,htons(3266),0,0,0,0xff,{0xd5,0xaa,0x96,0,0,0,0,0,0,0,0,0xde},{0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff},{0xd5,0xaa,0xad}};
	int status=0;

	status|=enc_4and4(cdb.id_pat+3,id_field_raw,4);
	status|=fc_bulk_cdb(&cdb,sizeof(cdb),4000,NULL,raw,xferlen,&xferlen_out);
	if(xferlen_out!=xferlen)
		status|=1;
	status|=dec_6and2(out,raw,256);
	return status;
}

static int read_sector_dos33(struct phys *this, unsigned char *out, int track, int side, int sector) {
	static int dos33_to_phys[]={0,13,11,9,7,5,3,1,14,12,10,8,6,4,2,15};

	return read_sector_direct(this,out,track,side,dos33_to_phys[sector]);
}

static void best_read_order_dos33(struct phys *this, struct sector_list *out, int track, int side) {
	struct sector_list sector_list[]={{0,0},{14,3584},{13,3328},{12,3072},{11,2816},{10,2560},{9,2304},{8,2048},{7,1792},{6,1536},{5,1280},{4,1024},{3,768},{2,512},{1,256},{15,3840}};

	memcpy(out,sector_list,sizeof(sector_list));
}

static int read_sector_prodos(struct phys *this, unsigned char *out, int track, int side, int sector) {
	/* ProDOS actually uses LBA addressing of 512-byte blocks which are each
	   the concatenation of two 256-byte physical sectors, but we pretend
	   that it uses track/sector addressing so that fcimage can make .po
	   images. "Sector 0" is the first half of the lowest-numbered block on
	   a given track, "sector 1" is the second half, 2 is the first half of
	   the next block, etc. */
	/* Beneath Apple ProDOS p. 3-17
	   ProDOS 8 Technical Reference Manual p. 174 */
	static int prodos_to_phys[]={0,2,4,6,8,10,12,14,1,3,5,7,9,11,13,15};

	return read_sector_direct(this,out,track,side,prodos_to_phys[sector]);
}

static void best_read_order_prodos(struct phys *this, struct sector_list *out, int track, int side) {
	struct sector_list sector_list[]={{0,0},{1,256},{2,512},{3,768},{4,1024},{5,1280},{6,1536},{7,1792},{8,2048},{9,2304},{10,2560},{11,2816},{12,3072},{13,3328},{14,3584},{15,3840}};

	memcpy(out,sector_list,sizeof(sector_list));
}

static int prepare(struct phys *this) {
	unsigned char buf[2];
	int status=0;

	status|=fc_READ_ID(buf,2,0,FORMAT_APPLE_GCR,3266,0xd5,0xaa,0x96);
	status|=dec_4and4(&volume,buf,1);
	return status;
}

struct phys phys_apple33={
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
	.best_read_order = best_read_order_dos33,
	.read_sector = read_sector_dos33,
	.prepare = prepare
};

struct phys phys_applepro={
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
	.best_read_order = best_read_order_prodos,
	.read_sector = read_sector_prodos,
	.prepare = prepare
};

