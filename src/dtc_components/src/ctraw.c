#include "dtc_components.h"
#include <stdlib.h>
#include <string.h>
typedef struct{char magic[8];uint32_t ver,flags,clock;uint16_t cyl,heads;uint32_t tracks;} H;
int dtc_ctraw_write(FILE*f,const dtc_ctraw_info*i,const uint32_t*const*t,const uint32_t*n,const uint32_t*dens){H h={{'D','T','C','C','T','R','W',0},1,i?i->flags:0,i?i->clock_ns:0,i?i->cylinders:0,i?i->heads:0,i?i->track_count:0};if(!f||!i||!t||!n)return-1;if(fwrite(&h,sizeof h,1,f)!=1)return-2;for(uint32_t k=0;k<h.tracks;k++){uint32_t d=dens?dens[k]:0;if(fwrite(&n[k],4,1,f)!=1||fwrite(&d,4,1,f)!=1||fwrite(t[k],4,n[k],f)!=n[k])return-2;}return 0;}
int dtc_ctraw_read(FILE*f,dtc_ctraw_info*i,uint32_t***tp,uint32_t**np,uint32_t**dp){H h;uint32_t**t=0,*n=0,*d=0;if(!f||!i||!tp||!np||!dp||fread(&h,sizeof h,1,f)!=1||memcmp(h.magic,"DTCCTRW",7))return-1;t=calloc(h.tracks,sizeof*t);n=calloc(h.tracks,4);d=calloc(h.tracks,4);if(!t||!n||!d)goto fail;for(uint32_t k=0;k<h.tracks;k++){if(fread(&n[k],4,1,f)!=1||fread(&d[k],4,1,f)!=1||(t[k]=malloc((size_t)n[k]*4))==0||fread(t[k],4,n[k],f)!=n[k])goto fail;}i->version=h.ver;i->flags=h.flags;i->clock_ns=h.clock;i->cylinders=h.cyl;i->heads=h.heads;i->track_count=h.tracks;*tp=t;*np=n;*dp=d;return 0;fail:dtc_ctraw_free(t,n,d,h.tracks);return-2;}
void dtc_ctraw_free(uint32_t**t,uint32_t*n,uint32_t*d,uint32_t c){if(t){for(uint32_t k=0;k<c;k++)free(t[k]);free(t);}free(n);free(d);}

