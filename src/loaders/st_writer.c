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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "types.h"

#include "libflux.h"
#include "track_generator.h"
#include "libflux.h"
#include "st_loader.h"
#include "st_writer.h"
#include "tracks/sector_extractor.h"
#include "uft_compat.h"

// Main writer function
int ST_libWrite_DiskFile(LIBFLUX_IMGLDR* imgldr_ctx,LIBFLUX_FLOPPY * floppy,char * filename)
{
	int nbsector;
	int nbtrack;
	int nbside;
	int sectorsize;

	int writeret;

	FILE * stfile;

	unsigned int sectorcnt_s0;
	unsigned int sectorcnt_s1;

	sectorsize = 512;

	libflux_imgCallProgressCallback(imgldr_ctx,0,floppy->floppyNumberOfTrack*2 );

	imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"Write ST file %s...",filename);

	sectorcnt_s0 = count_sector(imgldr_ctx->ctx,floppy,1,0,0,sectorsize,ISOIBM_MFM_ENCODING,0x0000);
	sectorcnt_s1 = count_sector(imgldr_ctx->ctx,floppy,1,0,1,sectorsize,ISOIBM_MFM_ENCODING,0x0000);

	if(sectorcnt_s0>21 || sectorcnt_s0<9)
	{
		imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"Error : Disk format doesn't match...",filename);
		return LIBFLUX_FILECORRUPTED;
	}

	nbtrack = 85;
	while(nbtrack && !count_sector(imgldr_ctx->ctx,floppy,1,nbtrack-1,0,sectorsize,ISOIBM_MFM_ENCODING,0x0000))
	{
		nbtrack--;
	}

	nbside = 1;
	if(sectorcnt_s1)
		nbside = 2;

	nbsector = sectorcnt_s0;

	imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"%d sectors (%d bytes), %d tracks, %d sides...",nbsector,sectorsize,nbtrack,nbside);

	writeret = LIBFLUX_ACCESSERROR;
	stfile = libflux_fopen(filename,"wb");
	if(stfile)
	{
		writeret = write_raw_file(imgldr_ctx,stfile,floppy,1,nbsector,nbtrack,nbside,sectorsize,ISOIBM_MFM_ENCODING,0);

		libflux_fclose(stfile);
	}

	return writeret;
}
