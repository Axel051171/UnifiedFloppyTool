/*
//
// Copyright (C) 2006-2025 Jean-Franois DEL NERO
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

///////////////////////////////////////////////////////////////////////////////////
// File : arburg_raw_loader.c
// Contains: Arburg floppy image loader
//
// Written by: Jean-Franois DEL NERO
//
// Change History (most recent first):
///////////////////////////////////////////////////////////////////////////////////

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "types.h"

#include "libflux.h"
#include "track_generator.h"
#include "libflux.h"

#include "uft_floppy_loader.h"
#include "uft_floppy_utils.h"

#include "tracks/track_formats/arburg_track.h"
#include "arburg_raw_loader.h"
#include "arburg_raw_writer.h"

#include "uft_compat.h"

int ARBURG_RAW_libIsValidDiskFile( LIBFLUX_IMGLDR * imgldr_ctx, LIBFLUX_IMGLDR_FILEINFOS * imgfile )
{
	imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"ARBURG_RAW_libIsValidDiskFile");

	if( libflux_checkfileext(imgfile->path,"arburgfd",SYS_PATH_TYPE) )
	{
		if( imgfile->file_size == (ARBURB_DATATRACK_SIZE*2*80) )
		{
			imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"ARBURG_RAW_libIsValidDiskFile : Arburg Data raw file !");
			return LIBFLUX_VALIDFILE;
		}
		else
		{

			if( imgfile->file_size == ( (ARBURB_DATATRACK_SIZE*10)+(ARBURB_SYSTEMTRACK_SIZE*70)+(ARBURB_SYSTEMTRACK_SIZE*80) ))
			{
				imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"ARBURG_RAW_libIsValidDiskFile : Arburg System raw file !");
				return LIBFLUX_VALIDFILE;
			}
			else
			{
				imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"ARBURG_RAW_libIsValidDiskFile : non Arburg raw file !");
				return LIBFLUX_BADFILE;
			}
		}
	}
	else
	{
		imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"ARBURG_RAW_libIsValidDiskFile : non Arburg raw file !");
		return LIBFLUX_BADFILE;
	}
}

int ARBURG_RAW_libLoad_DiskFile(LIBFLUX_IMGLDR * imgldr_ctx,LIBFLUX_FLOPPY * floppydisk,char * imgfile,void * parameters)
{
	FILE * f;
	unsigned short i;
	int filesize;
	LIBFLUX_CYLINDER* currentcylinder;
	LIBFLUX_SIDE*    currentside;
	unsigned char  sector_data[ARBURB_SYSTEMTRACK_SIZE + 2];
	unsigned short tracknumber,sidenumber;
	int systemdisk;
	int fileoffset,blocksize;

	imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"ARBURG_RAW_libLoad_DiskFile %s",imgfile);

	filesize=libflux_getfilesize(imgfile);

	if(filesize<0)
		return LIBFLUX_ACCESSERROR;

	systemdisk = 0;

	if(filesize==( (ARBURB_DATATRACK_SIZE*10)+(ARBURB_SYSTEMTRACK_SIZE*70)+(ARBURB_SYSTEMTRACK_SIZE*80) ))
	{
		systemdisk = 1;
		imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"ARBURG_RAW_libLoad_DiskFile : Arburg Data raw file !");
	}

	f = libflux_fopen(imgfile,"rb");
	if( f == NULL )
	{
		imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"Cannot open %s !",imgfile);
		return LIBFLUX_ACCESSERROR;
	}

	floppydisk->floppyNumberOfTrack=80;
	floppydisk->floppyNumberOfSide=2;
	floppydisk->floppyBitRate=250000;
	floppydisk->floppySectorPerTrack=1;
	floppydisk->floppyiftype=GENERIC_SHUGART_DD_FLOPPYMODE;

	imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"Arburg File : %d track, %d side, %d bit/s, %d sectors, mode %d",
		floppydisk->floppyNumberOfTrack,
		floppydisk->floppyNumberOfSide,
		floppydisk->floppyBitRate,
		floppydisk->floppySectorPerTrack,
		floppydisk->floppyiftype);

	floppydisk->tracks=(LIBFLUX_CYLINDER**)malloc(sizeof(LIBFLUX_CYLINDER*)*floppydisk->floppyNumberOfTrack);
	if( !floppydisk->tracks )
	{
		libflux_fclose(f);
		return LIBFLUX_INTERNALERROR;
	}

	memset(floppydisk->tracks,0,sizeof(LIBFLUX_CYLINDER*)*floppydisk->floppyNumberOfTrack);

	for(i=0;i<floppydisk->floppyNumberOfTrack*floppydisk->floppyNumberOfSide;i++)
	{
		libflux_imgCallProgressCallback(imgldr_ctx, i,floppydisk->floppyNumberOfTrack*2);

		tracknumber = i % floppydisk->floppyNumberOfTrack;
		if(i>=floppydisk->floppyNumberOfTrack)
			sidenumber = 1;
		else
			sidenumber = 0;

		if(i<10 || !systemdisk )
		{
			blocksize = ARBURB_DATATRACK_SIZE;
			fileoffset = i * blocksize;
		}
		else
		{
			blocksize = ARBURB_SYSTEMTRACK_SIZE;
			fileoffset = (10*ARBURB_DATATRACK_SIZE) + ((i-10)*blocksize);
		}

		if (fseek(f,fileoffset,SEEK_SET) != 0) { /* seek error */ }
		libflux_fread(&sector_data,blocksize,f);

		if(!floppydisk->tracks[tracknumber])
		{
			floppydisk->tracks[tracknumber] = allocCylinderEntry(300,floppydisk->floppyNumberOfSide);
		}

		currentcylinder=floppydisk->tracks[tracknumber];

		imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"read track %d side %d at offset 0x%x (0x%x bytes)",
			tracknumber,
			sidenumber,
			fileoffset,
			blocksize);

		currentcylinder->sides[sidenumber]=tg_alloctrack(floppydisk->floppyBitRate,ARBURGDAT_ENCODING,currentcylinder->floppyRPM,256*49 * 8,2000,-2000,0x00);
		currentside=currentcylinder->sides[sidenumber];
		currentside->number_of_sector=floppydisk->floppySectorPerTrack;

		if(i<10 || !systemdisk )
		{
			BuildArburgTrack(imgldr_ctx->ctx,tracknumber,sidenumber,sector_data,currentside->databuffer,&currentside->tracklen,2);
			currentside->track_encoding = ARBURGDAT_ENCODING;
		}
		else
		{
			BuildArburgSysTrack(imgldr_ctx->ctx,tracknumber,sidenumber,sector_data,currentside->databuffer,&currentside->tracklen,2);
			currentside->track_encoding = ARBURGSYS_ENCODING;
		}

	}

	libflux_fclose(f);

	return LIBFLUX_NOERROR;
}

int ARBURG_RAW_libGetPluginInfo(LIBFLUX_IMGLDR * imgldr_ctx,uint32_t infotype,void * returnvalue)
{
	static const char plug_id[]="ARBURG";
	static const char plug_desc[]="ARBURG RAW Loader";
	static const char plug_ext[]="arburgfd";

	plugins_ptr plug_funcs=
	{
		(ISVALIDDISKFILE)   ARBURG_RAW_libIsValidDiskFile,
		(LOADDISKFILE)      ARBURG_RAW_libLoad_DiskFile,
		(WRITEDISKFILE)     ARBURG_RAW_libWrite_DiskFile,
		(GETPLUGININFOS)    ARBURG_RAW_libGetPluginInfo
	};

	return libGetPluginInfo(
			imgldr_ctx,
			infotype,
			returnvalue,
			plug_id,
			plug_desc,
			&plug_funcs,
			plug_ext
			);
}
