#include "dtc_components.h"
#include <stdlib.h>
int dtc_track_match(const dtc_bits*a,const dtc_bits*b,size_t maxshift,dtc_match*r){size_t best=SIZE_MAX,bs=0,n;if(!a||!b||!r)return-1;n=a->bit_count<b->bit_count?a->bit_count:b->bit_count;for(size_t s=0;s<=maxshift&&s<n;s++){size_t d=dtc_bits_compare(a,0,b,s,n-s);if(d<best){best=d;bs=s;r->compared_bits=n-s;}}r->best_shift=bs;r->differing_bits=best;r->similarity=r->compared_bits?1.0-(double)best/r->compared_bits:0;return 0;}
size_t dtc_weak_regions(const uint8_t*const*c,size_t passes,size_t bits,double ratio,size_t*out,size_t cap){size_t k=0;if(!c||passes<2)return 0;for(size_t p=0;p<bits;p++){size_t ones=0;for(size_t j=0;j<passes;j++)ones+=(c[j][p>>3]>>(7-(p&7)))&1;double q=(double)ones/passes;if(q>=ratio&&q<=1.0-ratio){if(k<cap)out[k]=p;k++;}}return k;}

