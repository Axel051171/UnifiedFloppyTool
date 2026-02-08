#ifndef UFT_EMU_EMULATOR_FM_TRACK_H
#define UFT_EMU_EMULATOR_FM_TRACK_H

/*
//
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

#define DEFAULT_EMUII_BITRATE 312500

int32_t BuildEmuIITrack(LIBFLUX_CTX* flux_ctx,int tracknumber,int sidenumber,unsigned char* datain,unsigned char * fmdata,int32_t * fmsizebuffer,int trackformat);

int get_next_EMU_sector(LIBFLUX_CTX* flux_ctx,LIBFLUX_SIDE * track,LIBFLUX_SECTCFG * sector,int track_offset);

#endif /* UFT_EMU_EMULATOR_FM_TRACK_H */
