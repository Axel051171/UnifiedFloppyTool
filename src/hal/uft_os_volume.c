/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_os_volume.c
 * @brief Umsetzung von uft_os_volume.h.
 *
 * Grundsatz: jeder Sektor wird einzeln gelesen und einzeln bewertet. Das ist
 * langsamer als ein Blocklesen und genau deshalb richtig — ein Blocklesen
 * ueber 737.280 Byte kann nicht sagen, WELCHER Sektor gefehlt hat.
 */

/* pread/pwrite brauchen POSIX.1-2001; -std=c11 schaltet _POSIX_C_SOURCE sonst ab. */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#  define _POSIX_C_SOURCE 200809L
#endif

#include "uft/hal/uft_os_volume.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#  include <windows.h>
#  include <winioctl.h>
   typedef HANDLE uft_osvol_fd_t;
#  define UFT_OSVOL_BAD_FD INVALID_HANDLE_VALUE
#else
#  include <fcntl.h>
#  include <unistd.h>
   typedef int uft_osvol_fd_t;
#  define UFT_OSVOL_BAD_FD (-1)
#endif

struct uft_osvol {
    uft_osvol_fd_t        fd;
    uft_osvol_geometry_t  geo;
    uint8_t               retries;
    bool                  write_enable;
    bool                  locked;
};

/* ───────────────────────────── Geometrie ────────────────────────────────── */

bool uft_osvol_lba_to_chs(const uft_osvol_geometry_t *g, uint32_t lba,
                          unsigned sector_base, uft_osvol_chs_t *out) {
    if (!g || !out || g->heads == 0u || g->sectors == 0u) return false;
    if (lba >= uft_osvol_total_sectors(g)) return false;

    const uint32_t per_cyl = (uint32_t)g->heads * g->sectors;

    out->cyl    = (uint16_t)(lba / per_cyl);
    const uint32_t rem = lba % per_cyl;
    out->head   = (uint8_t)(rem / g->sectors);
    out->sector = (uint8_t)((rem % g->sectors) + sector_base);
    return true;
}

/* ───────────────────────────── Oeffnen ─────────────────────────────────── */

static int32_t last_os_error(void) {
#if defined(_WIN32)
    return (int32_t)GetLastError();
#else
    return (int32_t)errno;
#endif
}

uft_osvol_t *uft_osvol_open(const uft_osvol_open_params_t *p,
                            int32_t *os_error) {
    if (os_error) *os_error = 0;
    if (!p || !p->path) return NULL;
    if (p->geo.sector_size == 0u || p->geo.heads == 0u || p->geo.sectors == 0u)
        return NULL;

    uft_osvol_t *v = calloc(1, sizeof(*v));
    if (!v) return NULL;

    v->geo          = p->geo;
    v->retries      = p->retries ? p->retries : 1u;
    v->write_enable = p->write_enable;

#if defined(_WIN32)
    DWORD access = GENERIC_READ | (p->write_enable ? GENERIC_WRITE : 0u);
    DWORD flags  = p->no_buffering ? FILE_FLAG_NO_BUFFERING : 0u;

    v->fd = CreateFileA(p->path, access,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_EXISTING, flags, NULL);
    if (v->fd == UFT_OSVOL_BAD_FD) {
        if (os_error) *os_error = last_os_error();
        free(v);
        return NULL;
    }

    /* Sperren, damit das Dateisystem nicht dazwischenschreibt. Das Orakel
     * macht das auf 9x per DOS-IOCTL 440D/084B; auf NT ist FSCTL_LOCK_VOLUME
     * der richtige Weg. Ein Fehlschlag ist KEIN Abbruchgrund — auf einem
     * unformatierten oder fremdformatierten Medium hat Windows das Volume
     * ohnehin nicht gemountet. */
    if (p->lock_volume) {
        DWORD ret = 0;
        v->locked = DeviceIoControl(v->fd, FSCTL_LOCK_VOLUME,
                                    NULL, 0, NULL, 0, &ret, NULL) != 0;
    }
#else
    int oflags = (p->write_enable ? O_RDWR : O_RDONLY);
#  ifdef O_DIRECT
    if (p->no_buffering) oflags |= O_DIRECT;
#  endif
    v->fd = open(p->path, oflags);
    if (v->fd == UFT_OSVOL_BAD_FD) {
        if (os_error) *os_error = last_os_error();
        free(v);
        return NULL;
    }
    (void)p->lock_volume;   /* unter Linux uebernimmt das der Exklusivzugriff */
#endif

    return v;
}

void uft_osvol_close(uft_osvol_t *v) {
    if (!v) return;
#if defined(_WIN32)
    if (v->locked) {
        DWORD ret = 0;
        DeviceIoControl(v->fd, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0,
                        &ret, NULL);
    }
    if (v->fd != UFT_OSVOL_BAD_FD) CloseHandle(v->fd);
#else
    if (v->fd != UFT_OSVOL_BAD_FD) close(v->fd);
#endif
    free(v);
}

/* ───────────────────────── Ein Sektor, ein Versuch ─────────────────────── */

static bool xfer_one(uft_osvol_t *v, uint32_t lba, void *buf, bool writing,
                     int32_t *os_error) {
    const uint64_t off = (uint64_t)lba * v->geo.sector_size;
    const uint32_t n   = v->geo.sector_size;

#if defined(_WIN32)
    LARGE_INTEGER li; li.QuadPart = (LONGLONG)off;
    if (!SetFilePointerEx(v->fd, li, NULL, FILE_BEGIN)) {
        *os_error = last_os_error(); return false;
    }
    DWORD done = 0;
    BOOL ok = writing
        ? WriteFile(v->fd, buf, n, &done, NULL)
        : ReadFile (v->fd, buf, n, &done, NULL);
    if (!ok || done != n) { *os_error = last_os_error(); return false; }
    return true;
#else
    ssize_t done = writing
        ? pwrite(v->fd, buf, n, (off_t)off)
        : pread (v->fd, buf, n, (off_t)off);
    if (done != (ssize_t)n) { *os_error = last_os_error(); return false; }
    return true;
#endif
}

static uint32_t xfer_range(uft_osvol_t *v, uint32_t lba, uint32_t count,
                           uint8_t *buf, bool writing,
                           uft_osvol_sec_status_t *status,
                           uft_osvol_progress_fn progress, void *user) {
    if (!v || !buf || !status) return 0u;
    if (writing && !v->write_enable) return 0u;

    const uint32_t total = uft_osvol_total_sectors(&v->geo);
    uint32_t good = 0u;

    for (uint32_t i = 0; i < count; ++i) {
        uft_osvol_sec_status_t *st = &status[i];
        memset(st, 0, sizeof(*st));

        if (lba + i >= total) {
            st->state = UFT_OSVOL_SEC_BAD;
            st->os_error = -1;      /* ausserhalb der Geometrie */
            continue;
        }

        uint8_t *p = buf + (size_t)i * v->geo.sector_size;
        int32_t err = 0;
        bool ok = false;

        for (uint8_t a = 1; a <= v->retries; ++a) {
            st->attempts = a;
            if (xfer_one(v, lba + i, p, writing, &err)) { ok = true; break; }
        }

        if (ok) {
            st->state = (st->attempts > 1u) ? UFT_OSVOL_SEC_OK_RETRY
                                            : UFT_OSVOL_SEC_OK;
            good++;
        } else {
            st->state = UFT_OSVOL_SEC_BAD;
            st->os_error = err;
            /* KEIN Abbruch: ein schlechter Sektor darf die restlichen 719
             * nicht verhindern. Genau das kann das Blocklesen des Orakels
             * nicht leisten. */
        }

        if (progress && !progress(lba + i, count, st, user)) {
            for (uint32_t k = i + 1u; k < count; ++k) {
                memset(&status[k], 0, sizeof(status[k]));
                status[k].state = UFT_OSVOL_SEC_SKIPPED;
            }
            break;
        }
    }
    return good;
}

uint32_t uft_osvol_read(uft_osvol_t *v, uint32_t lba, uint32_t count,
                        uint8_t *buf, uft_osvol_sec_status_t *status,
                        uft_osvol_progress_fn progress, void *user) {
    return xfer_range(v, lba, count, buf, false, status, progress, user);
}

uint32_t uft_osvol_write(uft_osvol_t *v, uint32_t lba, uint32_t count,
                         const uint8_t *buf, uft_osvol_sec_status_t *status,
                         uft_osvol_progress_fn progress, void *user) {
    return xfer_range(v, lba, count, (uint8_t *)buf, true, status,
                      progress, user);
}

bool uft_osvol_query_geometry(uft_osvol_t *v, uft_osvol_geometry_t *out) {
    if (!v || !out) return false;
#if defined(_WIN32)
    DISK_GEOMETRY dg;
    DWORD ret = 0;
    if (!DeviceIoControl(v->fd, IOCTL_DISK_GET_DRIVE_GEOMETRY,
                         NULL, 0, &dg, sizeof(dg), &ret, NULL))
        return false;
    out->cylinders   = (uint16_t)dg.Cylinders.QuadPart;
    out->heads       = (uint8_t) dg.TracksPerCylinder;
    out->sectors     = (uint8_t) dg.SectorsPerTrack;
    out->sector_size = (uint16_t)dg.BytesPerSector;
    out->double_step = false;
    return true;
#else
    (void)v; (void)out;
    return false;   /* FDGETPRM waere der Linux-Weg; bewusst offen gelassen */
#endif
}

/* ───────────────────────────── Zusammenfassung ─────────────────────────── */

size_t uft_osvol_status_summary(const uft_osvol_sec_status_t *status,
                                uint32_t count, char *buf, size_t buflen) {
    if (!status || !buf || buflen == 0u) return 0u;

    uint32_t ok = 0, retry = 0, bad = 0, skipped = 0, unread = 0;
    uint32_t first_bad = 0xFFFFFFFFu;
    uint32_t max_attempts = 0;

    for (uint32_t i = 0; i < count; ++i) {
        switch (status[i].state) {
        case UFT_OSVOL_SEC_OK:       ok++;      break;
        case UFT_OSVOL_SEC_OK_RETRY: retry++;   break;
        case UFT_OSVOL_SEC_BAD:
            bad++;
            if (first_bad == 0xFFFFFFFFu) first_bad = i;
            break;
        case UFT_OSVOL_SEC_SKIPPED:  skipped++; break;
        default:                     unread++;  break;
        }
        if (status[i].attempts > max_attempts) max_attempts = status[i].attempts;
    }

    size_t n = 0;
    int r = snprintf(buf, buflen,
        "Sektoren: %u gut, %u nach Wiederholung, %u defekt, %u uebersprungen, "
        "%u unversucht\n"
        "Maximale Versuche an einem Sektor: %u\n",
        ok, retry, bad, skipped, unread, max_attempts);
    if (r > 0) n = (size_t)r;
    if (n >= buflen) return buflen - 1u;

    if (bad > 0u && n < buflen) {
        r = snprintf(buf + n, buflen - n,
            "Erster defekter Sektor: %u (OS-Fehler %d)\n",
            first_bad, status[first_bad].os_error);
        if (r > 0) n += (size_t)r;
        if (n >= buflen) n = buflen - 1u;
    }
    if (retry > 0u && bad == 0u && n < buflen) {
        r = snprintf(buf + n, buflen - n,
            "Hinweis: das Medium liefert nur mit Wiederholungen — "
            "Alterung, weitere Abzuege bald anlegen.\n");
        if (r > 0) n += (size_t)r;
        if (n >= buflen) n = buflen - 1u;
    }
    return n;
}
