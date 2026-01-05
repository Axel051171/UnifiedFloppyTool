/* MFM (double density) with FC5025 compression scheme */

#include <string.h>

static unsigned char *mfmdata;
static unsigned char *mfmmask;
static int bitnum;
static int one_pending;
static int zero_pending;
static int lastbit;
static unsigned char mfmbyte;

/*
 * Decoder
 */

static unsigned char get_raw_bit(void) {
	unsigned char bit;

	bit=(mfmbyte&0x80)?1:0;
	mfmbyte<<=1;
	bitnum++;
	if(bitnum>7) {
		mfmbyte=*mfmdata;
		mfmdata++;
		bitnum=0;
	}
	return bit;
}

static unsigned char get_disk_bit(void) {
	if(one_pending) {
		one_pending=0;
		return 1;
	}
	
	if(get_raw_bit()==1)
		one_pending=1;

	return 0;
}

static unsigned char get_decoded_byte(void) {
	int i;
	unsigned char byte=0;

	for(i=0;i<8;i++) {
		get_disk_bit(); /* throw away clock bit */
		byte<<=1;
		byte|=get_disk_bit();
	}
	return byte;
}

static void start_decoder(unsigned char *in) {
	mfmdata=in;
	bitnum=0;
	one_pending=0;
	mfmbyte=*mfmdata;
	mfmdata++;
}

int dec_mfm(unsigned char *out, unsigned char *in, int count) {
	start_decoder(in);
	while(count--) {
		*out=get_decoded_byte();
		out++;
	}

	return 0;
}

/*
 * Encoder
 */

static void emit_raw_bit(char bit) {
	*mfmmask|=bitnum;
	if(bit==1)
		*mfmdata|=bitnum;
	bitnum>>=1;
	if(bitnum==0) {
		bitnum=0x80;
		mfmdata++;
		mfmmask++;
	}
}

static void emit_disk_bit(char bit) {
	if(zero_pending) {
		if(bit==0) {
			emit_raw_bit(0);
		} else {
			emit_raw_bit(1);
			zero_pending=0;
		}
		return;
	}

	if(bit==0) {
		zero_pending=1;
	} else {
		emit_raw_bit(1);
	}
}

static void encode_bit(char bit) {
	char clock, data;

	data=bit;
	if(bit==1) {
		clock=0;
	} else {
		clock=lastbit^1;
	}
	lastbit=bit;
	emit_disk_bit(clock);
	emit_disk_bit(data);
}

static void encode_byte(unsigned char byte) {
	int i;

	for(i=0;i<8;i++) {
		encode_bit((byte&0x80)?1:0);
		byte<<=1;
	}
}

static void start_encoder(unsigned char *outbits, unsigned char *outmask) {
	mfmdata=outbits;
	mfmmask=outmask;
	bitnum=0x80;
	lastbit=0;
	zero_pending=0;
}

int enc_mfm(unsigned char *out, unsigned char *outmask, unsigned char *in, int count) {
	memset(out,0,count+(count/2)+1);
	memset(outmask,0,count+(count/2)+1);
	start_encoder(out,outmask);
	while(count--) {
		encode_byte(*in);
		in++;
	}

	return 0;
}
