#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef WIN32
#include <winsock.h>
#else
#include <arpa/inet.h>
#endif
#include "phys.h"
#include "log.h"
#include "log_rsdos.h"

#ifdef WIN32
#define S_IRGRP 0
#define S_IWGRP 0
#define S_IROTH 0
#define S_IWOTH 0
#endif

struct log log_rsdos;

static uint8_t *fat;

static void elide_trailing_spaces(char *s) {
	while(s[0]!='\0' && s[strlen(s)-1]==0x20)
		s[strlen(s)-1]='\0';
}

static char *get_name(struct dentry *dentry) {
	char *name=malloc(8+1+3+1);

	if(!name)
		return NULL;
	memset(name,0,8+1+3+1);
	memcpy(name,dentry->basename,8);
	elide_trailing_spaces(name);
	if(dentry->ext[0]!=0x20) {
		strcat(name,".");
		memcpy(name+strlen(name),dentry->ext,3);
		elide_trailing_spaces(name);
	}
	return name;
}

static off_t get_size(struct dentry *dentry) {
	off_t size=0;
	uint8_t granule=dentry->starting_granule;

	for(;;) {
		if(fat[granule]<0xc0) {
			size+=2304;
			granule=fat[granule];
		} else {
			size+=(fat[granule]-0xc1)*256;
			size+=ntohs(dentry->last_sec_bytes);
			return size;
		}
	}
}

static void granule_to_sector(unsigned int granule, int *track, int *start_sector) {
	*start_sector=(granule&1)?10:1;
	*track=granule>>1;
	if(*track>16)
		(*track)++; /* granule numbers skip the directory track */
	if(granule>=1000)
		*track=17; /* special case to read the directory */
}

static int read_granule(struct phys *phys, unsigned char *buf, int granule) {
	int track, start_sector;
	struct sector_list granule0_read_order[]={{2,256},{6,1280},{3,512},{7,1536},{4,768},{8,1792},{1,0},{5,1024},{9,2048}};
	struct sector_list granule1_read_order[]={{10,0},{14,1024},{11,256},{15,1280},{12,512},{16,1536},{13,768},{17,1792},{18,2048}};
	struct sector_list *sector_list;
	int num_sectors=9;

	granule_to_sector(granule,&track,&start_sector);
	sector_list=(start_sector==1)?granule0_read_order:granule1_read_order;
	if(fc_SEEK_abs(phys->physical_track(phys,track))!=0)
		return 1;
	while(num_sectors--) {
		if(phys->read_sector(phys,buf+sector_list->offset,track,0,sector_list->sector)!=0)
			return 1;
		sector_list++;
	}
	return 0;
}

static int read_track17(struct phys *phys, unsigned char *buf) {
	if(read_granule(phys,buf,1000)!=0 || read_granule(phys,buf+2304,1001)!=0)
		return 1;
	return 0;
}

static int iterate_over_dentries(struct phys *phys, int (*cb)(void *, void *), void *cbdata) {
	uint8_t track17[4608];
	struct dentry *dentry=(struct dentry *)(track17+512);
	int dentries=72;
	int cbreturn;

	if(read_track17(phys,track17)!=0)
		return -1;
	fat=track17+256;

	while(dentries>0) {
		if(dentry->basename[0]==0) { /* unused dentry */
			dentry++; dentries--; continue;
		}
		if(dentry->basename[0]==0xff) /* end of directory */
			break;
		cbreturn=cb(cbdata,dentry);
		if(cbreturn!=0)
			return cbreturn;
		dentry++; dentries--;
	}
	return 0;
}

struct file_list_state {
	int file_count;
	int max_output;
	struct directory_entry *list;
	struct directory_entry *out;
};

static int file_list_cb(void *cbdata, void *stuff) {
	struct file_list_state *state=cbdata;
	struct dentry *dentry=stuff;
	struct directory_entry *e;
	char *name=get_name(dentry);

	if(!name)
		return -1;
	state->file_count++;
	if(state->file_count<=state->max_output) {
		e=state->out;
		e->name=name;
		e->mode=S_IRUSR|S_IRGRP|S_IROTH|S_IWUSR|S_IWGRP|S_IWOTH;
		e->mtime=0;
		e->size=get_size(dentry);
		state->out++;
	} else {
		free(name);
	}
	return 0;
}

static int file_list(struct phys *phys, struct log *log, struct directory_entry *out, int max_output, char *path) {
	struct file_list_state file_list_cbdata;

	file_list_cbdata.file_count=0;
	file_list_cbdata.max_output=max_output;
	file_list_cbdata.list=out;
	file_list_cbdata.out=out;
	if(iterate_over_dentries(phys,file_list_cb,&file_list_cbdata)!=0)
		return -1;
	return file_list_cbdata.file_count;
}

static int is_path_valid(struct phys *phys, struct log *log, char *path) {
	if(strcmp(path,"/")==0)
		return 1;
	return 0;
}

static int namecmp(struct dentry *dentry, char *wantname) {
	char *gotname=get_name(dentry);
	int result;

	if(!gotname)
		return -1;
	result=strcasecmp(gotname,wantname);
	free(gotname);
	return result;
}

struct file_size_state {
	int size;
	char *filename;
};

static int file_size_cb(void *cbdata, void *stuff) {
	struct file_size_state *state=cbdata;
	struct dentry *dentry=stuff;

	if(namecmp(dentry,state->filename)==0) {
		state->size=get_size(dentry);
		return 1;
	}
	return 0;
}

static int file_size(struct phys *phys, struct log *log, char *path) {
	struct file_size_state file_size_cbdata;

	while(*path=='/')
		path++;

	file_size_cbdata.size=-1;
	file_size_cbdata.filename=path;
	if(iterate_over_dentries(phys,file_size_cb,&file_size_cbdata)==1)
		return file_size_cbdata.size;
	return -1;
}

struct lookup_file_state {
	struct dentry *out;
	char *want;
};

static int lookup_file_cb(void *cbdata, void *stuff) {
	struct lookup_file_state *state=cbdata;
	struct dentry *dentry=stuff;

	if(namecmp(dentry,state->want)==0) {
		memcpy(state->out,dentry,sizeof(struct dentry));
		return 1;
	}
	return 0;
}

static int lookup_file(struct phys *phys, struct dentry *out, char *want) {
	struct lookup_file_state lookup_file_cbdata;

	lookup_file_cbdata.out=out;
	lookup_file_cbdata.want=want;
	if(iterate_over_dentries(phys,lookup_file_cb,&lookup_file_cbdata)==1)
		return 0;
	return 1;
}

static int read_file(struct phys *phys, struct log *log, unsigned char *out, char *path) {
	struct dentry dentry;
	uint8_t granule;
	uint8_t buf[2304];
	uint8_t fat_copy[256];
	int bytes_to_read;

	while(*path=='/')
		path++;

	if(lookup_file(phys,&dentry,path)!=0)
		return 1;
	if(read_granule(phys,buf,1000)!=0)
		return 1;
	memcpy(fat_copy,buf+256,256);
	fat=fat_copy;
	granule=dentry.starting_granule;
	for(;;) {
		if(fat[granule]<0xc0) {
			bytes_to_read=2304;
		} else {
			bytes_to_read=(fat[granule]-0xc1)*256;
			bytes_to_read+=ntohs(dentry.last_sec_bytes);
		}
		if(read_granule(phys,buf,granule)!=0)
			return 1;
		memcpy(out,buf,bytes_to_read);
		out+=bytes_to_read;
		if(fat[granule]<0xc0) {
			granule=fat[granule];
		} else {
			return 0;
		}
	}
}

struct log log_rsdos={
	.file_list = file_list,
	.is_path_valid = is_path_valid,
	.file_size = file_size,
	.read_file = read_file
};
