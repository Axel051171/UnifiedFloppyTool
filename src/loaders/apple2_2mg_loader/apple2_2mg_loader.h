#ifndef UFT_LOADERS_APPLE2_2MG_LOADER_APPLE2_2MG_LOADER_H
#define UFT_LOADERS_APPLE2_2MG_LOADER_APPLE2_2MG_LOADER_H

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

extern const unsigned char LogicalToPhysicalSectorMap_Dos33[16];
extern const unsigned char PhysicalToLogicalSectorMap_Dos33[16];
extern const unsigned char LogicalToPhysicalSectorMap_ProDos[16];
extern const unsigned char PhysicalToLogicalSectorMap_ProDos[16];

int Apple2_2mg_libGetPluginInfo(LIBFLUX_IMGLDR * imgldr_ctx,uint32_t infotype,void * returnvalue);


#endif /* UFT_LOADERS_APPLE2_2MG_LOADER_APPLE2_2MG_LOADER_H */
