#include "dtc_components.h"
size_t dtc_scan_amiga_sync(const uint8_t*b,size_t n,size_t*out,size_t cap){size_t k=0;for(size_t i=0;i+3<n;i++)if(b[i]==0x44&&b[i+1]==0x89&&b[i+2]==0x44&&b[i+3]==0x89){if(k<cap)out[k]=i*8;k++;}return k;}
size_t dtc_scan_mfm_ibm(const uint8_t*b,size_t n,dtc_sector*out,size_t cap){size_t k=0;for(size_t i=0;i+10<n;i++)if(b[i]==0xa1&&b[i+1]==0xa1&&b[i+2]==0xa1&&b[i+3]==0xfe){if(k<cap){dtc_sector*s=&out[k];s->bit_offset=i*8;s->bit_length=0;s->cylinder=b[i+4];s->head=b[i+5];s->sector=b[i+6];s->data_crc=dtc_crc16_ccitt(b+i+3,5,0xffff);s->header_ok=dtc_crc16_ccitt(b+i+3,7,0xffff)==0;s->data_ok=0;}k++;}return k;}

