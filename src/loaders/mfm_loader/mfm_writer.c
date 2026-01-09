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
#include "libflux.h"

#include "mfm_loader.h"
#include "mfm_format.h"

#include "uft_compat.h"

int MFM_libWrite_DiskFile(LIBFLUX_IMGLDR* imgldr_ctx,LIBFLUX_FLOPPY * floppy,char * filename)
{

	FILE * hxcmfmfile;

	MFMTRACKIMG mfmtrackdesc;
	MFMIMG mfmheader;
	unsigned char * mfmtrack;
	int32_t * offsettrack;
	int mfmsize;
	unsigned int i,j;
	unsigned int trackpos;

	imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"Write MFM file %s...",filename);

	hxcmfmfile = libflux_fopen(filename,"wb");
	if(hxcmfmfile)
	{
		memcpy(mfmheader.headername, "HXCMFM", 7);  /* Fixed tag */
		mfmheader.number_of_track=floppy->floppyNumberOfTrack;
		mfmheader.number_of_side=floppy->floppyNumberOfSide;
		mfmheader.floppyBitRate=floppy->floppyBitRate/1000;

		if(floppy->floppyBitRate!=VARIABLEBITRATE)
		{
			mfmheader.floppyBitRate=floppy->floppyBitRate/1000;
		}
		else
		{
			mfmheader.floppyBitRate=floppy->tracks[0]->sides[0]->timingbuffer[(floppy->tracks[0]->sides[0]->tracklen/8)/2]/1000;
		}

		mfmheader.floppyRPM=0;//floppy->floppyRPM;
		mfmheader.floppyiftype=(unsigned char)floppy->floppyiftype;
		mfmheader.mfmtracklistoffset=sizeof(mfmheader);
		if (fwrite(&mfmheader,sizeof(mfmheader),1,hxcmfmfile) != 1) { /* I/O error */ }
		imgldr_ctx->ctx->libflux_printf(MSG_INFO_1,"%d Tracks, %d side(s)",mfmheader.number_of_track,mfmheader.number_of_side);

		offsettrack = (int32_t*) malloc(((mfmheader.number_of_track*mfmheader.number_of_side)+1)*sizeof(int32_t));
		if( !offsettrack )
		{
			libflux_fclose(hxcmfmfile);
			return LIBFLUX_INTERNALERROR;
		}

		i=0;
		trackpos=sizeof(mfmheader)+(sizeof(mfmtrackdesc)*(mfmheader.number_of_track*mfmheader.number_of_side));
		if(trackpos&0x1FF)
		{
			trackpos=(trackpos&(~0x1FF))+0x200;
		}
		do
		{
			for(j=0;j<(mfmheader.number_of_side);j++)
			{
				memset(&mfmtrackdesc,0,sizeof(mfmtrackdesc));
				mfmsize=floppy->tracks[i]->sides[j]->tracklen;
				if(mfmsize&7)
					mfmsize=(mfmsize/8)+1;
				else
					mfmsize=mfmsize/8;

				mfmtrackdesc.mfmtracksize=mfmsize;
				mfmtrackdesc.side_number=j;
				mfmtrackdesc.track_number=i;
				offsettrack[(i*mfmheader.number_of_side)+j]=(int32_t)trackpos;
				mfmtrackdesc.mfmtrackoffset=trackpos;

				imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"Write Track %d:%d [%x] %x bytes",i,j,mfmtrackdesc.mfmtrackoffset,mfmsize);
				trackpos=trackpos+mfmsize;
				if(trackpos&0x1FF)
				{
					trackpos=(trackpos&(~0x1FF))+0x200;
				}

				if (fwrite(&mfmtrackdesc,sizeof(mfmtrackdesc),1,hxcmfmfile) != 1) { /* I/O error */ }
			}

			i++;
		}while(i<(mfmheader.number_of_track));

		mfmtrack = NULL;
		i=0;
		do
		{
			libflux_imgCallProgressCallback(imgldr_ctx,i,(mfmheader.number_of_track) );

			for(j=0;j<(mfmheader.number_of_side);j++)
			{
				memset(&mfmtrackdesc,0,sizeof(mfmtrackdesc));
				mfmsize=floppy->tracks[i]->sides[j]->tracklen;
				if(mfmsize&7)
					mfmsize=(mfmsize/8)+1;
				else
					mfmsize=mfmsize/8;

				mfmtrack = (unsigned char*) malloc(mfmsize);
				if( !mfmtrack )
				{
					free( offsettrack );
					libflux_fclose(hxcmfmfile);
					return LIBFLUX_INTERNALERROR;
				}

				if(ftell(hxcmfmfile)<offsettrack[(i*mfmheader.number_of_side)+j])
				{
					memset(mfmtrack,0,offsettrack[(i*mfmheader.number_of_side)+j]-ftell(hxcmfmfile));
					if (fwrite(mfmtrack,offsettrack[(i*mfmheader.number_of_side)+j]-ftell(hxcmfmfile),1,hxcmfmfile) != 1) { /* I/O error */ }
				}

				memcpy(mfmtrack,floppy->tracks[i]->sides[j]->databuffer,mfmsize);

				if (fwrite(mfmtrack,mfmsize,1,hxcmfmfile) != 1) { /* I/O error */ }
				free(mfmtrack);
				mfmtrack = NULL;
			}

			i++;
		}while(i<(mfmheader.number_of_track));

		free(offsettrack);
		offsettrack = NULL;

		libflux_fclose(hxcmfmfile);

		return LIBFLUX_NOERROR;
	}
	else
	{
		imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"Cannot create %s!",filename);

		return LIBFLUX_ACCESSERROR;
	}
}
