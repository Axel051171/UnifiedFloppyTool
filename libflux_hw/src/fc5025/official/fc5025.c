/* Communicate with the FC5025 hardware */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <usb.h>
#ifdef WIN32
#include <winsock.h>
#else
#include <arpa/inet.h>
#endif
#include <time.h>
#include "fc5025.h"

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define usleep(x) Sleep(x/1000)
#endif

#define swap32(x) (((((uint32_t)x) & 0xff000000) >> 24) | \
                  ((((uint32_t)x) & 0x00ff0000) >>  8) | \
                  ((((uint32_t)x) & 0x0000ff00) <<  8) | \
                  ((((uint32_t)x) & 0x000000ff) << 24))
#define htov32(x) swap32(htonl(x))

static usb_dev_handle *udev;

static struct {
	uint8_t signature[4];
	uint32_t tag, xferlen;
	uint8_t flags;
	uint8_t padding1, padding2;
	uint8_t cdb[48];
} __attribute__ ((__packed__)) cbw={{'C','F','B','C'},0x12345678,0,0x80,0,0,{0,}};

int fc_bulk_cdb(void *cdb, int length, int timeout, void *csw_out, void *xferbuf, int xferlen, int *xferlen_out) {
	struct {
		uint32_t signature, tag;
		uint8_t status, sense, asc, ascq;
		uint8_t padding[20];
	} __attribute__ ((__packed__)) csw;
	int ret;

	cbw.tag++;
	cbw.xferlen=htov32(xferlen);
	memset(&(cbw.cdb),0,48);
	memcpy(&(cbw.cdb),cdb,length);
	if(xferlen_out!=NULL)
		*xferlen_out=0;

	ret=usb_bulk_write(udev,1,(char *)&cbw,63,1500);
	if(ret!=63)
		return 1;

	if(xferlen!=0) {
		ret=usb_bulk_read(udev,0x81,xferbuf,xferlen,timeout);
		if(ret<0)
			return 1;
		if(xferlen_out!=NULL)
			*xferlen_out=ret;
		timeout=500;
	}

	ret=usb_bulk_read(udev,0x81,(char *)&csw,32,timeout);
	if(ret<12 || ret>31)
		return 1;

	if(csw.signature!=htov32(0x46435342))
		return 1;
	if(csw.tag!=cbw.tag)
		return 1;

	if(csw_out!=NULL)
		memcpy(csw_out,&csw,12);
	return(csw.status);
}

int fc_recalibrate(void) {
	int ret;

	struct {
		uint8_t opcode, mode, steprate, track;
	} __attribute__ ((__packed__)) cdb={OPCODE_SEEK,3,15,100};

	ret=fc_bulk_cdb(&cdb,sizeof(cdb),600,NULL,NULL,0,NULL);
	usleep(15000);
	return ret;
}

int fc_SEEK_abs(int track) {
	int ret;

	struct {
		uint8_t opcode, mode, steprate, track;
	} __attribute__ ((__packed__)) cdb={OPCODE_SEEK,0,15,track};

	ret=fc_bulk_cdb(&cdb,sizeof(cdb),600,NULL,NULL,0,NULL);
	usleep(15000);
	return ret;
}

int fc_READ_ID(unsigned char *out, int length, char side, char format, int bitcell, unsigned char idam0, unsigned char idam1, unsigned char idam2) {
	struct {
		uint8_t opcode, side, format;
		uint16_t bitcell;
		uint8_t idam0, idam1, idam2;
	} __attribute__ ((__packed__)) cdb={OPCODE_READ_ID,side,format,htons(bitcell),idam0,idam1,idam2};
	int xferlen_out;
	int status=0;

	status|=fc_bulk_cdb(&cdb,sizeof(cdb),3000,NULL,out,length,&xferlen_out);
	if(xferlen_out!=length)
		status|=1;
	return status;
} 

int fc_FLAGS(int in, int mask, int *out) {
	struct {
		uint8_t opcode, mask, flags;
	} __attribute__ ((__packed__)) cdb={OPCODE_FLAGS,mask,in};
	unsigned char buf;
	int ret;
	int xferlen_out;

	ret=fc_bulk_cdb(&cdb,sizeof(cdb),1500,NULL,&buf,1,&xferlen_out);
	if(xferlen_out==1) {
		if(out!=NULL)
			*out=buf;
		return ret;
	}
	return 1;
}

int fc_set_density(int density) {
	return fc_FLAGS(density<<2,4,NULL);
}

int fc5025_open(struct usb_device *dev) {
	cbw.tag=time(NULL)&0xffffffff;
	udev=usb_open(dev);
	if(!udev)
		return 1;
#ifdef WIN32
	if(usb_set_configuration(udev,1)!=0) {
		usb_close(udev);
		return 1;
	}
#endif
	if(usb_claim_interface(udev,0)!=0) {
		usb_close(udev);
		return 1;
	}
	return 0;
}

int fc5025_close(void) {
	if(usb_release_interface(udev,0)!=0 || usb_close(udev)!=0)
		return 1;

	return 0;
}

int fc5025_find(struct usb_device **devs, int max) {
	struct usb_bus *bus;
	struct usb_device *dev;
	static int usb_inited=0;
	int num_found=0;

	if(!usb_inited) {
		usb_init();
		usb_inited=1;
	}

	usb_find_busses();
	usb_find_devices();

	for(bus=usb_get_busses();bus!=NULL;bus=bus->next) {
		for(dev=bus->devices;dev!=NULL;dev=dev->next) {
			if ((dev->descriptor.idVendor==FC5025_VID)&&(dev->descriptor.idProduct==FC5025_PID)) {
				num_found++;
				if(num_found<=max)
					*(devs++)=dev;
                        }
		}
	}

	return num_found;
}
