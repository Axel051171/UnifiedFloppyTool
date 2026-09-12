/**
 * @file test_libdsk_erzeugnisse.c
 * @brief Fuenf Container von fremder Hand, aus EINEM Traeger (MF-1063)
 *
 * `apridisk`, `cqm`, `td0`, `dc42` und `cfi` standen alle auf **T2**:
 * die Leser sind gepruefte Arbeit — `apridisk` gegen MAMEs `load()`
 * (MF-1009), `cfi` gegen `src/samdisk/cfi.cpp` (MF-1004) —, aber im
 * Korpus lag fuer keines ein Abbild, das eine fremde Hand erzeugt hat.
 *
 * ── Wie sie alle fuenf auf einmal hereinkamen ───────────────────────────
 *
 * `docs/ERZEUGER_ZENSUS.md` (MF-1062) beantwortet die Frage, die eine
 * Hebung entscheidet — **wer SCHREIBT dieses Format** —, und wies
 * `apridisk`, `cfi` und `dc42` als klar zugeordnete Kandidaten aus.
 * `cqm` und `td0` standen in seinem ausgewiesenen **blinden Fleck**:
 * libdsks Typnamen `copyqm` und `tele` haben mit keiner Dateiendung zu
 * tun, also sah die abgeleitete Zuordnung sie nicht. Genau dafuer
 * fuehrt der Zensus diesen Abschnitt.
 *
 * Der Kanal ist derselbe wie MF-1060/1061, nur mit libdsk statt hxcfe:
 * ein **IMD-Traeger**, der seine Geometrie selbst mitbringt.
 *
 *     dsktrans -itype imd -otype <typ> <traeger.imd> <ziel>
 *
 * Ein Traeger, fuenf Ziele — 80 x 2 x 9 x 512, Sektornummern ab 1,
 * jeder Sektor selbstbenennend (MF-1020) **bis auf den ersten**, und
 * der Grund dafuer ist ein Befund:
 *
 * ── Warum Sektor 1 ein echter Bootsektor ist ────────────────────────────
 *
 * Der erste Entwurf dieses Fixtures war durchgehend selbstbenennend.
 * Vier Plugins gingen durch, `cfi` nicht: **`probe` = 0, waehrend
 * `open` alle 1440 Sektoren richtig las** — die Gestalt von MF-961,
 * MF-1022 und MF-1036, wo Sonde und `open` sich widersprachen.
 *
 * Hier ist es keiner. `uft_cfi_probe()` hat keine Kennung zu pruefen —
 * CFI hat keine — und arbeitet deshalb **strukturell**: es entpackt die
 * erste Spur und verlangt eine gueltige **BPB** (`parse_bpb`: Sprungbyte
 * 0xEB/0xE9/0x00, Bytes je Sektor bei 11, Gesamtsektoren bei 19,
 * Sektoren je Spur bei 24, Koepfe bei 26). MF-1004 hat das ausdruecklich
 * so entschieden: *„Das ist ein schaerferer Filter als eine
 * Groessenschwelle, und er bleibt."*
 *
 * Ein Abbild ohne Dateisystem kann diese Sonde also nicht ansprechen.
 * Das ist kein Fehler, sondern eine **benannte Grenze**: CFI wird nur
 * auf DOS-formatierten Disketten erkannt.
 *
 * Die Antwort darauf ist, das Fixture realistischer zu machen statt die
 * Zusage weicher — Sektor 1 der Spur 0/0 traegt jetzt einen echten
 * PC-720K-Bootsektor (1440 Sektoren, 9 je Spur, 2 Koepfe, Medienbyte
 * 0xF9, Bootkennung 0x55AA). Die uebrigen **1439** benennen sich weiter
 * selbst, und der Test prueft beides getrennt.
 *
 * Der IMD-Schreiber ist
 * eigenstaendig in Python nach Dave Dunfields oeffentlicher
 * Beschreibung, ausdruecklich NICHT ueber UFTs `uft_imd.c` — sonst
 * waere die Kette ein geschlossener Kreis (MF-1009 `apridisk`,
 * MF-1028 `qrst`). Bei `apridisk` waere das besonders bitter gewesen:
 * dort WAR der Rundlauftest gruen, weil Packer und Entpacker
 * Spiegelbilder derselben Erfindung waren.
 *
 * ── Was libdsk dabei selbst gesetzt hat, gemessen ───────────────────────
 *
 *     apridisk  760 552 B  (+23 272)  Kopf „ACT Apricot "
 *     cqm       737 311 B  (+    31)  Kennung „CQ"
 *     td0       750 488 B  (+13 208)  Kennung „TD"
 *     dc42      737 364 B  (+    84)  Pascal-String-Diskettenname
 *     cfi       737 442 B  (+   162)  gepackter Strom
 *
 * Je 1439 von 1439 Mustertreffern, kein Fuellbyte — die Falle aus
 * MF-1021 greift bei keinem.
 *
 * **Die Ueberhaenge sind verschieden, und das ist der Punkt.** Fuenf
 * Container, fuenf eigene Aufbauten, fuenf unabhaengige Entscheidungen
 * einer fremden Hand darueber, wo ein Sektor liegt. Ein flaches Abbild
 * haette das nicht geleistet: bei `akai_s900` hat sich gezeigt, dass
 * ein Rundlauf durch ein flaches Format **layout-unabhaengig**
 * byteidentisch ist und als Beleg nichts taugt (MF-1061).
 *
 * ── Was NICHT geprueft ist ──────────────────────────────────────────────
 *
 * * **Die Packung.** libdsk schreibt hier mit seiner eigenen Wahl;
 *   TD0s Huffman-Variante und CQMs RLE-Grenzfaelle sind damit nicht
 *   abgedeckt.
 * * **Andere Geometrien.** Ein Traeger, eine Geometrie (PC 720K).
 * * **Der Schreibpfad.** Hier wird nur gelesen.
 * * **`dc42`s Pruefsummen.** Der Kopf traegt zwei; ob UFT sie prueft,
 *   sagt dieser Test nicht.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_track.h"   /* uft_track_release — implizite Deklaration
                              * ist auf macOS-Clang ein FEHLER */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR ""
#endif

extern const uft_format_plugin_t uft_format_plugin_apridisk;
extern const uft_format_plugin_t uft_format_plugin_cqm;
extern const uft_format_plugin_t uft_format_plugin_td0;
extern const uft_format_plugin_t uft_format_plugin_dc42;
extern const uft_format_plugin_t uft_format_plugin_cfi;

#define ZYL       80u
#define KOEPFE     2u
#define SPT        9u
#define SGR      512u
#define SEKTOREN (ZYL * KOEPFE * SPT)      /* 1440 */
#define NAMENSLAENGE 17u

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int bedingung, const char *detail)
{
    if (bedingung) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  FAIL %s - %s\n", was, detail ? detail : ""); }
}

static void soll_inhalt(unsigned c, unsigned h, unsigned s, uint8_t *b)
{
    char k[24];
    unsigned i;
    snprintf(k, sizeof k, "UFT-K C%02u H%u S%02u ", c, h, s);
    memcpy(b, k, NAMENSLAENGE);
    for (i = NAMENSLAENGE; i < SGR; i++)
        b[i] = (uint8_t)((c * 37u + h * 101u + s * 7u
                          + (i - NAMENSLAENGE) * 3u) & 0xFFu);
}

/* Ruft die Sonde GENAU so, wie der Produktionspfad es tut
 * (`src/core/uft_format_plugin.c:365-366`): hoechstens
 * UFT_PROBE_BUFFER_SIZE Byte im Puffer, die WIRKLICHE Dateigroesse als
 * drittes Argument. Bei MF-1061 hat ein zu kleiner Puffer im Test wie
 * ein Befund ausgesehen und war keiner. */
static int sonde(const uft_format_plugin_t *p, const char *pfad,
                 long gr, int *konf)
{
    size_t pgr = ((size_t)gr < (size_t)UFT_PROBE_BUFFER_SIZE)
                     ? (size_t)gr : (size_t)UFT_PROBE_BUFFER_SIZE;
    uint8_t *puffer = (uint8_t *)malloc(pgr);
    FILE *f;
    int ok;
    if (!puffer) return -1;
    f = fopen(pfad, "rb");
    if (!f || fread(puffer, 1, pgr, f) != pgr) {
        if (f) fclose(f);
        free(puffer);
        return -1;
    }
    fclose(f);
    *konf = -1;
    ok = p->probe(puffer, pgr, (size_t)gr, konf) ? 1 : 0;
    free(puffer);
    return ok;
}

static void durchlauf(const uft_format_plugin_t *p, const char *endung,
                      const char *wer, long soll_gr)
{
    uft_disk_t disk;
    char pfad[600], det[320], erster[220];
    uint8_t soll[SGR];
    FILE *f;
    long gr;
    unsigned c, s, gesehen = 0, gleich = 0, falsch = 0;
    int h, konf, ok, boot_ok = 0;

    printf("\n  -- %s --\n", wer);
    snprintf(pfad, sizeof pfad, "%s/libdsk_uftk_pc720.%s",
             UFT_CORPUS_DIR, endung);
    f = fopen(pfad, "rb");
    if (!f) {
        snprintf(det, sizeof det, "%s fehlt", pfad);
        pruefe("das libdsk-Erzeugnis liegt im Korpus", 0, det);
        return;
    }
    fseek(f, 0, SEEK_END);
    gr = ftell(f);
    fclose(f);

    snprintf(det, sizeof det, "%ld Byte, erwartet %ld", gr, soll_gr);
    pruefe("die Dateigroesse ist die gemessene", gr == soll_gr, det);

    ok = sonde(p, pfad, gr, &konf);
    snprintf(det, sizeof det, "probe=%d konf=%d", ok, konf);
    pruefe("die Sonde nimmt das Fremderzeugnis auf dem Produktionspfad an",
           ok == 1, det);

    memset(&disk, 0, sizeof disk);
    if (p->open(&disk, pfad, true) != UFT_OK) {
        pruefe("open liest das Fremderzeugnis", 0, pfad);
        return;
    }

    snprintf(det, sizeof det, "%d x %d x %d x %d",
             disk.geometry.cylinders, disk.geometry.heads,
             disk.geometry.sectors, disk.geometry.sector_size);
    pruefe("Geometrie 80 x 2 x 9 x 512 aus dem fremden Container",
           disk.geometry.cylinders == (int)ZYL
           && disk.geometry.heads == (int)KOEPFE
           && disk.geometry.sectors == (int)SPT
           && disk.geometry.sector_size == (int)SGR, det);

    erster[0] = 0;
    for (c = 0; c < ZYL; c++) {
        for (h = 0; h < (int)KOEPFE; h++) {
            uft_track_t t;
            memset(&t, 0, sizeof t);
            if (p->read_track(&disk, (int)c, h, &t) != UFT_OK) {
                falsch += SPT;
                if (!erster[0])
                    snprintf(erster, sizeof erster,
                             "Spur %u/%d nicht lesbar", c, h);
                continue;
            }
            for (s = 0; s < t.sector_count; s++) {
                const uft_sector_t *sec = &t.sectors[s];
                size_t len = sec->data_len ? sec->data_len : sec->data_size;
                /* Sektor 1 der Spur 0/0 traegt den Bootsektor, nicht
                 * seinen Namen — siehe den Dateikopf. Er wird getrennt
                 * geprueft, damit sein Fehlen hier kein stiller
                 * Freispruch ist. */
                if (c == 0 && h == 0 && s == 0) {
                    const uint8_t *d = sec->data;
                    boot_ok = (d && len == SGR
                               && d[0] == 0xEB
                               && d[11] == 0x00 && d[12] == 0x02
                               && d[19] == 0xA0 && d[20] == 0x05
                               && d[24] == 9 && d[26] == 2
                               && d[510] == 0x55 && d[511] == 0xAA);
                    continue;
                }
                gesehen++;
                soll_inhalt(c, (unsigned)h, s, soll);
                if (sec->data && len == SGR
                    && memcmp(sec->data, soll, SGR) == 0) gleich++;
                else {
                    falsch++;
                    if (!erster[0])
                        snprintf(erster, sizeof erster,
                                 "Spur %u/%d Sektor %u traegt '%.17s'",
                                 c, h, s,
                                 sec->data ? (const char *)sec->data
                                           : "(NULL)");
                }
            }
            uft_track_release(&t);
        }
    }
    p->close(&disk);

    pruefe("Sektor 1 der Spur 0/0 traegt den PC-720K-Bootsektor "
           "unveraendert durch den fremden Container", boot_ok,
           "BPB nicht wiedergefunden");

    snprintf(det, sizeof det, "%u gesehen, %u gleich, %u falsch%s%s",
             gesehen, gleich, falsch, erster[0] ? " - " : "", erster);
    pruefe("1439 von 1439 selbstbenennenden Sektoren byteidentisch, "
           "jeder an der Stelle, die der fremde Container ihm zuweist",
           gesehen == SEKTOREN - 1u && gleich == gesehen && falsch == 0,
           det);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Fuenf libdsk-Container aus EINEM IMD-Traeger - MF-1063\n");
    printf("======================================================\n");

    if (UFT_CORPUS_DIR[0] == '\0') {
        printf("SKIP: UFT_CORPUS_DIR nicht gesetzt - ohne Korpus prueft "
               "dieser Test nichts.\n");
        return 77;      /* SKIP_RETURN_CODE, seit MF-598 */
    }

    durchlauf(&uft_format_plugin_apridisk, "apridisk", "apridisk", 760552L);
    durchlauf(&uft_format_plugin_cqm,      "cqm",      "cqm",      737311L);
    durchlauf(&uft_format_plugin_td0,      "td0",      "td0",      750488L);
    durchlauf(&uft_format_plugin_dc42,     "dc42",     "dc42",     737364L);
    durchlauf(&uft_format_plugin_cfi,      "cfi",      "cfi",      737442L);

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
