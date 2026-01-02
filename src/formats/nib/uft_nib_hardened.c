/**
 * @file uft_nib_hardened.c
 * @brief Apple II NIB Format Plugin - HARDENED VERSION
 */

#include "uft/core/uft_safe_math.h"
#include "uft/uft_format_common.h"
#include "uft/uft_safe.h"

#define NIB_TRACKS 35
#define NIB_TRACK_SIZE 6656
#define NIB_FILE_SIZE 232960

static const uint8_t gcr62_decode[256] = {
    [0x96]=0,[0x97]=1,[0x9A]=2,[0x9B]=3,[0x9D]=4,[0x9E]=5,[0x9F]=6,[0xA6]=7,
    [0xA7]=8,[0xAB]=9,[0xAC]=10,[0xAD]=11,[0xAE]=12,[0xAF]=13,[0xB2]=14,[0xB3]=15,
    [0xB4]=16,[0xB5]=17,[0xB6]=18,[0xB7]=19,[0xB9]=20,[0xBA]=21,[0xBB]=22,[0xBC]=23,
    [0xBD]=24,[0xBE]=25,[0xBF]=26,[0xCB]=27,[0xCD]=28,[0xCE]=29,[0xCF]=30,[0xD3]=31,
    [0xD6]=32,[0xD7]=33,[0xD9]=34,[0xDA]=35,[0xDB]=36,[0xDC]=37,[0xDD]=38,[0xDE]=39,
    [0xDF]=40,[0xE5]=41,[0xE6]=42,[0xE7]=43,[0xE9]=44,[0xEA]=45,[0xEB]=46,[0xEC]=47,
    [0xED]=48,[0xEE]=49,[0xEF]=50,[0xF2]=51,[0xF3]=52,[0xF4]=53,[0xF5]=54,[0xF6]=55,
    [0xF7]=56,[0xF9]=57,[0xFA]=58,[0xFB]=59,[0xFC]=60,[0xFD]=61,[0xFE]=62,[0xFF]=63
};

typedef struct {
    uint8_t* data;
    size_t data_size;  // Track actual size
} nib_data_t;

static bool nib_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (file_size == NIB_FILE_SIZE) {
        *confidence = 85;
        return true;
    }
    return false;
}

static uft_error_t nib_open(uft_disk_t* disk, const char* path, bool read_only) {
    UFT_REQUIRE_NOT_NULL2(disk, path);
    
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    // Allocate plugin data
    nib_data_t* p = calloc(1, sizeof(nib_data_t));
    if (!p) {
        fclose(f);
        return UFT_ERROR_NO_MEMORY;
    }
    
    // Allocate data buffer
    p->data = malloc(NIB_FILE_SIZE);
    if (!p->data) {
        free(p);
        fclose(f);
        return UFT_ERROR_NO_MEMORY;
    }
    
    // Read with error check
    size_t read = fread(p->data, 1, NIB_FILE_SIZE, f);
    fclose(f);
    
    if (read != NIB_FILE_SIZE) {
        free(p->data);
        free(p);
        return UFT_ERROR_FILE_READ;
    }
    
    p->data_size = NIB_FILE_SIZE;
    disk->plugin_data = p;
    disk->geometry.cylinders = NIB_TRACKS;
    disk->geometry.heads = 1;
    disk->geometry.sectors = 16;
    disk->geometry.sector_size = 256;
    
    return UFT_OK;
}

static void nib_close(uft_disk_t* disk) {
    if (!disk) return;
    
    nib_data_t* p = disk->plugin_data;
    if (p) {
        free(p->data);  // Safe even if NULL
        p->data = NULL;
        free(p);
        disk->plugin_data = NULL;
    }
}

static int nib_find_addr(const uint8_t* t, size_t len, size_t start, 
                         uint8_t* vol, uint8_t* trk, uint8_t* sec) {
    // Bounds check
    if (start + 14 > len) return -1;
    
    for (size_t i = start; i < len - 14; i++) {
        if (t[i] == 0xD5 && t[i+1] == 0xAA && t[i+2] == 0x96) {
            *vol = ((t[i+3] << 1) | 1) & t[i+4];
            *trk = ((t[i+5] << 1) | 1) & t[i+6];
            *sec = ((t[i+7] << 1) | 1) & t[i+8];
            return (int)(i + 14);
        }
    }
    return -1;
}

static int nib_find_data(const uint8_t* t, size_t len, size_t start) {
    size_t end = (start + 100 < len - 3) ? start + 100 : len - 3;
    for (size_t i = start; i < end; i++) {
        if (t[i] == 0xD5 && t[i+1] == 0xAA && t[i+2] == 0xAD) {
            return (int)(i + 3);
        }
    }
    return -1;
}

static bool nib_decode_sector(const uint8_t* gcr, size_t gcr_len, uint8_t* out) {
    if (gcr_len < 342) return false;
    
    uint8_t buf[342];
    for (int i = 0; i < 342; i++) {
        uint8_t b = gcr[i];
        if (gcr62_decode[b] == 0 && b != 0x96) return false;
        buf[i] = gcr62_decode[b];
    }
    
    uint8_t prev = 0;
    for (int i = 0; i < 342; i++) {
        buf[i] ^= prev;
        prev = buf[i];
    }
    
    for (int i = 0; i < 256; i++) {
        int aux_idx = i % 86;
        int shift = (i / 86) * 2;
        out[i] = (buf[86 + i] << 2) | ((buf[aux_idx] >> shift) & 0x03);
    }
    return true;
}

static uft_error_t nib_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    UFT_REQUIRE_NOT_NULL2(disk, track);
    
    nib_data_t* p = disk->plugin_data;
    if (!p || !p->data) return UFT_ERROR_INVALID_STATE;
    
    // Bounds check
    if (head != 0 || cyl < 0 || cyl >= NIB_TRACKS) {
        return UFT_ERROR_INVALID_ARG;
    }
    
    uft_track_init(track, cyl, head);
    
    size_t track_offset = (size_t)cyl * NIB_TRACK_SIZE;
    if (track_offset + NIB_TRACK_SIZE > p->data_size) {
        return UFT_ERROR_BOUNDS;
    }
    
    const uint8_t* tdata = p->data + track_offset;
    uint8_t sec_buf[256];
    size_t pos = 0;
    
    while (pos < NIB_TRACK_SIZE - 400) {
        uint8_t vol, trk, sec;
        int addr_end = nib_find_addr(tdata, NIB_TRACK_SIZE, pos, &vol, &trk, &sec);
        if (addr_end < 0) break;
        
        if (trk != (uint8_t)cyl) {
            pos = (size_t)addr_end;
            continue;
        }
        
        int data_start = nib_find_data(tdata, NIB_TRACK_SIZE, (size_t)addr_end);
        if (data_start < 0 || data_start + 343 > (int)NIB_TRACK_SIZE) {
            pos = (size_t)addr_end;
            continue;
        }
        
        if (nib_decode_sector(&tdata[data_start], NIB_TRACK_SIZE - data_start, sec_buf)) {
            uft_error_t err = uft_format_add_sector(track, sec, sec_buf, 256, (uint8_t)cyl, 0);
            if (err != UFT_OK) {
                // Log but continue
            }
        }
        pos = (size_t)data_start + 343;
    }
    
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_nib_hardened = {
    .name = "NIB",
    .description = "Apple II Nibble (HARDENED)",
    .extensions = "nib",
    .version = 0x00010001,
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ,
    .probe = nib_probe,
    .open = nib_open,
    .close = nib_close,
    .read_track = nib_read_track,
};
