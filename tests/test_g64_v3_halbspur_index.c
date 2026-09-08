/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * test_g64_v3_halbspur_index.c — P3-36 / MF-923.
 *
 * ══════════════════════════════════════════════════════════════════════
 *  DER BEFUND, UND WARUM ER SICH SCHWARZ AUF WEISS MESSEN LAESST
 * ══════════════════════════════════════════════════════════════════════
 *
 * P3-36 (gemeldet vom Eigentuemer, Fundstellen nachgemessen MF-811)
 * nennt fuer `src/formats/g64/uft_g64_parser_v3.c` drei Fehler auf
 * einmal. Der teuerste ist die INDIZIERUNG:
 *
 *   Leseschleife (`:1286`, `:1300`)  fuellt die Indizes 0..83
 *   Verbraucher  (`:1331`, `:1336`)  liest `[half_track]` mit 1..84
 *
 * Ob das ein Fehler ist, entscheidet nicht die Auslegung, sondern die
 * Datei. Gemessen an `tests/corpus_free/vice_c1541_35trk.g64` — von
 * **VICE** erzeugt, also fremder Hand:
 *
 *   Kopf Byte 9      = 84   (Halbspuren, nicht Vollspuren)
 *   Eintrag  0       -> Spurdaten bei 684     <- Spur 1.0
 *   Eintrag  1       -> 0                     <- Spur 1.5, leer
 *   Eintrag  2       -> Spurdaten bei 8614    <- Spur 2.0
 *   Speed-Eintrag 0  = 3                      <- Zone 3, richtig fuer Spur 1
 *
 * **Dateieintrag 0 IST Spur 1.0.** `g64_full_to_half(1)` liefert aber
 * 1, und der Verbraucher liest damit Eintrag 1 — den LEEREN
 * Halbspur-Slot. Jede Vollspur wird aus dem falschen Fach gelesen.
 *
 * ── Wie dieser Test das ohne Blick in die Struktur belegt ───────────
 *
 * `struct g64_disk` ist dateilokal. Der Test macht es deshalb wie die
 * Bruecke (`src/formats/uft_v3_bridge.c`): ein undurchsichtiger Puffer,
 * und gemessen wird am ERGEBNIS. `g64_export_d64()` gibt eine D64
 * heraus — und daneben liegt `vice_c1541_35trk.d64`, von derselben
 * Hand, dieselbe Diskette:
 *
 *   Diskname "UFTCORPUS", ID "42", DOS-Typ "2A", BAM in Block 357
 *
 * Kommt die Indizierung aus dem richtigen Fach, steht das im
 * ausgegebenen Abbild. Kommt sie aus dem falschen, steht dort nichts.
 * Kein Blick in ein privates Feld, keine nachgebaute Struktur.
 *
 * ── Was dieser Test ausdruecklich NICHT belegt ──────────────────────
 *
 * Der v3-Parser ist heute **unerreichbar**: seine Einstiege werden nur
 * von `uft_v3_bridge.c` gerufen, und deren Exporte
 * (`uft_g64_v3_*`) haben ausserhalb ihrer eigenen Datei keinen
 * Aufrufer (gemessen MF-920, P3-193). **Kein Benutzer trifft diesen
 * Fehler heute.** Der Test misst einen Vertrag, keinen Vorfall — und
 * genau darum ist er jetzt billig: er haelt den Fehler fest, BEVOR die
 * Bruecke verdrahtet wird.
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Wie in der Bruecke: undurchsichtig, gross genug. */
#define V3_PARAMS_BUFFER_SIZE  (4096)

struct g64_disk;
struct g64_params;

extern bool     g64_parse(const uint8_t *data, size_t size,
                          struct g64_disk *disk, struct g64_params *params);
extern void     g64_disk_free(struct g64_disk *disk);
extern void     g64_get_default_params(struct g64_params *params);
extern uint8_t *g64_export_d64(const struct g64_disk *disk, size_t *out_size,
                               bool include_errors);
/* MF-923: die Groesse kommt vom Eigentuemer des Typs. Vorher stand
 * hier — wie in der Bruecke — ein festes 256-KiB-Feld, und der
 * Testlauf endete mit STATUS_HEAP_CORRUPTION. */
extern size_t   g64_disk_sizeof(void);
extern size_t   d64_disk_v3_sizeof(void);
extern size_t   scp_disk_sizeof(void);

/* Spur 18, Sektor 0 — Spuren 1..17 haben je 21 Sektoren. */
#define BAM_BLOCK   357
#define BAM_OFFSET  (BAM_BLOCK * 256)

static int  g_ok = 0;
static void ok(const char *n) { g_ok++; printf("  OK  %s\n", n); }

static uint8_t *lies(const char *name, size_t *len)
{
    char pfad[512];
    snprintf(pfad, sizeof pfad, "%s/%s", UFT_CORPUS_DIR, name);
    FILE *f = fopen(pfad, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    uint8_t *b = malloc((size_t)n);
    if (!b) { fclose(f); return NULL; }
    *len = fread(b, 1, (size_t)n, f);
    fclose(f);
    return b;
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* ═══════ 1. Die Grundwahrheit steht in der Datei, nicht in UFT ════ */

static void t_grundwahrheit(void)
{
    size_t n = 0;
    uint8_t *g = lies("vice_c1541_35trk.g64", &n);
    if (!g) { printf("  SKIP (Korpus fehlt)\n"); exit(77); }
    assert(n > 400);

    assert(memcmp(g, "GCR-1541", 8) == 0);
    /* Byte 9 zaehlt HALBSPUREN. 84 = 42 Vollspuren. */
    assert(g[9] == 84);

    /* Eintrag 0 traegt Daten, Eintrag 1 ist der leere Halbspur-Slot. */
    uint32_t e0 = le32(g + 12);
    uint32_t e1 = le32(g + 16);
    uint32_t e2 = le32(g + 20);
    if (e0 == 0 || e1 != 0 || e2 == 0) {
        printf("  FEHLER: Korpusdatei nicht wie erwartet aufgebaut "
               "(e0=%u e1=%u e2=%u) — dann misst dieser Test nichts\n",
               e0, e1, e2);
        assert(0);
    }
    /* Speed-Tabelle ab 12 + 84*4 = 348; Eintrag 0 = Zone 3 (Spur 1). */
    assert(le32(g + 348) == 3);

    free(g);
    ok("VICE-G64: Eintrag 0 ist Spur 1.0, Speed 0 ist Zone 3 (Grundwahrheit)");
}

/* ═══════ 2. Der Puffer der Bruecke war zu klein ═══════════ */

static void t_bruecken_puffer(void)
{
    /* `src/formats/uft_v3_bridge.c` hielt alle drei v3-Strukturen in
     * EINEM Feld `uint8_t disk_buffer[256 * 1024]`. Gemessen passt
     * keine hinein:
     *
     *     g64_disk_t     1 440 256 Byte    5,5x
     *     d64_disk_v3_t    600 664 Byte    2,3x
     *     scp_disk_t     3 119 512 Byte   11,9x
     *
     * Jeder Parse schrieb Megabytes hinter das Feldende. Der erste
     * Lauf dieses Tests endete deshalb reproduzierbar mit
     * STATUS_HEAP_CORRUPTION (0xC0000374) — mit demselben festen
     * Puffer, den die Bruecke benutzte.
     *
     * Seit MF-923 wird die Groesse ABGEFRAGT statt gepflegt. Diese
     * Pruefung nagelt fest, dass die alte Konstante nicht gereicht
     * haette: waere sie es wieder, ist die Klasse zurueck. */
    size_t g = g64_disk_sizeof();
    size_t d = d64_disk_v3_sizeof();
    size_t s = scp_disk_sizeof();
    printf("       (g64 %zu, d64 %zu, scp %zu Byte gegen 262144)\n", g, d, s);

    if (g <= 262144 || d <= 262144 || s <= 262144) {
        printf("  HINWEIS: eine der drei Strukturen passt jetzt in 256 KiB "
               "— dann hat sich etwas Grundlegendes geaendert und diese "
               "Pruefung ist nachzuziehen\n");
        assert(0);
    }
    ok("alle drei v3-Strukturen sprengen den alten 256-KiB-Puffer");
}

/* ═══════ 3. Die Spuren kommen aus dem richtigen Fach ══════ */

static void t_spuren_aus_dem_richtigen_fach(void)
{
    size_t gn = 0;
    uint8_t *g = lies("vice_c1541_35trk.g64", &gn);
    assert(g != NULL);

    void *disk   = calloc(1, g64_disk_sizeof());
    void *params = calloc(1, V3_PARAMS_BUFFER_SIZE);
    assert(disk && params);
    g64_get_default_params((struct g64_params *)params);
    assert(g64_parse(g, gn, (struct g64_disk *)disk,
                     (struct g64_params *)params));

    size_t out = 0;
    uint8_t *d64 = g64_export_d64((struct g64_disk *)disk, &out, false);
    assert(d64 != NULL && out == 174848);

    size_t nz = 0;
    for (size_t i = 0; i < out; i++) if (d64[i]) nz++;

    /* ── DER KERN ────────────────────────────────────────────────────
     *
     * VOR MF-923 stand hier **0**. Jede Vollspur wurde aus dem leeren
     * Halbspur-Fach gelesen (`[half_track]` statt `[half_track - 1]`),
     * und heraus kam ein D64 aus lauter Nullen — aus einer voellig
     * gewoehnlichen, von VICE erzeugten Datei.
     *
     * Dass es NICHT null ist, belegt die Indizierung. Mehr belegt es
     * ausdruecklich nicht: der Inhalt ist damit noch nicht richtig
     * (siehe unten). Eine Zusicherung darf nur sagen, was sie misst. */
    if (nz == 0) {
        printf("  FEHLER: der Export ist vollstaendig leer — die "
               "Halbspur-Indizierung liest wieder aus dem falschen "
               "Fach (P3-36)\n");
        assert(0);
    }
    printf("       (%zu Nicht-Null-Bytes; vor MF-923: 0)\n", nz);

    free(d64);
    g64_disk_free((struct g64_disk *)disk);
    free(disk); free(params); free(g);
    ok("VICE-G64 -> D64 traegt Inhalt (vorher: 0 Byte)");
}

/* ═══════ 4. Und was DAMIT noch nicht behoben ist ════════ */

static void t_dekoder_ist_weiter_falsch(void)
{
    /* Ehrlichkeit vor Vollstaendigkeit: die Indizierung ist behoben,
     * der SEKTORDEKODER des v3-Parsers ist es nicht.
     *
     * Gemessen an derselben Datei:
     *
     *   v3-Parser        558 von 683 Bloecken tragen Inhalt,
     *                    "UFTG64" steht NIRGENDS im Ergebnis
     *   Plugin-Pfad      35 Spuren, 683 Sektoren, BAM-Name "UFTG64"
     *                    (uft_g64.c + uft_cbm_d64_decode_via_plugin)
     *
     * Der Baum hat also ZWEI G64-Leser, und nur einer liest richtig.
     * Eine frisch formatierte 1541-Diskette hat EINEN belegten Block,
     * nicht 558. Als P3-196 gefuehrt.
     *
     * Diese Pruefung nagelt den Zustand fest, damit er nicht still
     * driftet — sie faellt mit einer Anleitung, sobald jemand P3-196
     * anfasst. */
    size_t gn = 0;
    uint8_t *g = lies("vice_c1541_35trk.g64", &gn);
    assert(g != NULL);

    void *disk   = calloc(1, g64_disk_sizeof());
    void *params = calloc(1, V3_PARAMS_BUFFER_SIZE);
    g64_get_default_params((struct g64_params *)params);
    assert(g64_parse(g, gn, (struct g64_disk *)disk,
                     (struct g64_params *)params));

    size_t out = 0;
    uint8_t *d64 = g64_export_d64((struct g64_disk *)disk, &out, false);
    assert(d64 != NULL);

    bool gefunden = false;
    for (size_t i = 0; i + 6 <= out; i++)
        if (memcmp(d64 + i, "UFTG64", 6) == 0) { gefunden = true; break; }

    if (gefunden) {
        printf("  HINWEIS: der v3-Export traegt jetzt den Diskettennamen "
               "'UFTG64' — dann ist P3-196 behoben. Gut; diese Pruefung "
               "auf einen Inhaltsvergleich gegen den Plugin-Pfad "
               "hochziehen.\n");
        assert(0);
    }

    free(d64);
    g64_disk_free((struct g64_disk *)disk);
    free(disk); free(params); free(g);
    ok("v3-Sektordekoder weiter falsch — festgenagelt als P3-196");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("test_g64_v3_halbspur_index — P3-36 (MF-923)\n");
    t_grundwahrheit();
    t_bruecken_puffer();
    t_spuren_aus_dem_richtigen_fach();
    t_dekoder_ist_weiter_falsch();
    printf("%d Pruefungen gruen\n", g_ok);
    return 0;
}
