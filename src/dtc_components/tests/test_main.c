#include "dtc_components.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 const char*s="123456789";
 assert((dtc_crc32(s,9,0xffffffff)^0xffffffff)==0xcbf43926);
 assert(dtc_crc16_ccitt(s,9,0xffff)==0x29b1);
 uint8_t in[]={0xa5,0x00,0xff},out[3];uint16_t e[3];size_t bad=0,vio=0;unsigned p=0;
 assert(!dtc_mfm_encode(in,3,e,&p));assert(!dtc_mfm_decode(e,3,out,&bad,&vio));
 assert(!memcmp(in,out,3)&&bad==0);
 uint8_t g[8]={0},d[3]={0};size_t bits=0,n=0;
 assert(!dtc_gcr_encode(in,3,dtc_gcr_cbm_4to5,5,g,&bits));
 assert(!dtc_gcr_decode(g,bits,dtc_gcr_cbm_4to5,5,d,&n));assert(n==3&&!memcmp(in,d,3));
 uint8_t bb[2]={0};dtc_bits bv;unsigned bit=0;dtc_bits_init(&bv,bb,16,1);
 assert(!dtc_bit_set(&bv,3,1));assert(!dtc_bit_get(&bv,3,&bit)&&bit==1);
 uint32_t flux[]={2,2,4,2};dtc_flux_stats fs;assert(!dtc_flux_measure(flux,4,100.0,&fs));assert(fs.mean_ns==250.0);
 uint8_t adf[901120]={0};assert(dtc_detect_buffer(adf,sizeof adf,"x.adf")==DTC_FMT_ADF);
 FILE*f=tmpfile();uint32_t tr0[]={10,20,30};const uint32_t*trs[]={tr0};uint32_t cnt[]={3},den[]={1};
 dtc_ctraw_info ci={1,0,25,1,1,1},co={0};uint32_t**rt=0,*rn=0,*rd=0;
 assert(f&&dtc_ctraw_write(f,&ci,trs,cnt,den)==0);rewind(f);
 assert(dtc_ctraw_read(f,&co,&rt,&rn,&rd)==0&&co.track_count==1&&rn[0]==3&&rt[0][2]==30);
 dtc_ctraw_free(rt,rn,rd,co.track_count);fclose(f);
 puts("all tests passed");return 0;
}
