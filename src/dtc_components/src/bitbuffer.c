#include "dtc_components.h"

void dtc_bits_init(dtc_bits*b,void*p,size_t n,int w){b->data=p;b->bit_count=n;b->writable=w;}
int dtc_bit_get(const dtc_bits*b,size_t p,unsigned*v){if(!b||!v||p>=b->bit_count)return-1;*v=(b->data[p>>3]>>(7-(p&7)))&1;return 0;}
int dtc_bit_set(dtc_bits*b,size_t p,unsigned v){uint8_t m;if(!b||!b->writable||p>=b->bit_count)return-1;m=(uint8_t)(0x80u>>(p&7));if(v)b->data[p>>3]|=m;else b->data[p>>3]&=(uint8_t)~m;return 0;}
int dtc_bits_copy(dtc_bits*d,size_t dp,const dtc_bits*s,size_t sp,size_t n){unsigned v=0;size_t i;if(!d||!s||dp+n>d->bit_count||sp+n>s->bit_count)return-1;for(i=0;i<n;i++){if(dtc_bit_get(s,sp+i,&v)||dtc_bit_set(d,dp+i,v))return-1;}return 0;}
size_t dtc_bits_compare(const dtc_bits*a,size_t ap,const dtc_bits*b,size_t bp,size_t n){size_t i,x=0;unsigned av=0,bv=0;if(!a||!b||ap+n>a->bit_count||bp+n>b->bit_count)return SIZE_MAX;for(i=0;i<n;i++){if(dtc_bit_get(a,ap+i,&av)||dtc_bit_get(b,bp+i,&bv))return SIZE_MAX;x+=av!=bv;}return x;}
size_t dtc_find_run_violation(const dtc_bits*b,size_t start,size_t n,unsigned max0,unsigned max1,int count){size_t i,hits=0;unsigned v=0,last=2,run=0;if(!b||start+n>b->bit_count)return SIZE_MAX;for(i=0;i<n;i++){if(dtc_bit_get(b,start+i,&v))return SIZE_MAX;run=v==last?run+1:1;last=v;if((!v&&max0&&run>max0)||(v&&max1&&run>max1)){if(!count)return start+i;hits++;}}return count?hits:SIZE_MAX;}
