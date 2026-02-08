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

#ifndef LIBFLUX_DEC_M2FM_ENCODING_H
#define LIBFLUX_DEC_M2FM_ENCODING_H

#ifdef __cplusplus
extern "C" {
#endif

int decm2fmtobin(unsigned char * input_data,int input_data_size,unsigned char * decod_data,int decod_data_size,int bit_offset,int lastbit);
int mfmtodecm2fm(unsigned char * input_data,int input_data_size,int bit_offset,int size);

#ifdef __cplusplus
}
#endif

#endif /* LIBFLUX_DEC_M2FM_ENCODING_H */
