/**
 * @file h17_writer.c
 * @brief Heathkit H17 disk image writer
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
#include <time.h>

#include "types.h"

#include "version.h"

#include "h17_format.h"

#include "types.h"

#include "libflux.h"
#include "track_generator.h"
#include "libflux.h"
#include "h17_loader.h"
#include "h17_writer.h"
#include "tracks/sector_extractor.h"
#include "uft_compat.h"

int write_meta_data_track(FILE * f,LIBFLUX_SECTORACCESS* ss,int32_t startidsector,int32_t sectorpertrack,int32_t trk,int32_t side,int32_t sectorsize,int32_t tracktype, int * badsect, int * missingsect )
{
	int sect;
	LIBFLUX_SECTCFG * scfg;

	h17_sect_metadata sect_meta;

	for( sect = 0 ; sect < sectorpertrack ; sect++ )
	{
		memset(&sect_meta,0,sizeof(sect_meta));

		scfg = libflux_searchSector ( ss, trk, side, startidsector + sect, tracktype );
		if( scfg )
		{
			if( scfg->use_alternate_data_crc || !scfg->input_data )
			{
				*badsect = *badsect + 1;

				if(!scfg->input_data)
					sect_meta.Sector_Status |= 0x10;

				if(scfg->use_alternate_data_crc)
					sect_meta.Sector_Status |= 0x20;
			}

			if( scfg->cylinder != trk )
			{
				sect_meta.Sector_Status |= 0x02;
			}

			sect_meta.Track = scfg->cylinder;
			sect_meta.Sector = scfg->sector;
			sect_meta.Data_Checksum = scfg->data_crc;
			sect_meta.Header_Checksum = scfg->header_crc;
			sect_meta.DSync = scfg->alternate_datamark;
			sect_meta.Volume = (scfg->alternate_addressmark>>8) & 0xFF;
			sect_meta.HSync  =  scfg->alternate_addressmark & 0xFF;
			sect_meta.Valid_Bytes = BIGENDIAN_WORD(256);
			sect_meta.Offset = BIGENDIAN_DWORD( 256 + (((trk * sectorpertrack) + sect) * sectorsize) ) ;

			libflux_freeSectorConfig( ss , scfg );
		}
		else
		{
			*missingsect = *missingsect + 1;

			sect_meta.Track = trk;
			sect_meta.Sector = sect;
			sect_meta.Data_Checksum = 0x0000;
			sect_meta.Header_Checksum = 0x0000;
			sect_meta.DSync = 0x00;
			sect_meta.Volume = 0x00;
			sect_meta.HSync  =  0x00;
			sect_meta.Valid_Bytes = 0;
			sect_meta.Offset = 0;
			sect_meta.Sector_Status = 0x59;
		}

		if (fwrite(&sect_meta,sizeof(sect_meta),1,f) != 1) { /* I/O error */ }
	}

	return 0;
}

int write_meta_data(LIBFLUX_IMGLDR * imgldr_ctx,FILE * f,LIBFLUX_FLOPPY * fp,int32_t startidsector,int32_t sectorpertrack,int32_t nboftrack,int32_t nbofside,int32_t sectorsize,int32_t tracktype,int32_t sidefilelayout)
{
	int trk,side;
	LIBFLUX_SECTORACCESS* ss;
	int badsect,missingsect;

	badsect = 0;
	missingsect = 0;

	if(f && fp)
	{
		ss = libflux_initSectorAccess( imgldr_ctx->ctx, fp );
		if(ss)
		{
			for( trk = 0 ; trk < nboftrack ; trk++ )
			{
				for( side = 0 ; side < nbofside; side++ )
				{
					write_meta_data_track(f,ss,startidsector,sectorpertrack,trk,side,sectorsize,tracktype,&badsect,&missingsect );
				}
			}

			libflux_deinitSectorAccess(ss);
		}
	}

	if(badsect || missingsect)
		return LIBFLUX_FILECORRUPTED;
	else
		return LIBFLUX_NOERROR;
}

// Main writer function
int H17_libWrite_DiskFile(LIBFLUX_IMGLDR* imgldr_ctx,LIBFLUX_FLOPPY * floppy,char * filename)
{
	int nbsector;
	int nbtrack;
	int nbside;
	int sectorsize;

	int writeret;
	int offset;
	int offset2;

	FILE * h8dfile;

	unsigned int sectorcnt_s0;
	unsigned int sectorcnt_s1;

	h17_block blk;
	h17_DskF DskF;
	h17_Parm Parm;
	h17_header hdr;

	struct tm * ts;
	time_t currenttime;

	char str_tmp[512];

	sectorsize = 256;

	libflux_imgCallProgressCallback(imgldr_ctx,0,floppy->floppyNumberOfTrack*2 );

	imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"Write H17 Heathkit file %s...",filename);

	sectorcnt_s0 = count_sector(imgldr_ctx->ctx,floppy,0,0,0,sectorsize,HEATHKIT_HS_FM_ENCODING,0x0000);
	sectorcnt_s1 = count_sector(imgldr_ctx->ctx,floppy,0,0,1,sectorsize,HEATHKIT_HS_FM_ENCODING,0x0000);

	writeret = LIBFLUX_ACCESSERROR;

	h8dfile = libflux_fopen(filename,"wb");
	if(h8dfile)
	{
		memcpy((void*)&hdr.file_tag,"H17D",4);
		memcpy((void*)&hdr.version,"200",3);
		hdr.check = 0xFF;
		if (fwrite(&hdr,sizeof(hdr),1,h8dfile) != 1) { /* I/O error */ }
		if(sectorcnt_s0 != 10)
		{
			imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"Error : Disk format doesn't match...",filename);
			return LIBFLUX_FILECORRUPTED;
		}

		nbtrack = 40;
		while(nbtrack && !count_sector(imgldr_ctx->ctx,floppy,0,nbtrack-1,0,sectorsize,HEATHKIT_HS_FM_ENCODING,0x0000))
		{
			nbtrack--;
		}

		nbside = 1;
		if(sectorcnt_s1)
			nbside = 2;

		blk.id = BLKID_DskF;
		blk.lenght = BIGENDIAN_DWORD(sizeof(DskF));
		if (fwrite(&blk,sizeof(blk),1,h8dfile) != 1) { /* I/O error */ }
		DskF.Tracks = nbtrack;
		DskF.Sides = nbside;
		DskF.ReadOnly = 0;
		if (fwrite(&DskF,sizeof(DskF),1,h8dfile) != 1) { /* I/O error */ }
		blk.id = BLKID_Parm;
		blk.lenght = BIGENDIAN_DWORD(sizeof(Parm));
		if (fwrite(&blk,sizeof(blk),1,h8dfile) != 1) { /* I/O error */ }
		Parm.Distribution_Disk = 0;
		Parm.Source_of_Header_Data = 0;
		if (fwrite(&Parm,sizeof(Parm),1,h8dfile) != 1) { /* I/O error */ }
		blk.id = BLKID_Prog;
		blk.lenght = BIGENDIAN_DWORD(strlen(str_tmp));
		if (fwrite(&blk,sizeof(blk),1,h8dfile) != 1) { /* I/O error */ }
		if (fwrite(&str_tmp,strlen(str_tmp),1,h8dfile) != 1) { /* I/O error */ }
		str_tmp[0] = '\0';
		blk.id = BLKID_Imgr;
		blk.lenght = BIGENDIAN_DWORD(strlen(str_tmp));
		if (fwrite(&blk,sizeof(blk),1,h8dfile) != 1) { /* I/O error */ }
		if (fwrite(&str_tmp,strlen(str_tmp),1,h8dfile) != 1) { /* I/O error */ }
		currenttime=time (NULL);
		ts=localtime(&currenttime);

		snprintf(str_tmp, sizeof(str_tmp),"%.4d-%.2d-%.2dT%.2d:%.2d:%.2d,000Z",ts->tm_year+1900,ts->tm_mon,ts->tm_mday,ts->tm_hour,ts->tm_min,ts->tm_sec);
		blk.id = BLKID_Date;
		blk.lenght = BIGENDIAN_DWORD(strlen(str_tmp));
		if (fwrite(&blk,sizeof(blk),1,h8dfile) != 1) { /* I/O error */ }
		if (fwrite(&str_tmp,strlen(str_tmp),1,h8dfile) != 1) { /* I/O error */ }
		offset = ftell(h8dfile);

		if( (offset + sizeof(blk)) & 0xFF )
		{
			blk.id = BLKID_Padd;
			offset += sizeof(blk);
			blk.lenght = BIGENDIAN_DWORD( (0x100 - ((offset + sizeof(blk)) & 0xFF)) & 0xFF );
			if (fwrite(&blk,sizeof(blk),1,h8dfile) != 1) { /* I/O error */ }
			memset(str_tmp,0,sizeof(str_tmp));
			if (fwrite(&str_tmp,BIGENDIAN_DWORD(blk.lenght),1,h8dfile) != 1) { /* I/O error */ }
		}

		blk.id = BLKID_H8DB;
		blk.lenght = 0;
		if (fwrite(&blk,sizeof(blk),1,h8dfile) != 1) { /* I/O error */ }
		offset = ftell(h8dfile);

		nbsector = sectorcnt_s0;

		imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"%d sectors (%d bytes), %d tracks, %d sides...",nbsector,sectorsize,nbtrack,nbside);

		writeret = write_raw_file(imgldr_ctx,h8dfile,floppy,0,nbsector,nbtrack,nbside,sectorsize,HEATHKIT_HS_FM_ENCODING,1);

		offset2 = ftell(h8dfile);
		// update size
		if (fseek(h8dfile,offset - sizeof(blk),SEEK_SET) != 0) { /* seek error */ }
		blk.lenght = BIGENDIAN_DWORD( offset2 - offset );
		if (fwrite(&blk,sizeof(blk),1,h8dfile) != 1) { /* I/O error */ }
		if (fseek(h8dfile,0,SEEK_END) != 0) { /* seek error */ }
		blk.id = BLKID_SecM;
		blk.lenght = 0;
		if (fwrite(&blk,sizeof(blk),1,h8dfile) != 1) { /* I/O error */ }
		offset = ftell(h8dfile);

		writeret = write_meta_data(imgldr_ctx,h8dfile,floppy,0,nbsector,nbtrack,nbside,sectorsize,HEATHKIT_HS_FM_ENCODING,1);

		offset2 = ftell(h8dfile);

		// update size
		if (fseek(h8dfile,offset - sizeof(blk),SEEK_SET) != 0) { /* seek error */ }
		blk.lenght = BIGENDIAN_DWORD( offset2 - offset );
		if (fwrite(&blk,sizeof(blk),1,h8dfile) != 1) { /* I/O error */ }
		if (fseek(h8dfile,0,SEEK_END) != 0) { /* seek error */ }
		libflux_fclose(h8dfile);
	}

	return writeret;
}
