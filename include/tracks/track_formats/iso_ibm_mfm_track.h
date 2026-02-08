#ifndef UFT_ISO_IBM_MFM_TRACK_H
#define UFT_ISO_IBM_MFM_TRACK_H

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

int get_next_MFM_sector(LIBFLUX_CTX* flux_ctx,LIBFLUX_SIDE * track,LIBFLUX_SECTCFG * sector,int track_offset);
int write_MFM_sectordata(LIBFLUX_CTX* flux_ctx,LIBFLUX_SIDE * track,LIBFLUX_SECTCFG * sector,unsigned char * buffer,int buffersize);

#endif /* UFT_ISO_IBM_MFM_TRACK_H */
