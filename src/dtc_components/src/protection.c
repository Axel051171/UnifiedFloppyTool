#include "dtc_components.h"
unsigned dtc_detect_protection(const dtc_bits*b,size_t nominal,size_t max0,size_t max1){unsigned f=0;if(!b)return 0;if(nominal&&b->bit_count>nominal+nominal/20)f|=DTC_PROT_LONG_TRACK;if(dtc_find_run_violation(b,0,b->bit_count,max0,max1,1))f|=DTC_PROT_ILLEGAL_MFM;return f;}

