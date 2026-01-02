/* 5/4 GCR, used by Commodore 1541 */

static unsigned char enc_tbl[]={
	0x0A, /* 0 */
	0x0B, /* 1 */
	0x12, /* 2 */
	0x13, /* 3 */
	0x0E, /* 4 */
	0x0F, /* 5 */
	0x16, /* 6 */
	0x17, /* 7 */
	0x09, /* 8 */
	0x19, /* 9 */
	0x1A, /* A */
	0x1B, /* B */
	0x0D, /* C */
	0x1D, /* D */
	0x1E, /* E */
	0x15, /* F */
};

static unsigned char dec_tbl[]={
	0xFF, /* 0 */
	0xFF, /* 1 */
	0xFF, /* 2 */
	0xFF, /* 3 */
	0xFF, /* 4 */
	0xFF, /* 5 */
	0xFF, /* 6 */
	0xFF, /* 7 */
	0xFF, /* 8 */
	0x08, /* 9 */
	0x00, /* A */
	0x01, /* B */
	0xFF, /* C */
	0x0C, /* D */
	0x04, /* E */
	0x05, /* F */
	0xFF, /* 10 */
	0xFF, /* 11 */
	0x02, /* 12 */
	0x03, /* 13 */
	0xFF, /* 14 */
	0x0F, /* 15 */
	0x06, /* 16 */
	0x07, /* 17 */
	0xFF, /* 18 */
	0x09, /* 19 */
	0x0A, /* 1A */
	0x0B, /* 1B */
	0xFF, /* 1C */
	0x0D, /* 1D */
	0x0E, /* 1E */
	0xFF, /* 1F */
};

static int baddog;

static void enc_54gcr_chunk(unsigned char out[5],unsigned char in[4]) {
	out[0]=enc_tbl[in[0]>>4]<<3;
	out[0]|=enc_tbl[in[0]&0xf]>>2;
	out[1]=enc_tbl[in[0]&0xf]<<6;
	out[1]|=enc_tbl[in[1]>>4]<<1;
	out[1]|=enc_tbl[in[1]&0xf]>>4;
	out[2]=enc_tbl[in[1]&0xf]<<4;
	out[2]|=enc_tbl[in[2]>>4]>>1;
	out[3]=enc_tbl[in[2]>>4]<<7;
	out[3]|=enc_tbl[in[2]&0xf]<<2;
	out[3]|=enc_tbl[in[3]>>4]>>3;
	out[4]=enc_tbl[in[3]>>4]<<5;
	out[4]|=enc_tbl[in[3]&0xf];
}

static int decode_nib(unsigned char foo) {
	if(dec_tbl[foo&0x1f]==0xff)
		baddog=1;
	return dec_tbl[foo&0x1f];
}

static int dec_54gcr_chunk(unsigned char out[4], unsigned char in[5]) {
	baddog=0;
	out[0]=decode_nib(in[0]>>3)<<4;
	out[0]|=decode_nib((in[0]<<2)|(in[1]>>6));
	out[1]=decode_nib(in[1]>>1)<<4;
	out[1]|=decode_nib((in[1]<<4)|(in[2]>>4));
	out[2]=decode_nib((in[2]<<1)|(in[3]>>7))<<4;
	out[2]|=decode_nib(in[3]>>2);
	out[3]=decode_nib((in[3]<<3)|(in[4]>>5))<<4;
	out[3]|=decode_nib(in[4]);
	return baddog;
}

int enc_54gcr(unsigned char *out, unsigned char *in, int count) {
	if(count&3)
		return 1;

	while(count) {
		enc_54gcr_chunk(out,in);
		out+=5; in+=4;
		count-=4;
	}

	return 0;
}

int dec_54gcr(unsigned char *out, unsigned char *in, int count) {
	int status=0;

	if(count&3)
		return 1;

	while(count) {
		status|=dec_54gcr_chunk(out,in);
		out+=4; in+=5;
		count-=4;
	}

	return status;
}
