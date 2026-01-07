// ufm_uft_as_a2r3.c
// Applesauce A2R 3.x reader (chunk-based) -> minimal UFM-style captures
//
// Sources / spec references:
// - Applesauce A2R 3.x Disk Image Reference (A2R3 header, chunk layout, INFO/RWCP/SLVD definitions)
//   https://applesaucefdc.com/a2r/  (accessed 2025-12-25)
//
// Preservation-first note:
// - This parser does NOT "fix" flux; it preserves the packed stream and exposes decoded delta-times
//   plus original packed bytes for lossless roundtrips.
//
// Build: cc -std=c11 -O2 -Wall -Wextra -pedantic ufm_uft_as_a2r3.c -o a2r_dump
//
// Public domain-ish single-file module: drop into libflux_format/ as a plugin base.

#include "uft/uft_error.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef UFM_A2R3_MAX_CREATOR
#define UFM_A2R3_MAX_CREATOR 64
#endif

// ----------- tiny IO helpers (little-endian) -----------

static bool read_u8(FILE* f, uint8_t* out){
    int c = fgetc(f);
    if(c == EOF) return false;
    *out = (uint8_t)c;
    return true;
}

static bool read_le16(FILE* f, uint16_t* out){
    uint8_t b0,b1;
    if(!read_u8(f,&b0) || !read_u8(f,&b1)) return false;
    *out = (uint16_t)(b0 | ((uint16_t)b1<<8));
    return true;
}

static bool read_le32(FILE* f, uint32_t* out){
    uint8_t b0,b1,b2,b3;
    if(!read_u8(f,&b0) || !read_u8(f,&b1) || !read_u8(f,&b2) || !read_u8(f,&b3)) return false;
    *out = (uint32_t)( (uint32_t)b0 |
                      ((uint32_t)b1<<8) |
                      ((uint32_t)b2<<16) |
                      ((uint32_t)b3<<24) );
    return true;
}

static bool skip_bytes(FILE* f, size_t n){
    return fseek(f, (long)n, SEEK_CUR) == 0;
}

static uint32_t fourcc(const char s[4]){
    return (uint32_t)( (uint8_t)s[0] |
                      ((uint32_t)(uint8_t)s[1] << 8) |
                      ((uint32_t)(uint8_t)s[2] << 16) |
                      ((uint32_t)(uint8_t)s[3] << 24) );
}

// ----------- minimal data model (can be mapped to your UFM later) -----------

typedef struct {
    uint32_t location;              // per spec
    uint32_t capture_type;          // 1=timing,2=bits,3=xtiming
    uint32_t resolution_ps;         // picoseconds per tick (chunk-wide)
    uint8_t  index_count;
    uint32_t* index_ticks;          // absolute tick times from start of capture (count=index_count)
    uint8_t*  packed;               // packed delta-ticks bytes (lossless)
    uint32_t packed_len;

    // Optional decoded deltas (expanded 255-run encoding) in ticks (lossy? no, exact)
    uint32_t* deltas_ticks;
    uint32_t  deltas_count;
} ufm_a2r3_capture_t;

typedef struct {
    uint8_t  info_version;
    char     creator[UFM_A2R3_MAX_CREATOR];
    uint8_t  drive_type;
    uint8_t  write_protected;
    uint8_t  synchronized;
    uint8_t  hard_sector_count;

    ufm_a2r3_capture_t* captures;
    size_t captures_len;
    size_t captures_cap;

    // SLVD: solved/looped streams (one per track location, typically)
    ufm_a2r3_capture_t* solved;
    size_t solved_len;
    size_t solved_cap;
} ufm_a2r3_image_t;

static void ufm_a2r3_image_free(ufm_a2r3_image_t* img){
    if(!img) return;
    for(size_t i=0;i<img->captures_len;i++){
        free(img->captures[i].index_ticks);
        free(img->captures[i].packed);
        free(img->captures[i].deltas_ticks);
    }
    for(size_t i=0;i<img->solved_len;i++){
        free(img->solved[i].index_ticks);
        free(img->solved[i].packed);
        free(img->solved[i].deltas_ticks);
    }
    free(img->captures);
    free(img->solved);
    memset(img,0,sizeof(*img));
}

static bool vec_push_capture(ufm_a2r3_capture_t** buf, size_t* len, size_t* cap, const ufm_a2r3_capture_t* v){
    if(*len >= *cap){
        size_t ncap = (*cap == 0) ? 16 : (*cap * 2);
        void* nb = realloc(*buf, ncap * sizeof(**buf));
        if(!nb) return false;
        *buf = (ufm_a2r3_capture_t*)nb;
        *cap = ncap;
    }
    (*buf)[*len] = *v;
    (*len)++;
    return true;
}

// Expand packed delta-ticks into a uint32_t array of tick counts (each flux transition delta in ticks)
// Spec: value 255 repeats, sum until non-255, then emit total. Example: 255,255,10 => 520 ticks. citeturn4view0
static bool expand_255_run(const uint8_t* packed, uint32_t packed_len, uint32_t** out_ticks, uint32_t* out_count){
    uint32_t cap = (packed_len > 0) ? packed_len : 1;
    uint32_t* arr = (uint32_t*)malloc(cap * sizeof(uint32_t));
    if(!arr) return false;

    uint32_t n=0;
    uint32_t acc=0;
    for(uint32_t i=0;i<packed_len;i++){
        uint8_t v = packed[i];
        acc += v;
        if(v != 255){
            if(n >= cap){
                cap *= 2;
                uint32_t* na = (uint32_t*)realloc(arr, cap * sizeof(uint32_t));
                if(!na){ free(arr); return false; }
                arr = na;
            }
            arr[n++] = acc;
            acc = 0;
        }
    }
    // If packed ends with 255-run without terminator, that's malformed. We'll ignore the tail.
    *out_ticks = arr;
    *out_count = n;
    return true;
}

// ----------- chunk readers -----------

static bool read_info_chunk(FILE* f, uint32_t chunk_size, ufm_a2r3_image_t* img){
    // INFO chunk v1 is 37 bytes. Fields as per spec. citeturn3view0
    if(chunk_size < 37) return false;

    uint8_t v=0;
    if(!read_u8(f,&v)) return false;
    img->info_version = v;

    // Creator: 32 bytes UTF-8 padded with spaces (0x20). citeturn3view0
    uint8_t creator_raw[32];
    if(fread(creator_raw,1,32,f) != 32) return false;
    // Copy and trim trailing spaces
    size_t n=32;
    while(n>0 && creator_raw[n-1] == 0x20) n--;
    if(n >= sizeof(img->creator)) n = sizeof(img->creator)-1;
    memcpy(img->creator, creator_raw, n);
    img->creator[n] = '\0';

    if(!read_u8(f, &img->drive_type)) return false;
    if(!read_u8(f, &img->write_protected)) return false;
    if(!read_u8(f, &img->synchronized)) return false;
    if(!read_u8(f, &img->hard_sector_count)) return false;

    // Skip any remaining bytes in INFO chunk if future versions add fields.
    if(chunk_size > 37){
        if(!skip_bytes(f, chunk_size - 37)) return false;
    }
    return true;
}

static bool read_rwcp_chunk(FILE* f, uint32_t chunk_size, ufm_a2r3_image_t* img){
    // RWCP v1 header:
    // +0 u8 version, +1 u32 resolution_ps, +5..+15 reserved (11 bytes), +16 entries. citeturn4view0
    if(chunk_size < 16) return false;

    long chunk_start = ftell(f);
    if(chunk_start < 0) return false;

    uint8_t v=0;
    uint32_t resolution_ps=0;
    if(!read_u8(f,&v)) return false;
    if(!read_le32(f,&resolution_ps)) return false;
    // reserved 11 bytes
    if(!skip_bytes(f, 11)) return false;

    long payload_start = ftell(f);
    if(payload_start < 0) return false;

    // Parse capture entries until mark 'X' (0x58) or end-of-chunk. citeturn4view0
    while(true){
        long pos = ftell(f);
        if(pos < 0) return false;
        if((uint32_t)(pos - chunk_start) >= chunk_size) break;

        uint8_t mark=0;
        if(!read_u8(f,&mark)) break;
        if(mark == 0x58){ // 'X' end of captures
            break;
        }
        if(mark != 0x43){ // 'C'
            // unknown / corrupt: stop parsing; preserve by skipping rest
            break;
        }

        uint8_t cap_type=0;
        uint16_t location=0;
        uint8_t idx_count=0;

        if(!read_u8(f,&cap_type)) return false;
        if(!read_le16(f,&location)) return false;
        if(!read_u8(f,&idx_count)) return false;

        uint32_t* idx_ticks = NULL;
        if(idx_count){
            idx_ticks = (uint32_t*)malloc((size_t)idx_count * sizeof(uint32_t));
            if(!idx_ticks) return false;
            for(uint8_t i=0;i<idx_count;i++){
                if(!read_le32(f, &idx_ticks[i])){ free(idx_ticks); return false; }
            }
        }

        uint32_t data_size=0;
        if(!read_le32(f,&data_size)){ free(idx_ticks); return false; }

        uint8_t* packed = NULL;
        if(data_size){
            packed = (uint8_t*)malloc(data_size);
            if(!packed){ free(idx_ticks); return false; }
            if(fread(packed,1,data_size,f) != data_size){
                free(idx_ticks); free(packed); return false;
            }
        }

        ufm_a2r3_capture_t cap = {0};
        cap.location = (uint32_t)location;
        cap.capture_type = cap_type;
        cap.resolution_ps = resolution_ps;
        cap.index_count = idx_count;
        cap.index_ticks = idx_ticks;
        cap.packed = packed;
        cap.packed_len = data_size;

        // Expand only for timing/xtiming/solved streams. Bits capture is deprecated but still packed bytes.
        if(cap_type == 1 || cap_type == 3){
            if(!expand_255_run(packed, data_size, &cap.deltas_ticks, &cap.deltas_count)){
                free(idx_ticks); free(packed); return false;
            }
        }

        if(!vec_push_capture(&img->captures, &img->captures_len, &img->captures_cap, &cap)){
            free(idx_ticks); free(packed); free(cap.deltas_ticks);
            return false;
        }
    }

    // Skip remainder of RWCP chunk if we stopped early.
    long end_pos = ftell(f);
    if(end_pos < 0) return false;
    long consumed = end_pos - chunk_start;
    if(consumed < 0) return false;
    if((uint32_t)consumed < chunk_size){
        if(!skip_bytes(f, chunk_size - (uint32_t)consumed)) return false;
    }
    return true;
}

static bool read_slvd_chunk(FILE* f, uint32_t chunk_size, ufm_a2r3_image_t* img){
    // SLVD v2 header similar to RWCP but entries are Tracks ('T' 0x54) or 'X' end. citeturn4view0
    if(chunk_size < 16) return false;

    long chunk_start = ftell(f);
    if(chunk_start < 0) return false;

    uint8_t v=0;
    uint32_t resolution_ps=0;
    if(!read_u8(f,&v)) return false;
    if(!read_le32(f,&resolution_ps)) return false;
    if(!skip_bytes(f, 11)) return false;

    while(true){
        long pos = ftell(f);
        if(pos < 0) return false;
        if((uint32_t)(pos - chunk_start) >= chunk_size) break;

        uint8_t mark=0;
        if(!read_u8(f,&mark)) break;
        if(mark == 0x58){ // 'X'
            break;
        }
        if(mark != 0x54){ // 'T'
            break;
        }

        uint16_t location=0;
        uint8_t mirror_out=0, mirror_in=0;
        if(!read_le16(f,&location)) return false;
        if(!read_u8(f,&mirror_out)) return false;
        if(!read_u8(f,&mirror_in)) return false;
        // reserved 6 bytes
        if(!skip_bytes(f,6)) return false;

        uint8_t idx_count=0;
        if(!read_u8(f,&idx_count)) return false;

        uint32_t* idx_ticks=NULL;
        if(idx_count){
            idx_ticks = (uint32_t*)malloc((size_t)idx_count * sizeof(uint32_t));
            if(!idx_ticks) return false;
            for(uint8_t i=0;i<idx_count;i++){
                if(!read_le32(f,&idx_ticks[i])){ free(idx_ticks); return false; }
            }
        }

        uint32_t data_size=0;
        if(!read_le32(f,&data_size)){ free(idx_ticks); return false; }

        uint8_t* packed=NULL;
        if(data_size){
            packed=(uint8_t*)malloc(data_size);
            if(!packed){ free(idx_ticks); return false; }
            if(fread(packed,1,data_size,f) != data_size){ free(idx_ticks); free(packed); return false; }
        }

        ufm_a2r3_capture_t cap={0};
        cap.location=(uint32_t)location;
        cap.capture_type=0x100; // arbitrary: solved
        cap.resolution_ps=resolution_ps;
        cap.index_count=idx_count;
        cap.index_ticks=idx_ticks;
        cap.packed=packed;
        cap.packed_len=data_size;

        if(!expand_255_run(packed, data_size, &cap.deltas_ticks, &cap.deltas_count)){
            free(idx_ticks); free(packed); return false;
        }

        // Mirrors are preservation-relevant metadata; store in high bits of capture_type for now.
        cap.capture_type |= ((uint32_t)mirror_out << 8) | ((uint32_t)mirror_in << 16);

        if(!vec_push_capture(&img->solved, &img->solved_len, &img->solved_cap, &cap)){
            free(idx_ticks); free(packed); free(cap.deltas_ticks);
            return false;
        }
    }

    long end_pos = ftell(f);
    if(end_pos < 0) return false;
    long consumed = end_pos - chunk_start;
    if(consumed < 0) return false;
    if((uint32_t)consumed < chunk_size){
        if(!skip_bytes(f, chunk_size - (uint32_t)consumed)) return false;
    }
    return true;
}

// ----------- public API: load A2R3 -----------

static int ufm_a2r3_load(const char* path, ufm_a2r3_image_t* out){
    memset(out,0,sizeof(*out));
    FILE* f = fopen(path,"rb");
    if(!f) return -1;

    // 8-byte header: "A2R3" + 0xFF 0x0A 0x0D 0x0A citeturn1view2
    uint8_t hdr[8];
    if(fread(hdr,1,8,f) != 8){ fclose(f); return -2; }
    if(!(hdr[0]=='A' && hdr[1]=='2' && hdr[2]=='R' && (hdr[3]=='3' || hdr[3]=='2'))){
        fclose(f); return -3;
    }
    if(!(hdr[4]==0xFF && hdr[5]==0x0A && hdr[6]==0x0D && hdr[7]==0x0A)){
        fclose(f); return -4;
    }

    // chunks start at byte 8 citeturn1view2
    while(true){
        uint32_t id=0, size=0;
        long pos = ftell(f);
        if(pos < 0) break;

        if(!read_le32(f,&id)) break;
        if(!read_le32(f,&size)) break;

        if(id == fourcc("INFO")){
            if(!read_info_chunk(f,size,out)){ fclose(f); ufm_a2r3_image_free(out); return -10; }
        } else if(id == fourcc("RWCP")){
            if(!read_rwcp_chunk(f,size,out)){ fclose(f); ufm_a2r3_image_free(out); return -11; }
        } else if(id == fourcc("SLVD")){
            if(!read_slvd_chunk(f,size,out)){ fclose(f); ufm_a2r3_image_free(out); return -12; }
        } else {
            // META or future chunks: skip
            if(!skip_bytes(f,size)){ fclose(f); ufm_a2r3_image_free(out); return -13; }
        }
    }

    fclose(f);
    return 0;
}

// ----------- demo CLI (optional) -----------

static double ticks_to_us(uint32_t ticks, uint32_t resolution_ps){
    // ticks * resolution_ps => picoseconds
    // 1 microsecond = 1e6 ps
    return (double)ticks * (double)resolution_ps / 1.0e6;
}

int main(int argc, char** argv){
    if(argc != 2){
        fprintf(stderr, "usage: %s file.a2r\n", argv[0]);
        return 2;
    }

    ufm_a2r3_image_t img;
    int rc = ufm_a2r3_load(argv[1], &img);
    if(rc){
        fprintf(stderr, "A2R load failed: %d\n", rc);
        return 1;
    }

    printf("A2R: creator='%s' drive_type=%u wp=%u sync=%u hard_sectors=%u\n",
           img.creator, img.drive_type, img.write_protected, img.synchronized, img.hard_sector_count);
    printf("RWCP captures: %zu, SLVD tracks: %zu\n", img.captures_len, img.solved_len);

    // print a small summary per location (first capture per location)
    for(size_t i=0;i<img.captures_len && i<10;i++){
        const ufm_a2r3_capture_t* c = &img.captures[i];
        printf("  CAP[%zu] loc=%u type=%u res=%ups idx=%u packed=%u deltas=%u first_delta=%.3fus\n",
               i, c->location, c->capture_type, c->resolution_ps, c->index_count, c->packed_len,
               c->deltas_count, (c->deltas_count? ticks_to_us(c->deltas_ticks[0], c->resolution_ps):0.0));
    }

    ufm_a2r3_image_free(&img);
    return 0;
}
