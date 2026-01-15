/**
 * @file d81_loader.c
 * @brief Commodore 1581 D81 loader (standalone)
 * @version 3.8.0
 */
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

///////////////////////////////////////////////////////////////////////////////////
// File : d81_loader.c
// Contains: D81 floppy image loader
//
// Written by: Jean-François DEL NERO
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

#include "loaders/common/raw_iso.h"

#include "d81_loader.h"

int D81_libIsValidDiskFile( LIBFLUX_IMGLDR * imgldr_ctx, LIBFLUX_IMGLDR_FILEINFOS * imgfile )
{
	return libflux_imgCheckFileCompatibility( imgldr_ctx, imgfile, "D81_libIsValidDiskFile", "d81", 512);
}

int D81_libLoad_DiskFile(LIBFLUX_IMGLDR * imgldr_ctx,LIBFLUX_FLOPPY * floppydisk,char * imgfile,void * parameters)
{
	raw_iso_cfg rawcfg;
	int ret;
	FILE * f_img;

	imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"D81_libLoad_DiskFile %s",imgfile);

	f_img = libflux_fopen(imgfile,"rb");
	if( f_img == NULL )
	{
		imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"Cannot open %s !",imgfile);
		return LIBFLUX_ACCESSERROR;
	}

	raw_iso_setdefcfg(&rawcfg);

	rawcfg.sector_size = 512;
	rawcfg.number_of_tracks = 80;
	rawcfg.number_of_sides = 2;
	rawcfg.number_of_sectors_per_track = 10;
	rawcfg.gap3 = 35;
	rawcfg.interleave = 1;
	rawcfg.rpm = 300;
	rawcfg.bitrate = 250000;
	rawcfg.interface_mode = GENERIC_SHUGART_DD_FLOPPYMODE;
	rawcfg.track_format = ISOFORMAT_DD;
	rawcfg.flip_sides = 1;

	ret = raw_iso_loader(imgldr_ctx, floppydisk, f_img, 0, 0, &rawcfg);

	libflux_fclose(f_img);

	return ret;
}

int D81_libGetPluginInfo(LIBFLUX_IMGLDR * imgldr_ctx,uint32_t infotype,void * returnvalue)
{
	static const char plug_id[]="C64_D81";
	static const char plug_desc[]="C64 D81 Loader";
	static const char plug_ext[]="d81";

	plugins_ptr plug_funcs=
	{
		(ISVALIDDISKFILE)   D81_libIsValidDiskFile,
		(LOADDISKFILE)      D81_libLoad_DiskFile,
		(WRITEDISKFILE)     0,
		(GETPLUGININFOS)    D81_libGetPluginInfo
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

