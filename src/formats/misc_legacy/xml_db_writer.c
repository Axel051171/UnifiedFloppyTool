/**
 * @file xml_db_writer.c
 * @brief XML database format writer
 * @version 3.8.0
 */
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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "types.h"

#include "libflux.h"
#include "track_generator.h"
#include "libflux.h"

#include "xml_db_loader.h"

#include "tracks/sector_extractor.h"

#include "uft_compat.h"

int XMLDB_libWrite_DiskFile(LIBFLUX_IMGLDR* imgldr_ctx,LIBFLUX_FLOPPY * floppy,char * filename)
{
	return 0;
}
