/**
 * @file uft_v3_bridge.c
 * @brief Bridge between v3 parsers and format handler API
 */

#include "uft/uft_formats_extended.h"
#include "uft/uft_error.h"
/* MF-1130: der gepruefte Uebergang zu den v3-Parsern. Bis dahin standen
 * ALLE Deklarationen in dieser Datei von Hand — und genau daran hat
 * MF-442 einen Schreibzugriff auf eine beliebige Adresse gefunden (drei
 * Parameter deklariert, vier definiert). Alles Neue laeuft ueber diesen
 * Header, damit eine Abweichung ein Baufehler ist. Die alten externs
 * darunter bleiben vorerst stehen; sie in denselben Header zu ziehen ist
 * der naechste Schritt und braucht seinen eigenen Rotbeweis. */
#include "uft/formats/uft_v3_parsers.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * External v3 Parser Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

struct d64_disk_v3;
struct d64_params;
struct d64_diagnosis_list;

/* MF-442: these three used to be declared here with THREE parameters while
 * every definition takes FOUR — the fourth a `float *confidence` that the
 * callee dereferences right after its NULL check. Since this file includes no
 * header for the v3 parsers (they have none; their types live inside the .c
 * files), the compiler could not see the mismatch. At runtime the callee would
 * have written 0.0f through whatever the fourth argument slot happened to
 * hold: an arbitrary-address write.
 *
 * Not currently reachable — neither uft_smart_open() nor uft_advanced_open()
 * has a caller — so this was a loaded trap rather than a live fault. The
 * confidence value is now forwarded rather than discarded, which also removes
 * an invented number downstream: uft_smart_open() hard-coded
 * `prot->confidence = 80` while the parser had computed a real one. */
extern bool d64_parse(const uint8_t* data, size_t size, struct d64_disk_v3* disk, struct d64_params* params);
extern uint8_t* d64_write(const struct d64_disk_v3* disk, struct d64_params* params, size_t* out_size);
extern bool d64_detect_protection(const struct d64_disk_v3* disk, char* protection_name,
                 size_t name_size, float* confidence);
extern void d64_disk_free(struct d64_disk_v3* disk);
extern void d64_get_default_params(struct d64_params* params);

struct g64_disk;
struct g64_params;

extern bool g64_parse(const uint8_t* data, size_t size, struct g64_disk* disk, struct g64_params* params);
extern uint8_t* g64_write(const struct g64_disk* disk, struct g64_params* params, size_t* out_size);
extern bool g64_detect_protection(const struct g64_disk* disk, char* protection_name,
                 size_t name_size, float* confidence);
extern void g64_disk_free(struct g64_disk* disk);
extern void g64_get_default_params(struct g64_params* params);
/* MF-442: was declared with two parameters; the definition takes three,
 * the third a `bool include_errors`. The callee read whatever the third
 * argument slot held and branched on it — the exported D64 got an error
 * map or not, at random. */
extern uint8_t* g64_export_d64(const struct g64_disk* g64, size_t* out_size,
                               bool include_errors);

struct scp_disk;
struct scp_params;

extern bool scp_parse(const uint8_t* data, size_t size, struct scp_disk* disk, struct scp_params* params);
extern uint8_t* scp_write(const struct scp_disk* disk, struct scp_params* params, size_t* out_size);
extern bool scp_detect_protection(const struct scp_disk* disk, char* protection_name,
                 size_t name_size, float* confidence);
extern void scp_disk_free(struct scp_disk* disk);
extern void scp_get_default_params(struct scp_params* params);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Handle Structure
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* MF-923 — HIER STAND `uint8_t disk_buffer[256 * 1024]`, UND KEINE DER
 * DREI STRUKTUREN PASSTE HINEIN.
 *
 * Gemessen (Bisektion bzw. sizeof gegen dieselben Uebersetzungsschalter):
 *
 *     g64_disk_t     1 440 256 Byte    5,5x   Ueberlauf 1 178 112 Byte
 *     d64_disk_v3_t    600 664 Byte    2,3x   Ueberlauf   338 520 Byte
 *     scp_disk_t     3 119 512 Byte   11,9x   Ueberlauf 2 857 368 Byte
 *
 * `g64_parse()` & Co. bekamen einen Zeiger auf dieses Feld und
 * schrieben ihre volle Struktur hinein — bis zu 2,8 MB hinter das
 * Feldende, mitten in eine heap-allozierte `v3_handle_t`. Ein
 * Testaufruf endete reproduzierbar mit STATUS_HEAP_CORRUPTION
 * (0xC0000374).
 *
 * Dass davon bisher niemand betroffen war, ist kein Verdienst: die
 * Bruecke hat keinen Aufrufer ausserhalb ihrer selbst (P3-193). Sie
 * war eine Mine mit gezogenem Stift, nicht ein harmloser Rest.
 *
 * Der Puffer ist jetzt dynamisch, und die Groesse wird ABGEFRAGT statt
 * gepflegt: `<x>_disk_sizeof()` liegt bei dem, der den Typ besitzt.
 * Eine zweite Konstante hier waere beim naechsten Feld in einer der
 * drei Strukturen wieder falsch — und wieder still. */
extern size_t g64_disk_sizeof(void);
extern size_t d64_disk_v3_sizeof(void);
extern size_t scp_disk_sizeof(void);

#define V3_PARAMS_BUFFER_SIZE (4096)

typedef struct {
    uint8_t*    raw_data;
    size_t      raw_size;
    uint8_t*    disk_buffer;        /* MF-923: belegt nach *_disk_sizeof() */
    size_t      disk_buffer_size;
    uint8_t     params_buffer[V3_PARAMS_BUFFER_SIZE];
    char        path[1024];
    bool        valid;
} v3_handle_t;

/* Legt einen Griff samt passend grossem Strukturpuffer an. */
static v3_handle_t *v3_handle_new(size_t disk_size)
{
    v3_handle_t *h = calloc(1, sizeof(v3_handle_t));
    if (!h) return NULL;
    h->disk_buffer = calloc(1, disk_size);
    if (!h->disk_buffer) { free(h); return NULL; }
    h->disk_buffer_size = disk_size;
    return h;
}

static void v3_handle_free(v3_handle_t *h)
{
    if (!h) return;
    free(h->disk_buffer);
    free(h->raw_data);
    free(h);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * D64 v3 Bridge
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uft_error_t d64_v3_open(const char* path, void** handle) {
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERR_FILE_NOT_FOUND;
    
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    v3_handle_t* h = v3_handle_new(d64_disk_v3_sizeof());
    if (!h) { fclose(f); return UFT_ERR_MEMORY; }
    
    h->raw_data = malloc(size);
    if (!h->raw_data) {
        fclose(f);
        v3_handle_free(h);
        return UFT_ERR_MEMORY;
    }
    
    if (fread(h->raw_data, 1, size, f) != size) {
        v3_handle_free(h);
        fclose(f);
        return UFT_ERR_IO;
    }
    fclose(f);
    
    h->raw_size = size;
    strncpy(h->path, path, sizeof(h->path) - 1);
    
    d64_get_default_params((struct d64_params*)h->params_buffer);
    
    if (!d64_parse(h->raw_data, size, 
                   (struct d64_disk_v3*)h->disk_buffer,
                   (struct d64_params*)h->params_buffer)) {
        v3_handle_free(h);
        return UFT_ERR_FORMAT;
    }
    
    h->valid = true;
    *handle = h;
    return UFT_OK;
}

static void d64_v3_close(void* handle) {
    if (!handle) return;
    v3_handle_t* h = (v3_handle_t*)handle;
    
    if (h->valid) {
        d64_disk_free((struct d64_disk_v3*)h->disk_buffer);
    }
    v3_handle_free(h);
}

/* MF-1130: hier stand `(void)handle;` und danach 35 / 1 / 21.
 *
 * Zwei Dinge daran waren falsch. Die 35 uebergeht einen vorhandenen
 * Messwert — `d64_disk_v3_t.tracks` traegt die Spurzahl aus der Datei,
 * und eine erweiterte D64 hat 40. Die 21 ist fuer die meisten Spuren
 * schlicht unwahr: eine 1541 hat VIER Zonen mit 21/19/18/17 Sektoren,
 * und die Tafel dafuer liegt im selben Parser.
 *
 * `sectors` heisst deshalb jetzt MAXIMUM ueber die Zonen. Wer die
 * Sektorzahl EINER Spur braucht, fragt `d64_v3_spur_sektoren()`. */
static uft_error_t d64_v3_get_geometry(void* handle, int* cyls, int* heads, int* sectors) {
    if (cyls) *cyls = 0;
    if (heads) *heads = 0;
    if (sectors) *sectors = 0;

    v3_handle_t* h = (v3_handle_t*)handle;
    if (!h || !h->valid) return UFT_ERR_INVALID_ARG;

    if (!d64_v3_geometrie((const struct d64_disk_v3*)h->disk_buffer,
                          cyls, heads, sectors)) {
        /* Kein Rueckfall auf eine Konstante: wer nicht messen kann,
         * sagt ab (MF-1129). */
        return UFT_ERR_FORMAT;
    }
    return UFT_OK;
}

/* MF-1130: `read_track` war im Feldsatz von `uft_format_handler_t`
 * vorhanden und wurde von KEINER Datei in `src/` gesetzt — gemessen ueber
 * `git ls-files`. `uft_advanced_get_track_quality()` konnte deshalb nie
 * messen, und die erfundene Guete 1.0 aus MF-1129 hat genau das
 * verdeckt. */
static uft_error_t d64_v3_read_track(void* handle, int cyl, int head,
                                     void* buffer, size_t* size) {
    if (!size) return UFT_ERR_INVALID_ARG;
    v3_handle_t* h = (v3_handle_t*)handle;
    if (!h || !h->valid) { *size = 0; return UFT_ERR_INVALID_ARG; }

    size_t n = *size;
    if (!d64_v3_spur_lesen((const struct d64_disk_v3*)h->disk_buffer,
                           cyl, head, (uint8_t*)buffer, &n)) {
        /* `n` traegt bei zu kleinem Puffer den BEDARF — durchgereicht,
         * damit der Aufrufer nachlegen kann statt zu raten. */
        *size = n;
        return (buffer && n > 0) ? UFT_ERR_BUFFER_TOO_SMALL : UFT_ERR_FORMAT;
    }
    *size = n;
    return UFT_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * G64 v3 Bridge  
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uft_error_t g64_v3_open(const char* path, void** handle) {
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERR_FILE_NOT_FOUND;
    
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    v3_handle_t* h = v3_handle_new(g64_disk_sizeof());
    if (!h) { fclose(f); return UFT_ERR_MEMORY; }
    
    h->raw_data = malloc(size);
    if (!h->raw_data) { fclose(f); v3_handle_free(h); return UFT_ERR_MEMORY; }
    
    if (fread(h->raw_data, 1, size, f) != size) {
        fclose(f); v3_handle_free(h);
        return UFT_ERR_IO;
    }
    fclose(f);
    
    h->raw_size = size;
    strncpy(h->path, path, sizeof(h->path) - 1);
    
    g64_get_default_params((struct g64_params*)h->params_buffer);
    
    if (!g64_parse(h->raw_data, size,
                   (struct g64_disk*)h->disk_buffer,
                   (struct g64_params*)h->params_buffer)) {
        v3_handle_free(h);
        return UFT_ERR_FORMAT;
    }
    
    h->valid = true;
    *handle = h;
    return UFT_OK;
}

static void g64_v3_close(void* handle) {
    if (!handle) return;
    v3_handle_t* h = (v3_handle_t*)handle;
    if (h->valid) g64_disk_free((struct g64_disk*)h->disk_buffer);
    v3_handle_free(h);
}

/* MF-1130: hier stand `(void)handle;` und danach 42 / 1 / 21 — siehe
 * `d64_v3_get_geometry` fuer die Rechnung. `g64_disk_t.track_count`
 * zaehlt HALBSPUREN; gemeldet werden Vollspuren, weil ein 0-basierter
 * Zylinder genau die adressiert. */
static uft_error_t g64_v3_get_geometry(void* handle, int* cyls, int* heads, int* sectors) {
    if (cyls) *cyls = 0;
    if (heads) *heads = 0;
    if (sectors) *sectors = 0;

    v3_handle_t* h = (v3_handle_t*)handle;
    if (!h || !h->valid) return UFT_ERR_INVALID_ARG;

    if (!g64_v3_geometrie((const struct g64_disk*)h->disk_buffer,
                          cyls, heads, sectors)) {
        return UFT_ERR_FORMAT;
    }
    return UFT_OK;
}

static uft_error_t g64_v3_read_track(void* handle, int cyl, int head,
                                     void* buffer, size_t* size) {
    if (!size) return UFT_ERR_INVALID_ARG;
    v3_handle_t* h = (v3_handle_t*)handle;
    if (!h || !h->valid) { *size = 0; return UFT_ERR_INVALID_ARG; }

    size_t n = *size;
    if (!g64_v3_spur_lesen((const struct g64_disk*)h->disk_buffer,
                           cyl, head, (uint8_t*)buffer, &n)) {
        *size = n;
        return (buffer && n > 0) ? UFT_ERR_BUFFER_TOO_SMALL : UFT_ERR_FORMAT;
    }
    *size = n;
    return UFT_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * SCP v3 Bridge
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uft_error_t scp_v3_open(const char* path, void** handle) {
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERR_FILE_NOT_FOUND;
    
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    v3_handle_t* h = v3_handle_new(scp_disk_sizeof());
    if (!h) { fclose(f); return UFT_ERR_MEMORY; }
    
    h->raw_data = malloc(size);
    if (!h->raw_data) { fclose(f); v3_handle_free(h); return UFT_ERR_MEMORY; }
    
    if (fread(h->raw_data, 1, size, f) != size) {
        fclose(f); v3_handle_free(h);
        return UFT_ERR_IO;
    }
    fclose(f);
    
    h->raw_size = size;
    strncpy(h->path, path, sizeof(h->path) - 1);
    
    scp_get_default_params((struct scp_params*)h->params_buffer);
    
    if (!scp_parse(h->raw_data, size,
                   (struct scp_disk*)h->disk_buffer,
                   (struct scp_params*)h->params_buffer)) {
        v3_handle_free(h);
        return UFT_ERR_FORMAT;
    }
    
    h->valid = true;
    *handle = h;
    return UFT_OK;
}

static void scp_v3_close(void* handle) {
    if (!handle) return;
    v3_handle_t* h = (v3_handle_t*)handle;
    if (h->valid) scp_disk_free((struct scp_disk*)h->disk_buffer);
    v3_handle_free(h);
}

/* MF-1130: hier stand `(void)handle;` und danach 84 / 2 / 0. Die 0 bei
 * den Sektoren war die einzige der neun Konstanten, die nichts erfunden
 * hat — ein Flussabbild hat keine Sektorebene. 84 und 2 galten dagegen
 * auch fuer eine einseitige 40-Spur-Aufnahme. */
static uft_error_t scp_v3_get_geometry(void* handle, int* cyls, int* heads, int* sectors) {
    if (cyls) *cyls = 0;
    if (heads) *heads = 0;
    if (sectors) *sectors = 0;

    v3_handle_t* h = (v3_handle_t*)handle;
    if (!h || !h->valid) return UFT_ERR_INVALID_ARG;

    if (!scp_v3_geometrie((const struct scp_disk*)h->disk_buffer,
                          cyls, heads, sectors)) {
        return UFT_ERR_FORMAT;
    }
    return UFT_OK;
}

/* MF-1130: SCP sagt hier AB, und das ist gemessen statt behauptet —
 * `scp_track_t.merged_bits` ist das einzige Bytefeld und wird in der
 * ganzen Datei nur deklariert und freigegeben, nie belegt. Flusszeiten
 * in einen Bytepuffer zu schreiben waere die Umdeutung, die P3-357 bei
 * `pri` und MF-1079 bei `ipf` gekostet hat. */
static uft_error_t scp_v3_read_track(void* handle, int cyl, int head,
                                     void* buffer, size_t* size) {
    (void)buffer;
    v3_handle_t* h = (v3_handle_t*)handle;
    if (size) *size = 0;
    if (!size) return UFT_ERR_INVALID_ARG;
    if (!h || !h->valid) return UFT_ERR_INVALID_ARG;
    (void)scp_v3_spur_lesen((const struct scp_disk*)h->disk_buffer,
                            cyl, head, NULL, size);
    return UFT_ERR_NOT_SUPPORTED;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Public Handlers
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* ═══════════════════════════════════════════════════════════════════════════════
 * MF-1130 — Befund je Spur, auf Brueckenebene
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * Diese drei nehmen den OPAKEN GRIFF (`v3_handle_t*`, was `open()`
 * herausgibt) und packen die Parser-Struktur daraus aus. Ohne sie muesste
 * ein Aufrufer den Griff auf `struct g64_disk*` casten — ein Zugriff auf
 * die falsche Stelle, weil die Struktur im Griff als `disk_buffer` liegt.
 * Genau diese Klasse hat MF-442 hier gefunden, nur mit einer
 * Parameterzahl statt einem Typ.
 */
bool uft_v3_griff_befund_d64(void* griff, int zylinder, int kopf,
                             uft_v3_spurbefund_t* aus) {
    v3_handle_t* h = (v3_handle_t*)griff;
    if (!aus) return false;
    if (!h || !h->valid) { memset(aus, 0, sizeof(*aus)); return false; }
    return d64_v3_spur_befund((const struct d64_disk_v3*)h->disk_buffer,
                              zylinder, kopf, aus);
}

bool uft_v3_griff_befund_g64(void* griff, int zylinder, int kopf,
                             uft_v3_spurbefund_t* aus) {
    v3_handle_t* h = (v3_handle_t*)griff;
    if (!aus) return false;
    if (!h || !h->valid) { memset(aus, 0, sizeof(*aus)); return false; }
    return g64_v3_spur_befund((const struct g64_disk*)h->disk_buffer,
                              zylinder, kopf, aus);
}

bool uft_v3_griff_befund_scp(void* griff, int zylinder, int kopf,
                             uft_v3_spurbefund_t* aus) {
    v3_handle_t* h = (v3_handle_t*)griff;
    if (!aus) return false;
    if (!h || !h->valid) { memset(aus, 0, sizeof(*aus)); return false; }
    return scp_v3_spur_befund((const struct scp_disk*)h->disk_buffer,
                              zylinder, kopf, aus);
}

uft_format_handler_t uft_d64_v3_handler = {
    .format             = UFT_FORMAT_D64,
    .name               = "D64 v3",
    .extension          = "d64",
    .description        = "Commodore 1541 Disk Image (Advanced Parser)",
    .mime_type          = "application/x-d64",
    .supports_read      = true,
    .supports_write     = true,
    .supports_flux      = false,
    .supports_weak_bits = false,
    .supports_multiple_revs = true,
    .open               = d64_v3_open,
    .close              = d64_v3_close,
    .get_geometry       = d64_v3_get_geometry,
    /* MF-1130: dieses Feld war nie gesetzt — von KEINER Datei in src/,
     * gemessen ueber git ls-files. */
    .read_track         = d64_v3_read_track,
};

uft_format_handler_t uft_g64_v3_handler = {
    .format             = UFT_FORMAT_G64,
    .name               = "G64 v3",
    .extension          = "g64",
    .description        = "Commodore 1541 GCR Image (Advanced Parser)",
    .mime_type          = "application/x-g64",
    .supports_read      = true,
    .supports_write     = true,
    .supports_flux      = true,
    .supports_weak_bits = true,
    .supports_multiple_revs = true,
    .open               = g64_v3_open,
    .close              = g64_v3_close,
    .get_geometry       = g64_v3_get_geometry,
    /* MF-1130: dieses Feld war nie gesetzt — von KEINER Datei in src/,
     * gemessen ueber git ls-files. */
    .read_track         = g64_v3_read_track,
};

uft_format_handler_t uft_scp_v3_handler = {
    .format             = UFT_FORMAT_SCP,
    .name               = "SCP v3",
    .extension          = "scp",
    .description        = "SuperCard Pro Flux Image (Advanced Parser)",
    .mime_type          = "application/x-scp",
    .supports_read      = true,
    .supports_write     = true,
    .supports_flux      = true,
    .supports_weak_bits = true,
    .supports_multiple_revs = true,
    .open               = scp_v3_open,
    .close              = scp_v3_close,
    .get_geometry       = scp_v3_get_geometry,
    /* MF-1130: dieses Feld war nie gesetzt — von KEINER Datei in src/,
     * gemessen ueber git ls-files. */
    .read_track         = scp_v3_read_track,
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Extended API Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool uft_d64_v3_detect_protection(void* handle, char* name, size_t name_size,
                                    float* confidence) {
    float local = 0.0f;
    if (!confidence) confidence = &local;   /* callee always writes it */
    *confidence = 0.0f;
    if (!handle) return false;
    v3_handle_t* h = (v3_handle_t*)handle;
    return d64_detect_protection((struct d64_disk_v3*)h->disk_buffer, name, name_size, confidence);
}

char* uft_d64_v3_get_diagnosis(void* handle) {
    if (!handle) return NULL;
    
    /* Build diagnosis string from disk analysis */
    v3_handle_t* h = (v3_handle_t*)handle;
    if (!h->valid) return NULL;
    
    char *diag = malloc(1024);
    if (!diag) return NULL;
    
    int len = 0;
    len += snprintf(diag + len, 1024 - len, "D64 Disk Diagnosis:\n");
    
    /* Check track count */
    size_t disk_size = h->raw_size;
    int tracks = (disk_size <= 174848) ? 35 : (disk_size <= 196608) ? 40 : 35;
    len += snprintf(diag + len, 1024 - len, "  Tracks: %d\n", tracks);
    len += snprintf(diag + len, 1024 - len, "  Size: %zu bytes\n", disk_size);
    
    /* Check for error bytes */
    bool has_errors = (disk_size == 175531 || disk_size == 197376);
    len += snprintf(diag + len, 1024 - len, "  Error info: %s\n",
                    has_errors ? "present" : "none");
    
    /* Check BAM */
    const uint8_t *bam = (const uint8_t *)h->disk_buffer + 0x16500;  /* Track 18, sector 0 */
    len += snprintf(diag + len, 1024 - len, "  BAM track pointer: %d\n", bam[0]);
    len += snprintf(diag + len, 1024 - len, "  DOS version: %c\n", bam[2]);
    
    return diag;
}

bool uft_g64_v3_detect_protection(void* handle, char* name, size_t name_size,
                                    float* confidence) {
    float local = 0.0f;
    if (!confidence) confidence = &local;   /* callee always writes it */
    *confidence = 0.0f;
    if (!handle) return false;
    v3_handle_t* h = (v3_handle_t*)handle;
    return g64_detect_protection((struct g64_disk*)h->disk_buffer, name, name_size, confidence);
}

uint8_t* uft_g64_v3_export_d64(void* handle, size_t* out_size) {
    if (!handle || !out_size) return NULL;
    v3_handle_t* h = (v3_handle_t*)handle;
    /* false: a D64 without a trailing error map is the plain 174848-byte
     * form every tool reads. The caller can ask for the error variant
     * once this bridge grows an option for it. */
    return g64_export_d64((struct g64_disk*)h->disk_buffer, out_size, false);
}

bool uft_scp_v3_detect_protection(void* handle, char* name, size_t name_size,
                                    float* confidence) {
    float local = 0.0f;
    if (!confidence) confidence = &local;   /* callee always writes it */
    *confidence = 0.0f;
    if (!handle) return false;
    v3_handle_t* h = (v3_handle_t*)handle;
    return scp_detect_protection((struct scp_disk*)h->disk_buffer, name, name_size, confidence);
}

uint8_t* uft_d64_v3_write(void* handle, size_t* out_size) {
    if (!handle || !out_size) return NULL;
    v3_handle_t* h = (v3_handle_t*)handle;
    return d64_write((struct d64_disk_v3*)h->disk_buffer,
                     (struct d64_params*)h->params_buffer, out_size);
}

uint8_t* uft_g64_v3_write(void* handle, size_t* out_size) {
    if (!handle || !out_size) return NULL;
    v3_handle_t* h = (v3_handle_t*)handle;
    return g64_write((struct g64_disk*)h->disk_buffer,
                     (struct g64_params*)h->params_buffer, out_size);
}

uint8_t* uft_scp_v3_write(void* handle, size_t* out_size) {
    if (!handle || !out_size) return NULL;
    v3_handle_t* h = (v3_handle_t*)handle;
    return scp_write((struct scp_disk*)h->disk_buffer,
                     (struct scp_params*)h->params_buffer, out_size);
}
