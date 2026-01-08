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
// File : raw_loader.c
// Contains: Amiga disk raw image loader
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
#include "libflux.h""
#include "./tracks/track_generator.h"

#include "uft_floppy_loader.h"
#include "uft_floppy_utils.h"

#include "raw_amiga.h"

#include "uft_compat.h"

int raw_amiga_loader(LIBFLUX_IMGLDR * imgldr_ctx,LIBFLUX_FLOPPY * floppydisk, FILE * f_img , unsigned char * imagebuffer, int size)
{
	LIBFLUX_FLPGEN * fb_ctx;
	int cur_offset;
	int ret;

	if( !f_img && !imagebuffer )
	{
		imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"File access error or allocation error");
		return LIBFLUX_INTERNALERROR;
	}

	cur_offset = 0;
	if( f_img )
	{
		cur_offset = ftell(f_img);
		if (fseek(f_img,0, SEEK_END) != 0) { /* seek error */ }
		size = ftell(f_img);
		if (fseek(f_img,cur_offset, SEEK_SET) != 0) { /* seek error */ }
	}

	if( !size )
	{
		imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"Null sized image !");
		return LIBFLUX_BADFILE;
	}

	fb_ctx = libflux_initFloppy( imgldr_ctx->ctx, 86, 2 );
	if( !fb_ctx )
	{
		imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"Alloc Error !");
		return LIBFLUX_INTERNALERROR;
	}

	if( size < 100*11*2*512 )
	{
		libflux_setNumberOfSector ( fb_ctx, 11 );
		libflux_setRPM( fb_ctx, DEFAULT_AMIGA_RPM ); // normal rpm
		libflux_setInterfaceMode( fb_ctx, AMIGA_DD_FLOPPYMODE);
	}
	else
	{
		libflux_setNumberOfSector ( fb_ctx, 22 );
		libflux_setRPM( fb_ctx, DEFAULT_AMIGA_RPM / 2); // 150 rpm
		libflux_setInterfaceMode( fb_ctx, AMIGA_HD_FLOPPYMODE);
	}

	if(( size / ( 512 * libflux_getCurrentNumberOfTrack(fb_ctx) * libflux_getCurrentNumberOfSide(fb_ctx)))<80)
		libflux_setNumberOfTrack ( fb_ctx, 80 );
	else
		libflux_setNumberOfTrack ( fb_ctx, size / (512* libflux_getCurrentNumberOfTrack(fb_ctx) * libflux_getCurrentNumberOfSide(fb_ctx) ) );

	libflux_setNumberOfSide ( fb_ctx, 2 );
	libflux_setSectorSize( fb_ctx, 512 );

	libflux_setTrackType( fb_ctx, AMIGAFORMAT_DD);
	libflux_setTrackBitrate( fb_ctx, DEFAULT_AMIGA_BITRATE );

	libflux_setStartSectorID( fb_ctx, 0 );
	libflux_setSectorGap3 ( fb_ctx, 0 );

	ret = libflux_generateDisk( fb_ctx, floppydisk, f_img, imagebuffer, size );

	return ret;
}
