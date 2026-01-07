#pragma once
/*
  uft_media_probe.h — kleine, sichere Probe-Helpers (PRG/P00 Strings + Keyword-Hits)

  Zweck:
  - PRG/P00 load address extrahieren (Host-seitig, ohne Emu)
  - ASCII-Strings aus dem Payload ziehen (sichtbar + '\n' für 0x0D)
  - Keyword-Hits zählen (Track/Sector/GCR/MFM/Protection/Verify/Error/...)

  Forensik:
  - Keine UB, keine Overflows, harte Buffer-Caps
  - Kein "Parsing" von CPU-Code, nur Oberflächen-Indikatoren
*/
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uft_probe_status {
    UFT_PROBE_OK = 0,
    UFT_PROBE_E_INVALID = 1,
    UFT_PROBE_E_TRUNC = 2,
    UFT_PROBE_E_BUF = 3
} uft_probe_status_t;

typedef enum uft_probe_kind {
    UFT_KIND_UNKNOWN = 0,
    UFT_KIND_PRG = 1,
    UFT_KIND_P00 = 2
} uft_probe_kind_t;

typedef struct uft_prg_view {
    uft_probe_kind_t kind;
    uint16_t load_address;
    const uint8_t *data;
    size_t data_len;
} uft_prg_view_t;

typedef struct uft_string_view {
    uint32_t offset;
    uint32_t length;
    const char *text; /* points into caller-provided text_buf */
} uft_string_view_t;

typedef struct uft_kw_hits {
    uint32_t track, sector, sync, gap, density, weak, bits;
    uint32_t gcr, mfm, crc, checksum, error, verify, format;
    uint32_t dev1541, bam, directory, dos;
    uint32_t copy, turbo, fast, protect;
} uft_kw_hits_t;

uft_probe_status_t uft_prg_view_from_blob(const uint8_t *blob, size_t blob_len, uft_prg_view_t *out);

uft_probe_status_t uft_extract_strings(const uint8_t *data, size_t data_len,
                                       uft_string_view_t *out, size_t max_out,
                                       char *text_buf, size_t text_cap,
                                       uint32_t min_visible_len,
                                       size_t *out_count, size_t *out_text_used);

void uft_score_keywords(const uft_string_view_t *strings, size_t count, uft_kw_hits_t *out_hits);

#ifdef __cplusplus
}
#endif
