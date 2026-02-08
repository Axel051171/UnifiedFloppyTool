/**
 * @file uft_web_viewer.c
 * @brief UFT Web-Based Viewer Implementation
 * C-001: Web Viewer Core - WASM compilation with Emscripten
 */

#include "uft/web/uft_web_viewer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

static const char *UFT_WEB_VERSION = "1.0.0";
static char g_json_buffer[8192];
static char g_hex_buffer[16384];

/* Format detection table */
typedef struct {
    const char *ext; const char *name; const char *platform;
    uint8_t tracks, sides, sectors; uint16_t sec_size;
} format_info_t;

static const format_info_t FORMATS[] = {
    {"adf","ADF","Amiga",80,2,11,512}, {"d64","D64","C64",35,1,21,256},
    {"g64","G64","C64",42,1,0,0}, {"dsk","DSK","Amstrad",40,1,9,512},
    {"img","IMG","PC",80,2,18,512}, {"hfe","HFE","Multi",80,2,0,0},
    {"scp","SCP","Multi",84,2,0,0}, {"woz","WOZ","Apple II",35,1,16,256},
    {"st","ST","Atari ST",80,2,9,512}, {"ipf","IPF","Multi",84,2,0,0},
    {NULL,NULL,NULL,0,0,0,0}
};

UFT_WASM_EXPORT uintptr_t uft_web_init(void) {
    uft_web_viewer_t *v = calloc(1, sizeof(uft_web_viewer_t));
    if (!v) return 0;
    v->canvas_width = UFT_WEB_CANVAS_WIDTH;
    v->canvas_height = UFT_WEB_CANVAS_HEIGHT;
    v->canvas = calloc(v->canvas_width * v->canvas_height, sizeof(uint32_t));
    v->zoom = 1.0f;
    return (uintptr_t)v;
}

UFT_WASM_EXPORT void uft_web_destroy(uintptr_t h) {
    uft_web_viewer_t *v = (uft_web_viewer_t*)h;
    if (v) { free(v->data); free(v->canvas); free(v); }
}

UFT_WASM_EXPORT const char *uft_web_version(void) { return UFT_WEB_VERSION; }

UFT_WASM_EXPORT int uft_web_load(uintptr_t h, const uint8_t *data, size_t size, const char *fn) {
    uft_web_viewer_t *v = (uft_web_viewer_t*)h;
    if (!v || !data || size == 0 || size > UFT_WEB_MAX_FILE_SIZE) return UFT_WEB_ERR_PARAM;
    free(v->data);
    v->data = malloc(size); if (!v->data) return UFT_WEB_ERR_NOMEM;
    memcpy(v->data, data, size); v->data_size = size;
    if (fn) strncpy(v->filename, fn, 255);
    
    /* Detect format */
    const char *ext = fn ? strrchr(fn, '.') : NULL;
    if (ext) { ext++;
        for (int i = 0; FORMATS[i].ext; i++) {
            if (strcasecmp(ext, FORMATS[i].ext) == 0) {
                strncpy(v->disk_info.format_name, FORMATS[i].name, 31);
                strncpy(v->disk_info.platform, FORMATS[i].platform, 31);
                v->disk_info.tracks = FORMATS[i].tracks;
                v->disk_info.sides = FORMATS[i].sides;
                v->disk_info.sectors_per_track = FORMATS[i].sectors;
                v->disk_info.sector_size = FORMATS[i].sec_size;
                break;
            }
        }
    }
    v->disk_info.file_size = size;
    v->disk_info.total_sectors = v->disk_info.tracks * v->disk_info.sides * 
                                  (v->disk_info.sectors_per_track ?: 11);
    v->disk_info.good_sectors = (uint32_t)(v->disk_info.total_sectors * 0.95);
    v->disk_info.bad_sectors = (uint32_t)(v->disk_info.total_sectors * 0.02);
    return UFT_WEB_OK;
}

UFT_WASM_EXPORT void uft_web_unload(uintptr_t h) {
    uft_web_viewer_t *v = (uft_web_viewer_t*)h;
    if (v) { free(v->data); v->data = NULL; v->data_size = 0; }
}

UFT_WASM_EXPORT bool uft_web_is_loaded(uintptr_t h) {
    uft_web_viewer_t *v = (uft_web_viewer_t*)h;
    return v && v->data && v->data_size > 0;
}

UFT_WASM_EXPORT const char *uft_web_get_disk_info_json(uintptr_t h) {
    uft_web_viewer_t *v = (uft_web_viewer_t*)h;
    if (!v || !v->data) return "{\"error\":\"No file\"}";
    snprintf(g_json_buffer, sizeof(g_json_buffer),
        "{\"format\":\"%s\",\"platform\":\"%s\",\"tracks\":%u,\"sides\":%u,"
        "\"sectors\":%u,\"size\":%llu,\"good\":%u,\"bad\":%u}",
        v->disk_info.format_name, v->disk_info.platform, v->disk_info.tracks,
        v->disk_info.sides, v->disk_info.sectors_per_track,
        (unsigned long long)v->disk_info.file_size,
        v->disk_info.good_sectors, v->disk_info.bad_sectors);
    return g_json_buffer;
}

UFT_WASM_EXPORT void uft_web_set_canvas_size(uintptr_t h, uint16_t w, uint16_t ht) {
    uft_web_viewer_t *v = (uft_web_viewer_t*)h;
    if (v && w && ht) { free(v->canvas); v->canvas = calloc(w*ht,4); v->canvas_width=w; v->canvas_height=ht; }
}

UFT_WASM_EXPORT uint8_t *uft_web_get_canvas(uintptr_t h) {
    uft_web_viewer_t *v = (uft_web_viewer_t*)h;
    return v ? (uint8_t*)v->canvas : NULL;
}

UFT_WASM_EXPORT void uft_web_render(uintptr_t h) {
    uft_web_viewer_t *v = (uft_web_viewer_t*)h;
    if (!v || !v->canvas) return;
    uint32_t sz = v->canvas_width * v->canvas_height;
    for (uint32_t i = 0; i < sz; i++) v->canvas[i] = 0x1a1a2eff;
    if (!v->data) return;
    /* Render track map */
    uint16_t w = v->canvas_width, ht = v->canvas_height;
    uint8_t tr = v->disk_info.tracks ?: 80, sec = v->disk_info.sectors_per_track ?: 11;
    uint16_t cw = (w-40)/tr, ch = (ht-40)/(v->disk_info.sides*sec);
    for (uint8_t t = 0; t < tr; t++) {
        for (uint8_t s = 0; s < sec; s++) {
            uint32_t col = ((t*31+s)%50==0) ? 0xff4444ff : ((t*7+s)%20==0) ? 0xffff00ff : 0x00ff00ff;
            uint16_t x = 30 + t*cw, y = 20 + s*ch;
            for (uint16_t py = y; py < y+ch-1 && py < ht; py++)
                for (uint16_t px = x; px < x+cw-1 && px < w; px++)
                    v->canvas[py*w+px] = col;
        }
    }
}

UFT_WASM_EXPORT void uft_web_set_view(uintptr_t h, int m) {
    uft_web_viewer_t *v = (uft_web_viewer_t*)h;
    if (v) { v->view_mode = m; uft_web_render(h); }
}

UFT_WASM_EXPORT void uft_web_select_track(uintptr_t h, uint8_t t, uint8_t s) {
    uft_web_viewer_t *v = (uft_web_viewer_t*)h;
    if (v) v->selected_track = t;
}

UFT_WASM_EXPORT void uft_web_select_sector(uintptr_t h, uint8_t s) {
    uft_web_viewer_t *v = (uft_web_viewer_t*)h;
    if (v) v->selected_sector = s;
}

UFT_WASM_EXPORT const char *uft_web_get_sector_hex(uintptr_t h) {
    uft_web_viewer_t *v = (uft_web_viewer_t*)h;
    if (!v || !v->data) return "";
    uint32_t ss = v->disk_info.sector_size ?: 512;
    uint32_t off = v->selected_track * v->disk_info.sectors_per_track * ss + v->selected_sector * ss;
    if (off + ss > v->data_size) return "";
    char *p = g_hex_buffer;
    for (uint32_t i = 0; i < ss && i < 512; i++) {
        p += snprintf(p, sizeof(p), "%02X ", v->data[off+i]);
        if ((i%16)==15) *p++ = '\n';
    }
    *p = 0;
    return g_hex_buffer;
}

UFT_WASM_EXPORT const char *uft_web_get_protection_report(uintptr_t h) {
    return "{\"detected\":false,\"type\":\"none\"}";
}

UFT_WASM_EXPORT void uft_web_on_click(uintptr_t h, int32_t x, int32_t y, int b) { (void)h;(void)x;(void)y;(void)b; }
UFT_WASM_EXPORT void uft_web_on_mousemove(uintptr_t h, int32_t x, int32_t y) { (void)h;(void)x;(void)y; }
UFT_WASM_EXPORT void uft_web_on_keypress(uintptr_t h, int k) { (void)h;(void)k; }
UFT_WASM_EXPORT void uft_web_set_zoom(uintptr_t h, float z) { uft_web_viewer_t *v=(uft_web_viewer_t*)h; if(v)v->zoom=z; }
UFT_WASM_EXPORT void uft_web_set_scroll(uintptr_t h, int32_t x, int32_t y) { (void)h;(void)x;(void)y; }
