/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file tests/emulators/fluxengine/wire_state_machine.c
 * @brief FluxEngine-Firmware auf Drahtebene (MF-857).
 *
 * Verhalten nach `davidgiven/fluxengine` Commit `909fac72`,
 * `FluxEngine.cydsn/main.c` (`handle_command`, `send_reply`), GPL-2.0-only
 * — eigen-
 * staendige Umsetzung, kein Port. Begruendung und Grenzen im Header und
 * in `WIRE_DIVERGENCES.md`.
 */
#include "wire_state_machine.h"

#include "uft/hal/uft_fluxengine.h"

#include <string.h>

void fe_wire_reset(fe_wire_t *fw)
{
    if (!fw) return;
    memset(fw, 0, sizeof *fw);
    fw->state         = FE_WIRE_IDLE;
    fw->version       = UFT_FE_PROTOCOL_VERSION;   /* echt: 17 */
    fw->period_ms     = 200;                       /* 200 ms = 300 U/min */
    fw->disk_present  = true;
    fw->max_track     = 83;
    fw->current_track = -1;                        /* unbekannt bis Recal */
}

/** Antwort ohne Nutzlast. */
static void antwort(uint8_t *out, size_t *out_len, uint8_t typ)
{
    out[0] = typ;
    out[1] = UFT_FE_SZ_ANY;
    *out_len = UFT_FE_SZ_ANY;
}

bool fe_wire_handle(fe_wire_t *fw, const uint8_t *in, size_t in_len,
                    uint8_t *out, size_t *out_len)
{
    if (!fw || !in || !out || !out_len) return false;

    /* Die echte Firmware liest bis zu 64 Byte und verzweigt NUR ueber
     * `f->f.type` (main.c:820-875) — sie prueft weder Laenge noch das
     * `size`-Feld. Weniger als zwei Byte ist aber gar kein Rahmen; dann
     * schweigt sie, weil `type` nicht existiert. */
    if (in_len < 2u) return false;

    const uint8_t typ = in[0];
    fw->cmd_count++;
    fw->last_cmd = typ;

    /* Debug-Rahmen auf Anforderung des Tests. Die heutige Firmware
     * sendet nie welche (`print()` geht auf UART, main.c:127-138) — der
     * Client behandelt sie trotzdem, und diese Behandlung gehoert
     * geprueft. Siehe WIRE_DIVERGENCES.md FEW-3. */
    if (fw->debug_frames_pending > 0) {
        fw->debug_frames_pending--;
        out[0] = UFT_FE_F_DEBUG;
        out[1] = UFT_FE_SZ_DEBUG;
        memset(out + 2, 0, UFT_FE_SZ_DEBUG - 2u);
        memcpy(out + 2, "wire", 4);
        *out_len = UFT_FE_SZ_DEBUG;
        fw->cmd_count--;          /* das war keine Kommandoverarbeitung */
        return true;
    }

    switch (typ) {

    case UFT_FE_F_GET_VERSION_CMD:
        /* version_frame: header(2) + version(1) = 3 */
        out[0] = UFT_FE_F_GET_VERSION_REPLY;
        out[1] = UFT_FE_SZ_VERSION;
        out[2] = fw->version;
        *out_len = UFT_FE_SZ_VERSION;
        return true;

    case UFT_FE_F_SEEK_CMD: {
        if (in_len < UFT_FE_SZ_SEEK) { antwort(out, out_len, UFT_FE_F_ERROR); return true; }
        const uint8_t track = in[2];
        if (track > fw->max_track) {
            fw->state = FE_WIRE_ERROR;
            antwort(out, out_len, UFT_FE_F_ERROR);
            return true;
        }
        fw->current_track = track;
        fw->state = FE_WIRE_SEEKED;
        antwort(out, out_len, UFT_FE_F_SEEK_REPLY);
        return true;
    }

    case UFT_FE_F_RECALIBRATE_CMD:
        fw->current_track = 0;
        fw->state = FE_WIRE_SEEKED;
        antwort(out, out_len, UFT_FE_F_RECALIBRATE_REPLY);
        return true;

    case UFT_FE_F_SET_DRIVE_CMD:
        if (in_len < UFT_FE_SZ_SET_DRIVE) { antwort(out, out_len, UFT_FE_F_ERROR); return true; }
        fw->drive        = in[2];
        fw->high_density = in[3] != 0;
        fw->index_mode   = in[4];
        antwort(out, out_len, UFT_FE_F_SET_DRIVE_REPLY);
        return true;

    case UFT_FE_F_MEASURE_SPEED_CMD: {
        if (in_len < UFT_FE_SZ_MEASURESPEED) { antwort(out, out_len, UFT_FE_F_ERROR); return true; }
        if (!fw->disk_present) {
            /* Ohne Diskette gibt es keinen Indexpuls. Die echte Firmware
             * misst dann nichts — modelliert als Fehlerrahmen; siehe
             * WIRE_DIVERGENCES.md FEW-2. */
            antwort(out, out_len, UFT_FE_F_ERROR);
            return true;
        }
        /* speed_frame: header(2) + period_ms(2) @ 2, little-endian. */
        out[0] = UFT_FE_F_MEASURE_SPEED_REPLY;
        out[1] = UFT_FE_SZ_SPEED;
        out[2] = (uint8_t)(fw->period_ms & 0xFFu);
        out[3] = (uint8_t)(fw->period_ms >> 8);
        *out_len = UFT_FE_SZ_SPEED;
        return true;
    }

    default:
        /* Kein stilles OK. Ein Rahmen, den der Automat nicht kennt,
         * bekommt einen Fehlerrahmen UND wird gezaehlt — damit ein Test
         * die Luecke SEHEN kann, statt sie zu erben. Das ist der
         * Unterschied zu einer Handfolge: die schweigt ueber alles,
         * wonach nicht gefragt wurde. */
        fw->unknown_cmds++;
        antwort(out, out_len, UFT_FE_F_ERROR);
        return true;
    }
}
