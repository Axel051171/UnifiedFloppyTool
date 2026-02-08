/**
 * @file teledisk_loader.c
 * @brief Teledisk TD0 loader (standalone)
 * @version 3.8.0
 */
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
// File : teledisk_loader.c
// Contains: Teledisk (TD0) floppy image loader
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
#include "track_generator.h"
#include "libflux.h"

#include "uft_floppy_loader.h"
#include "uft_floppy_utils.h"

#include "teledisk_loader.h"

#include "teledisk_format.h"
#include "td0_lzss.h"

#include "tracks/crc.h"

#include "uft_compat.h"

int TeleDisk_libIsValidDiskFile( LIBFLUX_IMGLDR * imgldr_ctx, LIBFLUX_IMGLDR_FILEINFOS * imgfile )
{
	int i;
	unsigned char crctable[32];
	unsigned char CRC16_High,CRC16_Low;
	TELEDISK_HEADER * td_header;
	unsigned char * ptr;

	imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"TeleDisk_libIsValidDiskFile");
	if(imgfile)
	{
		td_header = (TELEDISK_HEADER *)&imgfile->file_header;

		if ( ((td_header->TXT[0]!='t') || (td_header->TXT[1]!='d')) && ((td_header->TXT[0]!='T') || (td_header->TXT[1]!='D')))
		{
			imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"TeleDisk_libIsValidDiskFile : bad header tag !");
			return LIBFLUX_BADFILE;
		}

		CRC16_Init(&CRC16_High,&CRC16_Low,(unsigned char*)crctable,0xA097,0x0000);
		ptr=(unsigned char*)td_header;
		for(i=0;i<0xA;i++)
		{
			CRC16_Update(&CRC16_High,&CRC16_Low, ptr[i],(unsigned char*)crctable );
		}

		if(((td_header->CRC[1]<<8)|td_header->CRC[0]) != ((CRC16_High<<8)|CRC16_Low))
		{
			imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"bad header crc !");
			return LIBFLUX_BADFILE;
		}

		imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"TeleDisk_libIsValidDiskFile : it's a TeleDisk file!");

		return LIBFLUX_VALIDFILE;
	}

	return LIBFLUX_BADPARAMETER;
}


int RLEExpander(unsigned char *src,unsigned char *dst,int blocklen)
{
	unsigned char *s1,*s2,d1,d2;
	unsigned char type;
	unsigned short len,len2,rlen;

	unsigned int uBlock;
	unsigned int uCount;

	if( !dst )
		return -1;

	s2 = dst;

	len=(src[1]<<8)|src[0];
	type = src[2];
	len--;
	src += 3;

	switch ( type )
	{
		case 0:
			memcpy(dst,src,len);
			rlen=len;
		break;

		case 1:
			len = (src[1]<<8) | src[0];
			rlen = len<<1;
			d1 = src[2];
			d2 = src[3];
			while (len--)
			{
				*dst++ = d1;
				*dst++ = d2;
			}
			src=src+4;
		break;

		case 2:
			rlen=0;
			len2=len;
			s1=&src[0];
			s2=dst;
			do
			{
				if( !s1[0])
				{
					len2--;

					len=s1[1];
					s1 += 2;
					len2--;

					memcpy(s2,s1,len);
					rlen=rlen+len;
					s2 += len;
					s1 += len;

					len2=len2-len;
				}
				else
				{
					uBlock = 1<<*s1;
					s1++;
					len2--;

					uCount = *s1;
					s1++;
					len2--;

					while(uCount)
					{
						memcpy(s2, s1, uBlock);
						rlen=rlen+uBlock;
						s2 += uBlock;
						uCount--;
					}

					s1 += uBlock;
					len2 = len2-uBlock;
				}

			}while(len2);
		break;

		default:
			rlen=-1;
		break;
	}

	return rlen;
}


int TeleDisk_libLoad_DiskFile(LIBFLUX_IMGLDR * imgldr_ctx,LIBFLUX_FLOPPY * floppydisk,char * imgfile,void * parameters)
{
	FILE * f;
	unsigned int i;
	unsigned int file_offset;
	unsigned char interleave,trackformat;
	unsigned short rpm;
	int numberoftrack,sidenumber;
	unsigned short * datalen;
	LIBFLUX_CYLINDER* currentcylinder;
	LIBFLUX_SIDE* currentside;
	TELEDISK_HEADER        *td_header;
	TELEDISK_TRACK_HEADER  *td_track_header;
	TELEDISK_SECTOR_HEADER *td_sector_header;
	TELEDISK_COMMENT * td_comment;
	unsigned char tempdata[8*1024];
	unsigned char crctable[32];
	unsigned char CRC16_High,CRC16_Low;
	unsigned char * ptr;
	uint32_t filesize;
	LIBFLUX_SECTCFG  * sectorconfig;
	unsigned char * fileimage;
	uint32_t fileimage_buffer_offset;

	fileimage = NULL;
	sectorconfig = NULL;

	imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"TeleDisk_libLoad_DiskFile %s",imgfile);

	libflux_imgCallProgressCallback(imgldr_ctx,0,100 );

	f = libflux_fopen(imgfile,"rb");
	if( f == NULL )
	{
		imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"TeleDisk_libLoad_DiskFile : Cannot open %s !",imgfile);
		return LIBFLUX_ACCESSERROR;
	}

	filesize = libflux_fgetsize(f);

	if( !filesize )
	{
		imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"TeleDisk_libLoad_DiskFile : 0 byte file !");
		libflux_fclose(f);
		return LIBFLUX_BADFILE;
	}

	fileimage_buffer_offset = 0;
	fileimage=(unsigned char*)malloc(filesize+512);
	if(!fileimage)
	{
		imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"TeleDisk_libLoad_DiskFile : Malloc error !");
		libflux_fclose(f);
		return LIBFLUX_INTERNALERROR;
	}

	memset(fileimage,0,filesize+512);
	libflux_fread(fileimage,filesize,f);
	libflux_fclose(f);


	td_header = (TELEDISK_HEADER*)&fileimage[fileimage_buffer_offset];
	fileimage_buffer_offset += sizeof(TELEDISK_HEADER);
	if(fileimage_buffer_offset > filesize)
		goto error_bad_file;


	if ( ((td_header->TXT[0]!='t') || (td_header->TXT[1]!='d')) && ((td_header->TXT[0]!='T') || (td_header->TXT[1]!='D')))
	{
		imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"TeleDisk_libLoad_DiskFile : bad header tag !");
		free(fileimage);
		return LIBFLUX_BADFILE;
	}

	CRC16_Init(&CRC16_High,&CRC16_Low,(unsigned char*)crctable,0xA097,0x0000);
	ptr=(unsigned char*)td_header;
	for(i=0;i<0xA;i++)
	{
		CRC16_Update(&CRC16_High,&CRC16_Low, ptr[i],(unsigned char*)crctable );
	}

	if(((td_header->CRC[1]<<8)|td_header->CRC[0])!=((CRC16_High<<8)|CRC16_Low))
	{
		imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"TeleDisk_libLoad_DiskFile : bad header crc !");
		free(fileimage);
		return LIBFLUX_BADFILE;
	}

	imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"TeleDisk_libLoad_DiskFile : Teledisk version : %d",td_header->TDVer);
	if((td_header->TDVer>21) || (td_header->TDVer<10))
	{
		imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"TeleDisk_libLoad_DiskFile : Unsupported version !");
		free(fileimage);
		return LIBFLUX_BADFILE;
	}

	if(((td_header->TXT[0]=='T') && (td_header->TXT[1]=='D')))
	{
		imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"TeleDisk_libLoad_DiskFile : Normal compression");
	}

	if(((td_header->TXT[0]=='t') && (td_header->TXT[1]=='d')))
	{
		imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"TeleDisk_libLoad_DiskFile : Advanced compression");
		fileimage = unpack(fileimage,filesize,&filesize);

		if(!fileimage)
		{
			imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"TeleDisk_libLoad_DiskFile : Unpack failure !");
			return LIBFLUX_BADFILE;
		}
	}

	td_header=(TELEDISK_HEADER*)&fileimage[0];

	if(td_header->TrkDens&0x80)
	{

		CRC16_Init(&CRC16_High,&CRC16_Low,(unsigned char*)crctable,0xA097,0x0000);

		td_comment=(TELEDISK_COMMENT *)&fileimage[fileimage_buffer_offset];
		fileimage_buffer_offset += sizeof(TELEDISK_COMMENT);
		if(fileimage_buffer_offset > filesize)
			goto error_bad_file;


		//libflux_fread( &td_comment, sizeof(td_comment), f );
		ptr=(unsigned char*)td_comment;
		ptr=ptr+2;
		for(i=0;i<sizeof(TELEDISK_COMMENT)-2;i++)
		{
			CRC16_Update(&CRC16_High,&CRC16_Low, ptr[i],(unsigned char*)crctable );
		}

		memcpy(&tempdata,&fileimage[fileimage_buffer_offset],td_comment->Len);
		fileimage_buffer_offset += td_comment->Len;
		if(fileimage_buffer_offset > filesize)
			goto error_bad_file;

		ptr = (unsigned char*)&tempdata;
		for(i=0;i<td_comment->Len;i++)
		{
			CRC16_Update(&CRC16_High,&CRC16_Low, ptr[i],(unsigned char*)crctable );
		}

		imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"TeleDisk_libLoad_DiskFile : Creation date: %.2d/%.2d/%.4d %.2d:%.2d:%.2d",td_comment->bDay,\
																							td_comment->bMon+1,\
																							td_comment->bYear+1900,\
																							td_comment->bHour,\
																							td_comment->bMin,\
																							td_comment->bSec);

		imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"TeleDisk_libLoad_DiskFile : Comment: %s",tempdata);

	}

	interleave = 1;
	numberoftrack = 0;

	file_offset = fileimage_buffer_offset;

	floppydisk->floppyNumberOfSide=td_header->Surface;

	td_track_header=(TELEDISK_TRACK_HEADER  *)&fileimage[fileimage_buffer_offset];
	fileimage_buffer_offset += sizeof(TELEDISK_TRACK_HEADER);
	if(fileimage_buffer_offset > filesize)
		goto error_bad_file;

	while(td_track_header->SecPerTrk!=0xFF)
	{
		if(td_track_header->PhysCyl>numberoftrack)
		{
			numberoftrack=td_track_header->PhysCyl;
		}

		CRC16_Init(&CRC16_High,&CRC16_Low,(unsigned char*)crctable,0xA097,0x0000);
		ptr=(unsigned char*)td_track_header;
		for(i=0;i<0xA;i++)
		{
			CRC16_Update(&CRC16_High,&CRC16_Low, ptr[i],(unsigned char*)crctable );
		}

		for ( i=0;i < td_track_header->SecPerTrk;i++ )
		{
			td_sector_header=(TELEDISK_SECTOR_HEADER  *)&fileimage[fileimage_buffer_offset];
			fileimage_buffer_offset += sizeof(TELEDISK_SECTOR_HEADER);
			if(fileimage_buffer_offset > filesize)
				goto error_bad_file;

			if  ( (td_sector_header->Syndrome & 0x30) == 0 && (td_sector_header->SLen & 0xf8) == 0 )
			{
				//fileimage_buffer_offset=fileimage_buffer_offset+sizeof(unsigned short);

				datalen=(unsigned short*)&fileimage[fileimage_buffer_offset];
				fileimage_buffer_offset += ((*datalen)+2);
				if(fileimage_buffer_offset > filesize)
					goto error_bad_file;
			}
		}

		td_track_header=(TELEDISK_TRACK_HEADER  *)&fileimage[fileimage_buffer_offset];
		fileimage_buffer_offset += sizeof(TELEDISK_TRACK_HEADER);
		if(fileimage_buffer_offset > filesize)
			goto error_bad_file;
	}

	floppydisk->floppyNumberOfTrack=numberoftrack+1;
	floppydisk->floppySectorPerTrack=-1;
	floppydisk->tracks=(LIBFLUX_CYLINDER**)malloc(sizeof(LIBFLUX_CYLINDER*)*floppydisk->floppyNumberOfTrack);
	if(!floppydisk->tracks)
	{
		free(fileimage);
		return LIBFLUX_INTERNALERROR;
	}

	memset(floppydisk->tracks,0,sizeof(LIBFLUX_CYLINDER*)*floppydisk->floppyNumberOfTrack);

	//Source disk density (0 = 250K bps,  1 = 300K bps,  2 = 500K bps ; +128 = single-density FM)
	switch(td_header->Dens)
	{
		case 0:
			floppydisk->floppyBitRate=250000;
			break;
		case 1:
			floppydisk->floppyBitRate=300000;
			break;
		case 2:
			floppydisk->floppyBitRate=500000;
			break;
		default:
			floppydisk->floppyBitRate=250000;
			break;
	}

	floppydisk->floppyiftype=GENERIC_SHUGART_DD_FLOPPYMODE;

	rpm=300; // normal rpm
	imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"%d tracks, %d side(s), gap3:%d,rpm:%d bitrate:%d",floppydisk->floppyNumberOfTrack,floppydisk->floppyNumberOfSide,floppydisk->floppySectorPerTrack,rpm,floppydisk->floppyBitRate);

	//////////////////////////////////
	fileimage_buffer_offset=file_offset;

	td_track_header=(TELEDISK_TRACK_HEADER  *)&fileimage[fileimage_buffer_offset];
	fileimage_buffer_offset += sizeof(TELEDISK_TRACK_HEADER);
	if(fileimage_buffer_offset > filesize)
		goto error_bad_file;

	while(td_track_header->SecPerTrk!=0xFF)
	{
		if(td_track_header->PhysSide&0x7F)
		{
			sidenumber=1;
		}
		else
		{
			sidenumber=0;
		}

		if(td_track_header->PhysSide&0x80)
		{
			trackformat=IBMFORMAT_SD;
		}
		else
		{
			trackformat=IBMFORMAT_DD;
		}

		imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"------------- Track:%d, Side:%d, Number of Sector:%d -------------",td_track_header->PhysCyl,sidenumber,td_track_header->SecPerTrk);

		if(!floppydisk->tracks[td_track_header->PhysCyl])
		{
			floppydisk->tracks[td_track_header->PhysCyl]=(LIBFLUX_CYLINDER*)malloc(sizeof(LIBFLUX_CYLINDER));
			if( !floppydisk->tracks[td_track_header->PhysCyl] )
			{
				free(fileimage);
				return LIBFLUX_INTERNALERROR;
			}
			memset(floppydisk->tracks[td_track_header->PhysCyl],0,sizeof(LIBFLUX_CYLINDER));
		}

		currentcylinder=floppydisk->tracks[td_track_header->PhysCyl];
		currentcylinder->number_of_side=floppydisk->floppyNumberOfSide;
		if(!currentcylinder->sides)
		{
			currentcylinder->sides = (LIBFLUX_SIDE**)malloc(sizeof(LIBFLUX_SIDE*)*currentcylinder->number_of_side);
			if( !currentcylinder->sides )
			{
				free(fileimage);
				return LIBFLUX_INTERNALERROR;
			}

			memset(currentcylinder->sides,0,sizeof(LIBFLUX_SIDE*)*currentcylinder->number_of_side);
		}

		currentcylinder->floppyRPM=rpm;

		////////////////////crc track header///////////////////
		CRC16_Init(&CRC16_High,&CRC16_Low,(unsigned char*)crctable,0xA097,0x0000);
		ptr=(unsigned char*)td_track_header;
		for(i=0;i<0x3;i++)
		{
			CRC16_Update(&CRC16_High,&CRC16_Low, ptr[i],(unsigned char*)crctable );
		}

		if(CRC16_Low!=td_track_header->CRC)
		{
			imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"!!!! Track header CRC Error !!!!");
		}
		////////////////////////////////////////////////////////

		sectorconfig = (LIBFLUX_SECTCFG  *)malloc(sizeof(LIBFLUX_SECTCFG)*td_track_header->SecPerTrk);
		if( !sectorconfig )
		{
			free(fileimage);
			return LIBFLUX_INTERNALERROR;
		}

		memset(sectorconfig,0,sizeof(LIBFLUX_SECTCFG)*td_track_header->SecPerTrk);
		for ( i=0;i < td_track_header->SecPerTrk;i++ )
		{
			td_sector_header=(TELEDISK_SECTOR_HEADER  *)&fileimage[fileimage_buffer_offset];
			fileimage_buffer_offset += sizeof(TELEDISK_SECTOR_HEADER);
			if(fileimage_buffer_offset > filesize)
				goto error_bad_file;

			sectorconfig[i].cylinder=td_sector_header->Cyl;
			sectorconfig[i].head=td_sector_header->Side;
			sectorconfig[i].sector=td_sector_header->SNum;
			sectorconfig[i].sectorsize=128<<td_sector_header->SLen;
			sectorconfig[i].bitrate=floppydisk->floppyBitRate;
			sectorconfig[i].gap3=255;
			sectorconfig[i].trackencoding=trackformat;

			if(td_sector_header->Syndrome & 0x04)
			{
				sectorconfig[i].use_alternate_datamark=1;
				sectorconfig[i].alternate_datamark=0xF8;
			}

			if(td_sector_header->Syndrome & 0x02)
			{
				sectorconfig[i].use_alternate_data_crc=2;
			}

			if(td_sector_header->Syndrome & 0x20)
			{
				sectorconfig[i].missingdataaddressmark=1;
			}

			sectorconfig[i].input_data = malloc(sectorconfig[i].sectorsize);
			if  ( (td_sector_header->Syndrome & 0x30) == 0 && (td_sector_header->SLen & 0xf8) == 0 )
			{
				//fileimage_buffer_offset=fileimage_buffer_offset+sizeof(unsigned short);

				datalen=(unsigned short*)&fileimage[fileimage_buffer_offset];
				memcpy(&tempdata,&fileimage[fileimage_buffer_offset],(*datalen)+2);
				fileimage_buffer_offset += ((*datalen)+2);
				if(fileimage_buffer_offset > filesize)
					goto error_bad_file;

				RLEExpander(tempdata,sectorconfig[i].input_data,(int)*datalen);
			}
			else
			{
				if(sectorconfig[i].input_data)
					memset(sectorconfig[i].input_data,0,sectorconfig[i].sectorsize);
			}

			imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"track:%d, side:%d, sector:%d, sectorsize:%d, flag:%.2x",sectorconfig[i].cylinder,sectorconfig[i].head,sectorconfig[i].sector,sectorconfig[i].sectorsize,td_sector_header->Syndrome);
		}

		currentside=tg_generateTrackEx((unsigned short)td_track_header->SecPerTrk,sectorconfig,interleave,0,floppydisk->floppyBitRate,rpm,trackformat,0,2500 | NO_SECTOR_UNDER_INDEX,-2500);
		currentcylinder->sides[sidenumber]=currentside;

		for ( i=0;i < td_track_header->SecPerTrk;i++ )
		{
			libflux_freeSectorConfigData( 0, &sectorconfig[i] );
		}
		free(sectorconfig);
		sectorconfig = NULL;

		td_track_header=(TELEDISK_TRACK_HEADER  *)&fileimage[fileimage_buffer_offset];
		fileimage_buffer_offset += sizeof(TELEDISK_TRACK_HEADER);
		if(fileimage_buffer_offset > filesize)
			goto error_bad_file;
	}

	imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"track file successfully loaded and encoded!");

	free(fileimage);

	libflux_imgCallProgressCallback(imgldr_ctx,100,100 );

	libflux_sanityCheck(imgldr_ctx->ctx,floppydisk);

	return LIBFLUX_NOERROR;

error_bad_file:
	if(fileimage)
		free(fileimage);

	if(sectorconfig)
	{
		for ( i=0;i < td_track_header->SecPerTrk;i++ )
		{
			libflux_freeSectorConfigData( 0, &sectorconfig[i] );
		}

		free(sectorconfig);
	}

	imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"TeleDisk_libLoad_DiskFile : Unexpected end of file ! : offset %d > file size (%d)",fileimage_buffer_offset,filesize);

	return LIBFLUX_BADFILE;
}

int TeleDisk_libGetPluginInfo(LIBFLUX_IMGLDR * imgldr_ctx,uint32_t infotype,void * returnvalue)
{
	static const char plug_id[]="TELEDISK_TD0";
	static const char plug_desc[]="TELEDISK TD0 Loader";
	static const char plug_ext[]="td0";

	plugins_ptr plug_funcs=
	{
		(ISVALIDDISKFILE)   TeleDisk_libIsValidDiskFile,
		(LOADDISKFILE)      TeleDisk_libLoad_DiskFile,
		(WRITEDISKFILE)     0,
		(GETPLUGININFOS)    TeleDisk_libGetPluginInfo
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

