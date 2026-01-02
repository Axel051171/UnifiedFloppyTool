#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <usb.h>
#include "drives.h"
#include "fc5025.h"

static struct drive_info *drives=NULL;

int open_drive(struct drive_info *drive) {
	return fc5025_open(drive->usbdev);
}

int open_drive_by_id(char *id) {
	struct drive_info *drive;

	for(drive=drives;drive->id[0]!='\0';drive++) {
		if(strcmp(id,drive->id)==0)
			return open_drive(drive);
	}

	return 1;
}

char *open_default_drive(void) {
	if(open_drive(drives)!=0)
		return NULL;
	return drives->id;
}

int close_drive(void) {
	return fc5025_close();
}

static int get_desc(struct drive_info *drive) {
	struct usb_device_descriptor descriptor;
	usb_dev_handle *udev;

	udev=usb_open(drive->usbdev);
	if(!udev)
		return 1;
	if(usb_get_descriptor(udev,USB_DT_DEVICE,0,&descriptor,sizeof(descriptor))!=sizeof(descriptor)) {
		usb_close(udev);
		return 1;
	}
	if(usb_get_string_simple(udev,descriptor.iProduct,drive->desc,256)<=0) {
		usb_close(udev);
		return 1;
	}
	usb_close(udev);
	return 0;
}

struct drive_info *get_drive_list(void) {
	int total_devs, listed_devs;
	static struct usb_device **devs=NULL;
	int i;
	struct usb_device **dev;
	struct drive_info *drive;

	total_devs=fc5025_find(NULL,0);
	if(total_devs==0)
		return NULL;
	if(devs!=NULL)
		free(devs);
	devs=malloc(total_devs*sizeof(struct usb_device));
	if(!devs)
		return NULL;
	listed_devs=fc5025_find(devs,total_devs);
	if(listed_devs==0)
		return NULL;
	if(drives!=NULL)
		free(drives);
	drives=malloc((listed_devs+1)*sizeof(struct drive_info));
	if(!drives)
		return NULL;
	dev=devs; drive=drives;
	for(i=0;i<listed_devs;i++) {
		snprintf(drive->id,256,"%s/%s",(*dev)->bus->dirname,(*dev)->filename);
		drive->usbdev=*dev;
		if(get_desc(drive)==0) {
			dev++; drive++;
		}
	}
	drive->id[0]='\0'; drive->desc[0]='\0'; drive->usbdev=NULL;

	if(drive==drives)
		return NULL;
	return drives;
}
