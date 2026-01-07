// ufm_uft_as_moof.c
// Applesauce MOOF 1.0 reader (chunk-based) -> bitstream OR embedded flux stream extraction
//
// Sources / spec references:
// - Applesauce MOOF Disk Image Reference 1.0 (header, INFO/TMAP/TRKS/FLUX/META, TRK struct, flux packing)
//   https://applesaucefdc.com/moof-reference/  (accessed 2025-12-25)
//
// Key preservation behaviors implemented:
// - Track bitstreams are exposed bit-perfect (packed MSB->LSB).
// - Optional FLUX tracks are exposed as packed bytes AND expanded delta-ticks without smoothing.
// - 255-run delta encoding is expanded exactly as spec (255,255,10 => 520 ticks). citeturn2view0
//
// Build: cc -std=c11 -O2 -Wall -Wextra -pedantic ufm_uft_as_moof.c -o moof_dump

#include "uft/uft_error.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static bool read_u8(FILE* f, uint8_t* out){
    int c=fgetc(f); if(c==EOF) return false; *out=(uint8_t)c; return true;
}
static bool read_le16(FILE* f, uint16_t* out){
    uint8_t a,b; if(!read_u8(f,&a)||!read_u8(f,&b)) return false; *out=(uint16_t)(a | ((uint16_t)b<<8)); return true;
}
static bool read_le32(FILE* f, uint32_t* out){
    uint8_t a,b,c,d;
    if(!read_u8(f,&a)||!read_u8(f,&b)||!read_u8(f,&c)||!read_u8(f,&d)) return false;
    *out=(uint32_t)((uint32_t)a | ((uint32_t)b<<8) | ((uint32_t)c<<16) | ((uint32_t)d<<24));
    return true;
}
static bool skip_bytes(FILE* f, size_t n){ return fseek(f,(long)n,SEEK_CUR)==0; }
static uint32_t fourcc(const char s[4]){
    return (uint32_t)( (uint8_t)s[0] |
                      ((uint32_t)(uint8_t)s[1] << 8) |
                      ((uint32_t)(uint8_t)s[2] << 16) |
                      ((uint32_t)(uint8_t)s[3] << 24) );
}

// Expand packed flux deltas (byte stream with 255-run) into tick deltas. citeturn2view0
static bool expand_255_run(const uint8_t* packed, uint32_t packed_len, uint32_t** out_ticks, uint32_t* out_count){
    uint32_t cap = (packed_len>0) ? packed_len : 1;
    uint32_t* arr = (uint32_t*)malloc(cap*sizeof(uint32_t));
    if(!arr) return false;

    uint32_t n=0, acc=0;
    for(uint32_t i=0;i<packed_len;i++){
        uint8_t v=packed[i];
        acc += v;
        if(v!=255){
            if(n>=cap){
                cap*=2;
                uint32_t* na = (uint32_t*)realloc(arr, cap*sizeof(uint32_t));
                if(!na){ free(arr); return false; }
                arr=na;
            }
            arr[n++] = acc;
            acc=0;
        }
    }
    *out_ticks=arr;
    *out_count=n;
    return true;
}

typedef struct {
    uint8_t info_version;
    uint8_t disk_type;          // 1=SSDD GCR 400K, 2=DSDD GCR 800K, 3=DSHD MFM 1.44M, 4=Twiggy citeturn2view0
    uint8_t write_protected;
    uint8_t synchronized;
    uint8_t optimal_bit_timing_125ns; // in 125ns units citeturn2view0
    char creator[33];
    uint16_t largest_track_blocks;
    uint16_t flux_block;        // block where FLUX chunk resides, 0 if none citeturn2view0
    uint16_t largest_flux_track_blocks;
} moof_info_t;

typedef struct {
    uint16_t start_block; // relative to file start, multiply by 512 for byte offset citeturn2view0
    uint16_t block_count;
    uint32_t bit_count_or_byte_count; // bits for BITS, bytes for FLUX streams in TRKS citeturn2view0
} moof_trk_desc_t;

typedef struct {
    moof_info_t info;
    uint8_t tmap[160];          // physical map -> TRKS index; 0xFF blank citeturn2view0
    uint8_t fluxmap[160];       // optional FLUX map, if present citeturn2view0
    bool have_fluxmap;

    // TRK descriptor array (160 entries) is located at TRKS chunk start. citeturn2view0
    moof_trk_desc_t trk[160];

    // Track data blocks begin at byte 1536 (block 3) for TRKS chunk; offsets are absolute blocks. citeturn2view0
} moof_image_t;

static void trim_creator(char out[33], const uint8_t in[32]){
    size_t n=32; while(n>0 && in[n-1]==0x20) n--;
    memcpy(out,in,n); out[n]='\0';
}

static bool read_info_chunk(FILE* f, uint32_t size, moof_image_t* img){
    if(size < 60) return false;
    uint8_t creator_raw[32];
    uint8_t pad;
    if(!read_u8(f,&img->info.info_version)) return false;
    if(!read_u8(f,&img->info.disk_type)) return false;
    if(!read_u8(f,&img->info.write_protected)) return false;
    if(!read_u8(f,&img->info.synchronized)) return false;
    if(!read_u8(f,&img->info.optimal_bit_timing_125ns)) return false;
    if(fread(creator_raw,1,32,f)!=32) return false;
    trim_creator(img->info.creator, creator_raw);

    // padding at offset 57 (1 byte), then largest track, flux block, largest flux track. citeturn2view0
    // We have consumed 1+1+1+1+1+32=37 bytes so far. Next bytes: padding (1), largest track (2),
    // flux block (2), largest flux track (2) => total 44 bytes; remaining to 60 are reserved/padding.
    if(!read_u8(f,&pad)) return false;
    if(!read_le16(f,&img->info.largest_track_blocks)) return false;
    if(!read_le16(f,&img->info.flux_block)) return false;
    if(!read_le16(f,&img->info.largest_flux_track_blocks)) return false;

    // Skip remaining bytes in INFO to reach 60 total
    const uint32_t consumed = 37 + 1 + 2 + 2 + 2; // 44
    if(size < consumed) return false;
    if(!skip_bytes(f, size - consumed)) return false;
    return true;
}

static bool read_map_chunk(FILE* f, uint32_t size, uint8_t out_map[160]){
    if(size < 160) return false;
    if(fread(out_map,1,160,f)!=160) return false;
    if(size > 160 && !skip_bytes(f,size-160)) return false;
    return true;
}

static bool read_trks_chunk(FILE* f, uint32_t size, moof_image_t* img){
    // TRKS chunk begins at byte 256 and has 160 TRK descriptors (8 bytes each), then BITS blocks from block 3. citeturn2view0
    // We parse descriptors (160 * 8 = 1280 bytes).
    (void)size;
    for(int i=0;i<160;i++){
        if(!read_le16(f,&img->trk[i].start_block)) return false;
        if(!read_le16(f,&img->trk[i].block_count)) return false;
        if(!read_le32(f,&img->trk[i].bit_count_or_byte_count)) return false;
    }
    // Remaining TRKS payload (the BITS/FLUX data) is not read here.
    // Caller can seek and read the blocks using start_block and block_count.
    return true;
}

static int moof_load(const char* path, moof_image_t* out){
    memset(out,0,sizeof(*out));
    FILE* f=fopen(path,"rb");
    if(!f) return -1;

    // 12-byte header: "MOOF" + 0xFF + 0x0A0D0A + CRC32. citeturn1view1
    uint8_t hdr[12];
    if(fread(hdr,1,12,f)!=12){ fclose(f); return -2; }
    if(!(hdr[0]=='M'&&hdr[1]=='O'&&hdr[2]=='O'&&hdr[3]=='F')){ fclose(f); return -3; }
    if(!(hdr[4]==0xFF && hdr[5]==0x0A && hdr[6]==0x0D && hdr[7]==0x0A)){ fclose(f); return -4; }

    bool have_info=false, have_tmap=false, have_trks=false;

    // chunks begin at byte 12 citeturn1view1
    while(true){
        uint32_t id=0,size=0;
        if(!read_le32(f,&id)) break;
        if(!read_le32(f,&size)) break;

        if(id==fourcc("INFO")){
            if(!read_info_chunk(f,size,out)){ fclose(f); return -10; }
            have_info=true;
        } else if(id==fourcc("TMAP")){
            if(!read_map_chunk(f,size,out->tmap)){ fclose(f); return -11; }
            have_tmap=true;
        } else if(id==fourcc("FLUX")){
            if(!read_map_chunk(f,size,out->fluxmap)){ fclose(f); return -12; }
            out->have_fluxmap=true;
        } else if(id==fourcc("TRKS")){
            // Read TRK descriptor array from the start of TRKS chunk. citeturn2view0
            if(!read_trks_chunk(f,size,out)){ fclose(f); return -13; }
            // Skip rest of TRKS payload to keep scanning chunks.
            // We already consumed 1280 bytes from TRKS chunk payload.
            if(size > 1280 && !skip_bytes(f,size-1280)){ fclose(f); return -14; }
            have_trks=true;
        } else {
            // META or unknown: skip
            if(!skip_bytes(f,size)){ fclose(f); return -15; }
        }
    }

    fclose(f);
    if(!have_info || !have_tmap || !have_trks) return -20;
    return 0;
}

// Read raw BITS/FLUX bytes for a given TRKS index.
// - If you want a physical track/side mapping, you must look up that index via TMAP or FLUX map. citeturn2view0
static uint8_t* moof_read_trk_payload(const char* path, const moof_image_t* img, uint8_t trk_index, uint32_t* out_len){
    if(trk_index == 0xFF) return NULL;
    const moof_trk_desc_t* d = &img->trk[trk_index];
    if(d->start_block == 0 || d->block_count == 0) return NULL;

    uint32_t len = (uint32_t)d->block_count * 512u;
    uint64_t off = (uint64_t)d->start_block * 512u;

    FILE* f=fopen(path,"rb");
    if(!f) return NULL;
    if(fseek(f,(long)off,SEEK_SET)!=0){ fclose(f); return NULL; }

    uint8_t* buf=(uint8_t*)malloc(len);
    if(!buf){ fclose(f); return NULL; }
    if(fread(buf,1,len,f)!=len){ free(buf); fclose(f); return NULL; }

    fclose(f);
    *out_len = len;
    return buf;
}

// Convenience: decode flux payload into tick deltas (125ns ticks in MOOF flux streams per spec). citeturn2view0
static bool moof_decode_flux_ticks(const uint8_t* payload, uint32_t payload_len, uint32_t** out_ticks, uint32_t* out_count){
    return expand_255_run(payload, payload_len, out_ticks, out_count);
}

// ------- demo CLI --------

static void dump_moof(const char* path){
    moof_image_t img;
    int rc=moof_load(path,&img);
    if(rc){
        fprintf(stderr,"MOOF load failed: %d\n", rc);
        return;
    }

    printf("MOOF: disk_type=%u wp=%u sync=%u opt_bit_timing=%u*125ns creator='%s'\n",
           img.info.disk_type, img.info.write_protected, img.info.synchronized,
           img.info.optimal_bit_timing_125ns, img.info.creator);
    printf("  LargestTrack=%u blocks, FLUXblock=%u, LargestFlux=%u blocks\n",
           img.info.largest_track_blocks, img.info.flux_block, img.info.largest_flux_track_blocks);

    // show first few physical entries
    for(int p=0;p<8;p++){
        uint8_t idx_bits = img.tmap[p];
        uint8_t idx_flux = img.have_fluxmap ? img.fluxmap[p] : 0xFF;
        printf("  phys[%d]: TMAP=%02X FLUX=%02X\n", p, idx_bits, idx_flux);
    }

    // load one mapped track payload as demo
    uint8_t idx = img.tmap[0];
    if(idx != 0xFF){
        uint32_t len=0;
        uint8_t* payload = moof_read_trk_payload(path,&img,idx,&len);
        if(payload){
            printf("  TRK[%u] payload len=%u bytes (blocks=%u), declared_count=%u\n",
                   idx, len, img.trk[idx].block_count, img.trk[idx].bit_count_or_byte_count);
            free(payload);
        }
    }
}

int main(int argc, char** argv){
    if(argc != 2){
        fprintf(stderr,"usage: %s file.moof\n", argv[0]);
        return 2;
    }
    dump_moof(argv[1]);
    return 0;
}
