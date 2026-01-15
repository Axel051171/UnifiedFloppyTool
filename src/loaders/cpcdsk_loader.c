/**
 * @file cpcdsk_loader.c
 * @brief Amstrad CPC DSK loader
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
// File : cpcdsk_loader.c
// Contains: CPCDSK floppy image loader
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

#include "cpcdsk_loader.h"
#include "cpcdsk_format.h"

#include "uft_compat.h"

//#define CPCDSK_DBG 1

int CPCDSK_libIsValidDiskFile( LIBFLUX_IMGLDR * imgldr_ctx, LIBFLUX_IMGLDR_FILEINFOS * imgfile )
{
	cpcdsk_fileheader * fileheader;

	imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"CPCDSK_libIsValidDiskFile");

	if(libflux_checkfileext(imgfile->path,"dsk",SYS_PATH_TYPE))
	{
		fileheader = (cpcdsk_fileheader *)imgfile->file_header;

		if( !strncmp((char*)&fileheader->headertag,"EXTENDED CPC DSK File\r\nDisk-Info\r\n",16) ||
			!strncmp((char*)&fileheader->headertag,"MV - CPCEMU Disk-File\r\nDisk-Info\r\n",11) ||
			!strncmp((char*)&fileheader->headertag,"MV - CPC",8)
			)
		{
			imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"CPCDSK_libIsValidDiskFile : CPC Dsk file !");
			return LIBFLUX_VALIDFILE;
		}
		else
		{
			imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"CPCDSK_libIsValidDiskFile : non CPC Dsk file !(bad header)");
			return LIBFLUX_BADFILE;
		}
	}
	else
	{
		imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"CPCDSK_libIsValidDiskFile : non CPC Dsk file !");
		return LIBFLUX_BADFILE;
	}
}

int CPCDSK_libLoad_DiskFile(LIBFLUX_IMGLDR * imgldr_ctx,LIBFLUX_FLOPPY * floppydisk,char * imgfile,void * parameters)
{

	FILE * f;
	unsigned int filesize;
	int tracksize;
	int i,j,k,l,t,s;
	int sectorposition;
	int tracklen;
	int gap3len,interleave;
	int rpm;
	int extendformat;
	cpcdsk_fileheader fileheader;
	cpcdsk_trackheader trackheader;
	cpcdsk_sector sector;
	unsigned char * tracksizetab;
	int trackposition;
	LIBFLUX_SECTCFG* sectorconfig;
	LIBFLUX_CYLINDER** realloctracktable;

	LIBFLUX_CYLINDER* currentcylinder;
	//LIBFLUX_SIDE* currentside;
	unsigned char * temp_buffer;

	sectorconfig = NULL;
	currentcylinder = NULL;
	tracksizetab = NULL;
	realloctracktable = NULL;

	imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"CPCDSK_libLoad_DiskFile %s",imgfile);

	f = libflux_fopen(imgfile,"rb");
	if( f == NULL )
	{
		imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"Cannot open %s !",imgfile);
		return LIBFLUX_ACCESSERROR;
	}

	filesize = libflux_fgetsize(f);

	if( filesize )
	{

		libflux_fread(&fileheader,sizeof(fileheader),f);

#ifdef CPCDSK_DBG
		char tmp_str[64];
		imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"------ CPC DSK File Header ------");
		strncpy(tmp_str,&fileheader.headertag,sizeof(fileheader.headertag)-1);
		imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"Header Tag: %s",tmp_str);
		strncpy(tmp_str,&fileheader.creatorname,sizeof(fileheader.creatorname)-1);
		imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"Creator Name: %s",tmp_str);
		imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"Number of tracks: %d",fileheader.number_of_tracks);
		imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"Number of sides: %d",fileheader.number_of_sides);
		imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"Size per tracks: 0x%.4X",fileheader.size_of_a_track);
		imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"---------------------------------");
#endif

		if( !strncmp((char*)&fileheader.headertag,"EXTENDED CPC DSK File\r\nDisk-Info\r\n",16))
		{
			extendformat=1;
			imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"Extended CPC Dsk file\n");
		}
		else
		{
			if( !strncmp((char*)&fileheader.headertag,"MV - CPC",8) )
			{
				extendformat=0;
				imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"CPC Dsk standard file\n");
			}
			else
			{
				imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"non CPC Dsk : Bad header!\n");
				libflux_fclose(f);
				return LIBFLUX_BADFILE;
			}
		}

		if(extendformat)
		{
			// read the tracks size array.
			tracksizetab = (unsigned char *)malloc(fileheader.number_of_sides*fileheader.number_of_tracks);
			if( !tracksizetab )
				goto alloc_error;

			libflux_fread(tracksizetab,fileheader.number_of_sides*fileheader.number_of_tracks,f);

#ifdef CPCDSK_DBG
			for(i=0;i<fileheader.number_of_sides*fileheader.number_of_tracks;i++)
			{
				imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"Track %.2d:%d Size : 0x%.4X",i>>1,i&1,tracksizetab[i]<<8);
			}
#endif
		}

		floppydisk->floppyBitRate=250000;
		floppydisk->floppyiftype=CPC_DD_FLOPPYMODE;
		floppydisk->floppyNumberOfTrack=fileheader.number_of_tracks;
		floppydisk->floppyNumberOfSide=fileheader.number_of_sides;
		floppydisk->floppySectorPerTrack=9; // default value
		tracksize=fileheader.size_of_a_track;
		rpm=300;

		interleave=1;

		floppydisk->tracks=(LIBFLUX_CYLINDER**)malloc(sizeof(LIBFLUX_CYLINDER*)*floppydisk->floppyNumberOfTrack);
		if(!floppydisk->tracks)
			goto alloc_error;

		memset(floppydisk->tracks,0,sizeof(LIBFLUX_CYLINDER*)*floppydisk->floppyNumberOfTrack);

		imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"%d tracks, %d Side(s)\n",floppydisk->floppyNumberOfTrack,floppydisk->floppyNumberOfSide);

		trackposition=0x100;

		currentcylinder = NULL;

		for(i=0;i<(fileheader.number_of_sides*fileheader.number_of_tracks);i++)
		{
			libflux_imgCallProgressCallback(imgldr_ctx,i,fileheader.number_of_sides*fileheader.number_of_tracks);

#ifdef CPCDSK_DBG
			imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"Track %.2d:%d Position : 0x%.8X",i>>1,i&1,trackposition);
#endif

			(void)fseek(f , trackposition , SEEK_SET);

			memset(&trackheader,0,sizeof(trackheader));

			if( ( ftell(f) == (int32_t)trackposition ) && libflux_fread(&trackheader,sizeof(trackheader),f)>0 )
			{

				t = trackheader.track_number;
				s = trackheader.side_number;

				if(!strncmp((char*)&trackheader.headertag,"Track-Info\r\n",10) && (t<fileheader.number_of_tracks))//12))
				{
					if(extendformat)
					{
						tracksize=tracksizetab[i]*256;
					}

					if(tracksize)
					{
						// description d'une track au delà du nombre de track max !
						// realloc necessaire
						if(trackheader.track_number >= floppydisk->floppyNumberOfTrack)
						{
							floppydisk->floppyNumberOfTrack=trackheader.track_number+1;
							realloctracktable=(LIBFLUX_CYLINDER**)malloc(sizeof(LIBFLUX_CYLINDER*)*trackheader.track_number);
							if(!realloctracktable)
								goto alloc_error;

							memset(realloctracktable,0,sizeof(LIBFLUX_CYLINDER*)*trackheader.track_number);
							memcpy(realloctracktable,floppydisk->tracks,sizeof(LIBFLUX_CYLINDER*)*floppydisk->floppyNumberOfTrack);

							free(floppydisk->tracks);
							floppydisk->tracks=realloctracktable;
						}

						if(!floppydisk->tracks[t])
						{
							floppydisk->tracks[t] = allocCylinderEntry(rpm,floppydisk->floppyNumberOfSide);
							currentcylinder = floppydisk->tracks[t];
						}

						imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"\nnumber %d - track:%d side:%d sector:%d sc:%d gap3:%d fill:%x recmode:%d bitrate:%d\n ",i,trackheader.track_number,trackheader.side_number,trackheader.number_of_sector,trackheader.sector_size_code,trackheader.gap3_length,trackheader.filler_byte,trackheader.rec_mode,trackheader.datarate);

						if(trackheader.number_of_sector)
						{
							sectorconfig = (LIBFLUX_SECTCFG*)malloc(sizeof(LIBFLUX_SECTCFG)*trackheader.number_of_sector);
							if( !sectorconfig )
								goto alloc_error;

							memset(sectorconfig,0,sizeof(LIBFLUX_SECTCFG)*trackheader.number_of_sector);

							gap3len=trackheader.gap3_length;
							sectorconfig->sectorsize=trackheader.sector_size_code;

							sectorposition=0;
							for(j=0;j<trackheader.number_of_sector;j++)
							{
								(void)fseek(f,trackposition+sizeof(trackheader)+(sizeof(sector)*j), SEEK_SET);
								libflux_fread(&sector,sizeof(sector),f);

								sectorconfig[j].cylinder = sector.track;
								sectorconfig[j].head = sector.side;
								sectorconfig[j].sector = sector.sector_id;
								sectorconfig[j].sectorsize = 128<<(sector.sector_size_code&7);
								sectorconfig[j].input_data = malloc(sectorconfig[j].sectorsize);
								if( !sectorconfig[j].input_data )
									goto alloc_error;

								(void)fseek(f, trackposition+0x100+sectorposition/*sizeof(trackheader)+(sizeof(sector)*j)*/, SEEK_SET);
								libflux_fread(sectorconfig[j].input_data,sectorconfig[j].sectorsize,f);

								// Find the weak bits.
								if( (sectorconfig[j].sectorsize != sector.data_length) && sector.data_length )
								{
									sectorconfig[j].weak_bits_mask = malloc(sectorconfig[j].sectorsize);
									if( sectorconfig[j].weak_bits_mask )
									{
										memset(sectorconfig[j].weak_bits_mask, 0x00, sectorconfig[j].sectorsize);
										temp_buffer = malloc(sectorconfig[j].sectorsize);
										if( temp_buffer )
										{
											for(k=0;k< sector.data_length / sectorconfig[j].sectorsize;k++)
											{
												(void)fseek(f, trackposition+0x100+sectorposition + k*sectorconfig[j].sectorsize, SEEK_SET);
												libflux_fread(temp_buffer,sectorconfig[j].sectorsize,f);
												for(l=0;l<sectorconfig[j].sectorsize;l++)
												{
													sectorconfig[j].weak_bits_mask[l] |= (sectorconfig[j].input_data[l] ^ temp_buffer[l]);
												}
											}
											free(temp_buffer);
										}
									}
								}

								if( sector.data_length )
									sectorposition += sector.data_length;
								else
									sectorposition += sectorconfig[j].sectorsize;

								// ID part CRC ERROR ?
								if((sector.fdc_status_reg1&0x20) && !(sector.fdc_status_reg2&0x20))
								{
									sectorconfig[j].use_alternate_header_crc = 0x1;
								}

								// Data part CRC ERROR ?
								if((sector.fdc_status_reg1&0x20) &&  (sector.fdc_status_reg2&0x20))
								{
									sectorconfig[j].use_alternate_data_crc = 0x1;
								}

								// Deleted Data Address Mark ?
								if(sector.fdc_status_reg2&0x40)
								{
									sectorconfig[j].use_alternate_datamark = 1;
									sectorconfig[j].alternate_datamark = 0xF8;
								}

								sectorconfig[j].bitrate = floppydisk->floppyBitRate;
								sectorconfig[j].gap3 = gap3len;
								sectorconfig[j].trackencoding = IBMFORMAT_DD;

								imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"%d:%d track id:%d side id:%d sector id %d sector size (id):%d sector size :%d bad crc:%d sreg1:%x sreg2:%x",trackheader.track_number,trackheader.side_number,sector.track,sector.side,sector.sector_id,sectorconfig[j].sectorsize,sector.data_length,sectorconfig[j].use_alternate_data_crc,sector.fdc_status_reg1,sector.fdc_status_reg2);
							}
						}

						if(!currentcylinder)
							goto alloc_error;

						currentcylinder->floppyRPM = rpm;
						currentcylinder->sides[s] = tg_generateTrackEx(trackheader.number_of_sector,sectorconfig,interleave,0,floppydisk->floppyBitRate,300,IBMFORMAT_DD,0,2500|NO_SECTOR_UNDER_INDEX,-2500);

						for(j=0;j<trackheader.number_of_sector;j++)
						{
							libflux_freeSectorConfigData( 0, &sectorconfig[j] );
						}

						if(trackheader.number_of_sector)
						{
							free(sectorconfig);
							sectorconfig = NULL;
						}

						///////////////////////
						/*
						currentside=currentcylinder->sides[s];

						for(j=0;j<trackheader.number_of_sector;j++)
						{

						  if(sectorconfig[j].baddatacrc)
						  {

							if(!currentside->flakybitsbuffer)
							currentside->flakybitsbuffer=malloc(currentside->tracklen);

							  for(k=0;k<16;k++)
							  {
							  currentside->databuffer[sectorconfig[j].startdataindex+64+k]=0;
							  currentside->flakybitsbuffer[sectorconfig[j].startdataindex+64+k]=0xFF;
							  }


							}
						}*/
						///////////////////////////

						trackposition=trackposition+tracksize;
					}
					else
					{
						imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"\nnumber %d - empty !\n ",i);

						if(!floppydisk->tracks[t])
						{
							floppydisk->tracks[t]=allocCylinderEntry(rpm,floppydisk->floppyNumberOfSide);
						}

						currentcylinder = floppydisk->tracks[t];

						tracklen=((DEFAULT_DD_BITRATE/(rpm/60))/4)*8;
						currentcylinder->sides[s] = tg_alloctrack(floppydisk->floppyBitRate,ISOIBM_MFM_ENCODING,currentcylinder->floppyRPM,tracklen,2500,-2500,TG_ALLOCTRACK_RANDOMIZEDATABUFFER);
					}
				}
				else
				{

					imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"bad track header !\n");
				}
			}
			else
			{
				imgldr_ctx->ctx->libflux_printf(MSG_WARNING,"EOF ! Missing track(s) ?\n");
			}

		}

		// initialisation des tracks non formatées / non présente dans le fichier.
		for(j=0;j<floppydisk->floppyNumberOfTrack;j++)
		{
			for(i=0;i<floppydisk->floppyNumberOfSide;i++)
			{
				if(!floppydisk->tracks[j])
				{
					floppydisk->tracks[j]=allocCylinderEntry(rpm,floppydisk->floppyNumberOfSide);
				}

				currentcylinder = floppydisk->tracks[j];

				if(!floppydisk->tracks[j]->sides[i])
				{
					tracklen=((DEFAULT_DD_BITRATE/(rpm/60))/4)*8;
					floppydisk->tracks[j]->sides[i]=tg_alloctrack(floppydisk->floppyBitRate,ISOIBM_MFM_ENCODING,currentcylinder->floppyRPM,tracklen,2500,-2500,TG_ALLOCTRACK_ALLOCFLAKEYBUFFER|TG_ALLOCTRACK_RANDOMIZEDATABUFFER|TG_ALLOCTRACK_UNFORMATEDBUFFER);
				}
			}
		}

		free(tracksizetab);
		//free(sectorconfig);

		libflux_fclose(f);

		libflux_sanityCheck(imgldr_ctx->ctx,floppydisk);

		return LIBFLUX_NOERROR;
	}

	imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"file size=%d !?",filesize);
	libflux_fclose(f);
	return LIBFLUX_BADFILE;

alloc_error:

	libflux_freeFloppy(imgldr_ctx->ctx, floppydisk );

	if(f)
		libflux_fclose(f);

	free(tracksizetab);

	if( sectorconfig )
	{
		for(j=0;j<trackheader.number_of_sector;j++)
		{
			free(sectorconfig[j].input_data);
		}
		free(sectorconfig);
	}

	free(realloctracktable);

	return LIBFLUX_INTERNALERROR;
}

int CPCDSK_libWrite_DiskFile(LIBFLUX_IMGLDR* imgldr_ctx,LIBFLUX_FLOPPY * floppy,char * filename);

int CPCDSK_libGetPluginInfo(LIBFLUX_IMGLDR * imgldr_ctx,uint32_t infotype,void * returnvalue)
{
	static const char plug_id[]="AMSTRADCPC_DSK";
	static const char plug_desc[]="Amstrad CPC DSK Loader";
	static const char plug_ext[]="dsk";

	plugins_ptr plug_funcs=
	{
		(ISVALIDDISKFILE)   CPCDSK_libIsValidDiskFile,
		(LOADDISKFILE)      CPCDSK_libLoad_DiskFile,
		(WRITEDISKFILE)     CPCDSK_libWrite_DiskFile,
		(GETPLUGININFOS)    CPCDSK_libGetPluginInfo
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

