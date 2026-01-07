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
// File : dms_loader.c
// Contains: DMS floppy image loader
//
// Written by: Jean-François DEL NERO
//
// Change History (most recent first):
///////////////////////////////////////////////////////////////////////////////////

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "types.h"

#include "libflux.h""
#include "libflux.h""
#include "libhxcadaptor.h"
#include "uft_floppy_loader.h"

#include "loaders/common/raw_amiga.h"

#include "dms_loader.h"

#include "thirdpartylibs/xdms/vfile.h"

#include "thirdpartylibs/xdms/xdms-1.3.2/src/cdata.h"
#include "thirdpartylibs/xdms/xdms-1.3.2/src/pfile.h"

int DMS_libIsValidDiskFile( LIBFLUX_IMGLDR * imgldr_ctx, LIBFLUX_IMGLDR_FILEINFOS * imgfile )
{
	return libflux_imgCheckFileCompatibility( imgldr_ctx, imgfile, "DMS_libIsValidDiskFile", "dms", 0);
}

int DMS_libLoad_DiskFile(LIBFLUX_IMGLDR * imgldr_ctx,LIBFLUX_FLOPPY * floppydisk,char * imgfile,void * parameters)
{
	int ret;
	unsigned int filesize;
	HXCFILE * f_img;
	int retxdms;

	imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"DMS_libLoad_DiskFile %s",imgfile);

	// Unpack the dms file.
	f_img = HXC_fopen("","");
	if( !f_img )
	{
		imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"Alloc Error !");
		return LIBFLUX_INTERNALERROR;
	}

	retxdms = Process_File(imgfile,f_img, CMD_UNPACK, 0, 0, 0);
	if(retxdms)
	{
		imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"XDMS: Error %d while reading the file!",retxdms);
		HXC_fclose(f_img);
		return LIBFLUX_ACCESSERROR;
	}

	filesize = f_img->buffersize;

	ret = raw_amiga_loader(imgldr_ctx, floppydisk, 0, f_img->buffer, filesize );

	HXC_fclose( f_img );

	return ret;
}

int DMS_libGetPluginInfo(LIBFLUX_IMGLDR * imgldr_ctx,uint32_t infotype,void * returnvalue)
{
	static const char plug_id[]="AMIGA_DMS";
	static const char plug_desc[]="AMIGA DMS Loader";
	static const char plug_ext[]="dms";

	plugins_ptr plug_funcs=
	{
		(ISVALIDDISKFILE)   DMS_libIsValidDiskFile,
		(LOADDISKFILE)      DMS_libLoad_DiskFile,
		(WRITEDISKFILE)     0,
		(GETPLUGININFOS)    DMS_libGetPluginInfo
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
