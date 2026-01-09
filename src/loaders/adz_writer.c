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

#include "thirdpartylibs/zlib/zlib.h"

#include "libflux.h"
#include "track_generator.h"
#include "libflux.h"
#include "adz_loader.h"
#include "adz_writer.h"
#include "tracks/sector_extractor.h"
#include "uft_compat.h"

// Main writer function
int ADZ_libWrite_DiskFile(LIBFLUX_IMGLDR* imgldr_ctx,LIBFLUX_FLOPPY * floppy,char * filename)
{
	int sect,trk,side,i;
	LIBFLUX_SECTORACCESS* ss;
	LIBFLUX_SECTCFG * scfg;
	int badsect,missingsect;
	const char * badsectmess = "!! BAD SECTOR !!";
	const char * misssectmess= "!!  MISSING   !!";

	int nbsector;
	int nbtrack;
	int nbside;
	int sectorsize;

	gzFile adzfile;

	unsigned int sectorcnt_s0;
	unsigned int sectorcnt_s1;

	sectorsize = 512;

	libflux_imgCallProgressCallback(imgldr_ctx,0,floppy->floppyNumberOfTrack*2 );

	imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"Write ADZ file %s...",filename);

	sectorcnt_s0 = count_sector(imgldr_ctx->ctx,floppy,0,0,0,sectorsize,AMIGA_MFM_ENCODING,0x0000);
	sectorcnt_s1 = count_sector(imgldr_ctx->ctx,floppy,0,0,1,sectorsize,AMIGA_MFM_ENCODING,0x0000);

	if(sectorcnt_s0!=11 && sectorcnt_s0!=22)
	{
		imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"Error : Disk format doesn't match...",filename);
		return LIBFLUX_FILECORRUPTED;
	}

	nbtrack = 85;
	while(nbtrack && !count_sector(imgldr_ctx->ctx,floppy,0,nbtrack-1,0,sectorsize,AMIGA_MFM_ENCODING,0x0000))
	{
		nbtrack--;
	}

	nbside = 1;
	if(sectorcnt_s1)
		nbside = 2;

	nbsector = sectorcnt_s0;

	imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"%d sectors (%d bytes), %d tracks, %d sides...",nbsector,sectorsize,nbtrack,nbside);

	adzfile = gzopen(filename, "wb");
	if (!adzfile)
	{
		imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"gzopen: Error while creating the file!");
		return -1;
	}


	//writeret = write_raw_file(imgldr_ctx,adzfile,floppy,0,nbsector,nbtrack,nbside,sectorsize,AMIGA_MFM_ENCODING,0);

	badsect = 0;
	missingsect = 0;

	ss = libflux_initSectorAccess( imgldr_ctx->ctx, floppy );
	if(ss)
	{
		for( trk = 0 ; trk < nbtrack ; trk++ )
		{
			for( side = 0 ; side < nbside; side++ )
			{
				for( sect = 0 ; sect < nbsector ; sect++ )
				{
					scfg = libflux_searchSector ( ss, trk, side, 0 + sect, AMIGA_MFM_ENCODING );
					if( scfg )
					{
						if( scfg->use_alternate_data_crc || !scfg->input_data )
						{
							badsect++;
						}

						if( ( scfg->sectorsize == sectorsize ) && scfg->input_data )
						{
							gzwrite( adzfile, scfg->input_data, scfg->sectorsize );
						}
						else
						{
							for( i = 0 ; i < sectorsize ; i++ )
							{
								gzputc(adzfile, badsectmess[i&0xF]);
							}
						}

						libflux_freeSectorConfig( ss , scfg );
					}
					else
					{
						missingsect++;
						for( i = 0 ; i < sectorsize ; i++ )
						{
							gzputc(adzfile, misssectmess[i&0xF]);
						}
					}
				}

				libflux_imgCallProgressCallback(imgldr_ctx,trk*2,nbtrack*2 );
			}
		}

		libflux_deinitSectorAccess(ss);
	}

	gzclose(adzfile);

	if(badsect || missingsect)
		return LIBFLUX_FILECORRUPTED;
	else
		return LIBFLUX_NOERROR;
}
