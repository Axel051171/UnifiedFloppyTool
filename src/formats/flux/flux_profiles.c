// flux_profiles.c - helper to ship FluxEngine textpb profiles with UFT (C11)
#include "uft/formats/flux_profiles.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const UftFluxProfile kProfiles[] = {
    {"_global_options","_global_options.textpb"},
    {"acornadfs","acornadfs.textpb"},
    {"acorndfs","acorndfs.textpb"},
    {"aeslanier","aeslanier.textpb"},
    {"agat","agat.textpb"},
    {"amiga","amiga.textpb"},
    {"ampro","ampro.textpb"},
    {"apple2","apple2.textpb"},
    {"atarist","atarist.textpb"},
    {"bk","bk.textpb"},
    {"brother","brother.textpb"},
    {"commodore","commodore.textpb"},
    {"eco1","eco1.textpb"},
    {"epsonpf10","epsonpf10.textpb"},
    {"f85","f85.textpb"},
    {"fb100","fb100.textpb"},
    {"hplif","hplif.textpb"},
    {"ibm","ibm.textpb"},
    {"icl30","icl30.textpb"},
    {"juku","juku.textpb"},
    {"mac","mac.textpb"},
    {"micropolis","micropolis.textpb"},
    {"ms2000","ms2000.textpb"},
    {"mx","mx.textpb"},
    {"n88basic","n88basic.textpb"},
    {"northstar","northstar.textpb"},
    {"psos","psos.textpb"},
    {"rolandd20","rolandd20.textpb"},
    {"rx50","rx50.textpb"},
    {"smaky6","smaky6.textpb"},
    {"tartu","tartu.textpb"},
    {"ti99","ti99.textpb"},
    {"tids990","tids990.textpb"},
    {"tiki","tiki.textpb"},
    {"victor9k","victor9k.textpb"},
    {"zilogmcz","zilogmcz.textpb"},
};

const UftFluxProfile* uft_flux_profiles(size_t* count){
    if(count) *count = sizeof(kProfiles)/sizeof(kProfiles[0]);
    return kProfiles;
}

static int slurp(FILE* fp, char** out_buf, size_t* out_len){
    if(fseek(fp,0,SEEK_END)!=0) return -2;
    long sz = ftell(fp);
    if(sz < 0) return -2;
    if(fseek(fp,0,SEEK_SET)!=0) return -2;

    char* buf = (char*)malloc((size_t)sz + 1);
    if(!buf) return -3;
    size_t rd = fread(buf,1,(size_t)sz,fp);
    if(rd != (size_t)sz){ free(buf); return -2; }
    buf[sz] = '\0';
    *out_buf = buf;
    if(out_len) *out_len = (size_t)sz;
    return 0;
}

int uft_flux_profile_load_text(const char* profile_path, const char* filename, char** out_buf, size_t* out_len){
    if(!profile_path || !filename || !out_buf) return -1;
    *out_buf = NULL;
    if(out_len) *out_len = 0;

    char full[4096];
    size_t lp = strlen(profile_path);
    if(lp && (profile_path[lp-1]=='/' || profile_path[lp-1]=='\\')){
        snprintf(full, sizeof(full), "%s%s", profile_path, filename);
    }else{
        snprintf(full, sizeof(full), "%s/%s", profile_path, filename);
    }

    FILE* fp = fopen(full, "rb");
    if(!fp) return -4;
    int rc = slurp(fp, out_buf, out_len);
    fclose(fp);
    return rc;
}
