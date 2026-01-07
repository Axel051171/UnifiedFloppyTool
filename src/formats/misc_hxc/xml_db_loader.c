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
//----------------------------------------------------- http://hxc2001.free.fr --//
///////////////////////////////////////////////////////////////////////////////////
// File : xml_db_loader.c
// Contains: XML database floppy format loader
//
// Written by: Jean-François DEL NERO
//
// Change History (most recent first):
///////////////////////////////////////////////////////////////////////////////////

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "types.h"

#include "libflux.h""
#include "libflux.h""

#include "uft_floppy_loader.h"
#include "uft_floppy_utils.h"

#include "xml_db_loader.h"
#include "xml_db_writer.h"

#include "libhxcadaptor.h"

LIBFLUX_XMLLDR * rfb = 0;

int XMLDB_libIsValidDiskFile( LIBFLUX_IMGLDR * imgldr_ctx, LIBFLUX_IMGLDR_FILEINFOS * imgfile )
{
	LIBFLUX_XMLLDR * xmlldr;

	imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"XMLDB_libIsValidDiskFile");

	xmlldr = libflux_initXmlFloppy( imgldr_ctx->ctx );

	if(xmlldr)
	{

		libflux_deinitXmlFloppy( xmlldr );

		return LIBFLUX_VALIDFILE;

	}
	else
	{
		imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"XMLDB_libIsValidDiskFile : Internal error !");
		return LIBFLUX_INTERNALERROR;
	}
}

int XMLDB_libLoad_DiskFile(LIBFLUX_IMGLDR * imgldr_ctx,LIBFLUX_FLOPPY * floppydisk,char * imgfile,void * parameters)
{
	FILE * f;
	unsigned int filesize;
	LIBFLUX_XMLLDR* rfb;
	LIBFLUX_FLOPPY * fp;

	imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"XMLDB_libLoad_DiskFile %s",imgfile);

	f = libflux_fopen(imgfile,"rb");
	if( f == NULL )
	{
		imgldr_ctx->ctx->libflux_printf(MSG_ERROR,"Cannot open %s !",imgfile);
		return LIBFLUX_ACCESSERROR;
	}

	filesize = libflux_fgetsize(f);

	libflux_fclose(f);

	if(libflux_checkfileext(imgfile,"xml",SYS_PATH_TYPE) && filesize!=0)
	{
		rfb = libflux_initXmlFloppy(imgldr_ctx->ctx);
		if(rfb)
		{
			if(libflux_setXmlFloppyLayoutFile(rfb,imgfile) == LIBFLUX_NOERROR)
			{
				if(parameters)
				{
					fp = libflux_generateXmlFileFloppy(rfb,(char*)parameters);
				}
				else
				{
					fp = libflux_generateXmlFloppy(rfb,0,0);
				}

				if(fp)
				{
					memcpy(floppydisk,fp,sizeof(LIBFLUX_FLOPPY));
					free(fp);

					libflux_deinitXmlFloppy(rfb);

					imgldr_ctx->ctx->libflux_printf(MSG_DEBUG,"XMLDB_libLoad_DiskFile - disk generated !");

					return LIBFLUX_NOERROR;
				}
			}

			libflux_deinitXmlFloppy(rfb);
		}
	}

	return LIBFLUX_BADFILE;
}

int XMLDB_libGetPluginInfo(LIBFLUX_IMGLDR * imgldr_ctx,uint32_t infotype,void * returnvalue)
{
	static char plug_id[512]="XML_DATABASE_LOADER";
	static char plug_desc[512]="XML Format database Loader";
	static char plug_ext[512]="xml";
	char * tmp_ptr;

	if(imgldr_ctx)
	{
		if(returnvalue)
		{
			switch(infotype)
			{
				case GETPLUGINID:
					*(char**)(returnvalue)=(char*)plug_id;
					break;

				case GETDESCRIPTION:
					*(char**)(returnvalue)=(char*)plug_desc;
					break;

				case GETFUNCPTR:
					///memcpy(returnvalue,pluginfunc,sizeof(plugins_ptr));
					break;

				case GETEXTENSION:
					if(!rfb)
						rfb = libflux_initXmlFloppy(imgldr_ctx->ctx);

					*(char**)(returnvalue)=(char*)plug_ext;
					break;

				case GETNBSUBLOADER:
					if(!rfb)
						rfb = libflux_initXmlFloppy(imgldr_ctx->ctx);

					*(int*)(returnvalue) = libflux_numberOfXmlLayout(rfb);

					break;

				case SELECTSUBLOADER:
					imgldr_ctx->selected_subid = *((int*)returnvalue);

					if(!rfb)
						rfb = libflux_initXmlFloppy(imgldr_ctx->ctx);


					if( imgldr_ctx->selected_subid && rfb )
					{
						tmp_ptr = (char*)libflux_getXmlLayoutDesc( rfb, imgldr_ctx->selected_subid - 1 );
						if(tmp_ptr)
							strncpy(plug_desc, tmp_ptr, sizeof(plug_desc)-1); plug_desc[sizeof(plug_desc)-1] = '\0';

						tmp_ptr = (char*)libflux_getXmlLayoutName( rfb, imgldr_ctx->selected_subid - 1 );
						if(tmp_ptr)
							strncpy(plug_id, tmp_ptr, sizeof(plug_id)-1); plug_id[sizeof(plug_id)-1] = '\0';

						libflux_selectXmlFloppyLayout( rfb, imgldr_ctx->selected_subid - 1 );
					}
					break;

				default:
					return LIBFLUX_BADPARAMETER;
					break;
			}

			return LIBFLUX_NOERROR;
		}
	}
	return LIBFLUX_BADPARAMETER;
}
