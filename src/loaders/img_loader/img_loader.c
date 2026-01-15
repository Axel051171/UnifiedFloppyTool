/**
 * @file img_loader.c
 * @brief Raw IMG sector image loader
 * @version 3.8.0
 */
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
// File : img_loader.c
// Contains: IMG floppy image loader
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
#include "uft_compat.h"
#include "uft_floppy_loader.h"
#include "track_generator.h"

#include "img_loader.h"

#include "loaders/common/raw_iso.h"

#include "pcimgfileformat.h"

int pc_imggetfloppyconfig(unsigned char * img,uint32_t filesize, raw_iso_cfg *rawcfg)
{
	int i;
	unsigned char * uimg;
	int conffound,numberofsector;

	uimg=(unsigned char *)img;

	conffound=0;

	raw_iso_setdefcfg(rawcfg);

	rawcfg->track_format = IBMFORMAT_DD;
	rawcfg->fill_value = 0xE5;

	rawcfg->rpm = 300;
	rawcfg->sector_size = 512;
	rawcfg->start_sector_id = 1;
	rawcfg->interleave = 1;

	if(uimg[0x18]<24 && uimg[0x18]>7 && (uimg[0x1A]==1 || uimg[0x1A]==2) && !(filesize&0x1FF) )
	{
		rawcfg->rpm = 300;
		rawcfg->number_of_sectors_per_track = uimg[0x18];
		rawcfg->number_of_sides = uimg[0x1A];
		if( rawcfg->number_of_sectors_per_track <= 10 )
		{
			rawcfg->gap3 = 84;
			rawcfg->interleave = 1;
			rawcfg->bitrate=250000;
			rawcfg->interface_mode = IBMPC_DD_FLOPPYMODE;
		}
		else
		{
			if(rawcfg->number_of_sectors_per_track <= 21)
			{
				rawcfg->bitrate = 500000;
				rawcfg->gap3 = 84;
				rawcfg->interleave = 1;
				rawcfg->interface_mode = IBMPC_HD_FLOPPYMODE;

				if( rawcfg->number_of_sectors_per_track > 18 )
				{
					rawcfg->gap3 = 14;
					rawcfg->interleave = 2;
				}

				if(rawcfg->number_of_sectors_per_track == 15)
				{
					rawcfg->rpm = 360;
				}

			}
			else
			{
				rawcfg->bitrate = 1000000;
				rawcfg->gap3 = 84;
				rawcfg->interleave = 1;
				rawcfg->interface_mode = IBMPC_ED_FLOPPYMODE;
			}
		}
		numberofsector = uimg[0x13]+(uimg[0x14]*256);
		rawcfg->number_of_tracks = (numberofsector/(rawcfg->number_of_sectors_per_track * rawcfg->number_of_sides));

	//  if((unsigned int)((*numberofsectorpertrack) * (*numberoftrack) * (*numberofside) *512)==filesize)
		{
			conffound=1;
		}

	}

	if(conffound==0)
	{
		i=0;
		do
		{

			if(pcimgfileformats[i].filesize==filesize)
			{
				rawcfg->number_of_tracks = pcimgfileformats[i].numberoftrack;
				rawcfg->number_of_sectors_per_track = pcimgfileformats[i].sectorpertrack;
				rawcfg->number_of_sides = pcimgfileformats[i].numberofside;
				rawcfg->gap3 = pcimgfileformats[i].gap3len;
				rawcfg->interleave = pcimgfileformats[i].interleave;
				rawcfg->rpm = pcimgfileformats[i].RPM;
				rawcfg->bitrate = pcimgfileformats[i].bitrate;
				rawcfg->interface_mode = pcimgfileformats[i].interface_mode;
				conffound=1;
			}
			i++;

		}while(pcimgfileformats[i].filesize!=0 && conffound==0);
	}
	return conffound;
}


int IMG_libIsValidDiskFile( LIBFLUX_IMGLDR * imgldr_ctx, LIBFLUX_IMGLDR_FILEINFOS * imgfile )
{
	int i,conffound;

	imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"IMG_libIsValidDiskFile");

	if(libflux_checkfileext(imgfile->path,"img",SYS_PATH_TYPE) || libflux_checkfileext(imgfile->path,"ima",SYS_PATH_TYPE))
	{
		if(imgfile->file_size<0)
		{
			imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"IMG_libIsValidDiskFile : Cannot open %s !",imgfile->path);
			return LIBFLUX_ACCESSERROR;
		}

		if(imgfile->file_size&0x1FF)
		{
			imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"IMG_libIsValidDiskFile : non IMG file - bad file size !");
			return LIBFLUX_BADFILE;
		}

		i=0;
		conffound=0;
		do
		{
			if((int)pcimgfileformats[i].filesize == imgfile->file_size)
			{
				conffound=1;
			}
			i++;
		}while(pcimgfileformats[i].filesize!=0 && conffound==0);

		if(!conffound)
		{
			imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"IMG_libIsValidDiskFile : non IMG file - bad file size !");
			return LIBFLUX_BADFILE;
		}

		imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"IMG_libIsValidDiskFile : IMG file !");
		return LIBFLUX_VALIDFILE;
	}
	else
	{
		imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"IMG_libIsValidDiskFile : non IMG file !");
		return LIBFLUX_BADFILE;
	}
}



int IMG_libLoad_DiskFile(LIBFLUX_IMGLDR * imgldr_ctx,LIBFLUX_FLOPPY * floppydisk,char * imgfile,void * parameters)
{

	FILE * f_img;
	raw_iso_cfg rawcfg;
	unsigned int filesize;
	int ret;
	unsigned char boot_sector[512];

	imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"IMG_libLoad_DiskFile %s",imgfile);

	f_img = libflux_fopen(imgfile,"rb");
	if( f_img == NULL )
	{
		imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"Cannot open %s !",imgfile);
		return LIBFLUX_ACCESSERROR;
	}

	filesize = libflux_fgetsize( f_img );

	memset(boot_sector,0,sizeof(boot_sector));
	libflux_fread(boot_sector,512,f_img);

	if(pc_imggetfloppyconfig( boot_sector, filesize, &rawcfg)==1)
	{
		if (fseek(f_img,0,SEEK_SET) != 0) { /* seek error */ }
		ret = raw_iso_loader(imgldr_ctx, floppydisk, f_img, 0, 0, &rawcfg);

		libflux_fclose(f_img);

		return ret;
	}

	libflux_fclose(f_img);

	return LIBFLUX_BADFILE;
}

int IMG_libGetPluginInfo(LIBFLUX_IMGLDR * imgldr_ctx,uint32_t infotype,void * returnvalue)
{
	static const char plug_id[]="RAW_IMG";
	static const char plug_desc[]="IBM PC IMG Loader";
	static const char plug_ext[]="img";

	plugins_ptr plug_funcs=
	{
		(ISVALIDDISKFILE)   IMG_libIsValidDiskFile,
		(LOADDISKFILE)      IMG_libLoad_DiskFile,
		(WRITEDISKFILE)     0,
		(GETPLUGININFOS)    IMG_libGetPluginInfo
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
