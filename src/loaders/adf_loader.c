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
// File : adf_loader.c
// Contains: ADF floppy image loader
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

#include "loaders/common/raw_amiga.h"

#include "adf_loader.h"
#include "adf_writer.h"

int ADF_libIsValidDiskFile( LIBFLUX_IMGLDR * imgldr_ctx, LIBFLUX_IMGLDR_FILEINFOS * imgfile )
{
	return libflux_imgCheckFileCompatibility( imgldr_ctx, imgfile, "ADF_libIsValidDiskFile", "adf", 512*11);
}

int ADF_libLoad_DiskFile(LIBFLUX_IMGLDR * imgldr_ctx,LIBFLUX_FLOPPY * floppydisk,char * imgfile,void * parameters)
{
	int ret;
	FILE * f_img;

	imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"ADF_libLoad_DiskFile %s",imgfile);

	f_img = libflux_fopen(imgfile,"rb");
	if( f_img == NULL )
	{
		imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"Cannot open %s !",imgfile);
		return LIBFLUX_ACCESSERROR;
	}

	ret = raw_amiga_loader(imgldr_ctx, floppydisk, f_img, 0, 0);

	libflux_fclose(f_img);

	return ret;
}

int ADF_libGetPluginInfo(LIBFLUX_IMGLDR * imgldr_ctx,uint32_t infotype,void * returnvalue)
{
	static const char plug_id[]="AMIGA_ADF";
	static const char plug_desc[]="AMIGA ADF Loader";
	static const char plug_ext[]="adf";

	plugins_ptr plug_funcs=
	{
		(ISVALIDDISKFILE)   ADF_libIsValidDiskFile,
		(LOADDISKFILE)      ADF_libLoad_DiskFile,
		(WRITEDISKFILE)     ADF_libWrite_DiskFile,
		(GETPLUGININFOS)    ADF_libGetPluginInfo
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
