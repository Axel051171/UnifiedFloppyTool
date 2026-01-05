#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#ifdef WIN32
#include <winsock.h>
#else
#include <arpa/inet.h>
#endif
#include "phys.h"
#include "log.h"
#include "log_prodos.h"

#ifdef WIN32
#define S_IRGRP 0
#define S_IWGRP 0
#define S_IROTH 0
#define S_IWOTH 0
#endif

#define swap32(x) (((((uint32_t)x) & 0xff000000) >> 24) | \
                  ((((uint32_t)x) & 0x00ff0000) >>  8) | \
                  ((((uint32_t)x) & 0x0000ff00) <<  8) | \
                  ((((uint32_t)x) & 0x000000ff) << 24))
#define swap16(x) (((((uint16_t)x)&0xff00)>>8) | \
                  ((((uint16_t)x)&0xff)<<8))
#define vtoh32(x) swap32(ntohl(x))
#define vtoh16(x) swap16(ntohs(x))

static void block_to_sector(unsigned int block, int *track, int *sec1, int *sec2) {
	/* Beneath Apple ProDOS p. 3-17
	   ProDOS 8 Technical Reference Manual p. 174 */
	/* These are not physical sector numbers, but fake sector numbers for
	   read_sector_prodos. See phys_apple16.c. */
	*track=block>>3; block&=7;
	*sec1=block*2; *sec2=*sec1+1;
}

static int read_block(struct phys *phys, unsigned char *buf, int block) {
	int track, sec1, sec2;
	int status=0;

	block_to_sector(block,&track,&sec1,&sec2);
	if(fc_SEEK_abs(phys->physical_track(phys,track))!=0)
		return 1;
	status|=phys->read_sector(phys,buf,track,0,sec1);
	status|=phys->read_sector(phys,buf+256,track,0,sec2);
	return status;
}

static char *get_name(struct file_entry *file_entry) {
	char *name;
	int name_length=file_entry->stnl&15;

	name=malloc(name_length+1);
	if(!name)
		return NULL;
	memcpy(name,file_entry->file_name,name_length);
	name[name_length]='\0';
	return name;
}

static mode_t get_mode(struct file_entry *file_entry) {
	mode_t mode=0;

	if((file_entry->stnl&0xf0)==0xd0)
		mode|=S_IFDIR;
	if(file_entry->access&1) /* read-enable */
		mode|=S_IRUSR|S_IRGRP|S_IROTH;
	if(file_entry->access&2) /* write-enable */
		mode|=S_IWUSR|S_IWGRP|S_IWOTH;

	return mode;
}

static off_t get_size(struct file_entry *file_entry) {
	return file_entry->eof_low+(vtoh16(file_entry->eof_high)<<8);
}

static time_t get_mtime(struct file_entry *file_entry) {
	/* Beneath Apple ProDOS p. 6-36
	   ProDOS 8 Technical Reference Manual p. 171 */
	uint32_t last_mod=vtoh32(file_entry->last_mod);
	struct tm tm;
	time_t t;

	tm.tm_sec=0;
	tm.tm_min=(last_mod&0xff0000)>>16;
	tm.tm_hour=(last_mod&0xff000000)>>24;
	tm.tm_mday=(last_mod&0x1f);
	tm.tm_mon=((last_mod&0x1e0)>>5)-1;
	tm.tm_year=((last_mod&0xfe00)>>9);
	if(tm.tm_year<40) /* ProDOS 8 Technical Note #28 */
		tm.tm_year+=100;
	tm.tm_isdst=0;

	t=mktime(&tm);
	if(localtime(&t)->tm_isdst)
		t-=3600;

	return t;
}

static int lookup_file(struct phys *phys, struct file_entry *out, int key_block, char *want) {
	char *got;
	unsigned char buf[512];
	struct directory_block *directory_block=(void *)buf;
	struct file_entry *file_entry;
	int entry_length, entries_per_block, remaining_in_block, file_count;
	int entries_processed=0;

	if(read_block(phys,buf,key_block)!=0)
		return 1;
	entry_length=directory_block->entry_length;
	entries_per_block=directory_block->entries_per_block;
	remaining_in_block=entries_per_block-1;
	file_count=vtoh16(directory_block->file_count);
	file_entry=(void *)(buf+entry_length+4);
	while(entries_processed<file_count) {
		if(remaining_in_block==0) {
			if(directory_block->next==0)
				return 1;
			if(read_block(phys,buf,vtoh16(directory_block->next))!=0)
				return 1;
			remaining_in_block=entries_per_block;
			file_entry=(void *)(buf+4);
		}
		if(file_entry->stnl!=0) {
			got=get_name(file_entry);
			if(!got)
				return 1;
			if(strcasecmp(got,want)==0) {
				memcpy(out,file_entry,sizeof(struct file_entry));
				free(got);
				return 0;
			}
			free(got);
			entries_processed++;
		}
		file_entry=(void *) (((char *)file_entry)+entry_length);
		remaining_in_block--;
	}
	return 1;
}

static int directory_key_block(struct phys *phys, char *path_orig) {
	int key_block=2; /* root directory */
	struct file_entry file_entry;
	char *path=strdup(path_orig);
	char *element, *p, *q=path;

	if(!path)
		return -1;

	while(*q!='\0') {
		if(*q=='/') {
			q++;
			continue;
		}
		element=q;
		p=strchr(q,'/');
		if(p) {
			*p='\0';
			q=p+1;
		} else {
			q+=strlen(q);
		}
		if(lookup_file(phys,&file_entry,key_block,element)!=0) {
			free(path);
			return -1;
		}
		if((file_entry.stnl&0xf0)!=0xd0) {
			free(path);
			return -1;
		}
		key_block=vtoh16(file_entry.key_pointer);
	}

	free(path);
	return key_block;
}

static int file_list(struct phys *phys, struct log *log, struct directory_entry *out, int max, char *path) {
	/* ProDOS 8 Technical Reference Manual p.157-9
	   Beneath Apple ProDOS ch. 4 */
	int key_block;
	unsigned char buf[512];
	struct directory_block *directory_block=(void *)buf;
	struct file_entry *file_entry;
	int entry_length, entries_per_block, file_count, remaining_in_block;
	int entries_processed=0;

	key_block=directory_key_block(phys,path);
	if(key_block<0)
		return -1;

	if(read_block(phys,buf,key_block)!=0)
		return -1;
	entry_length=directory_block->entry_length;
	entries_per_block=directory_block->entries_per_block;
	remaining_in_block=entries_per_block-1;
	file_count=vtoh16(directory_block->file_count);
	file_entry=(void *)(buf+entry_length+4);
	while(entries_processed<file_count && entries_processed<max) {
		if(remaining_in_block==0) {
			if(directory_block->next==0)
				return -1;
			if(read_block(phys,buf,vtoh16(directory_block->next))!=0)
				return -1;
			remaining_in_block=entries_per_block;
			file_entry=(void *)(buf+4);
		}
		if(file_entry->stnl!=0) {
			out->name=get_name(file_entry);
			if(!out->name) {
				/* ideally we whould deallocate the previous
				   entries first... */
				return -1;
			}
			out->mode=get_mode(file_entry);
			out->size=get_size(file_entry);
			out->mtime=get_mtime(file_entry);
			out++;
			entries_processed++;
		}
		file_entry=(void *) (((char *)file_entry)+entry_length);
		remaining_in_block--;
	}

	return file_count;
}

static int is_path_valid(struct phys *phys, struct log *log, char *path) {
	if(directory_key_block(phys,path)>0)
		return 1;
	return 0;
}

static int lookup_file_fullpath(struct phys *phys, struct file_entry *out, char *path) {
	char *dir, *file;
	int key_block;
	int status;

	dir=strdup(path);
	if(!dir)
		return 1;

	file=strrchr(dir,'/');
	if(!file) {
		free(dir);
		return 1;
	}
	*file='\0';
	file++;

	key_block=directory_key_block(phys,dir);
	if(key_block<0) {
		free(dir);
		return 1;
	}

	status=lookup_file(phys,out,key_block,file);
	free(dir);
	return status;
}

static int file_size(struct phys *phys, struct log *log, char *path) {
	struct file_entry file_entry;

	if(lookup_file_fullpath(phys,&file_entry,path)!=0)
		return -1;
	return get_size(&file_entry);
}

static int read_file_seedling(struct phys *phys, struct log *log, unsigned char *out, off_t size, int data_block) {
	unsigned char sector[512];

	if(read_block(phys,sector,data_block)!=0)
		return 1;
	memcpy(out,sector,size);
	return 0;
}

static int read_file_sapling(struct phys *phys, struct log *log, unsigned char *out, off_t bytes, int index_block) {
	/* Beneath Apple ProDOS p. 4-16, 4-22
	   ProDOS 8 Technical Reference Manual p. 166 */
	unsigned char index_pointers[512];
	unsigned char *index_pointer=index_pointers;
	unsigned char data[512];
	int data_block;
	int copy_bytes;

	if(read_block(phys,index_pointers,index_block)!=0)
		return 1;

	while(bytes>0) {
		data_block=index_pointer[0]+(index_pointer[256]<<8);
		index_pointer++;
		copy_bytes=(bytes>512)?512:bytes;
		if(data_block==0) { /* sparse block */
			memset(out,0,copy_bytes);
		} else {
			if(read_block(phys,data,data_block)!=0)
				return 1;
			memcpy(out,data,copy_bytes);
		}
		out+=copy_bytes;
		bytes-=copy_bytes;
	}

	return 0;
}

static int read_file_tree(struct phys *phys, struct log *log, unsigned char *out, off_t bytes, int master_block) {
	/* Beneath Apple ProDOS pp. 4-17,18,22 */
	unsigned char master_pointers[512];
	unsigned char *master_pointer=master_pointers;
	int index_block;
	int copy_bytes;

	if(read_block(phys,master_pointers,master_block)!=0)
		return 1;

	while(bytes>0) {
		index_block=master_pointer[0]+(master_pointer[256]<<8);
		master_pointer++;
		copy_bytes=(bytes>131072)?131072:bytes;
		if(index_block==0) {
			memset(out,0,copy_bytes);
		} else {
			if(read_file_sapling(phys,log,out,copy_bytes,index_block)!=0)
				return 1;
		}
		out+=copy_bytes;
		bytes-=copy_bytes;
	}

	return 0;
}

static int read_file(struct phys *phys, struct log *log, unsigned char *buf, char *path) {
	struct file_entry file_entry;
	off_t size;
	int key_pointer;

	if(lookup_file_fullpath(phys,&file_entry,path)!=0)
		return 1;
	size=get_size(&file_entry);
	key_pointer=vtoh16(file_entry.key_pointer);
	switch(file_entry.stnl&0xf0) {
	case 0x10:
		return read_file_seedling(phys,log,buf,size,key_pointer);
	case 0x20:
		return read_file_sapling(phys,log,buf,size,key_pointer);
	case 0x30:
		return read_file_tree(phys,log,buf,size,key_pointer);
	default:
		return 1;
	}
}

struct log log_applepro={
	.file_list = file_list,
	.is_path_valid = is_path_valid,
	.file_size = file_size,
	.read_file = read_file
};
