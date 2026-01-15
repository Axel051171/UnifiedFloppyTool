/**
 * @file uft_autodetect.c
 * @brief Automatic format detection engine
 * @version 3.8.0
 */
// uft_autodetect.c - Unified format auto-detection & vtable routing (C11)

#include "uft_autodetect.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* Error codes: keep aligned with other modules */
enum { UFT_OK=0, UFT_EINVAL=-1, UFT_EIO=-2, UFT_ENOENT=-3, UFT_ENOTSUP=-4 };

static void log_msg(FloppyDevice *d, const char *m){
    if(d && d->log_callback) d->log_callback(m);
}

static const char* ext_lower(const char *path, char out[16]){
    out[0]=0;
    if(!path) return out;
    const char *dot = strrchr(path,'.');
    if(!dot || !dot[1]) return out;
    size_t n = strlen(dot+1);
    if(n>15) n=15;
    for(size_t i=0;i<n;i++) out[i]=(char)tolower((unsigned char)dot[1+i]);
    out[n]=0;
    return out;
}

static int read_head(const char *path, uint8_t *buf, size_t need){
    FILE *fp = fopen(path,"rb");
    if(!fp) return UFT_ENOENT;
    size_t got = fread(buf,1,need,fp);
    fclose(fp);
    return (got==need) ? UFT_OK : UFT_EIO;
}

static uint64_t file_size(const char *path){
    FILE *fp=fopen(path,"rb");
    if(!fp) return 0;
    if(fseek(fp,0,SEEK_END)!=0){ fclose(fp); return 0; }
    long p=ftell(fp);
    fclose(fp);
    return (p<0)?0:(uint64_t)p;
}

/* ---- Expected wrapper symbols (you provide these in your build) ----
 * Rationale: all modules share uft_floppy_open() etc; to link multiple formats
 * into one library, each must be compiled/aliased to unique names.
 *
 * Example wrapper file (pcimg_wrap.c):
 *   #include "pc_img.h"
 *   int pcimg_open(FloppyDevice*d,const char*p){return uft_floppy_open(d,p);}
 *   ...
 */
extern int pcimg_open(FloppyDevice*, const char*);
extern int pcimg_close(FloppyDevice*);
extern int pcimg_read_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,uint8_t*);
extern int pcimg_write_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,const uint8_t*);
extern int pcimg_analyze(FloppyDevice*);

extern int d88_open(FloppyDevice*, const char*);
extern int d88_close(FloppyDevice*);
extern int d88_read_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,uint8_t*);
extern int d88_write_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,const uint8_t*);
extern int d88_analyze(FloppyDevice*);

extern int fdi_open(FloppyDevice*, const char*);
extern int fdi_close(FloppyDevice*);
extern int fdi_read_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,uint8_t*);
extern int fdi_write_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,const uint8_t*);
extern int fdi_analyze(FloppyDevice*);

extern int imd_open(FloppyDevice*, const char*);
extern int imd_close(FloppyDevice*);
extern int imd_read_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,uint8_t*);
extern int imd_write_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,const uint8_t*);
extern int imd_analyze(FloppyDevice*);

extern int atx_open(FloppyDevice*, const char*);
extern int atx_close(FloppyDevice*);
extern int atx_read_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,uint8_t*);
extern int atx_write_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,const uint8_t*);
extern int atx_analyze(FloppyDevice*);

extern int f86_open(FloppyDevice*, const char*);
extern int f86_close(FloppyDevice*);
extern int f86_read_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,uint8_t*);
extern int f86_write_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,const uint8_t*);
extern int f86_analyze(FloppyDevice*);

extern int scp_open(FloppyDevice*, const char*);
extern int scp_close(FloppyDevice*);
extern int scp_read_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,uint8_t*);
extern int scp_write_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,const uint8_t*);
extern int scp_analyze(FloppyDevice*);

extern int gwraw_open(FloppyDevice*, const char*);
extern int gwraw_close(FloppyDevice*);
extern int gwraw_read_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,uint8_t*);
extern int gwraw_write_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,const uint8_t*);
extern int gwraw_analyze(FloppyDevice*);

extern int hdm_open(FloppyDevice*, const char*);
extern int hdm_close(FloppyDevice*);
extern int hdm_read_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,uint8_t*);
extern int hdm_write_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,const uint8_t*);
extern int hdm_analyze(FloppyDevice*);

extern int atr_open(FloppyDevice*, const char*);
extern int atr_close(FloppyDevice*);
extern int atr_read_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,uint8_t*);
extern int atr_write_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,const uint8_t*);
extern int atr_analyze(FloppyDevice*);

extern int d64_open(FloppyDevice*, const char*);
extern int d64_close(FloppyDevice*);
extern int d64_read_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,uint8_t*);
extern int d64_write_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,const uint8_t*);
extern int d64_analyze(FloppyDevice*);

extern int g64_open(FloppyDevice*, const char*);
extern int g64_close(FloppyDevice*);
extern int g64_read_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,uint8_t*);
extern int g64_write_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,const uint8_t*);
extern int g64_analyze(FloppyDevice*);

extern int st_open(FloppyDevice*, const char*);
extern int st_close(FloppyDevice*);
extern int st_read_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,uint8_t*);
extern int st_write_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,const uint8_t*);
extern int st_analyze(FloppyDevice*);

extern int msa_open(FloppyDevice*, const char*);
extern int msa_close(FloppyDevice*);
extern int msa_read_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,uint8_t*);
extern int msa_write_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,const uint8_t*);
extern int msa_analyze(FloppyDevice*);

extern int adf_open(FloppyDevice*, const char*);
extern int adf_close(FloppyDevice*);
extern int adf_read_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,uint8_t*);
extern int adf_write_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,const uint8_t*);
extern int adf_analyze(FloppyDevice*);

extern int ipf_open(FloppyDevice*, const char*);
extern int ipf_close(FloppyDevice*);
extern int ipf_read_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,uint8_t*);
extern int ipf_write_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,const uint8_t*);
extern int ipf_analyze(FloppyDevice*);

/* ---- vtable registry ---- */
static const UftFormatVtable g_tbl[] = {
    {UFT_FMT_PC_IMG,"PC/DOS IMG","img", pcimg_open,pcimg_close,pcimg_read_sector,pcimg_write_sector,pcimg_analyze},
    {UFT_FMT_D88,"D88","d88", d88_open,d88_close,d88_read_sector,d88_write_sector,d88_analyze},
    {UFT_FMT_FDI,"FDI","fdi", fdi_open,fdi_close,fdi_read_sector,fdi_write_sector,fdi_analyze},
    {UFT_FMT_IMD,"IMD","imd", imd_open,imd_close,imd_read_sector,imd_write_sector,imd_analyze},
    {UFT_FMT_ATX,"ATX","atx", atx_open,atx_close,atx_read_sector,atx_write_sector,atx_analyze},
    {UFT_FMT_86F,"86F","86f", f86_open,f86_close,f86_read_sector,f86_write_sector,f86_analyze},
    {UFT_FMT_SCP,"SCP","scp", scp_open,scp_close,scp_read_sector,scp_write_sector,scp_analyze},
    {UFT_FMT_GWRAW,"GWF/GWRAW","gwf", gwraw_open,gwraw_close,gwraw_read_sector,gwraw_write_sector,gwraw_analyze},
    {UFT_FMT_HDM,"HDM","hdm", hdm_open,hdm_close,hdm_read_sector,hdm_write_sector,hdm_analyze},
    {UFT_FMT_ATR,"ATR","atr", atr_open,atr_close,atr_read_sector,atr_write_sector,atr_analyze},
    {UFT_FMT_D64,"D64","d64", d64_open,d64_close,d64_read_sector,d64_write_sector,d64_analyze},
    {UFT_FMT_G64,"G64","g64", g64_open,g64_close,g64_read_sector,g64_write_sector,g64_analyze},
    {UFT_FMT_ST,"Atari ST (.ST)","st", st_open,st_close,st_read_sector,st_write_sector,st_analyze},
    {UFT_FMT_MSA,"Atari ST (.MSA)","msa", msa_open,msa_close,msa_read_sector,msa_write_sector,msa_analyze},
    {UFT_FMT_ADF,"Amiga ADF","adf", adf_open,adf_close,adf_read_sector,adf_write_sector,adf_analyze},
    {UFT_FMT_IPF,"IPF/CAPS","ipf", ipf_open,ipf_close,ipf_read_sector,ipf_write_sector,ipf_analyze},
};

const UftFormatVtable* uft_get_vtable(UftFormatId id){
    for(size_t i=0;i<sizeof(g_tbl)/sizeof(g_tbl[0]);i++){
        if(g_tbl[i].id==id) return &g_tbl[i];
    }
    return NULL;
}

static UftFormatId detect_by_magic(const char *path){
    uint8_t h[16];
    if(read_head(path,h,sizeof(h))!=UFT_OK) return UFT_FMT_UNKNOWN;

    if(!memcmp(h,"SCP",3)) return UFT_FMT_SCP;
    if(!memcmp(h,"GWFLUX",6)) return UFT_FMT_GWRAW;
    if(!memcmp(h,"AT8X",4)) return UFT_FMT_ATX;
    if(!memcmp(h,"GCR-1541",8)) return UFT_FMT_G64;
    if(!memcmp(h,"IMD",3)) return UFT_FMT_IMD;
    if(!memcmp(h,"IPF",3)) return UFT_FMT_IPF;
    if(h[0]==0x96 && h[1]==0x02) return UFT_FMT_ATR; /* ATR */
    /* D88 has magic at 0x1B: 0x00? (varies) - rely on extension */
    /* FDI: reserved0=0 and plausible header size at 0x08 */
    if(h[0]==0 && h[1]==0 && h[2]==0 && h[3]==0){
        uint32_t hdr = (uint32_t)h[8] | ((uint32_t)h[9]<<8) | ((uint32_t)h[10]<<16) | ((uint32_t)h[11]<<24);
        if(hdr>=32 && hdr<=65536) return UFT_FMT_FDI;
    }
    return UFT_FMT_UNKNOWN;
}

UftFormatId uft_detect_format(const char *path){
    if(!path||!path[0]) return UFT_FMT_UNKNOWN;

    /* First: magic-based */
    UftFormatId m = detect_by_magic(path);
    if(m != UFT_FMT_UNKNOWN) return m;

    /* Second: extension heuristics */
    char e[16]; ext_lower(path,e);
    if(!strcmp(e,"d88")) return UFT_FMT_D88;
    if(!strcmp(e,"fdi") || !strcmp(e,"hdi")) return UFT_FMT_FDI;
    if(!strcmp(e,"imd")) return UFT_FMT_IMD;
    if(!strcmp(e,"atx")) return UFT_FMT_ATX;
    if(!strcmp(e,"86f")) return UFT_FMT_86F;
    if(!strcmp(e,"scp")) return UFT_FMT_SCP;
    if(!strcmp(e,"gwf") || !strcmp(e,"raw")) return UFT_FMT_GWRAW;
    if(!strcmp(e,"hdm")) return UFT_FMT_HDM;
    if(!strcmp(e,"atr")) return UFT_FMT_ATR;
    if(!strcmp(e,"d64")) return UFT_FMT_D64;
    if(!strcmp(e,"g64")) return UFT_FMT_G64;
    if(!strcmp(e,"st")) return UFT_FMT_ST;
    if(!strcmp(e,"msa")) return UFT_FMT_MSA;
    if(!strcmp(e,"adf")) return UFT_FMT_ADF;
    if(!strcmp(e,"ipf")) return UFT_FMT_IPF;

    /* Third: size heuristics for raw images */
    uint64_t sz = file_size(path);
    if(sz == 77ull*2ull*8ull*1024ull) return UFT_FMT_HDM;

    /* Common PC raw sizes (IMG): */
    if(sz==368640ull||sz==737280ull||sz==1228800ull||sz==1474560ull||sz==2949120ull) return UFT_FMT_PC_IMG;

    return UFT_FMT_UNKNOWN;
}

int uft_open_auto(FloppyDevice *dev, const char *path, UftFormatId *out_id){
    if(!dev||!path) return UFT_EINVAL;
    UftFormatId id = uft_detect_format(path);
    if(out_id) *out_id = id;
    if(id==UFT_FMT_UNKNOWN) return UFT_EINVAL;

    const UftFormatVtable *vt = uft_get_vtable(id);
    if(!vt || !vt->open) return UFT_ENOTSUP;

    char msg[128];
    snprintf(msg,sizeof(msg),"AutoDetect: %s (%s)", vt->name, path);
    log_msg(dev,msg);

    return vt->open(dev,path);
}
