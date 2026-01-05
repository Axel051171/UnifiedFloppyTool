#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "phys.h"
#include "log.h"

extern struct phys phys_apple33;
extern struct phys phys_apple32;
extern struct phys phys_applepro;
extern struct phys phys_c1541;
extern struct phys phys_ti99;
extern struct phys phys_ti99ds180;
extern struct phys phys_ti99ds360;
extern struct phys phys_atari810;
extern struct phys phys_msdos1200;
extern struct phys phys_msdos360;
extern struct phys phys_mdsad;
extern struct phys phys_mdsad350;
extern struct phys phys_kaypro2;
extern struct phys phys_kaypro4;
extern struct phys phys_vg4500;
extern struct phys phys_pmc;
extern struct phys phys_coco;
extern struct phys phys_versa;

extern struct log log_applepro;
extern struct log log_fat12;
extern struct log log_cpm22_kp2;
extern struct log log_cpm22_kp4;
extern struct log log_cpm30_pmc;
extern struct log log_rsdos;
extern struct log log_versa;

static struct format_info {
	char *id;
	char *desc;
	char *suffix;
	struct phys *phys;
	struct log *log;
} formats[]={
	{"apple33","Apple DOS 3.3 (16-sector)","dsk",&phys_apple33,NULL},
	{"apple32","Apple DOS 3.2 (13-sector)","d13",&phys_apple32,NULL},
	{"applepro","Apple ProDOS","po",&phys_applepro,&log_applepro},
	{"c1541","Commodore 1541","d64",&phys_c1541,NULL},
	{"ti99","TI-99/4A 90k","v9t9",&phys_ti99,NULL},
	{"ti99ds180","TI-99/4A 180k","dsk",&phys_ti99ds180,NULL},
	{"ti99ds360","TI-99/4A 360k","dsk",&phys_ti99ds360,NULL},
	{"atari810","Atari 810","xfd",&phys_atari810,NULL},
	{"msdos12","MS-DOS 1200k","img",&phys_msdos1200,&log_fat12},
	{"msdos360","MS-DOS 360k","img",&phys_msdos360,&log_fat12},
	{"mdsad","North Star MDS-A-D 175k","nsi",&phys_mdsad,NULL},
	{"mdsad350","North Star MDS-A-D 350k","nsi",&phys_mdsad350,NULL},
	{"kaypro2","Kaypro 2 CP/M 2.2","dsk",&phys_kaypro2,&log_cpm22_kp2},
	{"kaypro4","Kaypro 4 CP/M 2.2","dsk",&phys_kaypro4,&log_cpm22_kp4},
	{"vg4500","CalComp Vistagraphics 4500","img",&phys_vg4500,NULL},
	{"pmc","PMC MicroMate","img",&phys_pmc,&log_cpm30_pmc},
	{"coco","Tandy Color Computer Disk BASIC","dsk",&phys_coco,&log_rsdos},
	{"versa","Motorola VersaDOS","img",&phys_versa,&log_versa},
	{NULL,NULL,NULL,NULL}
};

struct format_info *get_format_list(void) {
	return formats;
}

struct format_info *format_by_id(char *id) {
	struct format_info *format;

	for(format=formats;format->id!=NULL;format++) {
		if(strcasecmp(format->id,id)==0)
			return format;
	}

	return NULL;
}
