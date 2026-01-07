#pragma once
/*
 * uft_capsimg.h — "brauchbarer" C-Header für CAPSImg/IPF-Decoder.
 *
 * Das Backend wird als C++ gebaut, aber diese Header sind C-kompatibel.
 * Ziel: stabile API-Schicht für UFT (GUI orchestriert nur; keine versteckte Logik).
 */
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Upstream Headers (unmodifiziert, nur über Wrapper eingebunden) */
#include "CommonTypes.h"
#include "CapsAPI.h"

/* ---------------------------
 * UFT Wrapper API (C)
 * ---------------------------
 * Minimaler, stabiler Einstiegspunkt, der typische IPF-Workflows abdeckt:
 *  - init/exit
 *  - image handle erzeugen
 *  - IPF via Datei oder Memory locken + laden
 *  - Track lock/unlock
 */
typedef struct uft_capsimg_ctx uft_capsimg_ctx_t;

typedef struct uft_capsimg_open_params {
    uint32_t load_flags;   /* CAPSLoadImage flags */
} uft_capsimg_open_params_t;

typedef struct uft_capsimg_track_request {
    uint32_t cylinder;     /* C */
    uint32_t head;         /* H */
    uint32_t type;         /* lock flags: z.B. DI_LOCK_* */
} uft_capsimg_track_request_t;

typedef struct uft_capsimg_track_data {
    /* kapselt CAPSTRACKINFO + CAPSDATAINFO */
    CAPSTRACKINFO track_info;
    CAPSDATAINFO  data_info;
} uft_capsimg_track_data_t;

int  uft_capsimg_init(void);
void uft_capsimg_exit(void);

uft_capsimg_ctx_t *uft_capsimg_open_file(const char *path, const uft_capsimg_open_params_t *opt);
uft_capsimg_ctx_t *uft_capsimg_open_memory(const uint8_t *buf, size_t len, uint32_t lock_flags, const uft_capsimg_open_params_t *opt);
void uft_capsimg_close(uft_capsimg_ctx_t *ctx);

int uft_capsimg_get_image_info(uft_capsimg_ctx_t *ctx, CAPSIMAGEINFO *out_info);

int uft_capsimg_lock_track(uft_capsimg_ctx_t *ctx, const uft_capsimg_track_request_t *req, uft_capsimg_track_data_t *out);
int uft_capsimg_unlock_track(uft_capsimg_ctx_t *ctx);

#ifdef __cplusplus
} /* extern "C" */
#endif
