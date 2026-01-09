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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "types.h"

#include "libflux.h"
#include "track_generator.h"
#include "libflux.h"

#include "adf_loader.h"

#include "tracks/sector_extractor.h"

#include "uft_compat.h"

int ADF_libWrite_DiskFile(LIBFLUX_IMGLDR* imgldr_ctx,LIBFLUX_FLOPPY * floppy,char * filename)
{
	int i,j,k,s;
	FILE * rawfile;
	unsigned char blankblock[512];
	int sectorsize,maxsect;

	LIBFLUX_SECTORACCESS* ss;
	LIBFLUX_SECTCFG* sc;

	imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"Write ADF file %s...",filename);

	memset(blankblock,0x00,sizeof(blankblock));

	if((floppy->floppyNumberOfTrack < 80) || (floppy->floppyNumberOfSide != 2) )
	{
		return LIBFLUX_BADPARAMETER;
	}

	rawfile=libflux_fopen(filename,"wb");
	if(rawfile)
	{
		ss=libflux_initSectorAccess(imgldr_ctx->ctx,floppy);
		if(ss)
		{
			for(j=0;j<80;j++)
			{
				for(i=0;i<2;i++)
				{

					libflux_imgCallProgressCallback(imgldr_ctx,(j<<1) | (i&1),80*2 );

					maxsect = 0;

					for(s=0;s<22;s++)
					{
						sc = libflux_searchSector (ss,j,i,s,AMIGA_MFM_ENCODING);
						if(sc)
						{
							if( s > maxsect )
								maxsect = s;

							libflux_freeSectorConfig( ss, sc );
						}
					}

					if( maxsect > 11 )
						maxsect = 22;
					else
						maxsect = 11;

					for(s=0;s<maxsect;s++)
					{
						sc = libflux_searchSector (ss,j,i,s,AMIGA_MFM_ENCODING);

						if(sc)
						{
							sectorsize = sc->sectorsize;
							if(sectorsize == 512)
							{
								if (fwrite(sc->input_data,sectorsize,1,rawfile) != 1) { /* I/O error */ }
							}
							else
							{
								memset(blankblock,0x00,sizeof(blankblock));
								for(k=0;k<32;k++)
									strncat((char*)blankblock, ">MISSING BLOCK<!", 16);
								if (fwrite(blankblock,sizeof(blankblock),1,rawfile) != 1) { /* I/O error */ }
							}

							libflux_freeSectorConfig( ss, sc );
						}
						else
						{
							imgldr_ctx->ctx->libflux_printf(MSG_WARNING,"T%.2dH%dS%d : Amiga Sector not found !?!...",j,i,s);
							// Sector Not found ?!?
							// Put a blank data sector instead...
							memset(blankblock,0x00,sizeof(blankblock));
							for(k=0;k<31;k++)
								strncat((char*)blankblock, ">MISSING BLOCK<!", 16);
							if (fwrite(blankblock,sizeof(blankblock),1,rawfile) != 1) { /* I/O error */ }
						}
					}
				}
			}
			libflux_deinitSectorAccess(ss);
		}

		libflux_fclose(rawfile);

		return LIBFLUX_NOERROR;

	}

	return LIBFLUX_ACCESSERROR;
}
