/* Generic routines for physical formats */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <usb.h>
#ifdef WIN32
#include <winsock.h>
#else
#include <arpa/inet.h>
#endif
#include "phys.h"
#include "endec.h"
#include "fc5025.h"
#include "crc.h"

int phys_gen_num_tracks(struct phys *this) {
	return this->max_track(this)-this->min_track(this)+1;
}

int phys_gen_num_sectors(struct phys *this, int track, int side) {
	return this->max_sector(this,track,side)-this->min_sector(this,track,side)+1;
}

int phys_gen_num_sides(struct phys *this) {
	return this->max_side(this)-this->min_side(this)+1;
}

int phys_gen_track_bytes(struct phys *this, int track, int side) {
	int sector;
	int bytes=0;

	for(sector=this->min_sector(this,track,side);sector<=this->max_sector(this,track,side);sector++) {
		bytes+=this->sector_bytes(this,track,side,sector);
	}

	return bytes;
}

int phys_gen_physical_track(struct phys *this, int track) {
	track-=this->min_track(this);
	if(this->tpi(this)==48)
		track*=2;
	return track;
}

static void half_the_sectors(struct phys *this, struct sector_list **out, int track, int side, int which_half) {
	int first=this->min_sector(this,track,side);
	int last=this->max_sector(this,track,side);
	int sector;
	int offset=0;

	for(sector=first;sector<=last;sector++) {
		if((sector&1)==which_half) {
			(*out)->sector=sector;
			(*out)->offset=offset;
			(*out)++;
		}
		offset+=this->sector_bytes(this,track,side,sector);
	}
}

void phys_gen_best_read_order(struct phys *this, struct sector_list *out, int track, int side) {
	half_the_sectors(this,&out,track,side,1);
	half_the_sectors(this,&out,track,side,0);
}

int phys_gen_48tpi(struct phys *this) {
	return 48;
}

int phys_gen_96tpi(struct phys *this) {
	return 96;
}

int phys_gen_100tpi(struct phys *this) {
	return 100;
}

int phys_gen_low_density(struct phys *this) {
	return 1;
}

int phys_gen_high_density(struct phys *this) {
	return 0;
}

int phys_gen_no_prepare(struct phys *this) {
	return 0;
}

static unsigned char size_to_len(int size) {
	switch(size) {
	case 128: return 0;
	case 256: return 1;
	case 512: return 2;
	case 1024: return 3;
	default: return 0xFF;
	}
}

static int mfm_xferlen(int size) {
	int dam_trailing_bit=1;
	int encoded_payload_bytes=size+((size+1)/2);
	int encoded_crc_bytes=3;

	return dam_trailing_bit+encoded_payload_bytes+encoded_crc_bytes;
}

int phys_gen_read_mfm(unsigned char *out, int track, int phys_side, int id_side, int sector, int size, int bitcell) {
	unsigned char len=size_to_len(size);
	unsigned char id_field_raw[]={0xa1,0xa1,0xa1,0xfe,track,id_side,sector,len,0,0};
	unsigned int xferlen=mfm_xferlen(size);
	int xferlen_out;
	int raw_size=1+xferlen, plain_size=3+1+size+2;
	unsigned char *raw, *plain;
	struct {
		uint8_t opcode, flags, format;
		uint16_t bitcell;
		uint8_t sectorhole;
		uint8_t rdelayh; uint16_t rdelayl;
		uint8_t idam, id_pat[12], id_mask[12], dam[3];
	} __attribute__ ((__packed__)) cdb={OPCODE_READ_FLEXIBLE,READ_FLAG_ORUN_RECOV|phys_side,FORMAT_MFM,htons(bitcell),0,0,0,0xfe,{0,},{0,},{0xf9,0,0}};
	int status=0;

	raw=malloc(raw_size);
	if(!raw)
		return 1;
	plain=malloc(plain_size);
	if(!plain) {
		free(raw);
		return 1;
	}

	raw[0]=0xf9;
	plain[0]=0xa1; plain[1]=0xa1; plain[2]=0xa1;

	crc_append(id_field_raw,8);
	enc_mfm(cdb.id_pat,cdb.id_mask,id_field_raw+4,6);
	status|=fc_bulk_cdb(&cdb,sizeof(cdb),4000,NULL,raw+1,xferlen,&xferlen_out);
	if(xferlen_out!=xferlen)
		status|=1;
	dec_mfm(plain+3,raw,1+size+2);
	if(plain[3]!=0xfb) /* DAM */
		status|=1;
	if(crc_block(plain,3+1+size+2)!=0)
		status|=1;
	memcpy(out,plain+4,size);
	free(raw); free(plain);
	return status;
}

int phys_gen_read_fm(unsigned char *out, int track, int phys_side, int id_side, int sector, int size, int bitcell) {
	unsigned char len=size_to_len(size);
	unsigned char id_field_raw[]={0xfe,track,id_side,sector,len,0,0};
	unsigned int xferlen=size*2+4;
	int xferlen_out;
	int raw_size=xferlen, plain_size=1+size+2;
	unsigned char *raw, *plain;
	struct {
		uint8_t opcode, flags, format;
		uint16_t bitcell;
		uint8_t sectorhole;
		uint8_t rdelayh; uint16_t rdelayl;
		uint8_t idam, id_pat[12], id_mask[12], dam[3];
	} __attribute__ ((__packed__)) cdb={OPCODE_READ_FLEXIBLE,READ_FLAG_ORUN_RECOV|phys_side,FORMAT_FM,htons(bitcell),0,0,0,0x7e,{0,},{0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff},{0x6f,0,0}};
	int status=0;

	raw=malloc(raw_size);
	if(!raw)
		return 1;
	plain=malloc(plain_size);
	if(!plain) {
		free(raw);
		return 1;
	}

	plain[0]=0xfb;

	crc_append(id_field_raw,5);
	status|=enc_fm(cdb.id_pat,id_field_raw+1,6);
	status|=fc_bulk_cdb(&cdb,sizeof(cdb),4000,NULL,raw,xferlen,&xferlen_out);
	if(xferlen_out!=xferlen)
		status|=1;
	status|=dec_fm(plain+1,raw,size+2);
	if(crc_block(plain,1+size+2)!=0)
		status|=1;
	memcpy(out,plain+1,size);
	free(raw); free(plain);
	return status;
}

int phys_gen_read_sector(struct phys *this, unsigned char *out, int track, int side, int sector) {
	if(this->encoding(this,track,side,sector)==FORMAT_MFM) {
		return phys_gen_read_mfm(out,track,side,side,sector,this->sector_bytes(this,track,side,sector),this->bitcell(this,track,side,sector));
	} else {
		return phys_gen_read_fm(out,track,side,side,sector,this->sector_bytes(this,track,side,sector),this->bitcell(this,track,side,sector));
	}
}
