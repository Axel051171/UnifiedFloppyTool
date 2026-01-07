// ufm_uft_as_woz1.c
// Applesauce WOZ 1.0 reader (chunk-based) -> minimal track bitstream extraction
//
// Sources / spec references:
// - Applesauce WOZ Disk Image Reference 1.0 (header, CRC policy, INFO/TMAP/TRKS layout)
//   https://applesaucefdc.com/woz/reference1/  (accessed 2025-12-25)
//
// Notes:
// - WOZ is bitstream normalized to 4µs intervals; this module preserves raw packed bits.
// - For future integration, map the TRKS bitstream to your UFM as "quantized bitcells"
//   with metadata: bytes_used, bit_count, splice hints.
//
// Build: cc -std=c11 -O2 -Wall -Wextra -pedantic ufm_uft_as_woz1.c -o woz_dump

#include "uft/uft_error.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static bool read_u8(FILE* f, uint8_t* out){
    int c=fgetc(f);
    if(c==EOF) return false;
    *out=(uint8_t)c;
    return true;
}
static bool read_le16(FILE* f, uint16_t* out){
    uint8_t a,b;
    if(!read_u8(f,&a) || !read_u8(f,&b)) return false;
    *out=(uint16_t)(a | ((uint16_t)b<<8));
    return true;
}
static bool read_le32(FILE* f, uint32_t* out){
    uint8_t a,b,c,d;
    if(!read_u8(f,&a) || !read_u8(f,&b) || !read_u8(f,&c) || !read_u8(f,&d)) return false;
    *out=(uint32_t)( (uint32_t)a | ((uint32_t)b<<8) | ((uint32_t)c<<16) | ((uint32_t)d<<24) );
    return true;
}
static bool skip_bytes(FILE* f, size_t n){
    return fseek(f,(long)n,SEEK_CUR)==0;
}
static uint32_t fourcc(const char s[4]){
    return (uint32_t)( (uint8_t)s[0] |
                      ((uint32_t)(uint8_t)s[1] << 8) |
                      ((uint32_t)(uint8_t)s[2] << 16) |
                      ((uint32_t)(uint8_t)s[3] << 24) );
}

typedef struct {
    uint8_t info_version;
    uint8_t disk_type;        // 1=5.25, 2=3.5 citeturn1view0
    uint8_t write_protected;
    uint8_t synchronized;
    uint8_t cleaned;
    char creator[33];         // 32 + '\0' (trim spaces)
} woz1_info_t;

typedef struct {
    // TRK structure size 6656 bytes per spec (bitstream padded + metadata fields). citeturn3view2
    uint8_t  bitstream[6646];
    uint16_t bytes_used;
    uint16_t bit_count;
    uint16_t splice_point;
    uint8_t  splice_nibble;
    uint8_t  splice_bit_count;
    uint16_t reserved;
} woz1_trk_t;

typedef struct {
    woz1_info_t info;
    uint8_t tmap[160];         // map to TRKS track index or 0xFF blank citeturn3view2

    // File offsets for TRKS chunk base (start of first TRK record)
    uint32_t trks_data_offset;
    uint32_t trks_chunk_size;
} woz1_image_t;

static void creator_trim(char out[33], const uint8_t in[32]){
    size_t n=32;
    while(n>0 && in[n-1]==0x20) n--;
    memcpy(out,in,n);
    out[n]='\0';
}

static bool read_info(FILE* f, uint32_t size, woz1_info_t* info){
    // INFO chunk size is always 60 bytes in WOZ1. citeturn3view1
    if(size < 60) return false;

    uint8_t creator_raw[32];
    if(!read_u8(f,&info->info_version)) return false;
    if(!read_u8(f,&info->disk_type)) return false;
    if(!read_u8(f,&info->write_protected)) return false;
    if(!read_u8(f,&info->synchronized)) return false;
    if(!read_u8(f,&info->cleaned)) return false;
    if(fread(creator_raw,1,32,f)!=32) return false;
    creator_trim(info->creator, creator_raw);

    // INFO payload is 60; we consumed 1+1+1+1+1+32=37 bytes; pad/skip remainder.
    if(!skip_bytes(f, 60-37)) return false;

    if(size > 60){
        if(!skip_bytes(f, size-60)) return false;
    }
    return true;
}

static int woz1_load(const char* path, woz1_image_t* out){
    memset(out,0,sizeof(*out));
    FILE* f=fopen(path,"rb");
    if(!f) return -1;

    // 12-byte header: "WOZ1" + 0xFF + 0x0A0D0A + CRC32 (ignored unless you want). citeturn1view0
    uint8_t hdr[12];
    if(fread(hdr,1,12,f)!=12){ fclose(f); return -2; }
    if(!(hdr[0]=='W'&&hdr[1]=='O'&&hdr[2]=='Z'&&hdr[3]=='1')){ fclose(f); return -3; }
    if(!(hdr[4]==0xFF && hdr[5]==0x0A && hdr[6]==0x0D && hdr[7]==0x0A)){ fclose(f); return -4; }

    // Chunks begin at byte 12. citeturn1view0
    bool have_info=false, have_tmap=false, have_trks=false;

    while(true){
        uint32_t id=0,size=0;
        long chunk_hdr = ftell(f);
        if(chunk_hdr < 0) break;
        if(!read_le32(f,&id)) break;
        if(!read_le32(f,&size)) break;

        if(id==fourcc("INFO")){
            if(!read_info(f,size,&out->info)){ fclose(f); return -10; }
            have_info=true;
        } else if(id==fourcc("TMAP")){
            if(size < 160){ fclose(f); return -11; }
            if(fread(out->tmap,1,160,f)!=160){ fclose(f); return -12; }
            if(size > 160 && !skip_bytes(f,size-160)){ fclose(f); return -13; }
            have_tmap=true;
        } else if(id==fourcc("TRKS")){
            // Record the start of TRKS data: immediately after this chunk header (8 bytes),
            // but WOZ1 spec states TRKS data begins at byte 256 for fixed access; still,
            // we respect the file position here for robustness. citeturn3view2
            out->trks_data_offset = (uint32_t)ftell(f);
            out->trks_chunk_size = size;
            if(!skip_bytes(f,size)){ fclose(f); return -14; }
            have_trks=true;
        } else {
            if(!skip_bytes(f,size)){ fclose(f); return -15; }
        }
    }

    fclose(f);
    if(!have_info || !have_tmap || !have_trks) return -20;
    return 0;
}

// Read a TRK record by index from TRKS chunk (index is the value stored in TMAP).
static bool woz1_read_trk(const char* path, const woz1_image_t* img, uint8_t trk_index, woz1_trk_t* out_trk){
    if(trk_index == 0xFF) return false; // blank

    // Track record offset: TRKS data is tightly packed records of 6656 bytes each. citeturn3view2
    uint32_t off = img->trks_data_offset + (uint32_t)trk_index * 6656u;
    FILE* f = fopen(path,"rb");
    if(!f) return false;
    if(fseek(f,(long)off,SEEK_SET)!=0){ fclose(f); return false; }

    // Read packed struct carefully to avoid padding assumptions.
    if(fread(out_trk->bitstream,1,6646,f)!=6646){ fclose(f); return false; }
    if(!read_le16(f,&out_trk->bytes_used)) { fclose(f); return false; }
    if(!read_le16(f,&out_trk->bit_count))  { fclose(f); return false; }
    if(!read_le16(f,&out_trk->splice_point)) { fclose(f); return false; }
    if(!read_u8(f,&out_trk->splice_nibble)) { fclose(f); return false; }
    if(!read_u8(f,&out_trk->splice_bit_count)) { fclose(f); return false; }
    if(!read_le16(f,&out_trk->reserved)) { fclose(f); return false; }

    fclose(f);
    return true;
}

// ------- demo CLI --------

static void dump_one(const char* path){
    woz1_image_t img;
    int rc = woz1_load(path,&img);
    if(rc){
        fprintf(stderr,"WOZ load failed: %d\n", rc);
        return;
    }
    printf("WOZ1: disk_type=%u wp=%u sync=%u cleaned=%u creator='%s'\n",
           img.info.disk_type, img.info.write_protected, img.info.synchronized,
           img.info.cleaned, img.info.creator);

    // show first few mapped tracks
    for(int i=0;i<16;i++){
        uint8_t idx = img.tmap[i];
        if(idx==0xFF){
            printf("  map[%d]=FF blank\n", i);
            continue;
        }
        woz1_trk_t trk;
        if(woz1_read_trk(path,&img,idx,&trk)){
            printf("  map[%d]=%u bytes_used=%u bit_count=%u splice=%u\n",
                   i, idx, trk.bytes_used, trk.bit_count, trk.splice_point);
        }
    }
}

int main(int argc, char** argv){
    if(argc != 2){
        fprintf(stderr,"usage: %s file.woz\n", argv[0]);
        return 2;
    }
    dump_one(argv[1]);
    return 0;
}
