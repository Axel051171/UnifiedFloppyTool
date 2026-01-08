#ifndef UFT_LOADERS_HFE_LOADER_HFEV3_TRACKGEN_H
#define UFT_LOADERS_HFE_LOADER_HFEV3_TRACKGEN_H

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
// File : variablebitrate.h
// Contains: UFT HFE data generator
//
// Written by: Jean-François DEL NERO
//
// Change History (most recent first):
///////////////////////////////////////////////////////////////////////////////////

#define FLOPPYEMUFREQ 36000000

#define OPCODE_MASK       0xF0

#define NOP_OPCODE        0xF0
#define SETINDEX_OPCODE   0xF1
#define SETBITRATE_OPCODE 0xF2
#define SKIPBITS_OPCODE   0xF3
#define RAND_OPCODE       0xF4

typedef struct trackzone_
{
	int32_t  start;
	int32_t  end;
	uint32_t bitrate;

	uint8_t  code1;
	uint32_t code1lenint;

	uint8_t  code2;
	uint32_t code2lenint;

}trackzone;

typedef struct trackpart_
{
	uint8_t code;
	int32_t len;
}trackpart;

#define GRANULA 64

int32_t GenOpcodesTrack(LIBFLUX_CTX* flux_ctx,uint8_t * index_h0,uint8_t * datah0,uint32_t lendatah0,uint8_t * datah1,uint32_t lendatah1,uint8_t * randomh0,uint8_t * randomh1,int32_t fixedbitrateh0,uint32_t * timeh0,int32_t fixedbitrateh1,uint32_t * timeh1,uint8_t ** finalbuffer_H0_param,uint8_t ** finalbuffer_H1_param,uint8_t ** randomfinalbuffer_param);

#endif /* UFT_LOADERS_HFE_LOADER_HFEV3_TRACKGEN_H */
