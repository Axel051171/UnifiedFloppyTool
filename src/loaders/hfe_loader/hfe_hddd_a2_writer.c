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

#include "libflux.h""
#include "libflux.h""

#include "hfe_loader.h"
#include "hfe_format.h"

#include "uft_compat.h"

#include "tracks/luts.h"

extern void addpad(unsigned char * track,int mfmsize,int tracksize);

int HFE_HDDD_A2_libWrite_DiskFile(LIBFLUX_IMGLDR* imgldr_ctx,LIBFLUX_FLOPPY * floppy,char * filename)
{
	HXCRAMFILE rf;
	pictrack * track;

	FILE * hxcpicfile;

	picfileformatheader * FILEHEADER;
	unsigned char header_buffer[512];
	unsigned char * mfmtracks0,*mfmtracks1,*mfmtrackfinal;
	unsigned char * offsettrack;
	int mfmsize,mfmsize2,i_conv;
	unsigned int i,j,k;
	unsigned int trackpos;
	unsigned int tracklistlen;
	unsigned int tracksize;

	unsigned short fm_pulses;

	mfmtracks0 = NULL;
	mfmtracks1 = NULL;
	mfmtrackfinal = NULL;
	offsettrack = NULL;

	imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"Write HFE file %s for the standalone emulator (with HDDD A2 support).",filename);

	if(!floppy->floppyNumberOfTrack)
	{
		imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"Cannot create zero track HFE file");
		return LIBFLUX_BADPARAMETER;
	}

	hxcpicfile=libflux_ram_fopen(filename,"wb",&rf);
	if(hxcpicfile)
	{
		FILEHEADER = (picfileformatheader *) header_buffer;
		memset(FILEHEADER,0xFF,512);
		memcpy(&FILEHEADER->HEADERSIGNATURE,"HXCPICFE",8);

		FILEHEADER->number_of_track=(unsigned char)floppy->floppyNumberOfTrack;
		FILEHEADER->number_of_side=floppy->floppyNumberOfSide;
		if(floppy->floppyBitRate!=VARIABLEBITRATE)
		{
			FILEHEADER->bitRate=(floppy->floppyBitRate * 2)/1000;
		}
		else
		{
			if(floppy->tracks[0]->sides[0]->bitrate == VARIABLEBITRATE)
				FILEHEADER->bitRate=(unsigned short)( (floppy->tracks[0]->sides[0]->timingbuffer[ (floppy->tracks[0]->sides[0]->tracklen/8) / 2] * 2)/1000);
			else
				FILEHEADER->bitRate=(floppy->tracks[0]->sides[0]->bitrate * 2)/1000;
		}
		FILEHEADER->floppyRPM=0;//floppy->floppyRPM;

		FILEHEADER->floppyinterfacemode=(unsigned char)floppy->floppyiftype;

		imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"Floppy interface mode %s (%s)",   libflux_getFloppyInterfaceModeName(imgldr_ctx->ctx,FILEHEADER->floppyinterfacemode),
																			libflux_getFloppyInterfaceModeDesc(imgldr_ctx->ctx,FILEHEADER->floppyinterfacemode) );

		FILEHEADER->track_encoding=0;
		FILEHEADER->formatrevision=0;
		FILEHEADER->track_list_offset=1;
		FILEHEADER->write_protected=1;

		switch( libflux_getEnvVarValue( imgldr_ctx->ctx, "HFE_WRITER_WRITENOTALLOWED" ) )
		{
			default:
			case 0: // Write protect off
				FILEHEADER->write_allowed = 0xFF;
			break;
			case 1: // Write protect on
				FILEHEADER->write_allowed = 0x00;
			break;
			case 2: // From source
				if( libflux_floppyGetFlags( imgldr_ctx->ctx, floppy ) & LIBFLUX_FLOPPY_WRPROTECTED_FLAG )
				{
					FILEHEADER->write_allowed = 0;
				}
			break;
		}

		if(floppy->tracks[floppy->floppyNumberOfTrack/2]->sides[0]->track_encoding)
		{
			FILEHEADER->track_encoding=floppy->tracks[floppy->floppyNumberOfTrack/2]->sides[0]->track_encoding;
		}

		if( FILEHEADER->track_encoding == APPLEII_GCR1_ENCODING )
		{
			FILEHEADER->track_encoding = APPLEII_HDDD_A2_GCR1_ENCODING;
		}

		if( FILEHEADER->track_encoding == APPLEII_GCR2_ENCODING )
		{
			FILEHEADER->track_encoding = APPLEII_HDDD_A2_GCR2_ENCODING;
		}

		if( floppy->floppyNumberOfTrack > 42 )
		{
			FILEHEADER->single_step=0xFF;
		}
		else
		{
			FILEHEADER->single_step=0x00;
		}

		libflux_ram_fwrite(FILEHEADER,512,1,hxcpicfile,&rf);

		tracklistlen=((((((FILEHEADER->number_of_track)+1)*sizeof(pictrack))/512)+1));

		offsettrack=(unsigned char*) uft_safe_malloc_array(tracklistlen, 512);
		if( !offsettrack )
			goto alloc_error;

		memset(offsettrack,0xFF,tracklistlen*512);

		i=0;
		trackpos=FILEHEADER->track_list_offset+tracklistlen;

		while(i<(FILEHEADER->number_of_track))
		{
			libflux_imgCallProgressCallback(imgldr_ctx,i,(FILEHEADER->number_of_track) );

			mfmsize=0;
			mfmsize2=0;

			mfmsize=floppy->tracks[i]->sides[0]->tracklen * 2;
			if(mfmsize&7)
				mfmsize=(mfmsize/8)+1;
			else
				mfmsize=mfmsize/8;


			if(floppy->tracks[i]->number_of_side==2)
			{
				mfmsize2=floppy->tracks[i]->sides[1]->tracklen * 2;
				if(mfmsize2&7)
					mfmsize2=(mfmsize2/8)+1;
				else
					mfmsize2=mfmsize2/8;
			}

			if(mfmsize2>mfmsize) mfmsize=mfmsize2;

			if(mfmsize*2>0xFFFF)
			{
				imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"Argg!! track %d too long (%x) and shorten to 0xFFFF !",i,mfmsize*2);
				mfmsize=0x7FFF;
			}

			track=(pictrack *)(offsettrack+(i*sizeof(pictrack)));
			track->track_len=mfmsize*2;
			track->offset=trackpos;

			if((mfmsize*2)%512)
				trackpos=trackpos+(((mfmsize*2)/512)+1);
			else
				trackpos=trackpos+((mfmsize*2)/512);

			//trackpos=trackpos+(((mfmsize*2)/512)+1);
			i++;
		};

		libflux_ram_fwrite(offsettrack,512*tracklistlen,1,hxcpicfile,&rf);

		i=0;
		while(i<(FILEHEADER->number_of_track))
		{
			mfmsize=floppy->tracks[i]->sides[0]->tracklen * 2;
			if(mfmsize&7)
				mfmsize=(mfmsize/8)+1;
			else
				mfmsize=mfmsize/8;

			mfmsize2=0;
			if(floppy->tracks[i]->number_of_side==2)
			{
				mfmsize2=floppy->tracks[i]->sides[1]->tracklen * 2;
				if(mfmsize2&7)
					mfmsize2=(mfmsize2/8)+1;
				else
					mfmsize2=mfmsize2/8;
			}

			if(mfmsize>0x7FFF)
			{
				mfmsize=0x7FFF;
			}
			if(mfmsize2>0x7FFF)
			{
				mfmsize2=0x7FFF;
			}
			track=(pictrack *)(offsettrack+(i*sizeof(pictrack)));

			if(track->track_len%512)
				tracksize=((track->track_len&(~0x1FF))+0x200)/2;//(((track->track_len/512)+1)*512)/2;
			else
				tracksize=track->track_len/2;

			mfmtracks0 = (unsigned char*) calloc( 1, tracksize );
			mfmtracks1 = (unsigned char*) calloc( 1, tracksize );
			mfmtrackfinal = (unsigned char*) uft_safe_malloc_array(2, tracksize);

			if( !mfmtracks0 || !mfmtracks1 || !mfmtrackfinal)
				goto alloc_error;

			memset(mfmtrackfinal,0x55,tracksize*2);

			for(i_conv=0;i_conv<(mfmsize/2);i_conv++)
			{
				// Add the FM Clocks
				fm_pulses = LUT_Byte2ShortEvenBitsExpander[floppy->tracks[i]->sides[0]->databuffer[i_conv]] | 0x2222;
				mfmtracks0[(i_conv*2)+0] = fm_pulses >> 8;
				mfmtracks0[(i_conv*2)+1] = fm_pulses &  0xFF;
			}

			addpad(mfmtracks0,mfmsize,tracksize);

			if(floppy->tracks[i]->number_of_side==2)
			{
				for(i_conv=0;i_conv<(mfmsize2/2);i_conv++)
				{
					// Add the FM Clocks
					fm_pulses = LUT_Byte2ShortEvenBitsExpander[floppy->tracks[i]->sides[1]->databuffer[i_conv]] | 0x2222;
					mfmtracks1[(i_conv*2)+0] = fm_pulses >> 8;
					mfmtracks1[(i_conv*2)+1] = fm_pulses &  0xFF;
				}
				addpad(mfmtracks1,mfmsize2,tracksize);
			}
			else
			{
				memset(mfmtracks1,0xAA,tracksize);
			}

			for(k=0;k<tracksize/256;k++)
			{

				for(j=0;j<256;j++)
				{
					// inversion des bits pour le EUSART du PIC.

					// head 0
					mfmtrackfinal[(k*512)+j]=     LUT_ByteBitsInverter[mfmtracks0[(k*256)+j]];
					// head 1
					mfmtrackfinal[(k*512)+j+256]= LUT_ByteBitsInverter[mfmtracks1[(k*256)+j]];

				}
			}

			libflux_ram_fwrite(mfmtrackfinal,tracksize*2,1,hxcpicfile,&rf);

			free(mfmtracks0);
			mfmtracks0 = NULL;
			free(mfmtracks1);
			mfmtracks1 = NULL;
			free(mfmtrackfinal);
			mfmtrackfinal = NULL;

			i++;
		};

		free(offsettrack);
		offsettrack = NULL;

		hxcpicfile=libflux_fopen(filename,"wb");
		if(hxcpicfile)
		{
			if (fwrite(rf.ramfile,rf.ramfile_size,1,hxcpicfile) != 1) { /* I/O error */ }
			libflux_fclose(hxcpicfile);
		}
		else
		{
			libflux_ram_fclose(hxcpicfile,&rf);
			imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"Cannot create %s!",filename);
			return LIBFLUX_ACCESSERROR;
		}

		libflux_ram_fclose(hxcpicfile,&rf);

		imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"%d tracks written to the file",FILEHEADER->number_of_track);

		return LIBFLUX_NOERROR;
	}
	else
	{
		imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"Cannot create %s!",filename);

		return LIBFLUX_ACCESSERROR;
	}

alloc_error:
	if(hxcpicfile)
		libflux_ram_fclose(hxcpicfile,&rf);

	free( offsettrack );
	free(mfmtracks0);
	free(mfmtracks1);
	free(mfmtrackfinal);

	return LIBFLUX_INTERNALERROR;
}
