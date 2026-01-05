#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef WIN32
#include <winsock.h>
#else
#include <arpa/inet.h>
#endif
#include "phys.h"
#include "log.h"
#include "log_versa.h"

#ifdef WIN32
#define S_IRGRP 0
#define S_IWGRP 0
#define S_IROTH 0
#define S_IWOTH 0
#endif

static void psn_to_chs(struct phys *phys, unsigned int psn, int *c, int *h, int *s) {
	/* "psn" actually refers to 256-byte increments, regardless of the
	   actual physical sector size */
	*c=phys->min_track(phys); *h=phys->min_side(phys);
	while(psn*256>=phys->track_bytes(phys,*c,*h)) {
		psn-=phys->track_bytes(phys,*c,*h)/256;
		(*h)++;
		if(*h>phys->max_side(phys)) {
			(*c)++; *h=phys->min_side(phys);
		}
	}
	*s=(psn*256)/phys->sector_bytes(phys,*c,*h,phys->min_sector(phys,*c,*h))+phys->min_sector(phys,*c,*h);
}

static void advance_chs(struct phys *phys, int *c, int *h, int *s) {
	(*s)++;
	if(*s>phys->max_sector(phys,*c,*h)) {
		(*h)++;
		if(*h>phys->max_side(phys)) {
			(*c)++; *h=phys->min_side(phys);
		}
		*s=phys->min_sector(phys,*c,*h);
	}
}

static int read_psn(struct phys *phys, unsigned char *buf, int psn) {
	int c, h, s;
	int count=256;

	psn_to_chs(phys,psn,&c,&h,&s);
	while(count>0) {
		if(fc_SEEK_abs(phys->physical_track(phys,c))!=0)
			return 1;
		if(phys->read_sector(phys,buf,c,h,s)!=0)
			return 1;
		buf+=phys->sector_bytes(phys,c,h,s);
		count-=phys->sector_bytes(phys,c,h,s);
		advance_chs(phys,&c,&h,&s);
	}
	return 0;
}

static int read_block(struct phys *phys, unsigned char *buf, int psn, int l) {
	while(l>0) {
		if(read_psn(phys,buf,psn)!=0)
			return 1;
		buf+=256; psn++; l--;
	}
	return 0;
}

static void elide_trailing_spaces(char *s) {
	while(s[0]!='\0' && s[strlen(s)-1]==0x20)
		s[strlen(s)-1]='\0';
}

static int path_is_rootdir(char *path) {
	if(strcmp(path,"/")==0)
		return 1;
	return 0;
}

static int iterate_over_SDEs(struct phys *phys, int (*cb)(void *, struct sde *), void *cbdata) {
	struct vid vid;
	unsigned char sdb[256];
	struct sdbhdr *sdbhdr=(void *)sdb;
	struct sde *sde;
	int remaining_in_block, next_sdb;
	int cbreturn;

	if(read_psn(phys,(void *)&vid,0)!=0)
		return -1;
	if(strncmp((char *)vid.mac,"EXORMACS",8)!=0)
		return -1;
	next_sdb=ntohl(vid.sds);

	remaining_in_block=0;
	for(;;) {
		if(remaining_in_block==0) {
			if(next_sdb==0)
				return 0;
			if(read_psn(phys,sdb,next_sdb)!=0)
				return -1;
			next_sdb=ntohl(sdbhdr->fpt);
			remaining_in_block=15;
			sde=(void *)(sdb+sizeof(struct sdbhdr));
		}
		if(sde->pdp!=0) {
			cbreturn=cb(cbdata,sde);
			if(cbreturn!=0)
				return cbreturn;
		}
		sde++;
		remaining_in_block--;
	}

	return -1;
}

static char *get_sde_name(struct sde *sde) {
	char *name=malloc(4+1+8+1);

	if(!name)
		return NULL;
	memset(name,0,4+1+8+1);
	sprintf(name,"%04d.",ntohs(sde->usn));
	memcpy(name+strlen(name),sde->clg,8);
	elide_trailing_spaces(name);
	return name;
}

static int catalog_pdp_cb(void *cbdata, struct sde *sde) {
	char *want=cbdata;
	char *got=get_sde_name(sde);

	if(!got)
		return -1;
	if(strcasecmp(got,want)==0) {
		free(got);
		return ntohl(sde->pdp);
	}
	free(got);
	return 0;
}

static char *extract_dir(char *path) {
	char *rawdir, *normdir;
	int digits, prepend;

	while(*path=='/')
		path++;
	rawdir=strdup(path);
	if(strchr(rawdir,'/'))
		*(strchr(rawdir,'/'))='\0';
	if(!strchr(rawdir,'.'))
		return rawdir;
	digits=strchr(rawdir,'.')-rawdir;
	if(digits>=4)
		return rawdir;
	prepend=4-digits;
	normdir=malloc(prepend+strlen(rawdir)+1);
	normdir[0]='\0';
	strncat(normdir,"0000",prepend);
	strcat(normdir,rawdir);
	free(rawdir);
	return normdir;
}

static int catalog_pdp(struct phys *phys, char *path) {
	char *want=extract_dir(path);
	int pdp;

	if(!want)
		return -1;
	pdp=iterate_over_SDEs(phys,catalog_pdp_cb,want);
	free(want);
	return pdp;
}

struct file_list_state {
	int file_count;
	int max_output;
	struct directory_entry *out;
};

static int catalog_list_cb(void *cbdata, struct sde *sde) {
	struct file_list_state *state=cbdata;

	state->file_count++;
	if(state->file_count<=state->max_output) {
		state->out->name=get_sde_name(sde);
		if(!state->out->name)
			return -1;
		state->out->mode=S_IFDIR;
		state->out->size=0;
		state->out->mtime=0;
		state->out++;
	}
	return 0;
}

static int catalog_list(struct phys *phys, struct log *log, struct directory_entry *out, int max_output) {
	struct file_list_state catalog_list_cbdata;

	catalog_list_cbdata.file_count=0;
	catalog_list_cbdata.max_output=max_output;
	catalog_list_cbdata.out=out;
	if(iterate_over_SDEs(phys,catalog_list_cb,&catalog_list_cbdata)!=0)
		return -1;

	return catalog_list_cbdata.file_count;
}

static int iterate_over_PDEs(struct phys *phys, int pdp, int (*cb)(void *, struct pde *), void *cbdata) {
	unsigned char pdb[1024];
	struct pdbhdr *pdbhdr=(void *)pdb;
	struct pde *pde;
	int remaining_in_block, next_pdb;
	int cbreturn;

	remaining_in_block=0; next_pdb=pdp;
	for(;;) {
		if(remaining_in_block==0) {
			if(next_pdb==0)
				return 0;
			if(read_block(phys,pdb,next_pdb,4)!=0)
				return -1;
			next_pdb=ntohl(pdbhdr->fpt);
			remaining_in_block=20;
			pde=(void *)(pdb+sizeof(struct pdbhdr));
		}
		if(pde->fil[0]!='\0') {
			cbreturn=cb(cbdata,pde);
			if(cbreturn!=0)
				return cbreturn;
		}
		pde++;
		remaining_in_block--;
	}

	return -1;
}

static char *get_pde_name(struct pde *pde) {
	char *name=malloc(8+1+2+1);

	if(!name)
		return NULL;
	memset(name,0,8+1+2+1);
	memcpy(name,pde->fil,8);
	elide_trailing_spaces(name);
	if(pde->ext[0]!=0x20) {
		strcat(name,".");
		memcpy(name+strlen(name),pde->ext,2);
		elide_trailing_spaces(name);
	}
	return name;
}

static mode_t get_pde_mode(struct pde *pde) {
	/* VERSAdos Data Management Services and Program Loader 8ed p.13 */
	mode_t mode=0;

	if(pde->wcd==0) /* write unprotected */
		mode|=S_IWUSR|S_IWGRP|S_IWOTH;
	if(pde->rcd==0) /* read unprotected */
		mode|=S_IRUSR|S_IRGRP|S_IROTH;
	return mode;
}

static off_t get_pde_size(struct pde *pde) {
	switch(pde->att&0x0f) {
	case DATCON:
		/* VERSAdos System Facilities Reference Manual p. 232 */
		return (ntohl(pde->fe)+1)*256;
	case DATSEQ: case DATISK: case DATISD:
		if(pde->lrl!=0) {
			/* Fixed size records. Calculate and return the size of
			   all concatenated records. */
			return (ntohl(pde->eor)+1)*ntohs(pde->lrl);
		} else {
			/* Variable size records. Return the total number of
			   bytes used by all the Data Blocks of the file,
			   including zero padding. */
			return (ntohl(pde->eof)+1)*256;
		}
	default: /* bogus value */
		return -1;
	}
}

static time_t get_pde_mtime(struct pde *pde) {
	/* Directory entry contains number of days elapsed since
	   December 31, 1979. */
	struct tm tm;

	tm.tm_hour=12; tm.tm_min=0; tm.tm_sec=0;
	tm.tm_year=79; tm.tm_mon=11; tm.tm_mday=31;
	return ntohs(pde->dtec)*86400+mktime(&tm);
}

static int file_list_cb(void *cbdata, struct pde *pde) {
	struct file_list_state *state=cbdata;

	state->file_count++;
	if(state->file_count<=state->max_output) {
		state->out->name=get_pde_name(pde);
		if(!state->out->name)
			return -1;
		state->out->mode=get_pde_mode(pde);
		state->out->size=get_pde_size(pde);
		state->out->mtime=get_pde_mtime(pde);
		state->out++;
	}
	return 0;
}

static int file_list(struct phys *phys, struct log *log, struct directory_entry *out, int max_output, char *path) {
	struct file_list_state file_list_cbdata;
	int pdp=catalog_pdp(phys,path);

	if(pdp<=0)
		return -1;
	file_list_cbdata.file_count=0;
	file_list_cbdata.max_output=max_output;
	file_list_cbdata.out=out;
	if(iterate_over_PDEs(phys,pdp,file_list_cb,&file_list_cbdata)!=0)
		return -1;

	return file_list_cbdata.file_count;
}

struct lookup_file_state {
	struct pde *out;
	char *want;
};

static int lookup_file_cb(void *cbdata, struct pde *pde) {
	struct lookup_file_state *state=cbdata;
	char *got=get_pde_name(pde);

	if(!got)
		return -1;
	if(strcasecmp(got,state->want)==0) {
		memcpy(state->out,pde,sizeof(struct pde));
		free(got);
		return 1;
	}
	free(got);
	return 0;
}

static int lookup_file(struct phys *phys, struct pde *out, int pdp, char *want) {
	struct lookup_file_state lookup_file_cbdata;

	lookup_file_cbdata.out=out;
	lookup_file_cbdata.want=want;
	if(iterate_over_PDEs(phys,pdp,lookup_file_cb,&lookup_file_cbdata)==1)
		return 0;
	return 1;
}

static int lookup_file_fullpath(struct phys *phys, struct pde *out, char *path) {
	int pdp=catalog_pdp(phys,path);
	char *file;

	if(pdp<=0)
		return 1;
	if(!strrchr(path,'/'))
		return 1;
	file=strrchr(path,'/')+1;
	if(*file=='\0')
		return 1;
	return lookup_file(phys,out,pdp,file);
}

static int file_or_catalog_list(struct phys *phys, struct log *log, struct directory_entry *out, int max_output, char *path) {
	if(path_is_rootdir(path))
		return catalog_list(phys,log,out,max_output);
	return file_list(phys,log,out,max_output,path);
}

static int too_many_slashes(char *path) {
	if(path[0]=='/')
		path++;
	if(strchr(path,'/'))
		return 1;
	return 0;
}

static int is_path_valid(struct phys *phys, struct log *log, char *path) {
	if(path_is_rootdir(path))
		return 1;
	if(too_many_slashes(path))
		return 0;
	if(catalog_pdp(phys,path)>0)
		return 1;
	return 0;
}

static int file_size(struct phys *phys, struct log *log, char *path) {
	struct pde pde;

	if(lookup_file_fullpath(phys,&pde,path)!=0)
		return -1;
	return get_pde_size(&pde);
}

static int read_file_contiguous(struct phys *phys, struct log *log, unsigned char *out, struct pde *pde) {
	return read_block(phys,out,ntohl(pde->fs),ntohl(pde->fe)+1);
}

static int read_file_indirect(struct phys *phys, struct log *log, unsigned char *out, struct pde *pde) {
	uint8_t *fab, *db;
	struct fabseg *seg;
	struct fabhdr *fabhdr;
	int total_records_left=ntohl(pde->eor)+1;
	int lrl=ntohs(pde->lrl);
	int record_length=lrl;
	int sectors_per_block=pde->dat;
	int bytes_per_block=sectors_per_block*256;
	uint32_t next_fab=ntohl(pde->fs);
	int segments_left_in_fab=0;
	int count;
	unsigned char *record;

	fab=malloc(pde->fab*256);
	fabhdr=(void *)fab;
	if(!fab)
		return 1;
	db=malloc(pde->dat*256);
	if(!db) {
		free(fab);
		return 1;
	}

	/* max valid record length is 0xff00; pad output file with 0xffff for
	   files with variable length records */
	memset(out,0xff,get_pde_size(pde));

	while(total_records_left>0) {
		if(segments_left_in_fab==0) {
			if(next_fab==0) {
				free(db);
				free(fab);
				return 1;
			}
			if(read_block(phys,fab,next_fab,pde->fab)!=0) {
				free(db);
				free(fab);
				return 1;
			}
			seg=(void *)(fab+sizeof(struct fabhdr)+fabhdr->pky);
			segments_left_in_fab=((pde->fab*256)-sizeof(struct fabhdr)-fabhdr->pky)/(sizeof(struct fabseg)+pde->key);
			next_fab=ntohl(fabhdr->flk);
		}
		if(seg->psn==0) {
			segments_left_in_fab=0;
			continue;
		}
		if(read_block(phys,db,ntohl(seg->psn),seg->sgs)!=0) {
			free(db);
			free(fab);
			return 1;
		}
		/* If there is padding at the end of the data blocks, we
		   concentrate it all at the end of the output file */
		if(lrl!=0) { /* fixed length records */
			memcpy(out,db,record_length*htons(seg->rec));
			out+=record_length*htons(seg->rec);
		} else { /* variable length records */
			/* note records with and without keys are the same,
			   except the key is prepended to the record contents
			   when it exists */
			count=htons(seg->rec);
			record=db;
			while(count>0) {
				record_length=(record[0]*256)+record[1];
				memcpy(out,record,record_length+2);
				out+=record_length+2;
				record+=record_length+2;
				if(record_length&1) {
					record++; *out=0; out++;
				}
				count--;
			}
		}
		total_records_left-=htons(seg->rec);
		seg=(void *) (((char *)seg)+seg->key);
		seg++; segments_left_in_fab--;
	}
	free(db);
	free(fab);
	return 0;
}

static int read_file(struct phys *phys, struct log *log, unsigned char *out, char *path) {
	struct pde pde;

	if(lookup_file_fullpath(phys,&pde,path)!=0)
		return 1;
	switch(pde.att&0x0f) {
	case DATCON:
		return read_file_contiguous(phys,log,out,&pde);
	case DATSEQ: case DATISK: case DATISD:
		return read_file_indirect(phys,log,out,&pde);
	default: /* bogus value */
		return 1;
	}
	return 1;
}

struct log log_versa={
	.file_list = file_or_catalog_list,
	.is_path_valid = is_path_valid,
	.file_size = file_size,
	.read_file = read_file,
};
