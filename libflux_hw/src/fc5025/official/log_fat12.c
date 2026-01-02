#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#ifdef WIN32
#include <winsock.h>
#else
#include <arpa/inet.h>
#endif
#include "phys.h"
#include "log.h"
#include "log_fat12.h"

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

static int cached_fat_sector=-1;

static void lsn_to_chs(struct phys *phys, int lsn, int *c, int *h, int *s) {
	/* ecma-107:1995 p. 5 */
	*s=((lsn%(phys->num_sectors(phys,0,0)*phys->num_sides(phys)))%phys->num_sectors(phys,0,0))+1;
	*c=lsn/(phys->num_sectors(phys,0,0)*phys->num_sides(phys));
	*h=(lsn%(phys->num_sectors(phys,0,0)*phys->num_sides(phys)))/phys->num_sectors(phys,0,0);
}

static int read_lsn(struct phys *phys, unsigned char *buf, int lsn) {
	int c, h, s;

	lsn_to_chs(phys,lsn,&c,&h,&s);
	if(fc_SEEK_abs(phys->physical_track(phys,c))!=0)
		return 1;
	return phys->read_sector(phys,buf,c,h,s);
}

static int cluster_first_sector(struct bpb *bpb, int cluster) {
	/* fatgen103 p. 13-14 */
	int rootdir_secs, data_region_start, sector;

	rootdir_secs=(vtoh16(bpb->BPB_RootEntCnt)+15)/16;
	data_region_start=vtoh16(bpb->BPB_RsvdSecCnt)+(bpb->BPB_NumFATs*vtoh16(bpb->BPB_FATSz16))+rootdir_secs;
	sector=((cluster-2)*bpb->BPB_SecPerClus)+data_region_start;
	return sector;
}

static void flush_fat_cache(void) {
	cached_fat_sector=-1;
}

static int get_next_cluster(struct phys *phys, struct bpb *bpb, int prev_cluster) {
	/* fatgen103 p. 16-17 */
	static uint8_t fatdata[1024];
	uint16_t fatentry;
	int fatoffset=prev_cluster+(prev_cluster/2);
	int fatsector=vtoh16(bpb->BPB_RsvdSecCnt)+(fatoffset/vtoh16(bpb->BPB_BytsPerSec));
	int secoffset=fatoffset%vtoh16(bpb->BPB_BytsPerSec);

	if(cached_fat_sector!=fatsector && read_lsn(phys,fatdata,fatsector)!=0)
		return -1;
	cached_fat_sector=fatsector;
	if(secoffset==511) {
		if(read_lsn(phys,fatdata+512,fatsector+1)!=0)
			return -1;
	}
	fatentry=fatdata[secoffset]|(fatdata[secoffset+1]<<8);
	if(prev_cluster&1) {
		return fatentry>>4;
	} else {
		return fatentry&0xfff;
	}
}

static void elide_trailing_spaces(char *s) {
	while(s[0]!='\0' && s[strlen(s)-1]==0x20)
		s[strlen(s)-1]='\0';
}

static char *get_name(struct dentry *dentry) {
	/* fatgen103 p. 23-24 */
	char *name=malloc(8+1+3+1);

	if(!name)
		return NULL;
	memset(name,0,8+1+3+1);
	memcpy(name,dentry->DIR_Name,8);
	elide_trailing_spaces(name);
	if(dentry->DIR_Name[8]!=0x20) {
		strcat(name,".");
		memcpy(name+strlen(name),dentry->DIR_Name+8,3);
		elide_trailing_spaces(name);
	}
	if(name[0]==0x05)
		name[0]=0xe5;
	return name;
}

static mode_t get_mode(struct dentry *dentry) {
	mode_t mode=0;

	if(dentry->DIR_Attr&ATTR_DIRECTORY)
		mode|=S_IFDIR;
	if(!(dentry->DIR_Attr&ATTR_HIDDEN))
		mode|=S_IRUSR|S_IRGRP|S_IROTH;
	if(!(dentry->DIR_Attr&ATTR_READ_ONLY))
		mode|=S_IWUSR|S_IWGRP|S_IWOTH;

	return mode;
}

static off_t get_size(struct dentry *dentry) {
	return vtoh32(dentry->DIR_FileSize);
}

static time_t get_mtime(struct dentry *dentry) {
	/* fatgen103 p. 23, 25 */
	uint16_t time=vtoh16(dentry->DIR_WrtTime);
	uint16_t date=vtoh16(dentry->DIR_WrtDate);
	struct tm tm;
	time_t t;

	tm.tm_sec=(time&0x1f)*2;
	tm.tm_min=(time&0x7e0)>>5;
	tm.tm_hour=(time&0xf800)>>11;
	tm.tm_mday=date&0x1f;
	tm.tm_mon=((date&0x1e0)>>5)-1;
	tm.tm_year=((date&0xfe00)>>9)+80;
	tm.tm_isdst=0;

	t=mktime(&tm);
	if(localtime(&t)->tm_isdst)
		t-=3600;

	return t;
}

static void conjure_360k_bpb(uint8_t *out) {
	static uint8_t bpb_360k[]={
		0xeb, 0x34, 0x90, 0x4d, 0x53, 0x44, 0x4f, 0x53,
		0x33, 0x2e, 0x32, 0x00, 0x02, 0x02, 0x01, 0x00,
		0x02, 0x70, 0x00, 0xd0, 0x02, 0xfd, 0x02, 0x00,
		0x09, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	memcpy(out,bpb_360k,sizeof(bpb_360k));
}

static int bpb_valid(uint8_t *bpb) {
	if(bpb[510]==0x55 && bpb[511]==0xaa)
		return 1;
	return 0;
}

static int read_bpb(struct phys *phys, uint8_t *out) {
	if(read_lsn(phys,out,0)!=0)
		return -1; /* physical read error */

	if(bpb_valid(out))
		return 0;

	if(phys->num_sectors(phys,0,0)!=9)
		return -1; /* physical format ok, but no bpb, and not 360k */

	if(read_lsn(phys,out,1)!=0)
		return -1;

	if(out[0]==0xfd && out[1]==0xff && out[2]==0xff) {
		/* no bpb on disk, but 360k media type in fat */
		conjure_360k_bpb(out);
		return 0;
	}

	return -1;
}

static int iterate_over_dentries(struct phys *phys, int cluster, int (*cb)(void *, struct dentry *), void *cbdata) {
	uint8_t buf[512], bpbbuf[512];
	struct bpb *bpb=(void *)bpbbuf;
	struct dentry *dentry;
	int next_sector;
	int remaining_in_cluster;
	int max_entries_left;
	int remaining_in_block;
	int cbreturn;
	int rootdir=(cluster==0)?1:0;

	flush_fat_cache();

	if(read_bpb(phys,bpbbuf)!=0)
		return -1;

	if(rootdir) {
		next_sector=vtoh16(bpb->BPB_RsvdSecCnt)+(bpb->BPB_NumFATs * vtoh16(bpb->BPB_FATSz16)); /* fatgen103 p. 22 */
		max_entries_left=vtoh16(bpb->BPB_RootEntCnt);
	} else {
		next_sector=cluster_first_sector(bpb,cluster);
		remaining_in_cluster=bpb->BPB_SecPerClus-1;
		max_entries_left=65536;
	}

	remaining_in_block=0;
	while(max_entries_left>0) {
		if(remaining_in_block==0) {
			if(cluster>=0xff8) /* End Of Clusterchain */
				return 0;
			if(read_lsn(phys,buf,next_sector)!=0)
				return -1;
			if(rootdir || remaining_in_cluster>0) {
				next_sector++;
				remaining_in_cluster--;
			} else {
				cluster=get_next_cluster(phys,bpb,cluster);
				if(cluster<0)
					return -1;
				next_sector=cluster_first_sector(bpb,cluster);
				remaining_in_cluster=bpb->BPB_SecPerClus-1;
			}
			remaining_in_block=512/32;
			dentry=(void *)buf;
		}
		if(dentry->DIR_Name[0]==0)
			break; /* no more dentries */
		if(dentry->DIR_Name[0]!=0xe5 && !(dentry->DIR_Attr&ATTR_VOLUME_ID)) {
			cbreturn=cb(cbdata,dentry);
			if(cbreturn!=0)
				return cbreturn;
		}
		max_entries_left--;
		dentry++;
		remaining_in_block--;
	}
	return 0;
}

struct lookup_file_state {
	struct dentry *out;
	char *want;
};

static int lookup_file_cb(void *cbdata, struct dentry *dentry) {
	struct lookup_file_state *state=cbdata;
	char *got=get_name(dentry);

	if(!got)
		return -1;
	if(strcasecmp(got,state->want)==0) {
		memcpy(state->out,dentry,sizeof(struct dentry));
		free(got);
		return 1;
	}
	free(got);
	return 0;
}

static int lookup_file(struct phys *phys, struct dentry *out, int start_cluster, char *want) {
	struct lookup_file_state lookup_file_cbdata;

	lookup_file_cbdata.out=out;
	lookup_file_cbdata.want=want;
	if(iterate_over_dentries(phys,start_cluster,lookup_file_cb,&lookup_file_cbdata)==1)
		return 0;
	return 1;
}

static int directory_start_cluster(struct phys *phys, char *path_orig) {
	int start_cluster=0; /* special case value = root directory */
	struct dentry dentry;
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
		if(lookup_file(phys,&dentry,start_cluster,element)!=0) {
			free(path);
			return -1;
		}
		if(!S_ISDIR(get_mode(&dentry))) {
			free(path);
			return -1;
		}
		start_cluster=vtoh16(dentry.DIR_FstClusLO);
	}

	free(path);
	return start_cluster;
}

struct file_list_state {
	int file_count;
	int max_output;
	struct directory_entry *out;
};

static int file_list_cb(void *cbdata, struct dentry *dentry) {
	struct file_list_state *state=cbdata;

	state->file_count++;
	if(state->file_count<=state->max_output) {
		state->out->name=get_name(dentry);
		if(!state->out->name)
			return -1;
		state->out->mode=get_mode(dentry);
		state->out->size=get_size(dentry);
		state->out->mtime=get_mtime(dentry);
		state->out++;
	}
	return 0;
}

static int file_list(struct phys *phys, struct log *log, struct directory_entry *out, int max_output, char *path) {
	struct file_list_state file_list_cbdata;
	int cluster=directory_start_cluster(phys,path);

	if(cluster<0)
		return -1;
	file_list_cbdata.file_count=0;
	file_list_cbdata.max_output=max_output;
	file_list_cbdata.out=out;
	if(iterate_over_dentries(phys,cluster,file_list_cb,&file_list_cbdata)!=0)
		return -1;
	return file_list_cbdata.file_count;
}

static int is_path_valid(struct phys *phys, struct log *log, char *path) {
	if(directory_start_cluster(phys,path)>=0)
		return 1;
	return 0;
}

static int lookup_file_fullpath(struct phys *phys, struct dentry *out, char *path) {
	char *dir, *file;
	int dircluster;
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

	dircluster=directory_start_cluster(phys,dir);
	if(dircluster<0) {
		free(dir);
		return 1;
	}

	status=lookup_file(phys,out,dircluster,file);
	free(dir);
	return status;
}

static int file_size(struct phys *phys, struct log *log, char *path) {
	struct dentry dentry;

	if(lookup_file_fullpath(phys,&dentry,path)!=0)
		return -1;
	return get_size(&dentry);
}

static void get_cluster_sectors(struct bpb *bpb, int *sectors, int bytes, int cluster) {
	int sector=cluster_first_sector(bpb,cluster);

	while(bytes>0) {
		*sectors=sector;
		sectors++; sector++;
		bytes-=512;
	}
}

static int get_sector_list(struct phys *phys, struct log *log, struct bpb *bpb, int *sectors, int cluster, off_t bytes) {
	int bytes_to_read;
	int bytes_per_cluster=512*bpb->BPB_SecPerClus;

	flush_fat_cache();

	while(bytes>0) {
		bytes_to_read=(bytes>bytes_per_cluster)?bytes_per_cluster:bytes;
		get_cluster_sectors(bpb,sectors,bytes_to_read,cluster);
		bytes-=bytes_to_read;
		sectors+=(bytes_to_read+511)/512;
		if(bytes>0) {
			cluster=get_next_cluster(phys,bpb,cluster);
			if(cluster<0)
				return 1;
		}
	}
	return 0;
}

static int read_some_sectors(struct phys *phys, unsigned char *out, int *sectors, int bytes, int which_half) {
	uint8_t buf[512];
	int copy_bytes;

	while(bytes>0) {
		copy_bytes=(bytes>512)?512:bytes;
		if(((*sectors)&1)==which_half) {
			if(read_lsn(phys,buf,*sectors)!=0)
				return 1;
			memcpy(out,buf,copy_bytes);
		}
		sectors++;
		bytes-=copy_bytes;
		out+=copy_bytes;
	}
	return 0;
}

static int read_file(struct phys *phys, struct log *log, unsigned char *out, char *path) {
	uint8_t buf[512];
	struct bpb *bpb=(void *)buf;
	struct dentry dentry;
	off_t bytes;
	int sector_count;
	int *sectors;

	if(read_bpb(phys,buf)!=0)
		return 1;
	if(lookup_file_fullpath(phys,&dentry,path)!=0)
		return 1;
	bytes=get_size(&dentry);
	sector_count=(bytes+511)/512;
	sectors=malloc(sector_count*sizeof(int *));
	if(!sectors)
		return 1;
	if(get_sector_list(phys,log,bpb,sectors,vtoh16(dentry.DIR_FstClusLO),bytes)!=0) {
		free(sectors);
		return 1;
	}
	if(read_some_sectors(phys,out,sectors,bytes,0)!=0) {
		free(sectors);
		return 1;
	}
	if(read_some_sectors(phys,out,sectors,bytes,1)!=0) {
		free(sectors);
		return 1;
	}
	free(sectors);
	return 0;
}

struct log log_fat12={
	.file_list = file_list,
	.is_path_valid = is_path_valid,
	.file_size = file_size,
	.read_file = read_file
};
