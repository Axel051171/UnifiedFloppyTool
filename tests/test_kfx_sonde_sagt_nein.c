/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * test_kfx_sonde_sagt_nein.c — P3-171 / MF-919.
 *
 * ══════════════════════════════════════════════════════════════════════
 *  DER BEFUND
 * ══════════════════════════════════════════════════════════════════════
 *
 * Beim Bau des Rotbeweises zu P3-170 sollte eine Datei dienen, die KEIN
 * Plugin oeffnen kann: 4097 Byte pseudozufaellig. Gemessen ueber den
 * echten Registry-Pfad nahm KFX sie an und meldete
 * `1 Zylinder x 1 Kopf, 1 Sektor` (MF-893).
 *
 * Der Grund stand im Code: `kfx_probe()` ZAEHLT das Byte 0x0D. In 512
 * Zufallsbytes stehen erwartungsgemaess zwei davon — das genuegte.
 * MF-729 hat daraufhin die Konfidenzen gesenkt (80/40 -> 45/35), aber
 * die Sonde sagt weiterhin `true`. Eine gesenkte Zahl macht aus einem
 * Erkenner, der nie „nein" sagen kann, keinen Erkenner.
 *
 * Und dahinter lag der groessere Fall: `kfx_read_track()` uebergab die
 * ROHEN DATEIBYTES als „Sektor 0" (`uft_format_add_sector`). Ein
 * KryoFlux-Strom ist Fluss, kein Sektor. Das ist dieselbe Klasse wie
 * MF-883, nur andersherum: dort wurde ein Schreibvorgang gemeldet, der
 * nicht stattfand — hier ein Sektor, der nicht dekodiert wurde.
 *
 * ══════════════════════════════════════════════════════════════════════
 *  WORAUF DIE PRUEFSPUR BERUHT — ZWEI HAENDE, NICHT EINE
 * ══════════════════════════════════════════════════════════════════════
 *
 * Der gueltige Strom unten ist von Hand gebaut. Sein Aufbau ist NICHT
 * aus der Datei abgeschrieben, die ihn spaeter prueft, sondern zwischen
 * ZWEI unabhaengigen Umsetzungen im Baum abgeglichen:
 *
 *   A  src/formats/kryoflux/uft_kryoflux_checker.c
 *      („Inspiriert von sdstrowes/kryoflux-stream-checker", GPL-2+)
 *   B  src/a8rawconv/rawdiskkf.cpp
 *      (a8rawconv, Avery Lee, GPL-2-or-later, vendort)
 *
 * Sie stimmen Zeichen fuer Zeichen ueberein:
 *
 *   Opcode      A                          B
 *   0x00-0x07   FLUX2, 2 Byte              `c < 8` -> ein Byte nach
 *   0x08        NOP1                       `c == 8` -> nichts
 *   0x09        NOP2, +1                   `c == 9` -> ein Byte nach
 *   0x0A        NOP3, +2                   `c == 10` -> zwei nach
 *   0x0B        OVL16, +1                  `c == 11` -> t += 0x10000
 *   0x0C        FLUX3, +2                  `c == 12` -> Value16
 *   0x0D        OOB                        `c == 13` -> OOB
 *   0x0E-0xFF   FLUX1, 1 Byte              sonst
 *
 *   OOB: 0x0D, Typbyte, 16-Bit-Groesse (LE), Nutzlast
 *   Typ 2 = Index, Nutzlast 12 Byte, Stromposition bei Versatz 0 (LE32)
 *   Typ 3 = Stream End,  Typ 4 = KF-Info,  Typ 13 = EOF
 *
 * B nennt fuer Typ 2 ausdruecklich `if (oobLen != 12) fatal(...)` —
 * daher die 12 unten. Die Stromposition zaehlt nur Nicht-OOB-Bytes;
 * B sagt es woertlich: „we have to subtract the OOB block sizes".
 *
 * Baut auch einzeln:
 *   gcc -I include -o t tests/test_kfx_sonde_sagt_nein.c \
 *       src/formats/kfx/uft_kfx.c src/formats/kryoflux/uft_kryoflux_checker.c ...
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_track.h"   /* MF-996: uft_track_release() -- ohne
                              * diesen Kopf implizit deklariert; unter
                              * Clang und in C23 ist das ein Fehler. */

extern bool kfx_probe(const uint8_t *data, size_t size, size_t file_size,
                      int *confidence);
extern const uft_format_plugin_t uft_format_plugin_kfx;

static int  g_ok = 0;
static void ok(const char *n) { g_ok++; printf("  OK  %s\n", n); }

/* ─────────────────── Puffer, die niemand deuten kann ────────────── */

/* Fester Keim — Reproduzierbarkeit ist Pflicht (CLAUDE.md). */
static uint32_t rng_state;
static void rng_seed(uint32_t s) { rng_state = s ? s : 1u; }
static uint8_t rng_byte(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return (uint8_t)(rng_state & 0xFFu);
}

/* ──────────────────── Ein gueltiger KryoFlux-Strom ──────────────── */

static void put_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

/* Baut: 16 Flux1 | Index(pos=16) | 16 Flux1 | Index(pos=32) | StreamEnd
 * Rueckgabe: Laenge. Puffer muss >= 128 Byte sein. */
static size_t baue_strom(uint8_t *b)
{
    size_t n = 0;
    uint32_t pos = 0;

    for (int i = 0; i < 16; i++) { b[n++] = 0x40; pos++; }   /* Flux1 */

    b[n++] = 0x0D; b[n++] = 0x02; b[n++] = 12; b[n++] = 0;   /* Index */
    put_le32(b + n, pos);      /* Stromposition */
    put_le32(b + n + 4, 1000); /* Timer */
    put_le32(b + n + 8, 2000); /* Systemzeit */
    n += 12;

    for (int i = 0; i < 16; i++) { b[n++] = 0x40; pos++; }

    b[n++] = 0x0D; b[n++] = 0x02; b[n++] = 12; b[n++] = 0;   /* Index 2 */
    put_le32(b + n, pos);
    put_le32(b + n + 4, 2000);
    put_le32(b + n + 8, 4000);
    n += 12;

    b[n++] = 0x0D; b[n++] = 0x03; b[n++] = 8; b[n++] = 0;    /* StreamEnd */
    put_le32(b + n, pos);
    put_le32(b + n + 4, 0);    /* Ergebnis: ok */
    n += 8;

    return n;
}

/* ═════════════ 1. Rauschen wird abgewiesen ══════════════════════ */

static void t_rauschen(void)
{
    static uint8_t rausch[4097];
    rng_seed(0xC0FFEEu);
    for (size_t i = 0; i < sizeof rausch; i++) rausch[i] = rng_byte();

    int conf = -1;
    bool genommen = kfx_probe(rausch, sizeof rausch, sizeof rausch, &conf);

    /* DAS ist der Kern von P3-171: 4097 Byte, die nichts bedeuten,
     * duerfen kein Ergebnis erzeugen. */
    if (genommen) {
        printf("  FEHLER: 4097 Byte Rauschen angenommen, Konfidenz %d\n", conf);
        assert(0);
    }
    ok("4097 Byte Rauschen -> abgewiesen");
}

/* ═════════════ 2. Nullpuffer (MF-729 Eichung 1) ═════════════════ */

static void t_nullpuffer(void)
{
    static uint8_t null[4096];
    memset(null, 0, sizeof null);
    int conf = -1;
    bool genommen = kfx_probe(null, sizeof null, sizeof null, &conf);
    /* Ein Puffer aus lauter Nullen traegt keine Signatur. Nichts darf
     * hier >= 50 melden — MF-729 Eichung 1. */
    assert(!genommen || conf < 50);
    ok("Nullpuffer -> kein Struktur-Anspruch");
}

/* ═════════════ 3. Eichung: 500 Zufallspuffer ════════════════════ */

static void t_eichung_zufall(void)
{
    static uint8_t buf[2048];
    int angenommen = 0;
    const int LAEUFE = 500;

    rng_seed(0x1234ABCDu);
    for (int r = 0; r < LAEUFE; r++) {
        for (size_t i = 0; i < sizeof buf; i++) buf[i] = rng_byte();
        int conf = 0;
        if (kfx_probe(buf, sizeof buf, sizeof buf, &conf)) angenommen++;
    }

    /* MF-729: wer 50..79 beansprucht, muss >= 95 % zufaelliger Puffer
     * abweisen. Diese Sonde beansprucht das Band — also gilt die
     * Auflage. Gemessen wird die HARTE Zahl, nicht der Anspruch. */
    if (angenommen * 100 > LAEUFE * 5) {
        printf("  FEHLER: %d von %d Zufallspuffern angenommen (max 5 %%)\n",
               angenommen, LAEUFE);
        assert(0);
    }
    printf("       (%d von %d Zufallspuffern angenommen)\n", angenommen, LAEUFE);
    ok("Eichung: >= 95 % zufaelliger Puffer abgewiesen");
}

/* ═════════════ 4. Der gueltige Strom wird erkannt ═══════════════ */

static void t_gueltiger_strom(void)
{
    uint8_t b[256];
    memset(b, 0, sizeof b);
    size_t n = baue_strom(b);

    int conf = -1;
    bool genommen = kfx_probe(b, n, n, &conf);
    if (!genommen) {
        printf("  FEHLER: gueltiger Strom (%zu Byte) abgewiesen\n", n);
        assert(0);
    }
    /* Struktur GELESEN heisst Band 50..79 (MF-729). Nicht mehr: einen
     * Signaturtreffer gibt es bei KryoFlux-Stroemen nicht — sie haben
     * keine Kennung am Dateianfang. */
    if (conf < 50 || conf > 79) {
        printf("  FEHLER: Konfidenz %d ausserhalb 50..79\n", conf);
        assert(0);
    }
    ok("gueltiger Strom -> erkannt, Konfidenz im Struktur-Band");
}

/* ═════════════ 5. open() weist Rauschen ab ══════════════════════ */

static void t_open_weist_ab(void)
{
    static uint8_t rausch[4097];
    rng_seed(0xC0FFEEu);
    for (size_t i = 0; i < sizeof rausch; i++) rausch[i] = rng_byte();

    const char *pfad = "test_kfx_rauschen.raw";
    FILE *f = fopen(pfad, "wb");
    assert(f != NULL);
    assert(fwrite(rausch, 1, sizeof rausch, f) == sizeof rausch);
    fclose(f);

    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    uft_error_t e = uft_format_plugin_kfx.open(&disk, pfad, true);
    if (e == UFT_OK) {
        printf("  FEHLER: open() nahm 4097 Byte Rauschen an "
               "(%d Zyl x %d Kopf, %u Sektoren)\n",
               disk.geometry.cylinders, disk.geometry.heads,
               disk.geometry.total_sectors);
        uft_format_plugin_kfx.close(&disk);
        remove(pfad);
        assert(0);
    }
    remove(pfad);
    ok("open() auf Rauschen -> Fehler, kein Ergebnis");
}

/* ═════════════ 6. Kein erfundener Sektor ════════════════════════ */

static void t_kein_erfundener_sektor(void)
{
    uint8_t b[256];
    memset(b, 0, sizeof b);
    size_t n = baue_strom(b);

    const char *pfad = "test_kfx_gueltig.raw";
    FILE *f = fopen(pfad, "wb");
    assert(f != NULL);
    assert(fwrite(b, 1, n, f) == n);
    fclose(f);

    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    assert(uft_format_plugin_kfx.open(&disk, pfad, true) == UFT_OK);

    /* Die Geometrie darf keine Sektoren behaupten: aus einem
     * KryoFlux-Strom hat dieses Plugin noch keinen dekodiert. */
    if (disk.geometry.total_sectors != 0) {
        printf("  FEHLER: Geometrie meldet %u Sektoren, dekodiert sind 0\n",
               disk.geometry.total_sectors);
        uft_format_plugin_kfx.close(&disk);
        remove(pfad);
        assert(0);
    }

    uft_track_t track;
    memset(&track, 0, sizeof track);
    assert(uft_format_plugin_kfx.read_track(&disk, 0, 0, &track) == UFT_OK);

    if (track.sector_count != 0) {
        printf("  FEHLER: %u erfundene Sektoren aus einem Flussstrom\n",
               (unsigned)track.sector_count);
        uft_track_release(&track);
        uft_format_plugin_kfx.close(&disk);
        remove(pfad);
        assert(0);
    }
    /* Der Fluss selbst ist aber DA — abgewiesen wird die Erfindung,
     * nicht der Inhalt. */
    assert(track.raw_data != NULL);
    assert(track.raw_size == n);
    assert(memcmp(track.raw_data, b, n) == 0);

    uft_track_release(&track);
    uft_format_plugin_kfx.close(&disk);
    remove(pfad);
    ok("gueltiger Strom -> 0 Sektoren, Fluss byteidentisch erhalten");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("test_kfx_sonde_sagt_nein — P3-171 (MF-919)\n");
    t_rauschen();
    t_nullpuffer();
    t_eichung_zufall();
    t_gueltiger_strom();
    t_open_weist_ab();
    t_kein_erfundener_sektor();
    printf("%d Pruefungen gruen\n", g_ok);
    return 0;
}
