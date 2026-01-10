#include "uft/formatid/guess.h"
#include "uft/fs/fat_bpb.h"
#include <string.h>

typedef struct { size_t bytes; uft_geometry_hint_t g; const char *label; } geom_entry;

static const geom_entry k_common[] = {
    { 160*1024,  {40,1,8,512},   "PC 5.25\" SS 160K" },
    { 180*1024,  {40,1,9,512},   "PC 5.25\" SS 180K" },
    { 320*1024,  {40,2,8,512},   "PC 5.25\" DS 320K" },
    { 360*1024,  {40,2,9,512},   "PC 5.25\" DS 360K" },
    { 720*1024,  {80,2,9,512},   "PC 3.5\" DS 720K" },
    { 1200*1024, {80,2,15,512},  "PC 5.25\" HD 1.2M" },
    { 1440*1024, {80,2,18,512},  "PC 3.5\" HD 1.44M" },
    { 2880*1024, {80,2,36,512},  "PC 3.5\" ED 2.88M" }
};

uft_rc_t uft_guess_image(const uint8_t *img, size_t len, uft_guess_result_t *out, uft_diag_t *diag)
{
    if (!img || !out) { uft_diag_set(diag, "guess: invalid args"); return UFT_EINVAL; }
    memset(out, 0, sizeof(*out));
    out->kind = UFT_GUESS_UNKNOWN;

    /* 1) FAT probe */
    uft_fat_bpb_t bpb;
    uft_fat_layout_t lay;
    uft_diag_t d = {{0}};
    if (uft_fat_probe_image(img, len, &bpb, &lay, &d) == UFT_OK) {
        out->kind = UFT_GUESS_FAT;
        out->confidence = bpb.confidence;
        out->geom.cylinders = 0;
        out->geom.heads = bpb.heads;
        out->geom.sectors_per_track = bpb.sectors_per_track;
        out->geom.bytes_per_sector = bpb.bytes_per_sector;
        /* label */
        const char *t = (bpb.fat_type==UFT_FAT12)?"FAT12":(bpb.fat_type==UFT_FAT16)?"FAT16":"FAT32";
        size_t n = 0;
        const char *p = "DOS ";
        while (n+1<sizeof(out->label) && *p) out->label[n++]=*p++;
        p = t;
        while (n+1<sizeof(out->label) && *p) out->label[n++]=*p++;
        out->label[n]='\0';
        uft_diag_set(diag, "guess: FAT detected");
        return UFT_OK;
    }

    /* 2) geometry table */
    for (size_t i=0;i<sizeof(k_common)/sizeof(k_common[0]);i++){
        if (k_common[i].bytes == len) {
            out->kind = UFT_GUESS_RAW_GEOMETRY;
            out->confidence = 70;
            out->geom = k_common[i].g;
            /* copy label */
            size_t n=0; const char *p=k_common[i].label;
            while (n+1<sizeof(out->label) && *p) out->label[n++]=*p++;
            out->label[n]='\0';
            uft_diag_set(diag, "guess: geometry matched by size");
            return UFT_OK;
        }
    }

    uft_diag_set(diag, "guess: unknown");
    return UFT_EUNSUPPORTED;
}
