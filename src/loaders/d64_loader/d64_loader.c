#include "uft/uft_memory.h"
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
// File : d64_loader.c
// Contains: Commodore 64 floppy image loader
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
#include "libflux.h"

#include "uft_floppy_loader.h"
#include "uft_floppy_utils.h"

#include "tracks/track_formats/c64_gcr_track.h"

#include "d64_loader.h"

#include "uft_compat.h"

typedef struct d64trackpos_
{
	int number_of_sector;
	int bitrate;
	int fileoffset;
}d64trackpos;

int D64_libIsValidDiskFile( LIBFLUX_IMGLDR * imgldr_ctx, LIBFLUX_IMGLDR_FILEINFOS * imgfile )
{
	imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"D64_libIsValidDiskFile");

	if(libflux_checkfileext(imgfile->path,"d64",SYS_PATH_TYPE))
	{
		imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"D64_libIsValidDiskFile : D64 file !");
		return LIBFLUX_VALIDFILE;
	}
	else
	{
		imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"D64_libIsValidDiskFile : non D64 file !");
		return LIBFLUX_BADFILE;
	}
}

int D64_libLoad_DiskFile(LIBFLUX_IMGLDR * imgldr_ctx,LIBFLUX_FLOPPY * floppydisk,char * imgfile,void * parameters)
{
	FILE * f;
	unsigned int filesize;
	int i,j,k;
	unsigned char* trackdata;
	int tracklen;
	int rpm;
	int sectorsize;
	LIBFLUX_CYLINDER* currentcylinder;
	LIBFLUX_SIDE* currentside;
	unsigned char errormap[1024];
	d64trackpos * tracklistpos;
	int number_of_track;
	int errormap_size;

	tracklistpos = NULL;
	trackdata = NULL;

	imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"D64_libLoad_DiskFile %s",imgfile);

	f = libflux_fopen(imgfile,"rb");
	if( f == NULL )
	{
		imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"Cannot open %s !",imgfile);
		return LIBFLUX_ACCESSERROR;
	}

	filesize = libflux_fgetsize(f);

	errormap_size=0;
	switch(filesize)
	{
		case 174848:
			//35 track, no errors
			number_of_track=35;
			break;

		case 175531:
			//35 track, 683 error bytes
			number_of_track=35;

			errormap_size=683;
			memset(errormap,0,errormap_size);
			if (fseek(f,errormap_size,SEEK_END) != 0) { /* seek error */ }
			libflux_fread(errormap,errormap_size,f);

			break;

		case 196608:
			//40 track, no errors
			number_of_track=40;
			break;

		case 197376:
			//40 track, 768 error bytes
			number_of_track=40;

			errormap_size=768;
			memset(errormap,0,errormap_size);
			if (fseek(f,errormap_size,SEEK_END) != 0) { /* seek error */ }
			libflux_fread(errormap,errormap_size,f);

			break;

		default:
			// not supported !
			imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"Unsupported D64 file size ! (%d Bytes)",filesize);
			libflux_fclose(f);
			return LIBFLUX_UNSUPPORTEDFILE;

			break;
	}

	tracklistpos=(d64trackpos*)malloc(number_of_track*sizeof(d64trackpos));
	if( !tracklistpos )
		goto alloc_error;

	memset(tracklistpos,0,number_of_track*sizeof(d64trackpos));

	i=0;
	k=0;
	do
	{
		tracklistpos[i].number_of_sector=21;
		tracklistpos[i].fileoffset=k;
		tracklistpos[i].bitrate=307693;//XTAL_16_MHz / 13

		k=k+(21*256);
		i++;
	}while(i<number_of_track && i<17);

	i=17;
	do
	{
		tracklistpos[i].number_of_sector=19;
		tracklistpos[i].fileoffset=k;
		tracklistpos[i].bitrate=285715; //XTAL_16_MHz / 14

		k=k+(19*256);
		i++;
	}while(i<number_of_track && i<24);

	i=24;
	do
	{
		tracklistpos[i].number_of_sector=18;
		tracklistpos[i].fileoffset=k;
		tracklistpos[i].bitrate=266667;//XTAL_16_MHz / 15

		k=k+(18*256);
		i++;
	}while(i<number_of_track && i<30);

	i=30;
	do
	{
		tracklistpos[i].number_of_sector=17;
		tracklistpos[i].fileoffset=k;
		tracklistpos[i].bitrate=250000;//XTAL_16_MHz / 16

		k=k+(17*256);
		i++;
	}while(i<number_of_track);


	floppydisk->floppyNumberOfTrack=number_of_track;
	floppydisk->floppyNumberOfSide=1;
	floppydisk->floppySectorPerTrack=-1;

	sectorsize=256; // c64 file support only 256bytes/sector floppies.

	floppydisk->floppyBitRate=VARIABLEBITRATE;
	floppydisk->floppyiftype=C64_DD_FLOPPYMODE;
	floppydisk->tracks=(LIBFLUX_CYLINDER**)malloc(sizeof(LIBFLUX_CYLINDER*)*floppydisk->floppyNumberOfTrack);

	rpm=300;

	imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"filesize:%dkB, %d tracks, %d side(s), rpm:%d",filesize/1024,floppydisk->floppyNumberOfTrack,floppydisk->floppyNumberOfSide,rpm);

	for(j=0;j<floppydisk->floppyNumberOfTrack;j++)
	{
		trackdata = (unsigned char*)uft_safe_malloc_array(tracklistpos[j].number_of_sector, sectorsize);
		if( !trackdata )
			goto alloc_error;

		floppydisk->tracks[j]=allocCylinderEntry(rpm,floppydisk->floppyNumberOfSide);
		currentcylinder=floppydisk->tracks[j];

		for(i=0;i<floppydisk->floppyNumberOfSide;i++)
		{
			libflux_imgCallProgressCallback(imgldr_ctx,(j<<1) + (i&1),floppydisk->floppyNumberOfTrack*2 );

			tracklen=(tracklistpos[j].bitrate/(rpm/60))/4;

			currentcylinder->sides[i] = malloc(sizeof(LIBFLUX_SIDE));
			if( !currentcylinder->sides[i] )
				goto alloc_error;

			memset(currentcylinder->sides[i],0,sizeof(LIBFLUX_SIDE));
			currentside=currentcylinder->sides[i];

			currentside->number_of_sector=tracklistpos[j].number_of_sector;
			currentside->tracklen=tracklen;

			currentside->databuffer = malloc(currentside->tracklen);
			if( !currentside->databuffer )
				goto alloc_error;

			memset(currentside->databuffer,0,currentside->tracklen);

			currentside->flakybitsbuffer=0;

			currentside->indexbuffer = malloc(currentside->tracklen);
			if( !currentside->indexbuffer )
				goto alloc_error;

			memset(currentside->indexbuffer,0,currentside->tracklen);

			currentside->timingbuffer=0;
			currentside->bitrate=tracklistpos[j].bitrate;
			currentside->track_encoding=UNKNOWN_ENCODING;

			(void)fseek(f , tracklistpos[j].fileoffset , SEEK_SET);

			libflux_fread(trackdata,sectorsize*currentside->number_of_sector,f);

			imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"Track:%d Size:%d File offset:%d Number of sector:%d Bitrate:%d",j,currentside->tracklen,tracklistpos[j].fileoffset,tracklistpos[j].number_of_sector,currentside->bitrate);

			BuildC64GCRTrack(currentside->number_of_sector,sectorsize,j,i,trackdata,currentside->databuffer,&currentside->tracklen);

			currentside->tracklen=currentside->tracklen*8;

		}

		free(trackdata);
		trackdata = NULL;
	}

	imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"track file successfully loaded and encoded!");

	libflux_fclose(f);
	return LIBFLUX_NOERROR;

alloc_error:

	if ( f )
		libflux_fclose( f );

	libflux_freeFloppy(imgldr_ctx->ctx, floppydisk );

	free( trackdata );
	free( tracklistpos );

	return LIBFLUX_INTERNALERROR;
}

int D64_libGetPluginInfo(LIBFLUX_IMGLDR * imgldr_ctx,uint32_t infotype,void * returnvalue)
{
	static const char plug_id[]="C64_D64";
	static const char plug_desc[]="C64 D64 file image loader";
	static const char plug_ext[]="d64";

	plugins_ptr plug_funcs=
	{
		(ISVALIDDISKFILE)   D64_libIsValidDiskFile,
		(LOADDISKFILE)      D64_libLoad_DiskFile,
		(WRITEDISKFILE)     0,
		(GETPLUGININFOS)    D64_libGetPluginInfo
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
