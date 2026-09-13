#include "dtc_components.h"
#include <string.h>
static uint32_t be32(const uint8_t*p){return((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
int dtc_ipf_probe(FILE*f){uint8_t h[4];long p;if(!f)return 0;p=ftell(f);rewind(f);int ok=fread(h,1,4,f)==4&&!memcmp(h,"CAPS",4);fseek(f,p,SEEK_SET);return ok;}
size_t dtc_ipf_list_chunks(FILE*f,dtc_ipf_chunk*out,size_t cap){uint8_t h[16];size_t n=0;if(!f)return 0;rewind(f);while(fread(h,1,12,f)==12){uint32_t sz=be32(h+4);if(sz<12)break;if(n<cap){out[n].type=be32(h);out[n].size=sz;out[n].crc=be32(h+8);out[n].id=0;out[n].offset=ftell(f)-12;}n++;if(fseek(f,(long)sz-12,SEEK_CUR))break;}return n;}

