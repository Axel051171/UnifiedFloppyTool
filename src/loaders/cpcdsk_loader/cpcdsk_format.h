#ifndef UFT_LOADERS_CPCDSK_LOADER_CPCDSK_FORMAT_H
#define UFT_LOADERS_CPCDSK_LOADER_CPCDSK_FORMAT_H

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
#pragma pack(1)

typedef struct cpcdsk_fileheader_
{
 int8_t   headertag[34]; // "EXTENDED CPC DSK File\r\nDisk-Info\r\n" ou  "MV - CPCEMU Disk-File\r\nDisk-Info\r\n"
 int8_t   creatorname[14];
 uint8_t  number_of_tracks;
 uint8_t  number_of_sides;
 uint16_t size_of_a_track; // not used in extended cpc dsk file
}cpcdsk_fileheader;


typedef struct cpcdsk_trackheader_
{
 int8_t    headertag[13];   // "Track-Info\r\n"
 uint16_t  unused1;
 uint8_t   unused1b;
 uint8_t   track_number;
 uint8_t   side_number;
 uint8_t   datarate;
 uint8_t   rec_mode;
 uint8_t   sector_size_code;
 uint8_t   number_of_sector;
 uint8_t   gap3_length;
 uint8_t   filler_byte;
}cpcdsk_trackheader;

typedef struct cpcdsk_sector_
{
 uint8_t   track;
 uint8_t   side;
 uint8_t   sector_id;
 uint8_t   sector_size_code;
 uint8_t   fdc_status_reg1;
 uint8_t   fdc_status_reg2;
 uint16_t  data_length;
}cpcdsk_sector;

#pragma pack()

#endif /* UFT_LOADERS_CPCDSK_LOADER_CPCDSK_FORMAT_H */
