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
// File : raw_iso.c
// Contains: iso disk raw image loader
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

#include "raw_iso.h"

#include "uft_compat.h"

int raw_iso_loader(LIBFLUX_IMGLDR * imgldr_ctx,LIBFLUX_FLOPPY * floppydisk, FILE * f_img , unsigned char * imagebuffer, int size, raw_iso_cfg * cfg)
{
	LIBFLUX_FLPGEN * fb_ctx;
	int cur_offset;
	int ret;
	int32_t flags;

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

	fb_ctx = libflux_initFloppy( imgldr_ctx->ctx, 86, cfg->number_of_sides );
	if( !fb_ctx )
	{
		imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"Alloc Error !");
		return LIBFLUX_INTERNALERROR;
	}

	libflux_setNumberOfTrack( fb_ctx, cfg->number_of_tracks );
	libflux_setNumberOfSide( fb_ctx, cfg->number_of_sides );
	libflux_setNumberOfSector( fb_ctx, cfg->number_of_sectors_per_track );
	libflux_setSectorSize( fb_ctx, cfg->sector_size );
	libflux_setStartSectorID( fb_ctx, cfg->start_sector_id );
	libflux_setSectorGap3( fb_ctx, cfg->gap3 );
	libflux_setTrackPreGap( fb_ctx, cfg->pregap );
	libflux_setTrackType( fb_ctx, cfg->track_format );
	libflux_setTrackBitrate( fb_ctx, cfg->bitrate );
	libflux_setRPM( fb_ctx, cfg->rpm );
	libflux_setInterfaceMode( fb_ctx, cfg->interface_mode );
	libflux_setTrackSkew ( fb_ctx, cfg->skew_per_track );
	libflux_setSideSkew ( fb_ctx, cfg->skew_per_side );
	libflux_setTrackInterleave ( fb_ctx, cfg->interleave );
	libflux_setSectorFill ( fb_ctx, cfg->fill_value );

	if(cfg->force_side_id >= 0)
		libflux_setDiskSectorsHeadID( fb_ctx, cfg->force_side_id );

	flags = 0;

	if(cfg->trk_grouped_by_sides)
		flags |= FLPGEN_SIDES_GROUPED;

	if(cfg->flip_sides)
		flags |= FLPGEN_FLIP_SIDES;

	libflux_setDiskFlags( fb_ctx, flags );

	ret = libflux_generateDisk( fb_ctx, floppydisk, f_img, imagebuffer, size );

	return ret;
}

void raw_iso_setdefcfg(raw_iso_cfg *rawcfg)
{
	if(rawcfg)
	{
		memset(rawcfg, 0, sizeof(raw_iso_cfg));
		rawcfg->number_of_tracks = 80;
		rawcfg->number_of_sides = 2;
		rawcfg->number_of_sectors_per_track = 9;
		rawcfg->sector_size = 512;
		rawcfg->start_sector_id = 1;
		rawcfg->gap3 = 84;
		rawcfg->pregap = 0;
		rawcfg->interleave = 1;
		rawcfg->skew_per_track = 0;
		rawcfg->skew_per_side = 0;
		rawcfg->bitrate = 250000;
		rawcfg->rpm = 300;
		rawcfg->track_format = IBMFORMAT_DD;
		rawcfg->interface_mode = GENERIC_SHUGART_DD_FLOPPYMODE;
		rawcfg->fill_value = 0xF6;
		rawcfg->trk_grouped_by_sides = 0;
		rawcfg->flip_sides = 0;
		rawcfg->force_side_id = -1;
	}
}
