/**
 * @file d88_writer.c
 * @brief NEC PC-88/98 D88 image writer
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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "types.h"

#include "libflux.h"
#include "track_generator.h"
#include "libflux.h"

#include "d88_format.h"
#include "d88_writer.h"

#include "tracks/sector_extractor.h"

#include "uft_compat.h"
#include "uft_floppy_utils.h"

int D88_libWrite_DiskFile(LIBFLUX_IMGLDR* imgldr_ctx,LIBFLUX_FLOPPY * floppy,char * filename)
{
	int i,j,k;
	int32_t nbsector;
	FILE * outfile;
	char * log_str;
	int density;
	d88_fileheader d88_fh;
	d88_sector d88_s;
	uint32_t tracktable[164];

	int mfmdd_found;
	int mfmhd_found;
	//int fmdd_found;
	//int fmhd_found;

	int maxtrack;

	int nb_valid_sector;

	LIBFLUX_SECTORACCESS* ss;
	LIBFLUX_SECTCFG** sca;

	imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"Write D88 file %s...",filename);

	mfmdd_found = 0;
	mfmhd_found = 0;
	//fmdd_found = 0;
	//fmhd_found = 0;
	maxtrack = 0;

	outfile = libflux_fopen(filename,"wb");
	if( !outfile )
	{
		imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"Cannot create %s !",filename);
		return LIBFLUX_ACCESSERROR;
	}

	memset(&d88_fh,0,sizeof(d88_fileheader));
	memcpy(d88_fh.name, "UFTOOL", 6);  /* Fixed tag */
	if (fwrite(&d88_fh,sizeof(d88_fileheader),1,outfile) != 1) { /* I/O error */ }
	memset(tracktable,0,sizeof(tracktable));
	if (fwrite(&tracktable, sizeof(tracktable),1,outfile) != 1) { /* I/O error */ }
	ss = libflux_initSectorAccess(imgldr_ctx->ctx,floppy);
	if( !ss )
		goto error;

	for(j=0;j<(int)floppy->floppyNumberOfTrack;j++)
	{
		for(i=0;i<(int)floppy->floppyNumberOfSide;i++)
		{
			libflux_imgCallProgressCallback(imgldr_ctx,(j<<1) + (i&1),2*floppy->floppyNumberOfTrack);

			log_str = libflux_dyn_sprintfcat(NULL,"track:%.2d:%d file offset:0x%.6x, sectors: ",j,i,(unsigned int)ftell(outfile));

			if((j<<1 | i) <164)
			{
				sca = libflux_getAllTrackISOSectors(ss,j,i,&nbsector);
				if(sca)
				{
					tracktable[(j<<1 | i)] = ftell(outfile);

					nb_valid_sector = 0;
					for(k = 0; k < nbsector; k++)
					{
						if(sca[k]->sectorsize)
						{
							nb_valid_sector++;
						}
					}

					for(k = 0; k < nbsector; k++)
					{
						if(sca[k]->sectorsize)
						{
							memset(&d88_s,0,sizeof(d88_sector));

							density = 0;

							if(sca[k]->trackencoding == ISOFORMAT_DD )
							{
								density = 1;

								if(sca[k]->bitrate>400000)
									mfmhd_found++;
								else
									mfmdd_found++;
							}

							if( sca[k]->trackencoding == ISOFORMAT_SD )
							{
								/*
								if(sca[k]->bitrate>400000)
									fmhd_found++;
								else
									fmdd_found++;
								*/
							}

							d88_s.cylinder = sca[k]->cylinder;
							d88_s.head = sca[k]->head;
							d88_s.number_of_sectors = nb_valid_sector;
							d88_s.sector_id = sca[k]->sector;

							if(sca[k]->use_alternate_datamark && sca[k]->alternate_datamark == 0xF8)
								d88_s.deleted_data = 0x10;

							if(!density)
								d88_s.density = 0x40;

							d88_s.sector_size = size_to_code(sca[k]->sectorsize);
							d88_s.sector_length = sca[k]->sectorsize;
							if(!sca[k]->input_data)
								d88_s.sector_length = 0;

							if( sca[k]->alternate_addressmark != 0xFE )
							{
								d88_s.sector_status = 0xE0; // NO ADDRESS MARK
							}
							else
							{
								if( sca[k]->use_alternate_header_crc )
								{
									d88_s.sector_status = 0xA0; // ID CRC ERROR
								}
								else
								{
									if( sca[k]->alternate_datamark != 0xFB && sca[k]->alternate_datamark != 0xF8 )
									{
										d88_s.sector_status = 0xF0; // NO DATA MARK
									}
									else
									{
										if( sca[k]->use_alternate_data_crc )
										{
											d88_s.sector_status = 0xB0; // DATA CRC ERROR
										}
										else
										{
											if( sca[k]->alternate_datamark == 0xF8 )
												d88_s.sector_status = 0x10; // DELETED SECTOR
										}
									}
								}
							}

							if (fwrite(&d88_s,sizeof(d88_sector),1,outfile) != 1) { /* I/O error */ }
							if(d88_s.sector_length)
								if (fwrite(sca[k]->input_data,sca[k]->sectorsize,1,outfile) != 1) { /* I/O error */ }
						}

						libflux_freeSectorConfig( 0, sca[k]);
					}

					if(i==0)
						maxtrack++;

					free(sca);
				}
			}

			if(log_str)
				imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,log_str);

			free(log_str);
		}
	}

	libflux_deinitSectorAccess(ss);

	// Media flag. 00h = 2D, 10h = 2DD, 20h = 2HD.
	if( (maxtrack >= 46) )
	{
		// 2DD : 300RPM, 250Kb/s, 2 sides, 80 tracks floppy format
		// 2DD
		d88_fh.media_flag = 0x10;
	}

	if( (maxtrack >= 46) && (mfmhd_found>mfmdd_found))
	{
		// 2HD : 360RPM, 500Kb/s, 2 sides, 77 tracks floppy format
		// 2HD
		d88_fh.media_flag = 0x20;
	}

	if(maxtrack < 46)
	{
		// 2D : 300RPM, 250Kb/s, 2 sides, 40 tracks floppy format .
		// 2D
		d88_fh.media_flag = 0x00;
	}

	d88_fh.file_size = libflux_fgetsize(outfile);
	if (fseek(outfile,0,SEEK_SET) != 0) { /* seek error */ }
	if (fwrite(&d88_fh,sizeof(d88_fileheader),1,outfile) != 1) { /* I/O error */ }
	if (fwrite(&tracktable, sizeof(tracktable),1,outfile) != 1) { /* I/O error */ }
	libflux_fclose(outfile);

	return LIBFLUX_NOERROR;

error:
	if(outfile)
		libflux_fclose(outfile);

	return LIBFLUX_INTERNALERROR;
}
