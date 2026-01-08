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
///////////////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------------//
//-----------H----H--X----X-----CCCCC-----22222----0000-----0000-----11----------//
//----------H----H----X-X-----C--------------2---0----0---0----0---1-1-----------//
//---------HHHHHH-----X------C----------22222---0----0---0----0-----1------------//
//--------H----H----X--X----C----------2-------0----0---0----0-----1-------------//
//-------H----H---X-----X---CCCCC-----22222----0000-----0000----11111------------//
//-------------------------------------------------------------------------------//

///////////////////////////////////////////////////////////////////////////////////
// File : sector_extractor.c
// Contains: ISO/IBM sector reader
//
// Written by: Jean-Franois DEL NERO
//
// Change History (most recent first):
///////////////////////////////////////////////////////////////////////////////////

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "types.h"

#include "libflux.h""
#include "tracks/track_generator.h"
#include "sector_search.h"
#include "fdc_ctrl.h"

#include "libflux.h""

#include "sector_extractor.h"
#include "tracks/crc.h"

#include "tracks/track_formats/aed6200p_track.h"
#include "tracks/track_formats/amiga_mfm_track.h"
#include "tracks/track_formats/apple2_gcr_track.h"
#include "tracks/track_formats/apple_mac_gcr_track.h"
#include "tracks/track_formats/arburg_track.h"
#include "tracks/track_formats/c64_gcr_track.h"
#include "tracks/track_formats/centurion_mfm_track.h"
#include "tracks/track_formats/dec_rx02_track.h"
#include "tracks/track_formats/emu_emulator_fm_track.h"
#include "tracks/track_formats/heathkit_fm_track.h"
#include "tracks/track_formats/iso_ibm_fm_track.h"
#include "tracks/track_formats/iso_ibm_mfm_track.h"
#include "tracks/track_formats/membrain_mfm_track.h"
#include "tracks/track_formats/northstar_mfm_track.h"
#include "tracks/track_formats/tycom_fm_track.h"
#include "tracks/track_formats/qd_mo5_track.h"
#include "tracks/track_formats/victor9k_gcr_track.h"
#include "tracks/track_formats/micraln_fm_track.h"

#include "trackutils.h"

#include "tracks/luts.h"

void checkEmptySector(LIBFLUX_SECTCFG * sector)
{
	int k,sector_size;
	unsigned char c;

	sector_size = sector->sectorsize;
	c = sector->input_data[0];
	k = 0;
	while( ( k < sector_size ) && ( c == sector->input_data[k] ) )
	{
		k++;
	};

	if( k == sector_size )
	{
		sector->fill_byte = c;
		sector->fill_byte_used = 0xFF;
	}
}

LIBFLUX_SECTORACCESS* libflux_initSectorAccess(LIBFLUX_CTX* flux_ctx,LIBFLUX_FLOPPY *fp)
{
	LIBFLUX_SECTORACCESS* ss_ctx;
	int i;

	if(!fp)
		return NULL;

	ss_ctx = (LIBFLUX_SECTORACCESS*) malloc(sizeof(LIBFLUX_SECTORACCESS));

	if(!ss_ctx)
		return ss_ctx;

	memset(ss_ctx,0,sizeof(LIBFLUX_SECTORACCESS));

	ss_ctx->fp = fp;
	ss_ctx->bitoffset = 0;
	ss_ctx->old_bitoffset = 0;
	ss_ctx->cur_side = 0;
	ss_ctx->cur_track = 0;
	ss_ctx->ctx = flux_ctx;

	if(fp->floppyNumberOfTrack)
	{
		ss_ctx->track_cache = malloc(sizeof(SECTORSEARCHTRACKCACHE) * fp->floppyNumberOfTrack * 2);
		if(ss_ctx->track_cache)
		{
			for(i=0;i<fp->floppyNumberOfTrack * 2;i++)
			{
				ss_ctx->track_cache[i].nb_sector_cached = 0;
			}
		}
		else
		{
			free(ss_ctx);
			ss_ctx = NULL;
		}
	}

	return ss_ctx;
}

LIBFLUX_SECTCFG* libflux_getNextSector( LIBFLUX_SECTORACCESS* ss_ctx, int32_t track, int32_t side, int32_t type )
{
	LIBFLUX_SECTCFG * sc;
	SECTORSEARCHTRACKCACHE * trackcache;
	int bitoffset,tmp_bitoffset;
	int i;

	if((ss_ctx->bitoffset == -1) || (ss_ctx->cur_side != side) || (ss_ctx->cur_track != track))
	{
		bitoffset = 0;
	}
	else
		bitoffset = ss_ctx->bitoffset;

	ss_ctx->cur_track = track;
	ss_ctx->cur_side = side;

	if(!ss_ctx->fp)
		return 0;

	if(track >= ss_ctx->fp->floppyNumberOfTrack)
		return 0;

	if(!ss_ctx->fp->tracks[track])
		return 0;

	if(side >= ss_ctx->fp->tracks[track]->number_of_side)
		return 0;

	if(!ss_ctx->fp->tracks[track]->sides[side])
		return 0;

	// end of track already reached
	if( ss_ctx->old_bitoffset > ss_ctx->bitoffset )
		return 0;

	ss_ctx->old_bitoffset = ss_ctx->bitoffset;

	sc=(LIBFLUX_SECTCFG *) malloc(sizeof(LIBFLUX_SECTCFG));

	tmp_bitoffset = bitoffset;
	switch(type)
	{
		case ISOIBM_MFM_ENCODING:
			bitoffset = get_next_MFM_sector(ss_ctx->ctx,ss_ctx->fp->tracks[track]->sides[side],sc,bitoffset);
		break;
		case AMIGA_MFM_ENCODING:
			bitoffset = get_next_AMIGAMFM_sector(ss_ctx->ctx,ss_ctx->fp->tracks[track]->sides[side],sc,bitoffset);
		break;
		case ISOIBM_FM_ENCODING:
			bitoffset = get_next_FM_sector(ss_ctx->ctx,ss_ctx->fp->tracks[track]->sides[side],sc,bitoffset);
		break;
		case DEC_RX02_M2FM_ENCODING:
			bitoffset = get_next_dec_rx02_sector(ss_ctx->ctx,ss_ctx->fp->tracks[track]->sides[side],sc,bitoffset);
		break;
		case TYCOM_FM_ENCODING:
			bitoffset = get_next_TYCOMFM_sector(ss_ctx->ctx,ss_ctx->fp->tracks[track]->sides[side],sc,bitoffset);
		break;
		case MEMBRAIN_MFM_ENCODING:
			bitoffset = get_next_MEMBRAIN_sector(ss_ctx->ctx,ss_ctx->fp->tracks[track]->sides[side],sc,bitoffset);
		break;
		case EMU_FM_ENCODING:
			bitoffset = get_next_EMU_sector(ss_ctx->ctx,ss_ctx->fp->tracks[track]->sides[side],sc,bitoffset);
		break;
		case APPLEII_GCR1_ENCODING:
			bitoffset = get_next_A2GCR1_sector(ss_ctx->ctx,ss_ctx->fp->tracks[track]->sides[side],sc,bitoffset);
		break;
		case APPLEII_GCR2_ENCODING:
			bitoffset = get_next_A2GCR2_sector(ss_ctx->ctx,ss_ctx->fp->tracks[track]->sides[side],sc,bitoffset);
		break;
		case APPLEMAC_GCR_ENCODING:
			bitoffset = get_next_AppleMacGCR_sector(ss_ctx->ctx,ss_ctx->fp->tracks[track]->sides[side],sc,bitoffset);
		break;
		case ARBURGDAT_ENCODING:
			bitoffset = get_next_Arburg_sector(ss_ctx->ctx,ss_ctx->fp->tracks[track]->sides[side],sc,bitoffset);
		break;
		case ARBURGSYS_ENCODING:
			bitoffset = get_next_ArburgSyst_sector(ss_ctx->ctx,ss_ctx->fp->tracks[track]->sides[side],sc,bitoffset);
		break;
		case AED6200P_MFM_ENCODING:
			bitoffset = get_next_AED6200P_sector(ss_ctx->ctx,ss_ctx->fp->tracks[track]->sides[side],sc,bitoffset);
		break;
		case NORTHSTAR_HS_MFM_ENCODING:
			bitoffset = get_next_MFM_Northstar_sector(ss_ctx->ctx,ss_ctx->fp->tracks[track]->sides[side],sc,bitoffset);
		break;
		case HEATHKIT_HS_FM_ENCODING:
			bitoffset = get_next_FM_Heathkit_sector(ss_ctx->ctx,ss_ctx->fp->tracks[track]->sides[side],sc,bitoffset);
		break;
		case QD_MO5_ENCODING:
			bitoffset = get_next_QDMO5_sector(ss_ctx->ctx,ss_ctx->fp->tracks[track]->sides[side],sc,bitoffset);
		break;
		case C64_GCR_ENCODING:
			bitoffset = get_next_C64_sector(ss_ctx->ctx,ss_ctx->fp->tracks[track]->sides[side],sc,bitoffset);
		break;
		case VICTOR9K_GCR_ENCODING:
			bitoffset = get_next_Victor9k_sector(ss_ctx->ctx,ss_ctx->fp->tracks[track]->sides[side],sc,bitoffset);
		break;
		case MICRALN_HS_FM_ENCODING:
			bitoffset = get_next_FM_MicralN_sector(ss_ctx->ctx,ss_ctx->fp->tracks[track]->sides[side],sc,bitoffset);
		break;
		case CENTURION_MFM_ENCODING:
			bitoffset = get_next_Centurion_MFM_sector(ss_ctx->ctx,ss_ctx->fp->tracks[track]->sides[side],sc,bitoffset);
		break;
		default:
			bitoffset=-1;
		break;
	}

	if(bitoffset == tmp_bitoffset)
	{
		ss_ctx->bitoffset = -1;
		free(sc);
		return 0;
	}

	ss_ctx->bitoffset = bitoffset;

	if(track<ss_ctx->fp->floppyNumberOfTrack && ss_ctx->track_cache)
	{
		trackcache = &ss_ctx->track_cache[(track<<1) | (side&1)];
		if( trackcache->nb_sector_cached < MAX_CACHED_SECTOR && bitoffset>=0)
		{
			//Add a new cache entry
			i=0;
			while( i < trackcache->nb_sector_cached && ( trackcache->sectorcache[i].startsectorindex != sc->startsectorindex) && i < MAX_CACHED_SECTOR )
			{
				i++;
			}

			if( i < MAX_CACHED_SECTOR )
			{
				if(i == trackcache->nb_sector_cached)
				{
					memcpy(&(trackcache->sectorcache[i]),sc,sizeof(LIBFLUX_SECTCFG));
					trackcache->sectorcache[i].input_data = 0;
					trackcache->nb_sector_cached++;
				}
			}
		}
	}

	if(bitoffset!=-1)
		return sc;
	else
	{
		free(sc);
		return 0;
	}
}

void libflux_resetSearchTrackPosition(LIBFLUX_SECTORACCESS* ss_ctx)
{
	if(ss_ctx)
	{
		ss_ctx->bitoffset = 0;
		ss_ctx->old_bitoffset = 0;
	}
}

LIBFLUX_SECTCFG** libflux_getAllTrackSectors( LIBFLUX_SECTORACCESS* ss_ctx, int32_t track, int32_t side, int32_t type, int32_t * nb_sectorfound )
{
	int i;
	LIBFLUX_SECTCFG * sc;
	LIBFLUX_SECTCFG ** scarray;
	int nb_of_sector;

	nb_of_sector = 0;
	// First : Count the number of sectors
	libflux_resetSearchTrackPosition(ss_ctx);
	do
	{
		sc = libflux_getNextSector(ss_ctx,track,side,type);

		if(sc)
			nb_of_sector++;

		libflux_freeSectorConfig( ss_ctx, sc );

	}while(sc);

	if(nb_sectorfound)
		*nb_sectorfound = nb_of_sector;

	libflux_resetSearchTrackPosition(ss_ctx);
	scarray = 0;
	if(nb_of_sector)
	{
		scarray = malloc(sizeof(LIBFLUX_SECTCFG *) * (nb_of_sector+1));
		if(scarray)
		{
			memset(scarray,0,sizeof(LIBFLUX_SECTCFG *) * (nb_of_sector+1));
			for(i=0;i<nb_of_sector;i++)
			{
				sc = libflux_getNextSector(ss_ctx,track,side,type);
				scarray[i] = sc;
			}
		}
	}

	libflux_resetSearchTrackPosition(ss_ctx);

	return scarray;
}

LIBFLUX_SECTCFG** libflux_getAllTrackISOSectors( LIBFLUX_SECTORACCESS* ss_ctx, int32_t track, int32_t side, int32_t * nb_sectorfound )
{
	int i,i_fm,i_mfm;
	LIBFLUX_SECTCFG * sc;

	LIBFLUX_SECTCFG ** sc_fm_array;
	LIBFLUX_SECTCFG ** sc_mfm_array;

	LIBFLUX_SECTCFG ** scarray;
	int nb_of_sector;

	nb_of_sector = 0;
	// First : Count the number of sectors
	libflux_resetSearchTrackPosition(ss_ctx);
	do
	{
		sc = libflux_getNextSector(ss_ctx,track,side,ISOIBM_MFM_ENCODING);

		if(sc)
			nb_of_sector++;

		libflux_freeSectorConfig( ss_ctx, sc );

	}while(sc);

	// FM
	libflux_resetSearchTrackPosition(ss_ctx);
	do
	{
		sc = libflux_getNextSector(ss_ctx,track,side,ISOIBM_FM_ENCODING);

		if(sc)
			nb_of_sector++;

		libflux_freeSectorConfig( ss_ctx, sc );

	}while(sc);

	if(nb_sectorfound)
		*nb_sectorfound = nb_of_sector;

	scarray = 0;
	if(nb_of_sector)
	{
		sc_mfm_array = malloc(sizeof(LIBFLUX_SECTCFG *) * (nb_of_sector+1));
		sc_fm_array  = malloc(sizeof(LIBFLUX_SECTCFG *) * (nb_of_sector+1));
		scarray = malloc(sizeof(LIBFLUX_SECTCFG *) * (nb_of_sector+1));
		if(scarray && sc_mfm_array && sc_fm_array)
		{
			memset(scarray     ,0,sizeof(LIBFLUX_SECTCFG *) * (nb_of_sector+1));
			memset(sc_mfm_array,0,sizeof(LIBFLUX_SECTCFG *) * (nb_of_sector+1));
			memset(sc_fm_array ,0,sizeof(LIBFLUX_SECTCFG *) * (nb_of_sector+1));

			libflux_resetSearchTrackPosition(ss_ctx);
			i = 0;
			do
			{
				sc = libflux_getNextSector(ss_ctx,track,side,ISOIBM_FM_ENCODING);
				sc_fm_array[i] = sc;
				i++;
			}while(sc);

			libflux_resetSearchTrackPosition(ss_ctx);
			i = 0;
			do
			{
				sc = libflux_getNextSector(ss_ctx,track,side,ISOIBM_MFM_ENCODING);
				sc_mfm_array[i] = sc;
				i++;
			}while(sc);

			i_fm = 0;
			i_mfm = 0;
			for(i=0;i<nb_of_sector;i++)
			{
				if(sc_fm_array[i_fm] && sc_mfm_array[i_mfm])
				{
					if(sc_fm_array[i_fm]->startsectorindex < sc_mfm_array[i_mfm]->startsectorindex)
					{
						scarray[i] = malloc(sizeof(LIBFLUX_SECTCFG));
						if(scarray[i])
						{
							memset(scarray[i],0,sizeof(LIBFLUX_SECTCFG));
							memcpy(scarray[i], sc_fm_array[i_fm], sizeof(LIBFLUX_SECTCFG));
							free(sc_fm_array[i_fm]);
						}
						i_fm++;
					}
					else
					{
						scarray[i] = malloc(sizeof(LIBFLUX_SECTCFG));
						if(scarray[i])
						{
							memset(scarray[i],0,sizeof(LIBFLUX_SECTCFG));
							memcpy(scarray[i], sc_mfm_array[i_mfm], sizeof(LIBFLUX_SECTCFG));
							free(sc_mfm_array[i_mfm]);
						}
						i_mfm++;
					}
				}
				else
				{
					if(sc_fm_array[i_fm])
					{
						scarray[i] = malloc(sizeof(LIBFLUX_SECTCFG));
						if(scarray[i])
						{
							memset(scarray[i],0,sizeof(LIBFLUX_SECTCFG));
							memcpy(scarray[i], sc_fm_array[i_fm], sizeof(LIBFLUX_SECTCFG));
							free(sc_fm_array[i_fm]);
						}
						i_fm++;
					}
					else
					{
						if(sc_mfm_array[i_mfm])
						{
							scarray[i] = malloc(sizeof(LIBFLUX_SECTCFG));
							if(scarray[i])
							{
								memset(scarray[i],0,sizeof(LIBFLUX_SECTCFG));
								memcpy(scarray[i], sc_mfm_array[i_mfm], sizeof(LIBFLUX_SECTCFG));
								free(sc_mfm_array[i_mfm]);
							}
							i_mfm++;
						}
					}
				}
			}
		}

		if(sc_fm_array)
			free(sc_fm_array);
		if(sc_mfm_array)
			free(sc_mfm_array);

	}

	libflux_resetSearchTrackPosition(ss_ctx);

	return scarray;
}

void libflux_clearTrackCache(LIBFLUX_SECTORACCESS* ss_ctx)
{
	int i,t,s;
	SECTORSEARCHTRACKCACHE * trackcache;

	if(ss_ctx->track_cache)
	{
		for(t=0;t<ss_ctx->fp->floppyNumberOfTrack;t++)
		{
			for(s=0;s<ss_ctx->fp->floppyNumberOfSide;s++)
			{
				trackcache = &ss_ctx->track_cache[(t<<1) | (s&1)];

				i = 0;
				while( i < trackcache->nb_sector_cached )
				{
					memset(&trackcache->sectorcache[i],0,sizeof(LIBFLUX_SECTCFG));
					i++;
				}
				trackcache->nb_sector_cached = 0;
			}
		}
	}
}

LIBFLUX_SECTCFG* libflux_searchSector ( LIBFLUX_SECTORACCESS* ss_ctx, int32_t track, int32_t side, int32_t id, int32_t type )
{
	LIBFLUX_SECTCFG * sc;
	SECTORSEARCHTRACKCACHE * trackcache;
	int i;

	if(track<ss_ctx->fp->floppyNumberOfTrack && ss_ctx->track_cache)
	{
		trackcache = &ss_ctx->track_cache[(track<<1) | (side&1)];

		// Search in the cache
		i = 0;
		while( i < trackcache->nb_sector_cached )
		{
			if((trackcache->sectorcache[i].sector == id) && (trackcache->sectorcache[i].cylinder == track) && ( (trackcache->sectorcache[i].head == side) || (ss_ctx->flags & SECTORACCESS_IGNORE_SIDE_ID) ) )
			{
				ss_ctx->cur_side = side;
				ss_ctx->cur_track = track;
				ss_ctx->bitoffset = trackcache->sectorcache[i].startsectorindex;
				ss_ctx->old_bitoffset = ss_ctx->bitoffset;
				sc = libflux_getNextSector(ss_ctx,track,side,type);
				return sc;
			}
			i++;
		}

		if(trackcache->nb_sector_cached)
		{
			ss_ctx->bitoffset = trackcache->sectorcache[trackcache->nb_sector_cached-1].startdataindex+1;
			ss_ctx->cur_side = side;
			ss_ctx->cur_track = track;
			ss_ctx->old_bitoffset = ss_ctx->bitoffset;
		}
		else
		{
			libflux_resetSearchTrackPosition(ss_ctx);
		}
	}
	else
	{
		libflux_resetSearchTrackPosition(ss_ctx);

		if(track>=ss_ctx->fp->floppyNumberOfTrack)
		{
			return 0;
		}
	}

	do
	{
		sc = libflux_getNextSector(ss_ctx,track,side,type);

		if(sc)
		{
			if(sc->sector == id )
			{
				return sc;
			}
			else
			{
				libflux_freeSectorConfig( ss_ctx, sc );
			}
		}

	}while( sc );

	return 0;
}

void libflux_setSectorAccessFlags( LIBFLUX_SECTORACCESS* ss_ctx, uint32_t flags)
{
	ss_ctx->flags = flags;
}

int32_t libflux_getSectorSize( LIBFLUX_SECTORACCESS* ss_ctx, LIBFLUX_SECTCFG* sc )
{
	return sc->sectorsize;
}

uint8_t * libflux_getSectorData(LIBFLUX_SECTORACCESS* ss_ctx,LIBFLUX_SECTCFG* sc)
{
	return sc->input_data;
}

int32_t libflux_getFloppySize( LIBFLUX_CTX* flux_ctx, LIBFLUX_FLOPPY *fp, int32_t * nbsector )
{
	LIBFLUX_SECTORACCESS* ss_ctx;
	LIBFLUX_SECTCFG* sc;
	int floppysize;
	int nbofsector;
	int track;
	int side;
	int i,type,secfound,t;
	int typetab[16];

	floppysize=0;
	nbofsector=0;

	type=0;
	secfound=0;

	i=0;
	typetab[i++]=ISOIBM_MFM_ENCODING;
	typetab[i++]=AMIGA_MFM_ENCODING;
	typetab[i++]=ISOIBM_FM_ENCODING;
	typetab[i++]=TYCOM_FM_ENCODING;
	typetab[i++]=MEMBRAIN_MFM_ENCODING;
	typetab[i++]=EMU_FM_ENCODING;
	typetab[i++]=APPLEII_GCR1_ENCODING;
	typetab[i++]=APPLEII_GCR2_ENCODING;
	typetab[i++]=APPLEMAC_GCR_ENCODING;
	typetab[i++]=-1;

	ss_ctx=libflux_initSectorAccess(flux_ctx,fp);
	if(ss_ctx)
	{
		for(track=0;track<fp->floppyNumberOfTrack;track++)
		{
			for(side=0;side<fp->floppyNumberOfSide;side++)
			{
				secfound=0;
				type=0;

				while(typetab[type]!=-1 && !secfound)
				{
					libflux_resetSearchTrackPosition(ss_ctx);
					do
					{
						sc=libflux_getNextSector(ss_ctx,track,side,typetab[type]);
						if(sc)
						{
							floppysize=floppysize+sc->sectorsize;
							nbofsector++;
							secfound=1;

							libflux_freeSectorConfig(ss_ctx,sc);
						}

					}while(sc);

					if(!secfound)
					{
						libflux_resetSearchTrackPosition(ss_ctx);
						type++;
					}

				}

				if(secfound)
				{
					t=typetab[0];
					typetab[0]=typetab[type];
					typetab[type]=t;
				}
			}
		}
	}

	if(nbsector)
		*nbsector=nbofsector;

	libflux_deinitSectorAccess(ss_ctx);
	return floppysize;

}

int32_t libflux_readSectorData( LIBFLUX_SECTORACCESS* ss_ctx, int32_t track, int32_t side, int32_t sector, int32_t numberofsector, int32_t sectorsize, int32_t type, uint8_t * buffer, int32_t * fdcstatus )
{
	LIBFLUX_SECTCFG * sc;
	int nbsectorread;

	nbsectorread=0;

	if(fdcstatus)
		*fdcstatus = FDC_ACCESS_ERROR;

	if ( side < ss_ctx->fp->floppyNumberOfSide && track < ss_ctx->fp->floppyNumberOfTrack )
	{
		if(fdcstatus)
			*fdcstatus = FDC_NOERROR;

		do
		{
			sc = libflux_searchSector ( ss_ctx, track, side, sector + nbsectorread, type);
			if(sc)
			{
				if(sc->sectorsize == sectorsize)
				{
					if(sc->input_data)
					{
						memcpy(&buffer[sectorsize*(sc->sector-sector)],sc->input_data,sectorsize);
					}
					else
					{
						if(fdcstatus)
							*fdcstatus = FDC_NO_DATA;
					}

					if(sc->use_alternate_data_crc)
					{
						if(fdcstatus)
							*fdcstatus = FDC_BAD_DATA_CRC;

						ss_ctx->ctx->libflux_printf(MSG_ERROR,"libflux_readSectorData : ERROR -> Bad Data CRC ! track %d, side %d, sector %d,Sector size:%d,Type:%x",track,side,sector+nbsectorread,sectorsize,type);
					}

					libflux_freeSectorConfig( ss_ctx, sc );

					nbsectorread++;
				}
				else
				{
					libflux_freeSectorConfig( ss_ctx, sc );
					return 0;
				}
			}
			else
			{
				if(fdcstatus)
					*fdcstatus = FDC_SECTOR_NOT_FOUND;

				ss_ctx->ctx->libflux_printf(MSG_ERROR,"libflux_readSectorData : ERROR -> Sector not found ! track %d, side %d, sector %d,Sector size:%d,Type:%x",track,side,sector+nbsectorread,sectorsize,type);
			}

		}while((nbsectorread<numberofsector) && sc);
	}

	return nbsectorread;
}

int32_t libflux_writeSectorData( LIBFLUX_SECTORACCESS* ss_ctx, int32_t track, int32_t side, int32_t sector, int32_t numberofsector, int32_t sectorsize, int32_t type, uint8_t * buffer, int32_t * fdcstatus )
{
	LIBFLUX_SECTCFG * sc;
	int nbsectorwrite;

	nbsectorwrite=0;

	if(fdcstatus)
		*fdcstatus = FDC_ACCESS_ERROR;

	if ( side < ss_ctx->fp->floppyNumberOfSide && track < ss_ctx->fp->floppyNumberOfTrack )
	{
		if(fdcstatus)
			*fdcstatus = FDC_NOERROR;

		do
		{
			sc = libflux_searchSector ( ss_ctx, track, side, sector + nbsectorwrite, type);
			if(sc)
			{
				if(((sc->sector>=sector) && (sc->sector < ( sector + numberofsector )) ) )
				{
					switch(type)
					{
						case ISOIBM_MFM_ENCODING:
							write_MFM_sectordata(ss_ctx->ctx,ss_ctx->fp->tracks[track]->sides[side],sc,&buffer[sectorsize*nbsectorwrite],sectorsize);
						break;
						case AMIGA_MFM_ENCODING:
							write_AMIGAMFM_sectordata(ss_ctx->ctx,ss_ctx->fp->tracks[track]->sides[side],sc,&buffer[sectorsize*nbsectorwrite],sectorsize);
						break;
						case TYCOM_FM_ENCODING:
						case ISOIBM_FM_ENCODING:
							write_FM_sectordata(ss_ctx->ctx,ss_ctx->fp->tracks[track]->sides[side],sc,&buffer[sectorsize*nbsectorwrite],sectorsize);
						break;
						case DEC_RX02_M2FM_ENCODING:
							write_dec_rx02_sectordata(ss_ctx->ctx,ss_ctx->fp->tracks[track]->sides[side],sc,&buffer[sectorsize*nbsectorwrite],sectorsize);
						break;
						case MEMBRAIN_MFM_ENCODING:
							//write_ALT01_MFM_sectordata(ss_ctx->ctx,ss_ctx->fp->tracks[track]->sides[side],sc,&buffer[sectorsize*nbsectorwrite],sectorsize);
						break;
						case EMU_FM_ENCODING:
						break;
						default:
						break;
					}

					nbsectorwrite++;
				}

				libflux_freeSectorConfig( ss_ctx, sc );
			}
			else
			{
				if(fdcstatus)
					*fdcstatus = FDC_SECTOR_NOT_FOUND;

				ss_ctx->ctx->libflux_printf(MSG_ERROR,"libflux_writeSectorData : ERROR -> Sector not found ! track %d, side %d, sector %d,Sector size:%d,Type:%x",track,side,sector+nbsectorwrite,sectorsize,type);
			}

		}while(( nbsectorwrite < numberofsector ) && sc);
	}

	return nbsectorwrite;
}

void libflux_freeSectorConfigData( LIBFLUX_SECTORACCESS* ss_ctx, LIBFLUX_SECTCFG* sc )
{
	if(sc)
	{
		free(sc->input_data);
		sc->input_data = NULL;

		free(sc->input_data_index);
		sc->input_data_index = NULL;

		free(sc->weak_bits_mask);
		sc->weak_bits_mask = NULL;
	}
}

void libflux_freeSectorConfig( LIBFLUX_SECTORACCESS* ss_ctx, LIBFLUX_SECTCFG* sc )
{
	if(sc)
	{
		libflux_freeSectorConfigData( ss_ctx, sc );

		free(sc);
	}
}

int32_t libflux_getSectorConfigEncoding(LIBFLUX_CTX* flux_ctx,LIBFLUX_SECTCFG* sc)
{
	if(sc)
	{
		return sc->trackencoding;
	}
	return 0;
}

int32_t libflux_getSectorConfigSectorID(LIBFLUX_CTX* flux_ctx,LIBFLUX_SECTCFG* sc)
{
	if(sc)
	{
		return sc->sector;
	}
	return 0;
}

int32_t libflux_getSectorConfigDataMark(LIBFLUX_CTX* flux_ctx,LIBFLUX_SECTCFG* sc)
{
	if(sc)
	{
		return sc->alternate_datamark;
	}
	return 0;
}

int32_t libflux_getSectorConfigSideID(LIBFLUX_CTX* flux_ctx,LIBFLUX_SECTCFG* sc)
{
	if(sc)
	{
		return sc->head;
	}
	return 0;
}

int32_t libflux_getSectorConfigSizeID(LIBFLUX_CTX* flux_ctx,LIBFLUX_SECTCFG* sc)
{
	if(sc)
	{
		return sc->alternate_sector_size_id;
	}
	return 0;
}

int32_t libflux_getSectorConfigTrackID(LIBFLUX_CTX* flux_ctx,LIBFLUX_SECTCFG* sc)
{
	if(sc)
	{
		return sc->cylinder;
	}
	return 0;
}

uint32_t libflux_getSectorConfigHCRC(LIBFLUX_CTX* flux_ctx,LIBFLUX_SECTCFG* sc)
{
	if(sc)
	{
		return sc->header_crc;
	}
	return 0;
}

uint32_t libflux_getSectorConfigDCRC(LIBFLUX_CTX* flux_ctx,LIBFLUX_SECTCFG* sc)
{
	if(sc)
	{
		return sc->data_crc;
	}
	return 0;
}

int32_t libflux_getSectorConfigSectorSize(LIBFLUX_CTX* flux_ctx,LIBFLUX_SECTCFG* sc)
{
	if(sc)
	{
		return sc->sectorsize;
	}
	return 0;
}

int32_t libflux_getSectorConfigStartSectorIndex(LIBFLUX_CTX* flux_ctx,LIBFLUX_SECTCFG* sc)
{
	if(sc)
	{
		return sc->startsectorindex;
	}
	return 0;
}

int32_t libflux_getSectorConfigStartDataIndex(LIBFLUX_CTX* flux_ctx,LIBFLUX_SECTCFG* sc)
{
	if(sc)
	{
		return sc->startdataindex;
	}
	return 0;
}

int32_t libflux_getSectorConfigEndSectorIndex(LIBFLUX_CTX* flux_ctx,LIBFLUX_SECTCFG* sc)
{
	if(sc)
	{
		return sc->endsectorindex;
	}
	return 0;
}

uint8_t * libflux_getSectorConfigInputData(LIBFLUX_CTX* flux_ctx,LIBFLUX_SECTCFG* sc)
{
	if(sc)
	{
		return sc->input_data;
	}
	return 0;
}

int32_t libflux_getSectorConfigHCRCStatus(LIBFLUX_CTX* flux_ctx,LIBFLUX_SECTCFG* sc)
{
	if(sc)
	{
		return sc->use_alternate_header_crc;
	}
	return 0;
}

int32_t libflux_getSectorConfigDCRCStatus(LIBFLUX_CTX* flux_ctx,LIBFLUX_SECTCFG* sc)
{
	if(sc)
	{
		return sc->use_alternate_data_crc;
	}
	return 0;
}

void libflux_deinitSectorAccess(LIBFLUX_SECTORACCESS* ss_ctx)
{
	if(ss_ctx)
	{
		if(ss_ctx->track_cache)
			free(ss_ctx->track_cache);
		free(ss_ctx);
	}
}

LIBFLUX_FDCCTRL * libflux_initFDC (LIBFLUX_CTX* flux_ctx)
{
	LIBFLUX_FDCCTRL * fdc;

	fdc = malloc(sizeof(LIBFLUX_FDCCTRL));
	if( fdc )
	{
		memset(fdc,0,sizeof(LIBFLUX_FDCCTRL));
		fdc->flux_ctx = flux_ctx;
		return fdc;
	}

	return 0;
}

int32_t libflux_insertDiskFDC (LIBFLUX_FDCCTRL * fdc, LIBFLUX_FLOPPY *fp )
{
	if(fdc)
	{
		fdc->loadedfp = fp;

		if( fdc->ss_ctx )
		{
			libflux_deinitSectorAccess(fdc->ss_ctx);
			fdc->ss_ctx = 0;
		}
		fdc->ss_ctx = libflux_initSectorAccess(fdc->flux_ctx,fp);

		return LIBFLUX_NOERROR;
	}

	return LIBFLUX_BADPARAMETER;
}

int32_t libflux_readSectorFDC (LIBFLUX_FDCCTRL * fdc, uint8_t track, uint8_t side, uint8_t sector, int32_t sectorsize, int32_t mode, int32_t nbsector, uint8_t * buffer, int32_t buffer_size, int32_t * fdcstatus )
{
	if(fdc)
	{
		if(fdc->ss_ctx && fdc->loadedfp && ((sectorsize*nbsector)<=buffer_size))
			return libflux_readSectorData(fdc->ss_ctx,track,side,sector,nbsector,sectorsize,mode,buffer,fdcstatus);
		else
			return LIBFLUX_BADPARAMETER;
	}

	return LIBFLUX_BADPARAMETER;
}

int32_t libflux_writeSectorFDC (LIBFLUX_FDCCTRL * fdc, uint8_t track, uint8_t side, uint8_t sector, int32_t sectorsize, int32_t mode, int32_t nbsector, uint8_t * buffer, int32_t buffer_size, int32_t * fdcstatus )
{
	if(fdc)
	{
		if(fdc->ss_ctx && fdc->loadedfp && ((sectorsize*nbsector)<=buffer_size))
			return libflux_writeSectorData(fdc->ss_ctx,track,side,sector,nbsector,sectorsize,mode,buffer,fdcstatus);
		else
			return LIBFLUX_BADPARAMETER;
	}

	return LIBFLUX_BADPARAMETER;
}

void libflux_deinitFDC (LIBFLUX_FDCCTRL * fdc)
{
	if(fdc)
	{
		if(fdc->ss_ctx)
			libflux_deinitSectorAccess(fdc->ss_ctx);
		free(fdc);
	}
}

int32_t libflux_FDC_READSECTOR  ( LIBFLUX_CTX* flux_ctx, LIBFLUX_FLOPPY *fp, uint8_t track, uint8_t side, uint8_t sector, int32_t sectorsize, int32_t mode, int32_t nbsector, uint8_t * buffer, int32_t buffer_size, int32_t * fdcstatus )
{
	LIBFLUX_FDCCTRL * fdcctrl;
	int cnt;

	cnt = 0;

	fdcctrl = libflux_initFDC (flux_ctx);
	if( fdcctrl )
	{
		libflux_insertDiskFDC (fdcctrl,fp);

		cnt = libflux_readSectorFDC (fdcctrl,track,side,sector,sectorsize,mode,nbsector,buffer,buffer_size,fdcstatus);

		libflux_deinitFDC (fdcctrl);
	}

	return cnt;
}

int32_t libflux_FDC_WRITESECTOR ( LIBFLUX_CTX* flux_ctx, LIBFLUX_FLOPPY *fp, uint8_t track, uint8_t side, uint8_t sector, int32_t sectorsize, int32_t mode, int32_t nbsector, uint8_t * buffer, int32_t buffer_size, int32_t * fdcstatus )
{
	LIBFLUX_FDCCTRL * fdcctrl;
	int cnt;

	cnt = 0;

	fdcctrl = libflux_initFDC (flux_ctx);
	if( fdcctrl )
	{
		libflux_insertDiskFDC (fdcctrl,fp);

		cnt = libflux_writeSectorFDC (fdcctrl,track,side,sector,sectorsize,mode,nbsector,buffer,buffer_size,fdcstatus);

		libflux_deinitFDC (fdcctrl);
	}

	return cnt;
}

int32_t libflux_FDC_FORMAT( LIBFLUX_CTX* flux_ctx, uint8_t track, uint8_t side, uint8_t nbsector, int32_t sectorsize, int32_t sectoridstart, int32_t skew, int32_t interleave, int32_t mode, int32_t * fdcstatus )
{
	//TODO
	return 0;
}

int32_t libflux_FDC_SCANSECTOR  ( LIBFLUX_CTX* flux_ctx, uint8_t track, uint8_t side, int32_t mode, uint8_t * sector, uint8_t * buffer, int32_t buffer_size, int32_t * fdcstatus )
{
	//TODO
	return 0;
}


int write_raw_track(FILE * f,LIBFLUX_SECTORACCESS* ss,int32_t startidsector,int32_t sectorpertrack,int32_t trk,int32_t side,int32_t sectorsize,int32_t tracktype, int * badsect, int * missingsect )
{
	int sect,i;
	LIBFLUX_SECTCFG * scfg;
	const char * badsectmess = "!! BAD SECTOR !!";
	const char * misssectmess= "!!  MISSING   !!";

	for( sect = 0 ; sect < sectorpertrack ; sect++ )
	{
		scfg = libflux_searchSector ( ss, trk, side, startidsector + sect, tracktype );
		if( scfg )
		{
			if( scfg->use_alternate_data_crc || !scfg->input_data )
			{
				*badsect = *badsect + 1;
			}

			if( ( scfg->sectorsize == sectorsize ) && scfg->input_data )
			{
				if (fwrite( scfg->input_data, scfg->sectorsize, 1, f ) != 1) { /* I/O error */ }
			}
			else
			{
				for( i = 0 ; i < sectorsize ; i++ )
				{
					fputc(badsectmess[i&0xF],f);
				}
			}

			libflux_freeSectorConfig( ss , scfg );
		}
		else
		{
			*missingsect = *missingsect + 1;
			for( i = 0 ; i < sectorsize ; i++ )
			{
				fputc(misssectmess[i&0xF],f);
			}
		}
	}

	return 0;
}

int write_raw_file(LIBFLUX_IMGLDR * imgldr_ctx,FILE * f,LIBFLUX_FLOPPY * fp,int32_t startidsector,int32_t sectorpertrack,int32_t nboftrack,int32_t nbofside,int32_t sectorsize,int32_t tracktype,int32_t sidefilelayout)
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
			switch (sidefilelayout)
			{
				// Normal layout
				case 0:
					for( trk = 0 ; trk < nboftrack ; trk++ )
					{
						for( side = 0 ; side < nbofside; side++ )
						{
							write_raw_track(f,ss,startidsector,sectorpertrack,trk,side,sectorsize,tracktype,&badsect,&missingsect );

							libflux_imgCallProgressCallback(imgldr_ctx,trk*2,nboftrack*2 );
						}
					}
				break;
				// Side 0 Tracks then Side 1 Tracks
				case 1:
					for( side = 0 ; side < nbofside; side++ )
					{
						for( trk = 0 ; trk < nboftrack ; trk++ )
						{
							write_raw_track(f,ss,startidsector,sectorpertrack,trk,side,sectorsize,tracktype,&badsect,&missingsect );

							libflux_imgCallProgressCallback( imgldr_ctx, (side*nboftrack) + trk, nboftrack*nbofside );
						}
					}
				break;
				// Side 0 Tracks then Side 1 Tracks "Serpentine"
				case 2:
					for( side = 0 ; side < nbofside; side++ )
					{
						for( trk = 0 ; trk < nboftrack ; trk++ )
						{
							if(side == 0)
								write_raw_track(f,ss,startidsector,sectorpertrack,trk,side,sectorsize,tracktype,&badsect,&missingsect );
							else
								write_raw_track(f,ss,startidsector,sectorpertrack,(nboftrack - 1) - trk,side,sectorsize,tracktype,&badsect,&missingsect );

							libflux_imgCallProgressCallback( imgldr_ctx, (side*nboftrack) + trk, nboftrack*nbofside );
						}
					}
				break;
			}

			libflux_deinitSectorAccess(ss);
		}
	}

	if(badsect || missingsect)
		return LIBFLUX_FILECORRUPTED;
	else
		return LIBFLUX_NOERROR;
}

int count_sector(LIBFLUX_CTX* flux_ctx,LIBFLUX_FLOPPY * fp,int32_t startidsector,int32_t track,int32_t side,int32_t sectorsize,int32_t tracktype, uint32_t flags)
{
	int sect_cnt;
	LIBFLUX_SECTORACCESS* ss;
	LIBFLUX_SECTCFG * scfg;

	sect_cnt = 0;
	if(fp)
	{
		ss = libflux_initSectorAccess( flux_ctx, fp );
		if(ss)
		{
			libflux_setSectorAccessFlags( ss, flags);

			do
			{
				scfg = libflux_searchSector ( ss, track, side, startidsector + sect_cnt, tracktype );
				if(scfg)
				{
					if((scfg->sectorsize == sectorsize) && scfg->input_data)
					{
						sect_cnt++;
					}

					libflux_freeSectorConfig( ss , scfg );
				}
			}while(scfg);

			libflux_deinitSectorAccess(ss);
		}
	}

	return sect_cnt;
}
