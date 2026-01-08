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
// File : mfm_loader.c
// Contains: MFM floppy image loader
//
// Written by: Jean-Franois DEL NERO
//
// Change History (most recent first):
///////////////////////////////////////////////////////////////////////////////////

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "types.h"

#include "libflux.h""
#include "tracks/track_generator.h"
#include "libflux.h""

#include "uft_floppy_loader.h"
#include "uft_floppy_utils.h"

#include "mfm_loader.h"
#include "mfm_format.h"

#include "uft_compat.h"

int MFM_libIsValidDiskFile( LIBFLUX_IMGLDR * imgldr_ctx, LIBFLUX_IMGLDR_FILEINFOS * imgfile )
{
	MFMIMG * header;

	imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"MFM_libIsValidDiskFile");

	if(libflux_checkfileext(imgfile->path,"mfm",SYS_PATH_TYPE))
	{
		header = (MFMIMG *)&imgfile->file_header;

		if( !strncmp((char*)header->headername,"HXCMFM",6))
		{
			imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"MFM_libIsValidDiskFile : MFM file !");
			return LIBFLUX_VALIDFILE;
		}
		else
		{
			imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"MFM_libIsValidDiskFile : non MFM file !");
			return LIBFLUX_BADFILE;
		}
	}
	else
	{
		imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"MFM_libIsValidDiskFile : non MFM file !");
		return LIBFLUX_BADFILE;
	}
}

int MFM_libLoad_DiskFile(LIBFLUX_IMGLDR * imgldr_ctx,LIBFLUX_FLOPPY * floppydisk,char * imgfile,void * parameters)
{
	FILE * f;
	MFMIMG header;
	MFMTRACKIMG trackdesc;
	unsigned int i;
	LIBFLUX_CYLINDER* currentcylinder;
	LIBFLUX_SIDE* currentside;


	imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"MFM_libLoad_DiskFile %s",imgfile);

	f = libflux_fopen(imgfile,"rb");
	if( f == NULL )
	{
		imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"Cannot open %s !",imgfile);
		return LIBFLUX_ACCESSERROR;
	}

	memset(&header,0,sizeof(MFMIMG));
	memset(&trackdesc,0,sizeof(MFMTRACKIMG));

	libflux_fread(&header,sizeof(header),f);

	if(!strncmp((char*)header.headername,"HXCMFM",6))
	{

		floppydisk->floppyNumberOfTrack=header.number_of_track;
		floppydisk->floppyNumberOfSide=header.number_of_side;
		floppydisk->floppyBitRate=header.floppyBitRate*1000;
		floppydisk->floppySectorPerTrack=-1;
		floppydisk->floppyiftype=ATARIST_DD_FLOPPYMODE;

		imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"MFM File : %d track, %d side, %d bit/s, %d sectors, mode %d",
			floppydisk->floppyNumberOfTrack,
			floppydisk->floppyNumberOfSide,
			floppydisk->floppyBitRate,
			floppydisk->floppySectorPerTrack,
			floppydisk->floppyiftype);

		floppydisk->tracks = (LIBFLUX_CYLINDER**)malloc(sizeof(LIBFLUX_CYLINDER*)*floppydisk->floppyNumberOfTrack);
		if( !floppydisk->tracks )
			goto alloc_error;

		memset(floppydisk->tracks,0,sizeof(LIBFLUX_CYLINDER*)*floppydisk->floppyNumberOfTrack);

		for(i=0;i<(unsigned int)(floppydisk->floppyNumberOfTrack*floppydisk->floppyNumberOfSide);i++)
		{

			libflux_imgCallProgressCallback(imgldr_ctx,i,(floppydisk->floppyNumberOfTrack*floppydisk->floppyNumberOfSide) );

			if (fseek(f,(header.mfmtracklistoffset)+(i*sizeof(trackdesc)),SEEK_SET) != 0) { /* seek error */ }
			libflux_fread(&trackdesc,sizeof(trackdesc),f);
			if (fseek(f,trackdesc.mfmtrackoffset,SEEK_SET) != 0) { /* seek error */ }
			if(!floppydisk->tracks[trackdesc.track_number])
			{
				floppydisk->tracks[trackdesc.track_number]=allocCylinderEntry(header.floppyRPM,floppydisk->floppyNumberOfSide);
			}

			currentcylinder = floppydisk->tracks[trackdesc.track_number];

			imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"read track %d side %d at offset 0x%x (0x%x bytes)",
												trackdesc.track_number,
												trackdesc.side_number,
												trackdesc.mfmtrackoffset,
												trackdesc.mfmtracksize);

			currentcylinder->sides[trackdesc.side_number] = tg_alloctrack(floppydisk->floppyBitRate,UNKNOWN_ENCODING,currentcylinder->floppyRPM,trackdesc.mfmtracksize*8,2500,-2500,0x00);
			currentside=currentcylinder->sides[trackdesc.side_number];
			currentside->number_of_sector=floppydisk->floppySectorPerTrack;

			libflux_fread(currentside->databuffer,currentside->tracklen/8,f);
		}

		libflux_fclose(f);
		return LIBFLUX_NOERROR;
	}

	libflux_fclose(f);
	imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"bad header");
	return LIBFLUX_BADFILE;

alloc_error:
	libflux_fclose(f);

	libflux_freeFloppy(imgldr_ctx->ctx, floppydisk );

	return LIBFLUX_INTERNALERROR;
}

int MFM_libWrite_DiskFile(LIBFLUX_IMGLDR* imgldr_ctx,LIBFLUX_FLOPPY * floppy,char * filename);

int MFM_libGetPluginInfo(LIBFLUX_IMGLDR * imgldr_ctx,uint32_t infotype,void * returnvalue)
{
	static const char plug_id[]="HXCMFM_IMG";
	static const char plug_desc[]="HXC MFM IMG Loader";
	static const char plug_ext[]="mfm";

	plugins_ptr plug_funcs=
	{
		(ISVALIDDISKFILE)   MFM_libIsValidDiskFile,
		(LOADDISKFILE)      MFM_libLoad_DiskFile,
		(WRITEDISKFILE)     MFM_libWrite_DiskFile,
		(GETPLUGININFOS)    MFM_libGetPluginInfo
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
