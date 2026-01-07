/*
//
// Copyright (C) 2006-2025 Jean-François DEL NERO
//
//
// that this copyright statement is not removed from the file and that any
// derivative work contains the original copyright notice and the associated
// disclaimer.
//
// and/or modify  it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//   See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
//
*/
///////////////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------------//
//-------------------------------------------------------------------------------//
//-----------H----H--X----X-----CCCCC----22222----0000-----0000------11----------//
//----------H----H----X-X-----C--------------2---0----0---0----0--1--1-----------//
//---------HHHHHH-----X------C----------22222---0----0---0----0-----1------------//
//--------H----H----X--X----C----------2-------0----0---0----0-----1-------------//
//-------H----H---X-----X---CCCCC-----222222----0000-----0000----1111------------//
//-------------------------------------------------------------------------------//
//----------------------------------------------------- http://hxc2001.free.fr --//
///////////////////////////////////////////////////////////////////////////////////
// File : fs_amigados.c
// Contains: AmigaDos File system manager functions
//
// Written by: Jean-François DEL NERO
//
// Change History (most recent first):
///////////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "types.h"

#include "libflux.h""
#include "tracks/track_generator.h"
#include "sector_search.h"
#include "fdc_ctrl.h"
#include "fs_manager/fs_manager.h"
#include "libflux.h""

#include "libhxcadaptor.h"
#include "uft_floppy_loader.h"
#include "uft_floppy_utils.h"

#include "thirdpartylibs/adflib/Lib/adflib.h"
#include "thirdpartylibs/adflib/Lib/adf_str.h"
#include "thirdpartylibs/adflib/Lib/adf_nativ.h"
#include "thirdpartylibs/adflib/Lib/adf_err.h"
#include "thirdpartylibs/adflib/Lib/adf_dir.h"

static LIBFLUX_CTX* flux_ctx;
static LIBFLUX_FSMNG * gb_fsmng;

//struct Device * adfdevice;
//struct Volume * adfvolume;
extern struct Env adfEnv;


static void lba2chs(LIBFLUX_FSMNG * fsmng,int32_t lba, int32_t *track, int32_t *head, int32_t *sector)
{
	if(fsmng)
	{
		if(track)
		{
			if(fsmng->sectorpertrack &&  fsmng->sidepertrack)
			{
				*track = lba / (fsmng->sectorpertrack * fsmng->sidepertrack);
			}
			else
			{
				*track = 0;
			}
		}

		if(head)
		{
			if(fsmng->sectorpertrack && fsmng->sidepertrack)
			{
				*head = (lba / fsmng->sectorpertrack) % fsmng->sidepertrack;
			}
			else
			{
				*head = 0;
			}
		}

		if(sector)
		{
			if(fsmng->sectorpertrack)
			{
				*sector = (lba % fsmng->sectorpertrack);
			}
			else
			{
				*sector = 0;
			}
		}
	}
}

/*
 * myInitDevice
 *
 * must fill 'dev->size'
 */
RETCODE HxCADFLibInitDevice(struct Device* dev, char* name,BOOL ro)
{
	struct nativeDevice* nDev;

	nDev = (struct nativeDevice*)dev->nativeDev;

	nDev = (struct nativeDevice*)malloc(sizeof(struct nativeDevice));
	if (!nDev) {
		(*adfEnv.eFct)("myInitDevice : malloc");
		return RC_ERROR;
	}
	dev->nativeDev = nDev;
	if (!ro)
		/* check if device is writable, if not, force readOnly to TRUE */
		dev->readOnly = FALSE;
	else
		/* mount device as read only */
		dev->readOnly = TRUE;

	dev->size = gb_fsmng->trackperdisk *
				gb_fsmng->sidepertrack *
				gb_fsmng->sectorpertrack *
				gb_fsmng->sectorsize;


	return RC_OK;
}

RETCODE HxCADFLibReadSector(struct Device *dev, int32_t sector, int32_t size, uint8_t* buf)
{
	uint32_t i,c,sector_count;
	int32_t fp_track,fp_head,fp_sector,fdcstatus;
	unsigned char tmpSectBuf[512];
	int32_t remainSize;

	sector_count = size / gb_fsmng->sectorsize;
	if(size&0x1FF) sector_count++;

	remainSize = size;

	lba2chs(gb_fsmng,sector, &fp_track,&fp_head,&fp_sector);

	gb_fsmng->ctx->libflux_printf(MSG_DEBUG,"HxCADFLibReadSector : media_read, sector: 0x%.8X, sector count : %d, Track: %d, Side: %d, Sector: %d",sector,sector_count,fp_track,fp_head,fp_sector);
	c=0;
	for(i=0;i<sector_count;i++)
	{
		lba2chs(gb_fsmng,sector + i, &fp_track,&fp_head,&fp_sector);
		if(libflux_readSectorFDC (gb_fsmng->fdc,(unsigned char)fp_track,(unsigned char)fp_head,(unsigned char)fp_sector,gb_fsmng->sectorsize,AMIGA_MFM_ENCODING,1,(unsigned char*)&tmpSectBuf,gb_fsmng->sectorsize,&fdcstatus) == 1)
		{
			if(!fdcstatus)
			{
				if( remainSize >= gb_fsmng->sectorsize )
				{
					memcpy(&buf[i*gb_fsmng->sectorsize],tmpSectBuf,gb_fsmng->sectorsize);
					remainSize -= gb_fsmng->sectorsize;
				}
				else
				{
					memcpy(&buf[i*gb_fsmng->sectorsize],tmpSectBuf,remainSize);
					remainSize = 0;
				}
				c++;
			}
			else
			{
				gb_fsmng->ctx->libflux_printf(MSG_DEBUG,"HxCADFLibReadSector : media_read !!! ERROR !!!, sector: 0x%.8X, sector count : %d, Track: %d, Side: %d, Sector: %d, FDC Status : 0x%.2x",sector,sector_count,fp_track,fp_head,fp_sector,fdcstatus);
			}
		}
	}

	if( c == sector_count )
		return RC_OK;
	else
		return 0;
}

RETCODE HxCADFLibWriteSector(struct Device *dev, int32_t sector, int32_t size, unsigned char* buf)
{
	uint32_t i,c,sector_count;
	int32_t fp_track,fp_head,fp_sector,fdcstatus;
	unsigned char tmpSectBuf[512];
	int32_t remainSize;

	sector_count = size / gb_fsmng->sectorsize;
	if(size&0x1FF) sector_count++;

	remainSize = size;

	lba2chs(gb_fsmng,sector, &fp_track,&fp_head,&fp_sector);

	gb_fsmng->ctx->libflux_printf(MSG_DEBUG,"HxCADFLibWriteSector : media_write, sector: 0x%.8X, sector count : %d, Track: %d, Side: %d, Sector: %d",sector,sector_count,fp_track,fp_head,fp_sector);
	c = 0;
	for(i=0;i<sector_count;i++)
	{
		lba2chs(gb_fsmng,sector + i, &fp_track,&fp_head,&fp_sector);

		if(remainSize>=512)
		{
			memcpy(tmpSectBuf,&buf[i*gb_fsmng->sectorsize],512);
		}
		else
		{
			libflux_readSectorFDC (gb_fsmng->fdc,(unsigned char)fp_track,(unsigned char)fp_head,(unsigned char)fp_sector,gb_fsmng->sectorsize,AMIGA_MFM_ENCODING,1,(unsigned char*)&tmpSectBuf,gb_fsmng->sectorsize,&fdcstatus);
			memcpy(tmpSectBuf,&buf[i*gb_fsmng->sectorsize],remainSize);
		}

		if(libflux_writeSectorFDC (gb_fsmng->fdc,(unsigned char)fp_track,(unsigned char)fp_head,(unsigned char)fp_sector,gb_fsmng->sectorsize,AMIGA_MFM_ENCODING,1,(unsigned char*)&tmpSectBuf,gb_fsmng->sectorsize,&fdcstatus) == 1)
		{
			if(!fdcstatus)
			{
				c++;
			}
			else
			{
				gb_fsmng->ctx->libflux_printf(MSG_DEBUG,"HxCADFLibWriteSector : media_write  !!! ERROR !!!, sector: 0x%.8X, sector count : %d, Track: %d, Side: %d, Sector: %d",sector,sector_count,fp_track,fp_head,fp_sector,fdcstatus);
			}
		}
	}

	if( c == sector_count )
		return RC_OK;
	else
		return 0;
}

RETCODE HxCADFLibReleaseDevice(struct Device *dev)
{
	struct nativeDevice* nDev;

	nDev = (struct nativeDevice*)dev->nativeDev;

	free(nDev);

	return RC_OK;
}

BOOL HxCADFLibIsDevNative(char *devName)
{
	if(!strcmp("HXCDOSDISKBROWSER",devName))
	{
		return 1;
	}
	return 0;
}

void HxCadfInitNativeFct()
{
	struct nativeFunctions *nFct;

	nFct = (struct nativeFunctions*)adfEnv.nativeFct;

	nFct->adfInitDevice = HxCADFLibInitDevice ;
	nFct->adfNativeReadSector = HxCADFLibReadSector ;
	nFct->adfNativeWriteSector = HxCADFLibWriteSector ;
	nFct->adfReleaseDevice = HxCADFLibReleaseDevice ;
	nFct->adfIsDevNative = HxCADFLibIsDevNative;
}


static void adlib_printerror(char * msg)
{
	flux_ctx->libflux_printf(MSG_ERROR,"AdfLib Error: %s",msg);
}

static void adlib_printwarning(char * msg)
{
	flux_ctx->libflux_printf(MSG_WARNING,"AdfLib Warning: %s",msg);
}

static void adlib_printdebug(char * msg)
{
	flux_ctx->libflux_printf(MSG_DEBUG,"AdfLib Debug: %s",msg);
}

void init_amigados(LIBFLUX_FSMNG * fsmng)
{
	gb_fsmng = fsmng;
	flux_ctx = fsmng->ctx;
}

static int32_t changedir(LIBFLUX_FSMNG * fsmng,char * path,SECTNUM * curdir,int32_t dir)
{
	int32_t i,ret;
	char tmppath[512];
	char * tmpptr;
	struct bEntryBlock entry;
	struct Volume * adfvolume;

	adfvolume = (struct Volume *)fsmng->volume;

	ret = 1;

	if(strlen(path) && adfvolume)
	{

		i = 0;
		if(path[i]=='/')
		{
			ret = adfToRootDir(adfvolume);
			i++;
		}

		while(path[i])
		{
			tmpptr = strchr(&path[i],'/');
			memset(tmppath,0,sizeof(tmppath));
			if(tmpptr)
			{
				strncpy(tmppath,&path[i],tmpptr-&path[i]);
				i = i + (tmpptr-&path[i]) + 1;
			}
			else
			{
				strncpy(tmppath, &path[i], sizeof(tmppath) - 1); tmppath[sizeof(tmppath) - 1] = '\0';
				i = i + strlen(&path[i]);
			}

			if(strlen(tmppath))
				ret = adfChangeDir(adfvolume,tmppath);
			else
				break;
		}
	}

	if(!ret)
	{
		ret = adfReadEntryBlock(adfvolume,adfvolume->curDirPtr,&entry);
		if(!ret)
		{
			if(dir)
			{
				if(entry.secType == ST_DIR || entry.secType == ST_ROOT)
				{
					if(curdir)
						*curdir = adfvolume->curDirPtr;

					return 0;
				}
				else
					return 1;
			}
			else
			{
				if(entry.secType == ST_FILE || entry.secType == ST_LFILE)
				{
					if(curdir)
						*curdir = adfvolume->curDirPtr;

					return 0;
				}
				else
					return 1;
			}
		}

	}
	return ret;
}

int32_t amigados_mountImage(LIBFLUX_FSMNG * fsmng, LIBFLUX_FLOPPY *floppy)
{
	unsigned char sectorbuffer[512];
	int32_t nbsector,nbtrack,fdcstatus,badsectorfound;
	struct Volume * adfvolume;
	struct Device * adfdevice;

	badsectorfound = 0;

	if(!floppy || !fsmng)
		return LIBFLUX_BADPARAMETER;

	adfEnvInitDefault();
	HxCadfInitNativeFct();
	adfChgEnvProp(PR_EFCT,adlib_printerror);
	adfChgEnvProp(PR_WFCT,adlib_printwarning);
	adfChgEnvProp(PR_VFCT,adlib_printdebug);

	fsmng->fp = floppy;

	if( fsmng->fp->floppyNumberOfTrack > 85 )
		fsmng->trackperdisk = 85;
	else
		fsmng->trackperdisk = fsmng->fp->floppyNumberOfTrack;
	fsmng->sectorpertrack = 11;
	fsmng->sidepertrack = fsmng->fp->floppyNumberOfSide;
	fsmng->sectorsize = 512;

	memset(fsmng->dirhandletable,0xFF,sizeof(fsmng->dirhandletable));
	memset(fsmng->handletable,0xFF,sizeof(fsmng->handletable));

	if(fsmng->fdc)
		libflux_deinitFDC (fsmng->fdc);

	fsmng->fdc = libflux_initFDC (fsmng->ctx);
	if(fsmng->fdc)
	{
		if(libflux_insertDiskFDC (fsmng->fdc,floppy) == LIBFLUX_NOERROR)
		{
			// Count the number of sector
			nbsector = 0;
			while(libflux_readSectorFDC(fsmng->fdc,40,0,(unsigned char)(nbsector),512,AMIGA_MFM_ENCODING,1,(unsigned char*)sectorbuffer,sizeof(sectorbuffer),&fdcstatus))
			{
				if(fdcstatus == FDC_BAD_DATA_CRC)
					badsectorfound++;
				nbsector++;
			}

			if(nbsector)
			{
				nbtrack = 0;
				while(libflux_readSectorFDC(fsmng->fdc,(uint8_t)nbtrack,0,(uint8_t)(nbsector - 1),512,AMIGA_MFM_ENCODING,1,(unsigned char*)sectorbuffer,sizeof(sectorbuffer),&fdcstatus))
				{
					nbtrack++;
				}
				fsmng->trackperdisk = nbtrack;
			}

			fsmng->sidepertrack = 2;
			fsmng->sectorpertrack = nbsector;

			if(fsmng->sectorpertrack && !badsectorfound)
			{
				fsmng->ctx->libflux_printf(MSG_DEBUG,"AMIGADOSFS : %d Sectors per track (%d Bytes per sector)",fsmng->sectorpertrack,fsmng->sectorsize);

				fsmng->device = (void*)adfMountDev("HXCDOSDISKBROWSER",0);

				adfdevice = (struct Device *)fsmng->device;
				if(adfdevice)
				{
					fsmng->volume = (void*)adfMount(adfdevice, 0, 0);
					adfvolume = (struct Volume *)fsmng->volume;
					if(adfvolume)
					{
						flux_ctx->libflux_printf(MSG_DEBUG,"adfMount ok");
						return LIBFLUX_NOERROR;
					}
				}
			}
		}
	}

	return LIBFLUX_INTERNALERROR;
}

int32_t amigados_umountImage(LIBFLUX_FSMNG * fsmng)
{
	if(fsmng->fdc)
	{
		libflux_deinitFDC (fsmng->fdc);
		fsmng->fdc = 0;
	}

	return LIBFLUX_NOERROR;
}

int32_t amigados_getFreeSpace(LIBFLUX_FSMNG * fsmng)
{
	struct Volume * adfvolume;

	adfvolume = (struct Volume *)fsmng->volume;
	if(adfvolume)
		return adfCountFreeBlocks(adfvolume) * 512;
	else
		return LIBFLUX_ACCESSERROR;
}

int32_t amigados_getTotalSpace(LIBFLUX_FSMNG * fsmng)
{
	struct Device * adfdevice;

	adfdevice = (struct Device *)fsmng->device;
	if(adfdevice)
		return adfdevice->size;
	else
		return LIBFLUX_ACCESSERROR;

}

int32_t amigados_openDir(LIBFLUX_FSMNG * fsmng, char * path)
{
	int32_t i;
	SECTNUM snum;
	intptr_t tmp_int;

	if( changedir(fsmng,path,&snum,1) ==  RC_OK)
	{
		i = 0;
		while(fsmng->dirhandletable[i]!=(void*)-1 && i<128)
		{
			i++;
		}

		if(i == 128)
		{
			return LIBFLUX_ACCESSERROR;
		}

		tmp_int = snum;
		fsmng->dirhandletable[i] = (void*)tmp_int;

		fsmng->dirindex[i] = 0;

		return i+1;
	}

	return LIBFLUX_ACCESSERROR;
}

int32_t amigados_readDir(LIBFLUX_FSMNG * fsmng,int32_t dirhandle,LIBFLUX_FSENTRY * dirent)
{
	struct List *list, *cell;
	struct Entry *entry;
	int32_t i;
	struct Volume * adfvolume;
	intptr_t tmp_int;

	adfvolume = (struct Volume *)fsmng->volume;

	if(dirhandle<128 && adfvolume)
	{
		if(fsmng->dirhandletable[dirhandle-1]!=(void*)-1)
		{
			/* saves the head of the list */
			tmp_int = (intptr_t)fsmng->dirhandletable[dirhandle-1];
			cell = list = adfGetDirEnt(adfvolume,(int32_t)tmp_int);

			i=0;
			/* while cell->next is NULL, the last cell */
			while(cell && ( i <= fsmng->dirindex[dirhandle-1] ) )
			{
				entry = (struct Entry*)cell->content;

				strncpy(dirent->entryname, entry->name, sizeof(dirent->entryname) - 1); dirent->entryname[sizeof(dirent->entryname) - 1] = '\0';
				dirent->size = entry->size;

				dirent->isdir = 0;
				if(entry->type == ST_DIR)
					dirent->isdir = 1;

				dirent->flags = 0;

				cell = cell->next;
				i++;
			}


			/* frees the list and the content */

			fsmng->dirindex[dirhandle-1]++;

			adfFreeDirList(list);

			if(i==fsmng->dirindex[dirhandle-1])
			{
				return LIBFLUX_VALIDFILE;
			}

			return LIBFLUX_NOERROR;
		}
	}

	return LIBFLUX_ACCESSERROR;
}

int32_t amigados_closeDir(LIBFLUX_FSMNG * fsmng, int32_t dirhandle)
{
	if(dirhandle<128)
	{
		if(fsmng->dirhandletable[dirhandle-1]!=(void*)-1)
		{
			fsmng->dirhandletable[dirhandle-1] = (void*)-1;
			return LIBFLUX_NOERROR;
		}
	}
	return LIBFLUX_ACCESSERROR;
}

int32_t amigados_openFile(LIBFLUX_FSMNG * fsmng, char * filename)
{
	struct File *file;
	int32_t i;
	SECTNUM snum;
	char filen[256];
	struct Volume * adfvolume;

	adfvolume = (struct Volume *)fsmng->volume;

	if(!adfvolume)
		return LIBFLUX_ACCESSERROR;

	i = 0;
	while((fsmng->handletable[i]!=(void*)-1) && i<128)
	{
		i++;
	}
	if(i == 128) return LIBFLUX_ACCESSERROR;

	if( changedir(fsmng,filename,&snum,0) ==  RC_OK)
	{
		if( adfParentDir(adfvolume) == RC_OK )
		{
			libflux_getfilenamebase(filename,filen,UNIX_PATH_TYPE);

			file = adfOpenFile(adfvolume, filen, "r");
			if(file)
			{
				fsmng->handletable[i] = file;
				return i+1;
			}
		}
	}

	return LIBFLUX_ACCESSERROR;
}

int32_t amigados_createFile(LIBFLUX_FSMNG * fsmng, char * filename)
{
	struct File *file;
	int32_t i;
	char filen[256];
	char folderpath[256];
	SECTNUM snum;
	struct Volume * adfvolume;

	adfvolume = (struct Volume *)fsmng->volume;

	if(!adfvolume)
		return LIBFLUX_ACCESSERROR;

	i = 0;
	while(fsmng->handletable[i]!=(void*)-1 && i<128)
	{
		i++;
	}
	if(i == 128) return LIBFLUX_ACCESSERROR;

	libflux_getpathfolder(filename,folderpath,UNIX_PATH_TYPE);

	if( changedir(fsmng,folderpath,&snum,1) ==  RC_OK)
	{
		libflux_getfilenamebase(filename,filen,UNIX_PATH_TYPE);

		file = adfOpenFile(adfvolume, filen, "w");
		if(file)
		{
			fsmng->handletable[i] = file;
			return i+1;
		}
	}

	return LIBFLUX_ACCESSERROR;
}

int32_t amigados_writeFile(LIBFLUX_FSMNG * fsmng,int32_t filehandle,unsigned char * buffer,int32_t size)
{
	int32_t byteswrite;

	byteswrite = 0;

	if(filehandle<128)
	{
		if(fsmng->handletable[filehandle-1]!=(void*)-1)
		{
			if(amigados_getFreeSpace(fsmng) >= size)
			{
				byteswrite = adfWriteFile((struct File*)fsmng->handletable[filehandle-1], size, buffer);
			}
			return byteswrite;
		}
	}
	return LIBFLUX_ACCESSERROR;
}

int32_t amigados_readFile( LIBFLUX_FSMNG * fsmng,int32_t filehandle,unsigned char * buffer,int32_t size)
{
	int32_t bytesread;
	if(filehandle && filehandle<128)
	{
		if(fsmng->handletable[filehandle-1]!=(void*)-1)
		{
			bytesread = adfReadFile((struct File*)fsmng->handletable[filehandle-1], size, buffer);
			return bytesread;
		}
	}
	return LIBFLUX_ACCESSERROR;
}

int32_t amigados_deleteFile(LIBFLUX_FSMNG * fsmng, char * filename)
{
	char filen[256];
	char folderpath[256];
	SECTNUM snum;
	struct Volume * adfvolume;

	adfvolume = (struct Volume *)fsmng->volume;

	if(!adfvolume)
		return LIBFLUX_ACCESSERROR;

	libflux_getpathfolder(filename,folderpath,UNIX_PATH_TYPE);

	if( changedir(fsmng,folderpath,&snum,1) ==  RC_OK)
	{
		libflux_getfilenamebase(filename,filen,UNIX_PATH_TYPE);

		if(adfRemoveEntry(adfvolume, adfvolume->curDirPtr, filen) == RC_OK)
		{
			return LIBFLUX_NOERROR;
		}
	}

	return LIBFLUX_ACCESSERROR;
}

int32_t amigados_closeFile( LIBFLUX_FSMNG * fsmng,int32_t filehandle)
{
	if(filehandle && filehandle<128)
	{
		if(fsmng->handletable[filehandle-1]!=(void*)-1)
		{
			adfCloseFile((struct File *)fsmng->handletable[filehandle-1]);
			fsmng->handletable[filehandle-1] = (void*)-1;
			return LIBFLUX_NOERROR;
		}
	}
	return LIBFLUX_ACCESSERROR;
}

int32_t amigados_createDir( LIBFLUX_FSMNG * fsmng,char * foldername)
{
	char filen[256];
	char folderpath[256];
	SECTNUM snum;
	struct Volume * adfvolume;

	adfvolume = (struct Volume *)fsmng->volume;

	if(!adfvolume)
		return LIBFLUX_ACCESSERROR;

	libflux_getpathfolder(foldername,folderpath,UNIX_PATH_TYPE);

	if( changedir(fsmng,folderpath,&snum,1) ==  RC_OK)
	{
		libflux_getfilenamebase(foldername,filen,UNIX_PATH_TYPE);

		if(adfCreateDir(adfvolume, adfvolume->curDirPtr, filen) == RC_OK)
		{
			return LIBFLUX_NOERROR;
		}
	}

	return LIBFLUX_ACCESSERROR;
}

int32_t amigados_removeDir( LIBFLUX_FSMNG * fsmng,char * foldername)
{
	int len;

	len = strlen(foldername);
	if(len)
	{
		if(foldername[len-1] == '/')
		{
			foldername[len-1] = 0;
		}
	}

	return amigados_deleteFile(fsmng, foldername);
}

int32_t amigados_ftell( LIBFLUX_FSMNG * fsmng,int32_t filehandle)
{
	struct File * file;
	if(filehandle && filehandle<128)
	{
		if(fsmng->handletable[filehandle-1]!=(void*)-1)
		{
			file = (struct File *)fsmng->handletable[filehandle-1];
			return file->pos;
		}
	}
	return LIBFLUX_ACCESSERROR;
}

int32_t amigados_fseek( LIBFLUX_FSMNG * fsmng,int32_t filehandle,int32_t offset,int32_t origin)
{
	struct File * file;

	if(filehandle && filehandle<128)
	{
		if(fsmng->handletable[filehandle-1]!=(void*)-1)
		{
			file = (struct File *)fsmng->handletable[filehandle-1];
			switch(origin)
			{
				case SEEK_SET:
					adfFileSeek((struct File *)fsmng->handletable[filehandle-1], offset);
					return LIBFLUX_NOERROR;
				break;
				case SEEK_CUR:
					adfFileSeek((struct File *)fsmng->handletable[filehandle-1], offset + file->pos);
					return LIBFLUX_NOERROR;
				break;
				case SEEK_END:

					adfFileSeek((struct File *)fsmng->handletable[filehandle-1], file->fileHdr->byteSize);

					if((uint32_t)offset<file->pos)
						adfFileSeek((struct File *)fsmng->handletable[filehandle-1], file->pos - offset);
					else
						adfFileSeek((struct File *)fsmng->handletable[filehandle-1], 0);

					return LIBFLUX_NOERROR;
				break;
			}

			return LIBFLUX_ACCESSERROR;
		}
	}

	return LIBFLUX_ACCESSERROR;
}
