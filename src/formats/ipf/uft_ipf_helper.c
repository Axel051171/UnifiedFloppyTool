/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_ipf_helper.c
 * @brief IPF ueber eine Prozessgrenze — UFTs Seite.
 *
 * Vertrag: `docs/specs/capsimg-helper/PROTOCOL.md` (Fassung 1).
 * Begruendung und Lizenzlage: `include/uft/formats/ipf/uft_ipf_helper.h`.
 * Gemessen: `tests/test_ipf_helper.c` (MF-917).
 *
 * Diese Datei enthaelt KEINE Kenntnis ueber `capsimg`. Sie kennt nur das
 * Protokoll oben, und das ist aus UFTs eigenem Bedarf geschnitten — aus
 * den Zugriffsfunktionen von `uft_ipf_air.h`, nicht aus fremden
 * Datenstrukturen. Das ist die Brandmauer aus QUARANTINE_PROCESS §5.
 */
#include "uft/formats/ipf/uft_ipf_helper.h"

#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Obergrenzen. Sie stehen hier, weil eine Antwort auch von einem
 * fehlerhaften oder feindseligen Programm kommen kann — die Grenze
 * zwischen zwei Prozessen ist eine Vertrauensgrenze. Die Werte sind
 * grosszuegig gegenueber echter Hardware und trotzdem endlich. */
#define HELPER_MAX_CYLS    255u
#define HELPER_MAX_HEADS     2u
#define HELPER_MAX_TRACKS  (HELPER_MAX_CYLS * HELPER_MAX_HEADS)
/* 1 MiB je Spur: eine 84/2-Amiga-Spur hat ~12,7 kB, eine HD-Spur unter
 * 32 kB. Wer mehr meldet, meldet keinen Spurinhalt mehr. */
#define HELPER_MAX_PAYLOAD (1024u * 1024u)

static void set_err(char *err, size_t cap, const char *fmt, ...)
{
    if (!err || cap == 0) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err, cap, fmt, ap);
    va_end(ap);
}

/* ─────────────────────────── Auffinden ──────────────────────────── */

const char *uft_ipf_helper_path(void)
{
    const char *p = getenv(UFT_IPF_HELPER_ENV);
    if (!p || p[0] == '\0') return NULL;
    return p;
}

/* ─────────────────────────── Zerlegen ───────────────────────────── */

/* Ein Wort ab *pp holen; *pp hinter das Wort setzen. NULL am Zeilenende. */
static const char *token(const char **pp, char *buf, size_t cap)
{
    const char *s = *pp;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '\0') { *pp = s; return NULL; }
    size_t n = 0;
    while (*s && *s != ' ' && *s != '\t') {
        if (n + 1 < cap) buf[n++] = *s;
        s++;
    }
    buf[n] = '\0';
    *pp = s;
    return buf;
}

/* Vorzeichenlose Zahl aus einem Wort. false = kein gueltiges Wort. */
static bool u64_of(const char **pp, uint64_t *out)
{
    char w[32];
    if (!token(pp, w, sizeof w)) return false;
    if (w[0] < '0' || w[0] > '9') return false;   /* kein '-', kein '+' */
    errno = 0;
    char *end = NULL;
    unsigned long long v = strtoull(w, &end, 10);
    if (errno != 0 || !end || *end != '\0') return false;
    *out = (uint64_t)v;
    return true;
}

uft_error_t uft_ipf_helper_parse(const char *text,
                                 uft_ipf_helper_reply_t *out,
                                 char *err, size_t errcap)
{
    if (!text || !out) {
        set_err(err, errcap, "interner Fehler: leere Eingabe");
        return UFT_ERR_INVALID_PARAM;
    }
    memset(out, 0, sizeof *out);

    bool     saw_version = false, saw_end = false;
    bool     saw_cyls = false, saw_heads = false;
    size_t   cap = 0;
    unsigned lineno = 0;

    const char *p = text;
    while (*p) {
        /* Zeile abgrenzen (ohne sie zu kopieren). */
        const char *eol = strchr(p, '\n');
        size_t len = eol ? (size_t)(eol - p) : strlen(p);
        if (len > 0 && p[len - 1] == '\r') len--;
        lineno++;

        char line[512];
        if (len >= sizeof line) {
            set_err(err, errcap, "Zeile %u ist laenger als %zu Zeichen",
                    lineno, sizeof line - 1);
            goto fail;
        }
        memcpy(line, p, len);
        line[len] = '\0';
        p = eol ? eol + 1 : p + strlen(p);

        const char *c = line;
        char kw[32];
        if (!token(&c, kw, sizeof kw)) continue;      /* Leerzeile */
        if (kw[0] == '#') continue;                   /* Kommentar */

        if (!saw_version) {
            /* Die Kennung MUSS zuerst kommen. Sonst koennte eine
             * Antwort, die mit TRACK beginnt, halb gelesen werden. */
            if (strcmp(kw, "UFT-IPF-HELPER") != 0) {
                set_err(err, errcap,
                        "Zeile %u: erwartet 'UFT-IPF-HELPER', gelesen '%s'",
                        lineno, kw);
                goto fail;
            }
            uint64_t v = 0;
            if (!u64_of(&c, &v)) {
                set_err(err, errcap, "Zeile %u: Protokollfassung fehlt", lineno);
                goto fail;
            }
            if (v != (uint64_t)UFT_IPF_HELPER_PROTOCOL) {
                /* Auch AUFWAERTS abweisen: eine v2-Antwort halb zu
                 * verstehen waere schlimmer, als sie abzulehnen. */
                set_err(err, errcap,
                        "Protokollfassung %" PRIu64 ", dieser Stand spricht %d",
                        v, UFT_IPF_HELPER_PROTOCOL);
                goto fail;
            }
            saw_version = true;
            continue;
        }

        if (saw_end) {
            set_err(err, errcap, "Zeile %u steht hinter 'END'", lineno);
            goto fail;
        }

        if (strcmp(kw, "END") == 0) { saw_end = true; continue; }

        if (strcmp(kw, "ERROR") == 0) {
            while (*c == ' ' || *c == '\t') c++;
            set_err(err, errcap, "Helfer meldet: %s", *c ? c : "(ohne Grund)");
            goto fail;
        }

        if (strcmp(kw, "BLOB") == 0) {
            while (*c == ' ' || *c == '\t') c++;
            if (*c == '\0' || strlen(c) >= sizeof out->blob_path) {
                set_err(err, errcap, "Zeile %u: BLOB-Pfad fehlt oder zu lang",
                        lineno);
                goto fail;
            }
            memcpy(out->blob_path, c, strlen(c) + 1);
            continue;
        }

        if (strcmp(kw, "CYLS") == 0 || strcmp(kw, "HEADS") == 0 ||
            strcmp(kw, "PLATFORM") == 0) {
            uint64_t v = 0;
            if (!u64_of(&c, &v)) {
                set_err(err, errcap, "Zeile %u: %s ohne Zahl", lineno, kw);
                goto fail;
            }
            if (kw[0] == 'C') {
                if (v < 1 || v > HELPER_MAX_CYLS) {
                    set_err(err, errcap, "CYLS %" PRIu64 " ausserhalb 1..%u",
                            v, HELPER_MAX_CYLS);
                    goto fail;
                }
                out->cylinders = (int)v; saw_cyls = true;
            } else if (kw[0] == 'H') {
                if (v < 1 || v > HELPER_MAX_HEADS) {
                    set_err(err, errcap, "HEADS %" PRIu64 " ausserhalb 1..%u",
                            v, HELPER_MAX_HEADS);
                    goto fail;
                }
                out->heads = (int)v; saw_heads = true;
            } else {
                out->platform = (uint32_t)(v & 0xFFFFFFFFu);
            }
            continue;
        }

        if (strcmp(kw, "TRACK") == 0) {
            uint64_t f[7];
            for (int i = 0; i < 7; i++) {
                if (!u64_of(&c, &f[i])) {
                    set_err(err, errcap,
                            "Zeile %u: TRACK braucht 7 Zahlen, %d gelesen",
                            lineno, i);
                    goto fail;
                }
            }
            if (f[0] >= HELPER_MAX_CYLS || f[1] >= HELPER_MAX_HEADS) {
                set_err(err, errcap,
                        "Zeile %u: Spur %" PRIu64 "/%" PRIu64 " ausserhalb",
                        lineno, f[0], f[1]);
                goto fail;
            }
            if (f[6] > HELPER_MAX_PAYLOAD) {
                set_err(err, errcap,
                        "Zeile %u: %" PRIu64 " Byte Nutzdaten, Grenze %u",
                        lineno, f[6], HELPER_MAX_PAYLOAD);
                goto fail;
            }
            if (f[6] > 0 && out->blob_path[0] == '\0') {
                /* Eine Zusage ohne Beilage ist genau die stille
                 * Falschaussage, die dieses Protokoll ausschliessen
                 * soll: der Aufrufer wuerde Nutzdaten erwarten. */
                set_err(err, errcap,
                        "Zeile %u nennt %" PRIu64 " Byte Nutzdaten, aber keine BLOB-Zeile steht davor",
                        lineno, f[6]);
                goto fail;
            }
            if (out->track_count == cap) {
                if (cap >= HELPER_MAX_TRACKS) {
                    set_err(err, errcap, "mehr als %u Spuren", HELPER_MAX_TRACKS);
                    goto fail;
                }
                size_t ncap = cap ? cap * 2 : 32;
                if (ncap > HELPER_MAX_TRACKS) ncap = HELPER_MAX_TRACKS;
                void *n = realloc(out->tracks, ncap * sizeof *out->tracks);
                if (!n) { set_err(err, errcap, "Speicher"); goto oom; }
                out->tracks = n; cap = ncap;
            }
            uft_ipf_helper_track_t *t = &out->tracks[out->track_count++];
            t->cyl      = (int)f[0];
            t->head     = (int)f[1];
            t->bits     = (uint32_t)(f[2] & 0xFFFFFFFFu);
            t->density  = (uint32_t)(f[3] & 0xFFFFFFFFu);
            t->flags    = (uint32_t)(f[4] & 0xFFFFFFFFu);
            t->fuzzy    = (t->flags & 0x01u) != 0;
            t->blob_off = f[5];
            t->blob_len = (uint32_t)f[6];
            continue;
        }

        set_err(err, errcap, "Zeile %u: unbekanntes Schluesselwort '%s'",
                lineno, kw);
        goto fail;
    }

    if (!saw_version) { set_err(err, errcap, "Antwort ist leer"); goto fail; }
    if (!saw_end)     { set_err(err, errcap, "Antwort endet ohne 'END' — abgeschnitten"); goto fail; }
    if (!saw_cyls || !saw_heads) {
        set_err(err, errcap, "Antwort ohne %s",
                !saw_cyls ? "CYLS" : "HEADS");
        goto fail;
    }
    return UFT_OK;

fail:
    uft_ipf_helper_reply_free(out);
    return UFT_ERR_FORMAT_INVALID;
oom:
    uft_ipf_helper_reply_free(out);
    return UFT_ERR_MEMORY;
}

/* ─────────────────────────── Abfragen ───────────────────────────── */

/* Liest die Index-Datei ganz ein. Bewusst eng begrenzt: der Index ist
 * Text mit hoechstens 510 TRACK-Zeilen. */
#define HELPER_MAX_INDEX (512u * 1024u)

static char *index_lesen(const char *pfad, char *err, size_t errcap)
{
    FILE *f = fopen(pfad, "rb");
    if (!f) { set_err(err, errcap, "Index '%s' nicht lesbar", pfad); return NULL; }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n < 0 || (unsigned long)n > HELPER_MAX_INDEX) {
        set_err(err, errcap, "Index '%s': %ld Byte, Grenze %u", pfad, n,
                HELPER_MAX_INDEX);
        fclose(f);
        return NULL;
    }
    rewind(f);
    char *buf = malloc((size_t)n + 1);
    if (!buf) { set_err(err, errcap, "Speicher"); fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = '\0';
    return buf;
}

uft_error_t uft_ipf_helper_query(const uft_ipf_helper_runner_t *runner,
                                 const char *ipf_path,
                                 const char *index_path,
                                 const char *blob_path,
                                 uft_ipf_helper_reply_t *out,
                                 char *err, size_t errcap)
{
    if (!runner || !runner->run || !ipf_path || !index_path ||
        !blob_path || !out) {
        set_err(err, errcap, "interner Fehler: unvollstaendiger Aufruf");
        return UFT_ERR_INVALID_PARAM;
    }
    memset(out, 0, sizeof *out);

    const char *helper = uft_ipf_helper_path();
    if (!helper) {
        /* DER Satz. Er steht hier und nur hier, damit GUI, Protokoll
         * und Test denselben lesen. */
        set_err(err, errcap,
                "IPF erkannt — Inhalt nicht lesbar. Helfer einrichten: "
                "%s=<pfad>", UFT_IPF_HELPER_ENV);
        return UFT_ERR_NOT_SUPPORTED;
    }

    int rc = runner->run(runner->ctx, helper, ipf_path, index_path, blob_path);
    if (rc < 0) {
        set_err(err, errcap, "Helfer '%s' liess sich nicht starten", helper);
        return UFT_ERROR_TOOL_FAILED;
    }
    if (rc != 0) {
        /* Rueckgabewert UND Ausgabe — der Rueckgabewert allein sagt
         * dem Benutzer nichts. Der Index kann eine ERROR-Zeile
         * enthalten; wenn nicht, sagen wir auch das. */
        char *t = index_lesen(index_path, NULL, 0);
        set_err(err, errcap, "Helfer '%s' brach ab (Rueckgabe %d): %.160s",
                helper, rc, (t && t[0]) ? t : "(kein Index geschrieben)");
        free(t);
        return UFT_ERROR_TOOL_FAILED;
    }

    char *text = index_lesen(index_path, err, errcap);
    if (!text) return UFT_ERR_FORMAT_INVALID;

    uft_error_t e = uft_ipf_helper_parse(text, out, err, errcap);
    free(text);
    return e;
}

/* ──────────────────── Der Laeufer fuer die Produktion ───────────────
 *
 * argv-Feld, keine Shell. Auf Windows quotet die CRT die Argumente
 * NICHT selbst (`_spawnv` haengt sie mit Leerzeichen aneinander) — ein
 * Pfad mit Leerzeichen zerfiele sonst in zwei Argumente. Also von Hand,
 * nach den Regeln der MSVCRT-Kommandozeile.
 */
#ifdef _WIN32
#include <process.h>

/* Ein Argument nach MSVCRT-Regeln einpacken. false = Puffer zu klein. */
static bool win_quote(const char *arg, char *dst, size_t cap)
{
    bool noetig = (arg[0] == '\0');
    for (const char *s = arg; *s && !noetig; s++)
        if (*s == ' ' || *s == '\t' || *s == '"') noetig = true;
    size_t n = 0;
    if (!noetig) {
        size_t l = strlen(arg);
        if (l + 1 > cap) return false;
        memcpy(dst, arg, l + 1);
        return true;
    }
    if (n + 1 >= cap) return false;
    dst[n++] = '"';
    for (const char *s = arg; *s; s++) {
        size_t rueck = 0;
        while (*s == '\\') { rueck++; s++; }
        if (*s == '\0') {                 /* Backslashes vor dem Endzeichen */
            for (size_t i = 0; i < rueck * 2; i++) {
                if (n + 1 >= cap) return false;
                dst[n++] = '\\';
            }
            break;
        }
        if (*s == '"') {                  /* Backslashes vor einem " */
            for (size_t i = 0; i < rueck * 2 + 1; i++) {
                if (n + 1 >= cap) return false;
                dst[n++] = '\\';
            }
        } else {
            for (size_t i = 0; i < rueck; i++) {
                if (n + 1 >= cap) return false;
                dst[n++] = '\\';
            }
        }
        if (n + 1 >= cap) return false;
        dst[n++] = *s;
    }
    if (n + 2 > cap) return false;
    dst[n++] = '"';
    dst[n] = '\0';
    return true;
}

static int system_run(void *ctx, const char *helper, const char *ipf,
                      const char *idx, const char *blob)
{
    (void)ctx;
    static char q[4][1024];
    const char *ein[4] = { helper, ipf, idx, blob };
    const char *argv[5];
    for (int i = 0; i < 4; i++) {
        if (!win_quote(ein[i], q[i], sizeof q[i])) return -1;
        argv[i] = q[i];
    }
    argv[4] = NULL;
    intptr_t rc = _spawnv(_P_WAIT, helper, (const char *const *)argv);
    return (rc == -1) ? -1 : (int)rc;
}
#else
#include <sys/wait.h>
#include <unistd.h>

static int system_run(void *ctx, const char *helper, const char *ipf,
                      const char *idx, const char *blob)
{
    (void)ctx;
    pid_t p = fork();
    if (p < 0) return -1;
    if (p == 0) {
        char *argv[5] = { (char *)helper, (char *)ipf, (char *)idx,
                          (char *)blob, NULL };
        execv(helper, argv);
        _exit(127);                       /* liess sich nicht starten */
    }
    int st = 0;
    if (waitpid(p, &st, 0) < 0) return -1;
    if (!WIFEXITED(st)) return -1;
    int code = WEXITSTATUS(st);
    return (code == 127) ? -1 : code;
}
#endif

const uft_ipf_helper_runner_t *uft_ipf_helper_system_runner(void)
{
    static const uft_ipf_helper_runner_t r = { system_run, NULL };
    return &r;
}

const uft_ipf_helper_track_t *
uft_ipf_helper_find(const uft_ipf_helper_reply_t *reply, int cyl, int head)
{
    if (!reply || !reply->tracks) return NULL;
    for (size_t i = 0; i < reply->track_count; i++) {
        if (reply->tracks[i].cyl == cyl && reply->tracks[i].head == head)
            return &reply->tracks[i];
    }
    return NULL;
}

uft_error_t uft_ipf_helper_payload(const uft_ipf_helper_reply_t *reply,
                                   const uft_ipf_helper_track_t *track,
                                   uint8_t **out_buf, size_t *out_len,
                                   char *err, size_t errcap)
{
    if (!reply || !track || !out_buf || !out_len) {
        set_err(err, errcap, "interner Fehler: unvollstaendiger Aufruf");
        return UFT_ERR_INVALID_PARAM;
    }
    *out_buf = NULL; *out_len = 0;
    if (track->blob_len == 0) return UFT_OK;    /* nichts zugesagt */

    FILE *f = fopen(reply->blob_path, "rb");
    if (!f) {
        set_err(err, errcap, "Beilage '%s' nicht lesbar", reply->blob_path);
        return UFT_ERR_FILE_OPEN;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        set_err(err, errcap, "Beilage '%s' nicht positionierbar", reply->blob_path);
        fclose(f);
        return UFT_ERR_FILE_READ;
    }
    long size = ftell(f);
    if (size < 0) {
        set_err(err, errcap, "Beilage '%s': Groesse unbekannt", reply->blob_path);
        fclose(f);
        return UFT_ERR_FILE_READ;
    }
    /* Zusage gegen Wirklichkeit halten, BEVOR belegt wird. Ein Helfer,
     * der 12 kB zusagt und 0 liefert, darf keinen Puffer voller
     * Restspeicher erzeugen. */
    if (track->blob_off > (uint64_t)size ||
        (uint64_t)track->blob_len > (uint64_t)size - track->blob_off) {
        set_err(err, errcap,
                "Spur %d/%d sagt %u Byte ab Versatz %" PRIu64 " zu, "
                "die Beilage hat %ld",
                track->cyl, track->head, track->blob_len, track->blob_off, size);
        fclose(f);
        return UFT_ERR_FILE_READ;
    }
    if (fseek(f, (long)track->blob_off, SEEK_SET) != 0) {
        set_err(err, errcap, "Beilage: Versatz %" PRIu64 " nicht erreichbar",
                track->blob_off);
        fclose(f);
        return UFT_ERR_FILE_READ;
    }
    uint8_t *buf = malloc(track->blob_len);
    if (!buf) { set_err(err, errcap, "Speicher"); fclose(f); return UFT_ERR_MEMORY; }
    size_t got = fread(buf, 1, track->blob_len, f);
    fclose(f);
    if (got != track->blob_len) {
        set_err(err, errcap, "Beilage: %zu von %u Byte gelesen", got, track->blob_len);
        free(buf);
        return UFT_ERR_FILE_READ;
    }
    *out_buf = buf;
    *out_len = track->blob_len;
    return UFT_OK;
}

void uft_ipf_helper_reply_free(uft_ipf_helper_reply_t *reply)
{
    if (!reply) return;
    free(reply->tracks);
    reply->tracks = NULL;
    reply->track_count = 0;
}
