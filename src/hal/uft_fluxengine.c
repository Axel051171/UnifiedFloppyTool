/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_fluxengine.c
 * @brief FluxEngine-Drahtprotokoll: Rahmenschicht (MF-857).
 *
 * Verhalten nach `davidgiven/fluxengine` Commit `909fac72`
 * (`protocol.h`, `lib/usb/fluxengineusb.cc`, `FluxEngine.cydsn/main.c`),
 * GPL-2.0-only
 * — eigenstaendige Umsetzung, kein Port. Begruendung und Grenzen im
 * Header.
 */
#include "uft/hal/uft_fluxengine.h"

#include <stdlib.h>
#include <string.h>

struct uft_fe_device {
    const uft_fe_stream_ops_t *ops;
    int timeout_ms;
};

const char *uft_fe_error_string(int err)
{
    switch (err) {
    case UFT_FE_OK:                 return "OK";
    case UFT_FE_ERR_INVALID:        return "ungueltiger Parameter";
    case UFT_FE_ERR_IO:             return "Ein-/Ausgabefehler";
    case UFT_FE_ERR_TIMEOUT:        return "Zeitlimit ueberschritten";
    case UFT_FE_ERR_PROTOCOL:       return "Protokollfehler "
                                           "(Antworttyp oder -laenge)";
    case UFT_FE_ERR_VERSION:        return "Protokollfassung wird nicht "
                                           "unterstuetzt";
    case UFT_FE_ERR_DEVICE:         return "Geraet meldet einen Fehler";
    case UFT_FE_ERR_NOMEM:          return "kein Speicher";
    case UFT_FE_ERR_NOT_CONNECTED:  return "Geraet nicht verbunden";
    default:                        return "unbekannter Fehler";
    }
}

/* ─── Lebenszyklus ──────────────────────────────────────────────────── */

int uft_fe_open_stream(const uft_fe_stream_ops_t *ops, uft_fe_device_t **out)
{
    if (!ops || !out || !ops->write || !ops->read) return UFT_FE_ERR_INVALID;
    uft_fe_device_t *d = (uft_fe_device_t *)calloc(1, sizeof *d);
    if (!d) return UFT_FE_ERR_NOMEM;
    d->ops = ops;
    d->timeout_ms = 5000;
    *out = d;
    return UFT_FE_OK;
}

void uft_fe_close(uft_fe_device_t *dev) { free(dev); }

void uft_fe_set_timeout(uft_fe_device_t *dev, int timeout_ms)
{
    if (dev && timeout_ms > 0) dev->timeout_ms = timeout_ms;
}

/* ─── Rahmenschicht ─────────────────────────────────────────────────── */

/*
 * Ein Kommando besteht aus [typ | groesse | nutzlast…], wobei `groesse`
 * die GESAMTLAENGE einschliesslich der zwei Kopfbytes ist — und samt
 * der Fuellbytes, die das Original ueber `sizeof` mitschickt.
 *
 * Hier wird Byte fuer Byte geschrieben, nicht ein Struct kopiert. Der
 * Unterschied ist nicht Geschmack: der Original-Client verlaesst sich
 * darauf, dass Host- und PSoC-Compiler identisch ausrichten, und
 * niemand hat die PSoC-Seite je gemessen. Wer die Versaetze
 * ausschreibt, kann sie pruefen.
 */
static int fe_send(uft_fe_device_t *dev, const uint8_t *frame, size_t len)
{
    if (!dev || !dev->ops) return UFT_FE_ERR_NOT_CONNECTED;
    if (len < 2u || len > UFT_FE_FRAME_SIZE) return UFT_FE_ERR_INVALID;
    return dev->ops->write(dev->ops->user, frame, len) == UFT_FE_OK
               ? UFT_FE_OK : UFT_FE_ERR_IO;
}

/**
 * Wartet auf einen Rahmen vom Typ @p erwartet.
 *
 * Drei Abweichungen von der Vorlage, alle im Header begruendet:
 *   * Debug-Rahmen werden ueberlesen (wie dort) — die Firmware sendet
 *     heute keine, die Behandlung ist Altbestand, aber harmlos.
 *   * `F_FRAME_ERROR` wird als Geraetefehler gemeldet statt geworfen.
 *   * Die LAENGE wird geprueft. Die Firmware tut das nicht; ein
 *     verkuerzter Rahmen ginge sonst als gueltig durch.
 */
static int fe_await(uft_fe_device_t *dev, uint8_t erwartet,
                    uint8_t *buf, size_t erwartete_laenge, size_t *out_len)
{
    if (!dev || !dev->ops) return UFT_FE_ERR_NOT_CONNECTED;

    for (int runde = 0; runde < 8; runde++) {
        uint8_t roh[UFT_FE_FRAME_SIZE];
        size_t da = 0;
        int r = dev->ops->read(dev->ops->user, roh, sizeof roh, &da,
                               dev->timeout_ms);
        if (r != UFT_FE_OK) return r;
        if (da < 2u) return UFT_FE_ERR_PROTOCOL;

        const uint8_t typ = roh[0];
        const uint8_t groesse = roh[1];

        if (typ == UFT_FE_F_DEBUG) continue;      /* ueberlesen */
        if (typ == UFT_FE_F_ERROR) return UFT_FE_ERR_DEVICE;
        if (typ != erwartet)       return UFT_FE_ERR_PROTOCOL;

        /* `send_reply()` schickt `f.size` Byte (main.c:191-196). Weniger
         * als angekuendigt heisst: der Rahmen ist unterwegs abgeschnitten
         * worden. */
        if (groesse < erwartete_laenge || da < erwartete_laenge)
            return UFT_FE_ERR_PROTOCOL;

        if (buf) memcpy(buf, roh, erwartete_laenge);
        if (out_len) *out_len = erwartete_laenge;
        return UFT_FE_OK;
    }
    /* Acht Debug-Rahmen hintereinander ohne Antwort: das ist kein
     * Geschwaetz mehr, das ist ein haengendes Geraet. */
    return UFT_FE_ERR_TIMEOUT;
}

/** Kommando ohne Nutzlast, Antwort ohne Nutzlast. */
static int fe_simple(uft_fe_device_t *dev, uint8_t cmd, uint8_t reply)
{
    uint8_t f[2] = { cmd, UFT_FE_SZ_ANY };
    int r = fe_send(dev, f, sizeof f);
    if (r != UFT_FE_OK) return r;
    return fe_await(dev, reply, NULL, UFT_FE_SZ_ANY, NULL);
}

/* ─── Kommandos ─────────────────────────────────────────────────────── */

bool uft_fe_version_supported(uint8_t version)
{
    return version == UFT_FE_PROTOCOL_VERSION;
}

int uft_fe_get_version(uft_fe_device_t *dev, uint8_t *out_version)
{
    uint8_t f[2] = { UFT_FE_F_GET_VERSION_CMD, UFT_FE_SZ_ANY };
    int r = fe_send(dev, f, sizeof f);
    if (r != UFT_FE_OK) return r;

    uint8_t a[UFT_FE_SZ_VERSION];
    r = fe_await(dev, UFT_FE_F_GET_VERSION_REPLY, a, sizeof a, NULL);
    if (r != UFT_FE_OK) return r;

    if (out_version) *out_version = a[2];   /* version @ 2 */
    return UFT_FE_OK;
}

int uft_fe_seek(uft_fe_device_t *dev, uint8_t track)
{
    uint8_t f[UFT_FE_SZ_SEEK] = { UFT_FE_F_SEEK_CMD, UFT_FE_SZ_SEEK, track };
    int r = fe_send(dev, f, sizeof f);
    if (r != UFT_FE_OK) return r;
    return fe_await(dev, UFT_FE_F_SEEK_REPLY, NULL, UFT_FE_SZ_ANY, NULL);
}

int uft_fe_recalibrate(uft_fe_device_t *dev)
{
    return fe_simple(dev, UFT_FE_F_RECALIBRATE_CMD, UFT_FE_F_RECALIBRATE_REPLY);
}

int uft_fe_set_drive(uft_fe_device_t *dev, uint8_t drive,
                     bool high_density, uint8_t index_mode)
{
    uint8_t f[UFT_FE_SZ_SET_DRIVE] = {
        UFT_FE_F_SET_DRIVE_CMD, UFT_FE_SZ_SET_DRIVE,
        drive, (uint8_t)(high_density ? 1u : 0u), index_mode
    };
    int r = fe_send(dev, f, sizeof f);
    if (r != UFT_FE_OK) return r;
    return fe_await(dev, UFT_FE_F_SET_DRIVE_REPLY, NULL, UFT_FE_SZ_ANY, NULL);
}

int uft_fe_measure_speed(uft_fe_device_t *dev, uint8_t hard_sector_count,
                         uint16_t *out_period_ms)
{
    uint8_t f[UFT_FE_SZ_MEASURESPEED] = {
        UFT_FE_F_MEASURE_SPEED_CMD, UFT_FE_SZ_MEASURESPEED, hard_sector_count
    };
    int r = fe_send(dev, f, sizeof f);
    if (r != UFT_FE_OK) return r;

    uint8_t a[UFT_FE_SZ_SPEED];
    r = fe_await(dev, UFT_FE_F_MEASURE_SPEED_REPLY, a, sizeof a, NULL);
    if (r != UFT_FE_OK) return r;

    /* period_ms uint16 @ 2, little-endian.
     *
     * Die Leitung ist LE — der Client wandelt `bytes_to_write` und die
     * Spannungswerte ausdruecklich („the board always operates in
     * little-endian mode", fluxengineusb.cc:11-16). An DIESER Stelle
     * wandelt er NICHT (fluxengineusb.cc:164) und liest den Wert roh:
     * auf einem Little-Endian-Host folgenlos, auf einem Big-Endian-Host
     * falsch. Wir lesen ihn immer als LE, unabhaengig vom Host. */
    if (out_period_ms)
        *out_period_ms = (uint16_t)((uint16_t)a[2] | ((uint16_t)a[3] << 8));
    return UFT_FE_OK;
}
