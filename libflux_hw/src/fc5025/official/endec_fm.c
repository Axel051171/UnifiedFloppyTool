/* FM (single density) */

static unsigned char enc_fm_nibble(unsigned char in) {
	int i;
	unsigned char out;

	for(i=0;i<4;i++) {
		out<<=2;
		out|=2; /* clock */
		if(in&0x80) out|=1; /* data */
		in<<=1;
	}

	return out;
}

int enc_fm(unsigned char *out, unsigned char *in, int count) {
	while(count--) {
		out[0]=enc_fm_nibble(*in);
		out[1]=enc_fm_nibble(*in<<4);
		in++; out+=2;
	}
	return 0;
}

int dec_fm(unsigned char *raw, unsigned char *fm, int count) {
	int status=0;

	while(count--) {
		if(((*fm)&0xAA)!=0xAA)
			status=1;
		*raw=0;
		if((*fm)&0x40)
			*raw|=0x80;
		if((*fm)&0x10)
			*raw|=0x40;
		if((*fm)&0x04)
			*raw|=0x20;
		if((*fm)&0x01)
			*raw|=0x10;
		fm++;
		if((*fm)&0x40)
			*raw|=0x08;
		if((*fm)&0x10)
			*raw|=0x04;
		if((*fm)&0x04)
			*raw|=0x02;
		if((*fm)&0x01)
			*raw|=0x01;
		fm++;
		raw++;
	}

	return status;
}
