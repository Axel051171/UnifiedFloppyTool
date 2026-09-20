/**
 * @file test_td0_gepackt.c
 * @brief TD0 mit Advanced Compression — das Plugin las sie als Muell (MF-1284).
 *
 * ── Der Befund ───────────────────────────────────────────────────────
 *
 * `src/formats/td0/uft_td0.c` setzte in `td0_open()`
 *
 *     pdata->compressed = (magic == TD0_MAGIC_ADVANCED);
 *
 * und benutzte das Feld danach **nirgends**. Gelesen wurde mit blankem
 * `fread` — bei Magie `td` also der Huffman-Strom selbst, Byte fuer Byte
 * als Spur- und Sektorkoepfe gedeutet.
 *
 * Gemessen am Vorzustand, beide Dateien mit Magie `td`:
 *
 *   Transylvania.td0      angesagt **188 Zylinder x 192 Sektoren**,
 *                         376 Spuren „gelesen", **0 Sektoren**, UFT_OK
 *   sector_test_360k.td0  angesagt 1 Zylinder x 0 Sektoren,
 *                         2 Spuren „gelesen", **0 Sektoren**, UFT_OK
 *
 * Kein Fehlerkode, keine Warnung. Das ist „Erfolg ohne Tat" — die Klasse
 * aus MF-883/MF-930/MF-1009 —, verschaerft dadurch, dass die angesagte
 * Geometrie (188 Zylinder fuer eine 41-Spur-Diskette) frei erfunden ist.
 *
 * ── Die benannte Referenz ────────────────────────────────────────────
 *
 * Orakel ist **hxcfe 2.16.15.2** (Klon 05b53aa,
 * `tools/uft-scout/work/HxCFloppyEmulator/build/hxcfe.exe`, AUSGEFUEHRT,
 * keine Zeile uebernommen — Kanal *Oracle* nach MF-695). Es liest
 * dieselben Dateien:
 *
 *   Transylvania.td0      41 tracks, 2 side(s), je Spur „sectors: 1..9"
 *   sector_test_360k.td0  40 tracks, 2 side(s)
 *   libdsk_uftk_pc720.td0 80 tracks, 2 side(s), je Spur „sectors: 1..9"
 *
 * **Eine zweite fremde Hand widerspricht, und das gehoert hierher statt
 * in eine Behauptung von Einstimmigkeit:** libdsks `dsktrans` (LGPL-2+)
 * entpackt Transylvania ebenfalls, meldet dabei aber **40** Zylinder und
 * 720 Sektoren — es klemmt auf seine Standardgeometrie und verliert die
 * 41. Spur. Transylvania ist ein Spiel von 1982; eine Spur jenseits der
 * Standardgeometrie ist die gelaeufige Schutzform. hxcfe und UFT sagen
 * 41, libdsk sagt 40 — festgehalten, nicht aufgeloest.
 *
 * ── Was hier NICHT belegt ist ────────────────────────────────────────
 *
 * Die Gruppen 2, 3 und 4a **ueberspringen sich benannt**, weil im Baum
 * keine weitergabefaehige gepackte TD0 liegt. Der Grund ist gemessen und
 * kein Versaeumnis: **es gibt keine fremde Hand, die gepackte TD0
 * SCHREIBT.** libdsks `lib/drvtele.c:39` sagt es woertlich — „Advanced
 * compression is read-only" —, und hxcfes Modulliste fuehrt ueberhaupt
 * kein TD0-Schreibmodul. Siehe `docs/OPEN_ITEMS.md` P3-520.
 *
 * **Was IMMER laeuft — auch in CI —, ist mehr als nur Gruppe 1:**
 * 1  die unkomprimierte Korpusdatei, 1440 Sektoren (Regressionsprobe),
 * 4b dieselbe Datei auf 52 Byte gekuerzt, wo ihr Kommentarkopf 56
 *    ansagt — der Leser muss absagen,
 * 4c dieselbe Datei auf 20 000 Byte, mitten in einem Datensatz — 39
 *    Sektoren, nicht 40.
 * 4b und 4c sind nachtraeglich entstanden, weil die Mutationsmatrix
 * gezeigt hat, dass die beiden Schranken sonst NICHT ausloesbar waren.
 * Eine Pruefung, die nicht feuern kann, ist keine (MF-1000/Tor 64).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_error.h"
#include "uft/formats/uft_td0.h"   /* MF-1285: uft_td0_strom_t */
#include "uft/formats/uft_imd.h"   /* MF-1287: der Wandler */

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR fehlt — tests/CMakeLists.txt muss es fuer diesen Test setzen"
#endif
#ifndef UFT_CORPUS_RESTRICTED_DIR
#error "UFT_CORPUS_RESTRICTED_DIR fehlt — tests/CMakeLists.txt muss es setzen"
#endif

extern uft_disk_t  *uft_disk_open(const char *path, bool read_only);
extern void         uft_disk_close(void *disk);
extern uft_error_t  uft_register_all_formats(void);
extern const uft_format_plugin_t uft_format_plugin_td0;

/* Kein SKIP_RETURN_CODE: Gruppe 1 laeuft IMMER und leistet echte
 * Arbeit. Ein Test, der sich als Ganzes ueberspringt, weil zwei von
 * vier Gruppen ihre Datei nicht finden, verschweigt die Regression,
 * die er bewacht. */
#define OFFEN   UFT_CORPUS_DIR            "/libdsk_uftk_pc720.td0"
#define GEPACKT UFT_CORPUS_RESTRICTED_DIR "/fluxfox_sector_test_360k.td0"
#define SCHUTZ  UFT_CORPUS_RESTRICTED_DIR "/fluxfox_transylvania.td0"

/* Der Kommentar der unkomprimierten Korpusdatei, aus der Datei GEMESSEN
 * (56 Byte, ohne Abschluss-NUL — TD0 laengt ihn, es ist keine Zeichenkette). */
#define KOM_TEXT "UFT-K Traegerabbild, selbstbenennende Sektoren (MF-1020)"
#define KOM_LEN  56u

static int fehler = 0;
#define PRUEFE(bed, text) do { \
    if (!(bed)) { printf("    [ROT] %s (Zeile %d)\n", (text), __LINE__); fehler++; } \
} while (0)

/** Was eine TD0 ueber den Produktionspfad hergibt. */
typedef struct {
    int    offen;          /**< uft_disk_open lieferte einen Griff */
    unsigned zylinder;
    unsigned koepfe;
    size_t sektoren;       /**< ueber alle Spuren aufsummiert */
    size_t spuren_mit_sek; /**< Spuren, die ueberhaupt Sektoren lieferten */
    int    nummern_1_bis_9;/**< jede Spur mit Sektoren traegt genau 1..9 */
    uint8_t erste[3];      /**< Spur 0/0, Sektor 1, erste drei Byte */
    int    erste_crc_ok;   /**< Spur 0/0, Sektor 1: crc_ok */
    int    erste_deleted;  /**< Spur 0/0, Sektor 1: deleted */
} befund_t;

static befund_t lies(const char *pfad)
{
    befund_t b;
    memset(&b, 0, sizeof(b));
    b.nummern_1_bis_9 = 1;

    uft_disk_t *d = uft_disk_open(pfad, true);
    if (!d) return b;
    b.offen = 1;
    b.zylinder = (unsigned)d->geometry.cylinders;
    b.koepfe   = (unsigned)d->geometry.heads;

    for (unsigned c = 0; c < b.zylinder; c++) {
        for (unsigned h = 0; h < b.koepfe; h++) {
            uft_track_t t;
            memset(&t, 0, sizeof(t));
            if (uft_format_plugin_td0.read_track(d, (int)c, (int)h, &t) != UFT_OK)
                continue;
            if (t.sector_count > 0) b.spuren_mit_sek++;
            b.sektoren += t.sector_count;

            /* `uft_format_add_sector()` nimmt einen 0-basierten Index und
             * addiert 1 — die TD0-Nummern 1..9 kommen also als 1..9
             * zurueck, wenn die Spur vollstaendig ist. */
            if (t.sector_count == 9) {
                for (size_t s = 0; s < 9; s++)
                    if (t.sectors[s].id.sector != (int)(s + 1))
                        b.nummern_1_bis_9 = 0;
            } else if (t.sector_count > 0) {
                b.nummern_1_bis_9 = 0;
            }

            if (c == 0 && h == 0 && t.sector_count > 0) {
                if (t.sectors[0].data && t.sectors[0].data_len >= 3)
                    memcpy(b.erste, t.sectors[0].data, 3);
                b.erste_crc_ok  = t.sectors[0].crc_ok ? 1 : 0;
                b.erste_deleted = t.sectors[0].deleted ? 1 : 0;
            }

            uft_track_cleanup(&t);
        }
    }
    uft_disk_close(d);
    return b;
}

static int vorhanden(const char *pfad)
{
    FILE *f = fopen(pfad, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

/* ═══ 1. Die unkomprimierte Korpusdatei — Regressionsprobe ══════════
 *
 * Sie lief vor MF-1284 richtig und muss es danach tun. Ohne diese
 * Gruppe waere der Umbau eine Wette: der Strompuffer ersetzt NEUN
 * `fread`/`fseek`-Stellen auf einmal.                                 */
static void gruppe_1_unkomprimiert(void)
{
    printf("  [1] unkomprimiert (Magie \"TD\"), Orakel hxcfe: 80 Spuren, 2 Seiten\n");
    befund_t b = lies(OFFEN);
    PRUEFE(b.offen, "uft_disk_open lieferte keinen Griff");
    PRUEFE(b.zylinder == 80, "nicht 80 Zylinder");
    PRUEFE(b.koepfe == 2, "nicht 2 Koepfe");
    PRUEFE(b.sektoren == 1440, "nicht 1440 Sektoren");
    PRUEFE(b.spuren_mit_sek == 160, "nicht 160 Spuren mit Sektoren");
    PRUEFE(b.nummern_1_bis_9, "Sektornummern nicht durchgehend 1..9");
    printf("      %u x %u, %zu Sektoren auf %zu Spuren\n",
           b.zylinder, b.koepfe, b.sektoren, b.spuren_mit_sek);
}

/* ═══ 2. Gepackt, ohne Schutzspur ═══════════════════════════════════ */
static void gruppe_2_gepackt(void)
{
    printf("  [2] gepackt (Magie \"td\"), Orakel hxcfe: 40 Spuren, 2 Seiten\n");
    if (!vorhanden(GEPACKT)) { printf("      uebersprungen — Korpusdatei fehlt\n"); return; }
    befund_t b = lies(GEPACKT);
    PRUEFE(b.offen, "uft_disk_open lieferte keinen Griff");
    PRUEFE(b.zylinder == 40, "nicht 40 Zylinder");
    PRUEFE(b.koepfe == 2, "nicht 2 Koepfe");
    PRUEFE(b.sektoren == 720, "nicht 720 Sektoren");
    PRUEFE(b.spuren_mit_sek == 80, "nicht 80 Spuren mit Sektoren");
    PRUEFE(b.nummern_1_bis_9, "Sektornummern nicht durchgehend 1..9");
    printf("      %u x %u, %zu Sektoren auf %zu Spuren\n",
           b.zylinder, b.koepfe, b.sektoren, b.spuren_mit_sek);
}

/* ═══ 3. Gepackt, MIT der 41. Spur ══════════════════════════════════
 *
 * Die Spur, die libdsk wegklemmt. Und eine Inhaltsprobe: der erste
 * Sektor traegt `EB 34 90` — den Sprungbefehl eines PC-Bootsektors.
 * Eine Zaehlung allein sagt nicht, ob die Bytes stimmen (MF-1026).    */
static void gruppe_3_schutzspur(void)
{
    printf("  [3] gepackt mit 41. Spur, Orakel hxcfe: 41 Spuren, 2 Seiten\n");
    if (!vorhanden(SCHUTZ)) { printf("      uebersprungen — Korpusdatei fehlt\n"); return; }
    befund_t b = lies(SCHUTZ);
    PRUEFE(b.offen, "uft_disk_open lieferte keinen Griff");
    PRUEFE(b.zylinder == 41, "nicht 41 Zylinder — die Schutzspur fehlt");
    PRUEFE(b.koepfe == 2, "nicht 2 Koepfe");
    PRUEFE(b.sektoren == 738, "nicht 738 Sektoren");
    PRUEFE(b.spuren_mit_sek == 82, "nicht 82 Spuren mit Sektoren");
    PRUEFE(b.nummern_1_bis_9, "Sektornummern nicht durchgehend 1..9");
    PRUEFE(b.erste[0] == 0xEB && b.erste[1] == 0x34 && b.erste[2] == 0x90,
           "Spur 0/0 Sektor 1 beginnt nicht mit EB 34 90");
    printf("      %u x %u, %zu Sektoren auf %zu Spuren, erste Byte %02X %02X %02X\n",
           b.zylinder, b.koepfe, b.sektoren, b.spuren_mit_sek,
           b.erste[0], b.erste[1], b.erste[2]);
}

/* P3-516: feste Namen im Bauverzeichnis plus `remove()` bedeuten, dass
 * zwei `ctest`-Laeufe im selben Verzeichnis einander abraeumen —
 * gemessen MF-1274 an `test_convert_leaves_no_ghost`. Die drei
 * Pruefdateien hier tragen deshalb die Prozessnummer im Namen und
 * liegen im Arbeitsverzeichnis, nicht im Korpus. */
#ifdef _WIN32
#  include <process.h>
#  define UFT_PID() ((unsigned long)_getpid())
#else
#  include <unistd.h>
#  define UFT_PID() ((unsigned long)getpid())
#endif

static void eigener_name(char *aus, size_t n, const char *stamm)
{
    snprintf(aus, n, "td0_%s_%lu.td0", stamm, UFT_PID());
}

/** Legt `ziel` als die ersten `n` Byte von `quelle` an. 0 = misslungen. */
static int kuerze(const char *quelle, const char *ziel, size_t n)
{
    FILE *q = fopen(quelle, "rb");
    if (!q) return 0;
    uint8_t *puffer = (uint8_t *)malloc(n);
    if (!puffer) { fclose(q); return 0; }
    size_t gelesen = fread(puffer, 1, n, q);
    fclose(q);
    if (gelesen != n) { free(puffer); return 0; }

    FILE *z = fopen(ziel, "wb");
    if (!z) { free(puffer); return 0; }
    size_t geschrieben = fwrite(puffer, 1, n, z);
    fclose(z);
    free(puffer);
    return geschrieben == n;
}

/* ═══ 4. Anti-Tautologie: ein halber Strom darf nicht aufgehen ══════
 *
 * Die Gruppen 2 und 3 pruefen Zahlen. Eine Zusage, die nur Zahlen
 * prueft, ist gruen, sobald irgendetwas die Zahlen liefert — deshalb
 * hier die Gegenrichtung.
 *
 * ZWEI Faelle, und der zweite ist der wichtigere:
 *
 *   4a  gepackte Datei auf ein Drittel gekuerzt — darf NICHT dieselbe
 *       Geometrie melden. Ueberspringt sich mit der Korpusdatei.
 *   4b  UNKOMPRIMIERTE Korpusdatei auf 52 Byte gekuerzt. Ihr
 *       Kommentarkopf sagt 56 Byte Text an, der Strom hat aber nur 40 —
 *       `10 + 56 > 40`, und der Leser muss ABSAGEN statt auf gut Glueck
 *       hinter dem Puffer weiterzulesen. Dieser Fall laeuft IMMER, denn
 *       er braucht nur `corpus_free`. Ohne ihn waere die Schranke im
 *       Kommentarblock toter Code — die Mutationsmatrix hat genau das
 *       gezeigt (Mutation B rutschte durch), und eine Pruefung, die
 *       nicht feuern kann, ist keine (MF-1000/Tor 64).                 */
static void gruppe_4_gekuerzt(void)
{
    printf("  [4] Gegenprobe: gekuerzte Dateien duerfen nicht aufgehen\n");
    char ziel[512];

    /* 4a — gepackt, ein Drittel. */
    if (vorhanden(SCHUTZ)) {
        FILE *q = fopen(SCHUTZ, "rb");
        long n = 0;
        if (q) { fseek(q, 0, SEEK_END); n = ftell(q); fclose(q); }
        eigener_name(ziel, sizeof(ziel), "gedrittelt");
        if (n > 0 && kuerze(SCHUTZ, ziel, (size_t)(n / 3))) {
            befund_t b = lies(ziel);
            PRUEFE(!(b.zylinder == 41 && b.sektoren == 738),
                   "eine gedrittelte Datei meldet dieselbe Geometrie — der Leser raet");
            /* Die Zahl ist gemessen und festgenagelt, nicht nur „weniger":
             * ohne die Datensatz-Schranke im Spurleser kommt GENAU EINER
             * mehr heraus — der Sektor, dessen Datensatz hinter den Puffer
             * reicht. 248 gegen 249 (Mutation E). */
            PRUEFE(b.sektoren == 248,
                   "gedrittelt nicht 248 Sektoren — ein Datensatz reicht hinter den Puffer");
            printf("      4a gedrittelt: %u x %u, %zu Sektoren (erwartet 248)\n",
                   b.zylinder, b.koepfe, b.sektoren);
            remove(ziel);
        } else {
            PRUEFE(0, "gedrittelte Pruefdatei nicht anlegbar");
        }
    } else {
        printf("      4a uebersprungen — Korpusdatei fehlt\n");
    }

    /* 4b — unkomprimiert, mitten im Kommentar abgeschnitten. Laeuft immer. */
    eigener_name(ziel, sizeof(ziel), "kurzkommentar");
    if (!kuerze(OFFEN, ziel, 52u)) {
        PRUEFE(0, "gekuerzte Pruefdatei nicht anlegbar");
        return;
    }
    befund_t b = lies(ziel);
    PRUEFE(!b.offen,
           "52 Byte mit angesagten 56 Byte Kommentar wurden GEOEFFNET — "
           "der Leser laeuft hinter seinen Puffer");
    PRUEFE(b.sektoren == 0, "aus 52 Byte kamen Sektoren");
    printf("      4b 52 Byte, Kommentar sagt 56: %s\n",
           b.offen ? "GEOEFFNET (falsch)" : "abgesagt");
    remove(ziel);

    /* 4c — unkomprimiert, MITTEN IN EINEM DATENSATZ abgeschnitten.
     *
     * Derselbe Zweck wie 4a, aber mit `corpus_free`, laeuft also IMMER.
     * 20 000 Byte enden innerhalb eines Sektor-Datensatzes; die Schranke
     * `pos + data_len > strom_len` muss dort abbrechen. Gemessen: 39
     * Sektoren mit der Schranke, 40 ohne. Die Zahl ist absichtlich kein
     * Vielfaches von 9 — eine angebrochene Spur ist genau der Fall. */
    eigener_name(ziel, sizeof(ziel), "halberdatensatz");
    if (!kuerze(OFFEN, ziel, 20000u)) {
        PRUEFE(0, "gekuerzte Pruefdatei (20 000) nicht anlegbar");
        return;
    }
    befund_t c = lies(ziel);
    PRUEFE(c.offen, "20 000 Byte wurden gar nicht geoeffnet");
    PRUEFE(c.zylinder == 3, "nicht 3 Zylinder");
    PRUEFE(c.sektoren == 39,
           "nicht 39 Sektoren — ein Datensatz reicht hinter den Puffer");
    printf("      4c 20 000 Byte: %u x %u, %zu Sektoren (erwartet 39)\n",
           c.zylinder, c.koepfe, c.sektoren);
    remove(ziel);
}

/* ═══ 5. Der Kommentarblock — bis MF-1285 weggeworfen ══════════════
 *
 * `td0_open()` rechnete die Kommentarlaenge aus, sprang darueber und
 * behielt NICHTS. `uft_td0_read_mem()` las den Block, und
 * `uft_td0_to_imd()` holt daraus Zeitstempel und Kommentartext — wer
 * `read_mem` loescht, ohne das hier zu haben, verliert still genau das
 * METADATA-Merkmal, das `uft_format_traegt()` fuer TD0 UND IMD als
 * getragen fuehrt (MF-1283).
 *
 * **Es gibt hierfuer keinen Rotbeweis „vorher", und das ist kein
 * Versaeumnis:** vor MF-1285 gab es das Feld nicht, ein Test dagegen
 * haette nicht gefehlt, sondern nicht uebersetzt — ein fehlendes Symbol
 * ist kein roter Test (Bauform MF-991). Belegt wird deshalb wie dort:
 * jede Zusage hat ihre eigene Mutation, und die Matrix steht in der
 * Commitnachricht.
 *
 * **Das Monatsfeld wird ROH geprueft, nicht gedeutet.** Ob Teledisk den
 * Monat 0- oder 1-basiert zaehlt, ist im Baum nicht gemessen; der Wandler
 * rechnet `+1`, der Header sagt „Month (1-12)". Beide koennen nicht recht
 * haben. Bis das entschieden ist, steht hier die Zahl aus der Datei und
 * keine Auslegung — siehe `docs/OPEN_ITEMS.md` P3-522.              */
static void gruppe_5_kommentar(void)
{
    printf("  [5] Kommentarblock (MF-1285)\n");

    uft_disk_t *d = uft_disk_open(OFFEN, true);
    PRUEFE(d != NULL, "Korpusdatei liess sich nicht oeffnen");
    if (!d) return;

    const uft_td0_strom_t *s = (const uft_td0_strom_t *)d->plugin_data;
    PRUEFE(s != NULL, "kein Strom hinter dem Griff");
    if (!s) { uft_disk_close(d); return; }

    const uft_td0_anmerkung_t *a = &s->anmerkung;
    PRUEFE(a->vorhanden, "Kommentarblock nicht erkannt — Kopfbyte 7 Bit 7");
    PRUEFE(a->text != NULL, "kein Kommentartext");
    PRUEFE(a->text_len == KOM_LEN, "Kommentarlaenge nicht 56");
    if (a->text && a->text_len == KOM_LEN)
        PRUEFE(memcmp(a->text, KOM_TEXT, KOM_LEN) == 0,
               "Kommentartext weicht ab");

    /* Rohwerte aus der Datei, ungedeutet. */
    PRUEFE(a->jahr    == 126u, "Jahr-Rohwert nicht 126");
    PRUEFE(a->monat   ==   8u, "Monat-Rohwert nicht 8");
    PRUEFE(a->tag     ==  12u, "Tag nicht 12");
    PRUEFE(a->stunde  ==  19u, "Stunde nicht 19");
    PRUEFE(a->minute  ==  58u, "Minute nicht 58");
    PRUEFE(a->sekunde ==   6u, "Sekunde nicht 6");

    printf("      \"%.*s\" (%zu Byte), roh %u-%02u-%02u %02u:%02u:%02u\n",
           (int)(a->text_len > 40 ? 40 : a->text_len), a->text ? a->text : "",
           a->text_len, (unsigned)a->jahr, (unsigned)a->monat,
           (unsigned)a->tag, (unsigned)a->stunde, (unsigned)a->minute,
           (unsigned)a->sekunde);

    uft_disk_close(d);
}

/** Kopiert `quelle` nach `ziel` und setzt EIN Byte.
 *
 * Prueft vorher, dass dort der erwartete Altwert steht. Ohne diese
 * Pruefung veraendert der Test irgendwann etwas anderes, als er glaubt —
 * und meldet dann gruen ueber eine Stelle, die es nicht mehr gibt.
 * 0 = misslungen.
 */
static int kippe_byte(const char *quelle, const char *ziel,
                      size_t versatz, uint8_t alt, uint8_t neu)
{
    FILE *q = fopen(quelle, "rb");
    if (!q) return 0;
    fseek(q, 0, SEEK_END);
    long n = ftell(q);
    fseek(q, 0, SEEK_SET);
    if (n <= (long)versatz) { fclose(q); return 0; }
    uint8_t *p = (uint8_t *)malloc((size_t)n);
    if (!p) { fclose(q); return 0; }
    size_t gelesen = fread(p, 1, (size_t)n, q);
    fclose(q);
    if (gelesen != (size_t)n) { free(p); return 0; }
    if (p[versatz] != alt) { free(p); return 0; }   /* Lage stimmt nicht */
    p[versatz] = neu;

    FILE *z = fopen(ziel, "wb");
    if (!z) { free(p); return 0; }
    size_t geschrieben = fwrite(p, 1, (size_t)n, z);
    fclose(z);
    free(p);
    return geschrieben == (size_t)n;
}

/* ═══ 6. Die Sektorflaggen — das Plugin las das falsche Bit ════════
 *
 * `td0_read_track()` setzte „CRC falsch" bei `sec_flags & 0x01`.
 * **0x01 ist DUP — die doppelte Sektor-ID, nicht der CRC-Fehler.**
 * Vier Quellen sagen 0x02, zwei davon ausserhalb dieses Baums:
 *
 *   include/uft/formats/uft_td0.h:101   UFT_TD0_SEC_CRC  0x02
 *   src/formats/uft_format_converters.c:46  TD0_FLAG_CRC_ERROR 0x02
 *   src/samdisk/td0.cpp:257   bad_data = (ts.flags & 0x02) != 0
 *   libdsk lib/drvtele.c:138 (lesend) und :732 (SCHREIBEND)
 *           if (syndrome & 2) ...  und  secdata[4] |= 2;  (CRC error)
 *
 * Die Wirkung ging in BEIDE Richtungen: ein Sektor mit echtem
 * CRC-Fehler kam als GUT heraus, und ein Sektor mit doppelter ID als
 * CRC-kaputt. Beides ist eine stille Falschaussage ueber forensische
 * Daten.
 *
 * **Der Versatz 86 ist gerechnet, nicht geraten:** 12 Byte Dateikopf
 * + 10 Byte Kommentarkopf + 56 Byte Kommentar = 78 (Spurkopf, 4 Byte)
 * -> 82 (Sektorkopf) -> Flaggenbyte bei 82+4. Gemessen steht dort
 * 0x00, und der Sektorkopf lautet `00 00 01 02 00 2F` — Zylinder 0,
 * Kopf 0, Sektor 1, Groessencode 2. `kippe_byte()` prueft den Altwert,
 * damit der Test nicht stillschweigend woanders hinlangt.
 *
 * Dass die Datei danach ihre eigenen Kopf-Pruefsummen verletzt, stoert
 * hier nicht — und DAS ist ein eigener Befund: dieser Leser rechnet
 * keine davon nach.                                                   */
#define FLAGGE_VERSATZ 86u

static void gruppe_6_sektorflaggen(void)
{
    printf("  [6] Sektorflaggen: 0x02 ist CRC, 0x01 ist DUP\n");
    char ziel[512];

    /* 0x02 = CRC-Fehler -> der Sektor MUSS als CRC-kaputt herauskommen. */
    eigener_name(ziel, sizeof(ziel), "flagge_crc");
    if (!kippe_byte(OFFEN, ziel, FLAGGE_VERSATZ, 0x00, 0x02)) {
        PRUEFE(0, "Pruefdatei mit Flagge 0x02 nicht anlegbar "
                  "(steht bei Versatz 86 nicht mehr 0x00?)");
    } else {
        befund_t b = lies(ziel);
        PRUEFE(b.offen, "Datei mit Flagge 0x02 liess sich nicht oeffnen");
        PRUEFE(b.erste_crc_ok == 0,
               "Flagge 0x02 (CRC-Fehler) kam als GUTER Sektor heraus");
        PRUEFE(b.erste_deleted == 0, "0x02 darf nicht `deleted` setzen");
        printf("      0x02 -> crc_ok=%d deleted=%d (erwartet 0/0)\n",
               b.erste_crc_ok, b.erste_deleted);
        remove(ziel);
    }

    /* 0x01 = doppelte ID -> das ist KEIN CRC-Fehler. */
    eigener_name(ziel, sizeof(ziel), "flagge_dup");
    if (!kippe_byte(OFFEN, ziel, FLAGGE_VERSATZ, 0x00, 0x01)) {
        PRUEFE(0, "Pruefdatei mit Flagge 0x01 nicht anlegbar");
    } else {
        befund_t b = lies(ziel);
        PRUEFE(b.offen, "Datei mit Flagge 0x01 liess sich nicht oeffnen");
        PRUEFE(b.erste_crc_ok == 1,
               "Flagge 0x01 (doppelte ID) wurde als CRC-Fehler gemeldet");
        printf("      0x01 -> crc_ok=%d (erwartet 1)\n", b.erste_crc_ok);
        remove(ziel);
    }

    /* 0x04 = geloeschte Datenmarke. War schon richtig; steht hier als
     * Regressionsprobe, damit die Berichtigung nicht das Nachbarbit
     * mitnimmt (Klasse MF-519/MF-529: eine Korrektur an einer Stelle
     * sagt nichts ueber ihre Nachbarn). */
    eigener_name(ziel, sizeof(ziel), "flagge_dam");
    if (!kippe_byte(OFFEN, ziel, FLAGGE_VERSATZ, 0x00, 0x04)) {
        PRUEFE(0, "Pruefdatei mit Flagge 0x04 nicht anlegbar");
    } else {
        befund_t b = lies(ziel);
        PRUEFE(b.erste_deleted == 1,
               "Flagge 0x04 (geloeschte Datenmarke) ging verloren");
        PRUEFE(b.erste_crc_ok == 1, "0x04 darf nicht `crc_ok` loeschen");
        printf("      0x04 -> deleted=%d crc_ok=%d (erwartet 1/1)\n",
               b.erste_deleted, b.erste_crc_ok);
        remove(ziel);
    }
}

/* ═══ 7. Der Wandler — erster Test ueberhaupt (MF-1287) ═══════════
 *
 * `uft_td0_to_imd()` gibt es seit Jahren und **kein Test hat ihn je
 * ausgefuehrt**. Der einzige, der TD0->IMD anfasst
 * (`test_convert_imd_img_belegt.c`, MF-1277), prueft, dass das
 * PREFLIGHT das Paar SPERRT — der Wandler laeuft dabei nie.
 *
 * Das ist die Lage, in der MF-1287 ihn umgeschrieben hat: vom toten
 * `uft_td0_image_t` auf den Strom-Kern, und von den TD0-Flaggen auf die
 * kanonischen Sektorfelder. Ein Umbau ohne Zusage waere eine Wette.
 *
 * Geprueft wird das, was die Wandlung TRAGEN soll — und der
 * Kommentarblock ist dabei der eigentliche Punkt: vor MF-1285 warf das
 * Plugin ihn weg, und eine Loeschung des zweiten Lesers haette
 * Zeitstempel und Text still verloren.                               */
static void gruppe_7_wandler(void)
{
    printf("  [7] TD0 -> IMD ueber den Strom-Kern (MF-1287)\n");

    FILE *q = fopen(OFFEN, "rb");
    PRUEFE(q != NULL, "Korpusdatei nicht lesbar");
    if (!q) return;
    fseek(q, 0, SEEK_END);
    long n = ftell(q);
    fseek(q, 0, SEEK_SET);
    uint8_t *roh = (uint8_t *)malloc((size_t)n);
    if (!roh || fread(roh, 1, (size_t)n, q) != (size_t)n) {
        free(roh); fclose(q); PRUEFE(0, "Korpusdatei nicht vollstaendig lesbar"); return;
    }
    fclose(q);

    uft_td0_strom_t strom;
    memset(&strom, 0, sizeof(strom));
    int rc = uft_td0_strom_aus_bytes(roh, (size_t)n, &strom);
    free(roh);
    PRUEFE(rc == UFT_OK, "uft_td0_strom_aus_bytes scheiterte");
    if (rc != UFT_OK) return;

    uft_imd_image_t imd;
    memset(&imd, 0, sizeof(imd));
    rc = uft_td0_to_imd(&strom, &imd);
    PRUEFE(rc == UFT_OK, "uft_td0_to_imd scheiterte");
    if (rc != UFT_OK) { uft_td0_strom_frei(&strom); return; }

    PRUEFE(imd.num_tracks == 160, "nicht 160 IMD-Spuren");
    PRUEFE(imd.total_sectors == 1440, "nicht 1440 IMD-Sektoren");
    PRUEFE(imd.num_cylinders == 80, "nicht 80 Zylinder");
    PRUEFE(imd.num_heads == 2, "nicht 2 Koepfe");

    /* Der Kommentarblock — der Grund, warum MF-1285 vor der Loeschung
     * kam. Das Monatsfeld ist 0-basiert, der Wandler rechnet `+1`:
     * roh 8 wird zu 9 = September (gemessen MF-1285). */
    PRUEFE(imd.comment != NULL, "Kommentar ging bei der Wandlung verloren");
    PRUEFE(imd.comment_len == KOM_LEN, "Kommentarlaenge nicht 56");
    if (imd.comment && imd.comment_len == KOM_LEN)
        PRUEFE(memcmp(imd.comment, KOM_TEXT, KOM_LEN) == 0,
               "Kommentartext weicht ab");
    PRUEFE(imd.header.year  == 2026, "Jahr nicht 2026 (126 + 1900)");
    PRUEFE(imd.header.month == 9,    "Monat nicht 9 (roh 8, 0-basiert)");
    PRUEFE(imd.header.day   == 12,   "Tag nicht 12");

    printf("      %zu Spuren, %u Sektoren, %u-%02u-%02u, Kommentar %zu Byte\n",
           (size_t)imd.num_tracks, (unsigned)imd.total_sectors,
           (unsigned)imd.header.year, (unsigned)imd.header.month,
           (unsigned)imd.header.day, (size_t)imd.comment_len);


    /* MF-1287: die eigentliche Zusage dieses Commits — der Wandler liest
     * die KANONISCHEN Sektorfelder, nicht noch einmal die TD0-Flaggen.
     *
     * Belegt an derselben Datei mit gekipptem Flaggenbyte wie Gruppe 6:
     * setzt man dort den CRC-Fehler, muss er als IMD-Sektortyp BAD
     * ankommen; setzt man die geloeschte Marke, als DELETED. Kaeme die
     * Deutung aus einer zweiten Flaggentafel im Wandler, waere das hier
     * gruen, auch wenn der Leser etwas anderes gesehen hat. */
    {
        char kopie[512];
        static const struct { uint8_t flagge; uint8_t erwartet; const char *was; }
        faelle[] = {
            { 0x02, UFT_IMD_SEC_ERROR,     "CRC-Fehler -> UFT_IMD_SEC_ERROR" },
            { 0x04, UFT_IMD_SEC_DELETED, "geloeschte Marke -> UFT_IMD_SEC_DELETED" },
        };
        for (size_t f = 0; f < sizeof(faelle) / sizeof(faelle[0]); f++) {
            eigener_name(kopie, sizeof(kopie), "wandler_flagge");
            if (!kippe_byte(OFFEN, kopie, FLAGGE_VERSATZ, 0x00, faelle[f].flagge)) {
                PRUEFE(0, "Pruefdatei fuer den Wandler nicht anlegbar");
                continue;
            }
            FILE *kf = fopen(kopie, "rb");
            if (!kf) { PRUEFE(0, "Pruefkopie nicht lesbar"); continue; }
            fseek(kf, 0, SEEK_END);
            long kn = ftell(kf);
            fseek(kf, 0, SEEK_SET);
            uint8_t *kb = (uint8_t *)malloc((size_t)kn);
            if (!kb || fread(kb, 1, (size_t)kn, kf) != (size_t)kn) {
                free(kb); fclose(kf); PRUEFE(0, "Pruefkopie unvollstaendig"); continue;
            }
            fclose(kf);

            uft_td0_strom_t ks;
            memset(&ks, 0, sizeof(ks));
            int krc = uft_td0_strom_aus_bytes(kb, (size_t)kn, &ks);
            free(kb);
            if (krc != UFT_OK) { PRUEFE(0, "Strom aus Pruefkopie scheiterte"); continue; }

            uft_imd_image_t kimd;
            memset(&kimd, 0, sizeof(kimd));
            krc = uft_td0_to_imd(&ks, &kimd);
            if (krc == UFT_OK && kimd.num_tracks > 0) {
                PRUEFE(kimd.tracks[0].stype[0] == faelle[f].erwartet,
                       faelle[f].was);
                printf("      %s: stype=%u (erwartet %u)\n", faelle[f].was,
                       (unsigned)kimd.tracks[0].stype[0],
                       (unsigned)faelle[f].erwartet);
            } else {
                PRUEFE(0, "Wandlung der Pruefkopie scheiterte");
            }
            uft_imd_free(&kimd);
            uft_td0_strom_frei(&ks);
            remove(kopie);
        }
    }

    uft_imd_free(&imd);
    uft_td0_strom_frei(&strom);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== TD0 Advanced Compression (MF-1284) ===\n");

    if (uft_register_all_formats() != UFT_OK) {
        printf("uft_register_all_formats scheiterte\n");
        return 1;
    }

    gruppe_1_unkomprimiert();
    gruppe_2_gepackt();
    gruppe_3_schutzspur();
    gruppe_4_gekuerzt();
    gruppe_5_kommentar();
    gruppe_6_sektorflaggen();
    gruppe_7_wandler();

    if (fehler) { printf("\n%d Zusage(n) gefallen\n", fehler); return 1; }
    printf("\nalle Zusagen gehalten\n");
    return 0;
}
