#include "dtc_components.h"
#include <math.h>
int dtc_flux_measure(const uint32_t*f,size_t n,double tick,dtc_flux_stats*s){double sum=0,var=0;if(!f||!n||!s||tick<=0)return-1;s->min_ns=1e300;s->max_ns=0;s->count=n;for(size_t i=0;i<n;i++){double x=f[i]*tick;sum+=x;if(x<s->min_ns)s->min_ns=x;if(x>s->max_ns)s->max_ns=x;}s->mean_ns=sum/n;for(size_t i=0;i<n;i++){double d=f[i]*tick-s->mean_ns;var+=d*d;}s->stddev_ns=sqrt(var/n);return 0;}
int dtc_flux_to_bits(const uint32_t*f,size_t n,double cell,uint8_t*out,size_t cap,size_t*nb){size_t p=0;if(!f||!out||cell<=0)return-1;for(size_t i=0;i<n;i++){long cells=lround(f[i]/cell);if(cells<1)cells=1;if(p+(size_t)cells>cap)return-2;for(long j=1;j<cells;j++,p++)out[p>>3]&=(uint8_t)~(0x80u>>(p&7));out[p>>3]|=(uint8_t)(0x80u>>(p&7));p++;}if(nb)*nb=p;return 0;}

