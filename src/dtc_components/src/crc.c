#include "dtc_components.h"
uint32_t dtc_crc_generic(const void*p,size_t n,unsigned width,uint32_t poly,uint32_t init,int reflected){const uint8_t*d=p;uint32_t c=init,mask=width==32?0xffffffffu:((1u<<width)-1);size_t i;unsigned j;if(!d||width<1||width>32)return 0;for(i=0;i<n;i++){if(reflected){c^=d[i];for(j=0;j<8;j++)c=(c&1)?(c>>1)^poly:c>>1;}else{c^=(uint32_t)d[i]<<(width-8);for(j=0;j<8;j++)c=(c&(1u<<(width-1)))?((c<<1)^poly):(c<<1);}c&=mask;}return c&mask;}
uint32_t dtc_crc32(const void*p,size_t n,uint32_t init){return dtc_crc_generic(p,n,32,0xedb88320u,init,1);}
uint16_t dtc_crc16_ccitt(const void*p,size_t n,uint16_t init){return(uint16_t)dtc_crc_generic(p,n,16,0x1021u,init,0);}
uint16_t dtc_crc16_ibm(const void*p,size_t n,uint16_t init){return(uint16_t)dtc_crc_generic(p,n,16,0xa001u,init,1);}

