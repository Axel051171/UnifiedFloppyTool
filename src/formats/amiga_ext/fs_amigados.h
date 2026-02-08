#ifndef UFT_FORMATS_AMIGA_EXT_FS_AMIGADOS_H
#define UFT_FORMATS_AMIGA_EXT_FS_AMIGADOS_H

void init_amigados(LIBFLUX_FSMNG * fsmng);
int32_t amigados_mountImage(LIBFLUX_FSMNG * fsmng, LIBFLUX_FLOPPY *floppy);
int32_t amigados_umountImage(LIBFLUX_FSMNG * fsmng);
int32_t amigados_getFreeSpace(LIBFLUX_FSMNG * fsmng);
int32_t amigados_getTotalSpace(LIBFLUX_FSMNG * fsmng);
int32_t amigados_openFile(LIBFLUX_FSMNG * fsmng, char * filename);
int32_t amigados_createFile(LIBFLUX_FSMNG * fsmng, char * filename);
int32_t amigados_writeFile(LIBFLUX_FSMNG * fsmng,int32_t filehandle,unsigned char * buffer,int32_t size);
int32_t amigados_readFile( LIBFLUX_FSMNG * fsmng,int32_t filehandle,unsigned char * buffer,int32_t size);
int32_t amigados_deleteFile(LIBFLUX_FSMNG * fsmng, char * filename);
int32_t amigados_closeFile( LIBFLUX_FSMNG * fsmng,int32_t filehandle);
int32_t amigados_createDir( LIBFLUX_FSMNG * fsmng,char * foldername);
int32_t amigados_removeDir( LIBFLUX_FSMNG * fsmng,char * foldername);

int32_t amigados_ftell( LIBFLUX_FSMNG * fsmng,int32_t filehandle);
int32_t amigados_fseek( LIBFLUX_FSMNG * fsmng,int32_t filehandle,int32_t offset,int32_t origin);

int32_t amigados_openDir(LIBFLUX_FSMNG * fsmng, char * path);
int32_t amigados_readDir(LIBFLUX_FSMNG * fsmng,int32_t dirhandle,LIBFLUX_FSENTRY * dirent);
int32_t amigados_closeDir(LIBFLUX_FSMNG * fsmng, int32_t dirhandle);

#endif /* UFT_FORMATS_AMIGA_EXT_FS_AMIGADOS_H */
