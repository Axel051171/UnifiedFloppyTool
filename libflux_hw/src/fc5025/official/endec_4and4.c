/* 4-and-4 code, used in Apple address fields */

static void enc_4and4_byte(unsigned char *out, unsigned char *in_ptr) {
	unsigned char in=*in_ptr;
	int i;

	out[0]=0; out[1]=0;
	for(i=0;i<4;i++) {
		out[0]<<=2; out[1]<<=2;
		out[0]|=2; out[1]|=2;
		if(in&0x80) out[0]|=1;
		if(in&0x40) out[1]|=1;
		in<<=2;
	}
}

static void dec_4and4_byte(unsigned char *out, unsigned char *in) {
	*out=((in[0]<<1)|1)&in[1];
}

int enc_4and4(unsigned char *out, unsigned char *in, int count) {
	while(count--) {
		enc_4and4_byte(out,in);
		out+=2; in++;
	}

	return 0;
}

int dec_4and4(unsigned char *out, unsigned char *in, int count) {
	while(count--) {
		dec_4and4_byte(out,in);
		out++; in+=2;
	}

	return 0;
}
