#ifndef UFT_ARBURG_TRACK_H
#define UFT_ARBURG_TRACK_H

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

#define ARBURB_DATATRACK_SIZE 0xA00
#define ARBURB_SYSTEMTRACK_SIZE 0xF00

int32_t BuildArburgTrack(LIBFLUX_CTX* flux_ctx,int32_t tracknumber,int32_t sidenumber,uint8_t * datain,uint8_t * fmdata,int32_t * fmsizebuffer,int32_t trackformat);
int32_t BuildArburgSysTrack(LIBFLUX_CTX* flux_ctx,int32_t tracknumber,int32_t sidenumber,uint8_t * datain, uint8_t * fmdata, int32_t * fmsizebuffer,int32_t trackformat);

int get_next_Arburg_sector(LIBFLUX_CTX* flux_ctx,LIBFLUX_SIDE * track,LIBFLUX_SECTCFG * sector,int track_offset);
int get_next_ArburgSyst_sector(LIBFLUX_CTX* flux_ctx,LIBFLUX_SIDE * track,LIBFLUX_SECTCFG * sector,int track_offset);

#endif /* UFT_ARBURG_TRACK_H */
