#include "dtc_components.h"
#include <string.h>
static int ext(const char*n,const char*e){size_t a=n?strlen(n):0,b=strlen(e);return a>=b&&!memcmp(n+a-b,e,b);}
dtc_format dtc_detect_buffer(const uint8_t*b,size_t n,const char*name){if(!b)return DTC_FMT_UNKNOWN;if(n>=4&&!memcmp(b,"CAPS",4))return DTC_FMT_IPF;if(n>=3&&!memcmp(b,"SCP",3))return DTC_FMT_SCP;if(n>=8&&!memcmp(b,"DTCCTRW\0",8))return DTC_FMT_CTRAW;if(n>=8&&!memcmp(b,"GCR-1541",8))return DTC_FMT_G64;if(ext(name,".raw"))return DTC_FMT_KRYO_RAW;if(n==901120||n==1802240)return DTC_FMT_ADF;if(n==174848||n==175531||n==196608)return DTC_FMT_D64;if(n==737280||n==1228800||n==1474560||n==2949120)return DTC_FMT_IMG;return DTC_FMT_UNKNOWN;}
const char*dtc_format_name(dtc_format f){static const char*n[]={"unknown","ADF","IMG","D64","G64","SCP","KryoFlux RAW","CTRAW","IPF"};return f<=DTC_FMT_IPF?n[f]:n[0];}

