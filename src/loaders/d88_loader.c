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
// File : d88_loader.c
// Contains: D88 floppy image loader.
//
// Written by: Jean-François DEL NERO
//
// Change History (most recent first):
///////////////////////////////////////////////////////////////////////////////////

#include "uft/uft_safe_io.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "types.h"

#include "libflux.h""
#include "tracks/track_generator.h"
#include "libflux.h""

#include "uft_floppy_loader.h"
#include "uft_floppy_utils.h"

#include "d88_loader.h"
#include "d88_format.h"

#include "d88_writer.h"

#include "uft_compat.h"

int D88_libIsValidDiskFile( LIBFLUX_IMGLDR * imgldr_ctx, LIBFLUX_IMGLDR_FILEINFOS * imgfile )
{
	imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"D88_libIsValidDiskFile");

	if( libflux_checkfileext(imgfile->path,"d88",SYS_PATH_TYPE) ||
		libflux_checkfileext(imgfile->path,"d77",SYS_PATH_TYPE) ||
		libflux_checkfileext(imgfile->path,"88d",SYS_PATH_TYPE) ||
		libflux_checkfileext(imgfile->path,"d8u",SYS_PATH_TYPE) ||
		libflux_checkfileext(imgfile->path,"2d",SYS_PATH_TYPE)  ||
		libflux_checkfileext(imgfile->path,"d68",SYS_PATH_TYPE) )
	{
		imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"D88_libIsValidDiskFile : D88 file !");
		return LIBFLUX_VALIDFILE;
	}

	else
	{
		imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"D88_libIsValidDiskFile : non D88 file !");
		return LIBFLUX_BADFILE;
	}
}

int D88_libLoad_DiskFile(LIBFLUX_IMGLDR * imgldr_ctx,LIBFLUX_FLOPPY * floppydisk,char * imgfile,void * parameters)
{
	FILE * f;
	d88_fileheader fileheader;
	d88_sector sectorheader;
	int i,j;
	unsigned short rpm;
	unsigned char tracktype,side,interleave;
	LIBFLUX_SECTCFG* sectorconfig;
	LIBFLUX_CYLINDER* currentcylinder;
	unsigned int bitrate;
	int indexfile;
	uint32_t tracklen;
	int number_of_track,number_of_sector;
	uint32_t track_offset;
	char * indexstr;
	char str_file[512];
	int basefileptr;
	int totalfilesize,truetotalfilesize,partcount;

	imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"D88_libLoad_DiskFile %s",imgfile);

	indexfile=0;
	basefileptr=0;
	snprintf(str_file, sizeof(str_file),"%s",imgfile);
	indexstr=strstr( str_file,".d88" );
	if(indexstr)
	{
		indexstr=strstr( indexstr," " );
		if(indexstr)
		{
			if(indexstr[1]>='0' && indexstr[1]<='9')
			{
				indexfile=indexstr[1]-'0';
				indexstr[0]=0;
			}
		}
	}

	f = libflux_fopen(str_file,"rb");
	if( f == NULL )
	{
		imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"Cannot open %s !",imgfile);
		return LIBFLUX_ACCESSERROR;
	}

	//////////////////////////////////////////////////////
	// sanity check
	imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"Floppy disk in this file :");
	partcount=0;

	truetotalfilesize = libflux_fgetsize(f);

	totalfilesize=0;
	while((!feof(f)) && (partcount<256) && (totalfilesize<truetotalfilesize))
	{
		libflux_fread(&fileheader,sizeof(fileheader),f);
		if(fileheader.file_size)
		{
			imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"%s",fileheader.name);

			if (fseek(f,fileheader.file_size-sizeof(fileheader),SEEK_CUR) != 0) { /* I/O error */ }
			totalfilesize=totalfilesize+fileheader.file_size;
			partcount++;
		}
		else
		{
			truetotalfilesize=totalfilesize;
			if (fseek(f,truetotalfilesize,SEEK_SET) != 0) { /* I/O error */ }
		}
	}

	if((totalfilesize!=ftell(f)) || partcount==256)
	{
		// bad total size !
		imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"Bad D88 file size !",imgfile);
		libflux_fclose(f);
		return LIBFLUX_BADFILE;
	}

	imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"%d floppy in this file.",partcount);
	if (fseek(f,0,SEEK_SET) != 0) { /* I/O error */ }
	//////////////////////////////////////////////////////

	//////////////////////////////////////////////////////
	// Floppy selection
	if(indexfile>=partcount)
	{
		imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"bad selection index (%d). there are only %d disk(s) in this file!",indexfile,partcount);
		libflux_fclose(f);
		return LIBFLUX_ACCESSERROR;
	}

	while(indexfile)
	{
		libflux_fread(&fileheader,sizeof(fileheader),f);
		if (fseek(f,fileheader.file_size-sizeof(fileheader),SEEK_CUR) != 0) { /* I/O error */ }
		indexfile--;
	}

	basefileptr=ftell(f);
	//////////////////////////////////////////////////////

	//////////////////////////////////////////////////////
	// Header read
	libflux_fread(&fileheader,sizeof(fileheader),f);
	fileheader.reserved[0]=0;

	imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"Opening %s (%d), part %s , part size:%d",imgfile,indexfile,fileheader.name,fileheader.file_size);
	switch(fileheader.media_flag)
	{
		case 0x00: // 2D
			imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"2D disk");
			tracktype=IBMFORMAT_SD;
			bitrate=250000;
			side=2;
			break;
		case 0x10: // 2DD
			imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"2DD disk");
			tracktype=IBMFORMAT_DD;
			bitrate=250000;
			side=2;
			break;
		case 0x20: // 2HD
			imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"2HD disk");
			tracktype=IBMFORMAT_DD;
			bitrate=500000;
			side=2;
			break;
		case 0x40: // 1DD
			imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"1DD disk");
			tracktype=IBMFORMAT_DD;
			bitrate=250000;
			side=1;
			break;

		default:
			side=2;
			imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"unknow disk: %.2X !",fileheader.media_flag);
			libflux_fclose(f);
			return LIBFLUX_BADFILE;
			break;
	}

	if(fileheader.write_protect & 0x10)
	{
		imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"write protected disk");
	}

	if (fseek(f,basefileptr+sizeof(d88_fileheader),SEEK_SET) != 0) { /* I/O error */ }

	track_offset = 0;
	number_of_track = 0;

	if(libflux_fread(&track_offset,sizeof(uint32_t),f) <= 0)
	{
		imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"Can't read track(s) offset ?");
		libflux_fclose(f);
		return LIBFLUX_BADFILE;
	}

	if(track_offset >= (sizeof(d88_fileheader) + sizeof(uint32_t)) )
	{
		// Get the real number of tracks...
		number_of_track = (track_offset - sizeof(d88_fileheader))/sizeof(uint32_t);
		do
		{
			if (fseek(f,basefileptr+sizeof(d88_fileheader)+((number_of_track-1)*sizeof(uint32_t)),SEEK_SET) != 0) { /* I/O error */ }
			libflux_fread(&track_offset,sizeof(uint32_t),f);
			if(!track_offset)
			{
				number_of_track--;
			}
		} while(number_of_track>0 && !track_offset);
	}

	if(number_of_track <= 0)
	{
		imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"No track to load ?");
		libflux_fclose(f);
		return LIBFLUX_BADFILE;
	}

	if( ( number_of_track > 60*2 ) && ( number_of_track < 80*2 ) )
	{
		number_of_track = 80 * 2;
	}

	if( ( number_of_track > 25*2 ) && ( number_of_track < 40*2 ) )
	{
		number_of_track = 40 * 2;
	}

	if( ( number_of_track & 1 )  && (side == 2) )
		number_of_track++;

	imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"Number of track: %d",number_of_track);

	if (fseek(f,basefileptr+sizeof(d88_fileheader),SEEK_SET) != 0) { /* I/O error */ }

	libflux_fread(&track_offset,sizeof(uint32_t),f);
	imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"first track offset:%X",track_offset);


	floppydisk->floppyNumberOfTrack=number_of_track;
	floppydisk->tracks=(LIBFLUX_CYLINDER**)malloc(sizeof(LIBFLUX_CYLINDER*)*floppydisk->floppyNumberOfTrack);
	if(!floppydisk->tracks) { fclose(f); return LIBFLUX_INTERNALERROR; }  /* P0-SEC-001 */
	memset(floppydisk->tracks,0,sizeof(LIBFLUX_CYLINDER*)*floppydisk->floppyNumberOfTrack);

	floppydisk->floppyBitRate=bitrate;
	floppydisk->floppyiftype=GENERIC_SHUGART_DD_FLOPPYMODE;
	floppydisk->floppyNumberOfSide=side;
	floppydisk->floppySectorPerTrack=-1; // default value
	interleave=1;
	rpm=300;

	imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"%d tracks, %d Side(s)\n",floppydisk->floppyNumberOfTrack,floppydisk->floppyNumberOfSide);

	if(side==1)
		number_of_track=number_of_track*2;

	i=0;
	do
	{
		libflux_imgCallProgressCallback(imgldr_ctx,i,number_of_track);

		if(track_offset)
		{
			if (fseek(f,track_offset+basefileptr,SEEK_SET) != 0) { /* I/O error */ }
			libflux_fread(&sectorheader,sizeof(d88_sector),f);

			number_of_sector=sectorheader.number_of_sectors;
			imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"Number of sector: %d",number_of_sector);
			if(sectorheader.density&0x40)
			{
				tracktype=IBMFORMAT_SD;
			}
			else
			{
				tracktype=IBMFORMAT_DD;
			}

			sectorconfig=0;
			if(number_of_sector)
			{
				sectorconfig=(LIBFLUX_SECTCFG*)malloc(sizeof(LIBFLUX_SECTCFG)*number_of_sector);
				if(!sectorconfig) { fclose(f); return LIBFLUX_INTERNALERROR; }  /* P0-SEC-001 */
				memset(sectorconfig,0,sizeof(LIBFLUX_SECTCFG)*number_of_sector);

				j=0;
				do
				{

					if(!sectorheader.sector_length)
					{
						sectorheader.sector_length = (unsigned short)(128 * (1 << sectorheader.sector_size));
					}

					imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"Cylinder:%.3d, Head:%d, Size:%.1d (%d), Sector ID:%.3d, Status:0x%.2x, Density: %d, Deleted Data: %d, File offset:0x%.6x",
						sectorheader.cylinder,
						sectorheader.head,
						sectorheader.sector_length,
						sectorheader.sector_size,
						sectorheader.sector_id,
						sectorheader.sector_status,
						sectorheader.density,
						sectorheader.deleted_data,

						ftell(f)
						);

					sectorconfig[j].cylinder=sectorheader.cylinder;
					sectorconfig[j].head=sectorheader.head;
					sectorconfig[j].sector=sectorheader.sector_id;
					sectorconfig[j].sectorsize=sectorheader.sector_length;
					sectorconfig[j].gap3 = 255;
					if(number_of_sector>16)
						sectorconfig[j].gap3 = 22; // Silpheed (18 sectors) gap3 issue

					sectorconfig[j].trackencoding=tracktype;
					sectorconfig[j].input_data=malloc(sectorheader.sector_length);
					sectorconfig[j].bitrate=floppydisk->floppyBitRate;

					libflux_fread(sectorconfig[j].input_data,sectorheader.sector_length,f);

					if(sectorheader.deleted_data == 0x10)
					{
						sectorconfig[j].use_alternate_datamark = 0xFF;
						sectorconfig[j].alternate_datamark = 0xF8;
					}

					switch( sectorheader.sector_status & 0xF0)
					{
						case 0x00:
							// Normal
						break;
						case 0x10:
							// Deleted
							sectorconfig[j].use_alternate_datamark = 0xFF;
							sectorconfig[j].alternate_datamark = 0xF8;
						break;
						case 0xA0:
							// Bad ID CRC
							sectorconfig[j].use_alternate_header_crc = 0x01;
							sectorconfig[j].header_crc = 0xAA55;
						break;
						case 0xB0:
							// Bad DATA CRC
							sectorconfig[j].use_alternate_data_crc = 0x01;
							sectorconfig[j].data_crc = 0xAA55;
						break;
						case 0xE0:
							//no address mark
							sectorconfig[j].use_alternate_addressmark = 0xFF;
							sectorconfig[j].alternate_addressmark = 0x80;

						break;
						case 0xF0:
							//no data mark
							sectorconfig[j].use_alternate_datamark = 0xFF;
							sectorconfig[j].alternate_datamark = 0x80;
						break;
					}

					// fread datas
					libflux_fread(&sectorheader,sizeof(d88_sector),f);

					j++;
				}while(j<number_of_sector);
			}

			if(!floppydisk->tracks[i>>1])
			{
				floppydisk->tracks[i>>1]=allocCylinderEntry(rpm,floppydisk->floppyNumberOfSide);
			}
			currentcylinder=floppydisk->tracks[i>>1];

			currentcylinder->floppyRPM=rpm;
			currentcylinder->sides[i&1]=tg_generateTrackEx((unsigned short)number_of_sector,sectorconfig,interleave,0,floppydisk->floppyBitRate,rpm,tracktype,0,2500 | NO_SECTOR_UNDER_INDEX,-2500);

			for(j=0;j<number_of_sector;j++)
			{
				libflux_freeSectorConfigData( 0, &sectorconfig[j] );
			}

			if(sectorconfig)
				free(sectorconfig);

		}
		else
		{
			imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"Unformated track:%.3d",i);

			if(!floppydisk->tracks[i>>1])
			{
				floppydisk->tracks[i>>1]=allocCylinderEntry(rpm,floppydisk->floppyNumberOfSide);
			}
			currentcylinder=floppydisk->tracks[i>>1];

			tracklen=((bitrate/(rpm/60))/4)*8;
			currentcylinder->floppyRPM=rpm;
			currentcylinder->sides[i&1]=tg_alloctrack(floppydisk->floppyBitRate,ISOIBM_MFM_ENCODING,currentcylinder->floppyRPM,tracklen,2500,-2500,TG_ALLOCTRACK_ALLOCFLAKEYBUFFER|TG_ALLOCTRACK_RANDOMIZEDATABUFFER|TG_ALLOCTRACK_UNFORMATEDBUFFER);
		}

		if(side==2)
		{
			i++;
			imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"Track %d offset: 0x%X",i,track_offset);
			if (fseek(f,basefileptr + sizeof(d88_fileheader)  + (i * sizeof(uint32_t)),SEEK_SET) != 0) { /* I/O error */ }
		}
		else
		{
			i=i+2;
			imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"Track %d offset: 0x%X",i>>1,track_offset);
			if (fseek(f,basefileptr + sizeof(d88_fileheader)  + ((i>>1) * sizeof(uint32_t)),SEEK_SET) != 0) { /* I/O error */ }
		}

		libflux_fread(&track_offset,sizeof(uint32_t),f);

	}while(i<number_of_track);

	if(side==2)
		floppydisk->floppyNumberOfTrack=i/2;


	libflux_fclose(f);

	libflux_sanityCheck(imgldr_ctx->ctx,floppydisk);

	//imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"bad header");
	return LIBFLUX_NOERROR;
}

int D88_libGetPluginInfo(LIBFLUX_IMGLDR * imgldr_ctx,uint32_t infotype,void * returnvalue)
{
	static const char plug_id[]="NEC_D88";
	static const char plug_desc[]="NEC D88 Loader";
	static const char plug_ext[]="d88";

	plugins_ptr plug_funcs=
	{
		(ISVALIDDISKFILE)   D88_libIsValidDiskFile,
		(LOADDISKFILE)      D88_libLoad_DiskFile,
		(WRITEDISKFILE)     D88_libWrite_DiskFile,
		(GETPLUGININFOS)    D88_libGetPluginInfo
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
