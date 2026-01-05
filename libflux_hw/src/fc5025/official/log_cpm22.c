#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "phys.h"
#include "log.h"
#include "log_cpm22.h"

#ifdef WIN32
#define S_IRGRP 0
#define S_IWGRP 0
#define S_IROTH 0
#define S_IWOTH 0
#endif

struct log log_cpm22_kp2;
struct log log_cpm22_kp4;
struct log log_cpm30_pmc;

static int reserved_tracks;
static int max_dentry;
static int extent_shift; /* extent field >> extent_shift == entry number */
static int records_high_mask; /* bits in extent field that get multiplied by 128 and added to record count field to give actual record count */
static int dentries_per_sector;
static int block_shift; /* Allocation Unit size = (1<<block_shift)*128 */
static int sectors_per_track, sectors_per_cylinder;
static int sectors_per_block;
static int max_extent_size; /* 16 * block size */

static int init_parameters(struct log *log) {
	if(&log_cpm22_kp2==&log_cpm22_kp4) /* sanity check */
		return 1;
	if(log==&log_cpm22_kp2) {
		sectors_per_track=10;
		sectors_per_cylinder=10;
		sectors_per_block=2;
		block_shift=3;
		max_extent_size=16384;
		extent_shift=0;
		records_high_mask=0;
		max_dentry=63;
		dentries_per_sector=512/32;
		reserved_tracks=1;
		return 0;
	} else if(log==&log_cpm22_kp4) {
		sectors_per_track=10;
		sectors_per_cylinder=20;
		sectors_per_block=4;
		block_shift=4;
		max_extent_size=32768;
		extent_shift=1;
		records_high_mask=(~(0xff<<1))&0xff;
		max_dentry=63;
		dentries_per_sector=512/32;
		reserved_tracks=1;
		return 0;
	} else if(log==&log_cpm30_pmc) {
		sectors_per_track=5;
		sectors_per_cylinder=10;
		sectors_per_block=2;
		block_shift=4;
		max_extent_size=32768;
		extent_shift=1;
		records_high_mask=(~(0xff<<1))&0xff;
		max_dentry=127;
		dentries_per_sector=1024/32;
		reserved_tracks=2;
		return 0;
	} else {
		return 1;
	}
}

static void lba_to_chs(struct phys *phys, int lba, int *c, int *h, int *s) {
	*c=(lba/sectors_per_cylinder)+phys->min_track(phys);
	lba%=sectors_per_cylinder;
	*h=(lba/sectors_per_track)+phys->min_side(phys);
	lba%=sectors_per_track;
	*s=lba+phys->min_sector(phys,*c,*h);
}

static int read_lba(struct phys *phys, unsigned char *out, int lba) {
	int c, h, s;

	lba_to_chs(phys,lba,&c,&h,&s);
	if(fc_SEEK_abs(phys->physical_track(phys,c))!=0)
		return 1;
	return phys->read_sector(phys,out,c,h,s);
}

static int read_block(struct phys *phys, unsigned char *out, int blocknum, int bytes) {
	int sector_size=phys->sector_bytes(phys,phys->min_track(phys),phys->min_side(phys),phys->min_sector(phys,phys->min_track(phys),phys->min_side(phys)));
	unsigned char secbuf[1024];
	int lba=(blocknum*sectors_per_block)+(reserved_tracks*sectors_per_track);

	if(sector_size>1024)
		return 1;

	while(bytes>0) {
		if(read_lba(phys,secbuf,lba)!=0)
			return 1;
		memcpy(out,secbuf,(bytes>sector_size)?sector_size:bytes);
		bytes-=sector_size;
		lba++;
		out+=sector_size;
	}
	return 0;
}

static int iterate_over_dentries(struct phys *phys, int (*cb)(void *, void *), void *cbdata) {
	int this_dentry=0;
	int remaining_in_sector=0;
	int track=reserved_tracks/phys->num_sides(phys);
	int side=phys->min_side(phys)+(reserved_tracks&(phys->num_sides(phys)-1));
	int sector=phys->min_sector(phys,track,side);
	int sector_size=phys->sector_bytes(phys,track,side,sector);
	uint8_t buf[1024];
	struct dentry *dentry;
	int cbreturn;

	if(sector_size>1024)
		return -1;

	if(fc_SEEK_abs(phys->physical_track(phys,track))!=0)
		return -1;

	while(this_dentry<=max_dentry) {
		if(remaining_in_sector==0) {
			if(phys->read_sector(phys,buf,track,side,sector)!=0)
				return -1;
			sector++;
			remaining_in_sector=dentries_per_sector;
			dentry=(void *)buf;
		}
		if(dentry->usernum<16) {
			cbreturn=cb(cbdata,dentry);
			if(cbreturn!=0)
				return cbreturn;
		}
		dentry++;
		this_dentry++;
		remaining_in_sector--;
	}
	return 0;
}

static mode_t get_mode(struct dentry *dentry) {
	mode_t mode=0;

	if(!(dentry->ext[0]&0x80)) /* read-only bit */
		mode|=S_IWUSR|S_IWGRP|S_IWOTH;
	if(!(dentry->ext[1]&0x80)) /* system bit */
		mode|=S_IRUSR|S_IRGRP|S_IROTH;

	return mode;
}

static void elide_trailing_spaces(char *s) {
	while(s[0]!='\0' && s[strlen(s)-1]==0x20)
		s[strlen(s)-1]='\0';
}

static void clear_high_bits(char *p, int len) {
	while(len--) {
		*p&=0x7f;
		p++;
	}
}

static char *get_name(struct dentry *dentry) {
	char *name=malloc(3+8+1+3+1);
	char *p=name;

	if(!name)
		return NULL;
	memset(name,0,3+8+1+3+1);
	if(dentry->usernum!=0) {
		sprintf(name,"%02d,",dentry->usernum);
		p+=3;
	}
	memcpy(p,dentry->basename,8);
	clear_high_bits(p,8);
	elide_trailing_spaces(p);
	if((dentry->ext[0]&0x7f)!=0x20) {
		strcat(p,".");
		p+=strlen(p);
		memcpy(p,dentry->ext,3);
		clear_high_bits(p,3);
		elide_trailing_spaces(p);
	}
	return name;
}

static unsigned int get_extent(struct dentry *dentry) {
	return ((dentry->s2&0x3f)<<5)+(dentry->extent&0x1f);
}

static unsigned int get_entry_num(struct dentry *dentry) {
	return get_extent(dentry)>>extent_shift;
}

static unsigned int get_record_count(struct dentry *dentry) {
	return (dentry->records)+((get_extent(dentry)&records_high_mask)<<7);
}

static unsigned int get_extent_size(struct dentry *dentry) {
	return get_record_count(dentry)*128;
}

struct file_list_state {
	int file_count;
	int max_output;
	struct directory_entry *list;
	struct directory_entry *out;
};

static struct directory_entry *find_existing_entry(struct file_list_state *state, char *filename) {
	struct directory_entry *e=state->list;

	while(e!=state->out) {
		if(strcasecmp(e->name,filename)==0)
			return e;
		e++;
	}
	return NULL;
}

static int file_list_cb(void *cbdata, void *stuff) {
	struct file_list_state *state=cbdata;
	struct dentry *dentry=stuff;
	struct directory_entry *e;
	int size=get_extent_size(dentry)+(get_entry_num(dentry)*max_extent_size);
	char *name=get_name(dentry);

	if(!name)
		return -1;
	state->file_count++;
	e=find_existing_entry(state,name);
	if(e!=NULL) {
		free(name);
		state->file_count--;
		if(size>e->size)
			e->size=size;
	} else if(state->file_count<=state->max_output) {
		e=state->out;
		e->name=name;
		e->mode=get_mode(dentry);
		e->mtime=0;
		e->size=size;
		state->out++;
	} else {
		free(name);
	}

	return 0;
}

static int file_list(struct phys *phys, struct log *log, struct directory_entry *out, int max_output, char *path) {
	struct file_list_state file_list_cbdata;

	if(init_parameters(log)!=0)
		return -1;

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
	int size;

	if(namecmp(dentry,state->filename)==0) {
		size=get_extent_size(dentry)+(get_entry_num(dentry)*max_extent_size);
		if(size>state->size)
			state->size=size;
	}
	return 0;
}

static int file_size(struct phys *phys, struct log *log, char *path) {
	struct file_size_state file_size_cbdata;

	if(init_parameters(log)!=0)
		return -1;

	while(*path=='/')
		path++;

	file_size_cbdata.size=-1;
	file_size_cbdata.filename=path;
	if(iterate_over_dentries(phys,file_size_cb,&file_size_cbdata)==0)
		return file_size_cbdata.size;

	return -1;
}

struct read_file_state {
	struct phys *phys;
	unsigned char *out;
	char *filename;
	int status;
};

static int read_one_extent(struct phys *phys, unsigned char *out, struct dentry *dentry) {
	const int block_size=(1<<block_shift)*128;
	int record_count=get_record_count(dentry);
	int i=0;

	while(record_count>0) {
		if(dentry->blocks[i]!=0 && read_block(phys,out,dentry->blocks[i],(record_count>16)?2048:(record_count*128))!=0)
			return 1;
		out+=block_size;
		record_count-=1<<block_shift;
		i++;
	}

	if(fc_SEEK_abs(phys->physical_track(phys,reserved_tracks/phys->num_sides(phys)))!=0) /* return to directory track */
		return -1;

	return 0;
}

static int read_file_cb(void *cbdata, void *stuff) {
	struct read_file_state *state=cbdata;
	struct dentry *dentry=stuff;

	if(namecmp(dentry,state->filename)==0) {
		if(read_one_extent(state->phys,state->out+(get_entry_num(dentry)*max_extent_size),dentry)!=0)
			return -1;
		state->status=1;
	}
	return 0;
}

static int read_file(struct phys *phys, struct log *log, unsigned char *buf, char *path) {
	struct read_file_state read_file_cbdata;
	int size=file_size(phys,log,path);

	while(*path=='/')
		path++;

	if(size<0)
		return 1;

	memset(buf,0,size);

	read_file_cbdata.phys=phys;
	read_file_cbdata.out=buf;
	read_file_cbdata.filename=path;
	read_file_cbdata.status=0;
	if(iterate_over_dentries(phys,read_file_cb,&read_file_cbdata)==0 && read_file_cbdata.status==1)
		return 0;

	return 1;
}

struct log log_cpm22_kp2={
	.file_list = file_list,
	.is_path_valid = is_path_valid,
	.file_size = file_size,
	.read_file = read_file
};

struct log log_cpm22_kp4={
	.file_list = file_list,
	.is_path_valid = is_path_valid,
	.file_size = file_size,
	.read_file = read_file
};

struct log log_cpm30_pmc={
	.file_list = file_list,
	.is_path_valid = is_path_valid,
	.file_size = file_size,
	.read_file = read_file
};
