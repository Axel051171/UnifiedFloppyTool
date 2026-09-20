/**
 * @file test_td0_pruefsummen.c
 * @brief TD0 rechnet seine Pruefsummen nach — und sagt, wo es keine gibt (MF-1296).
 *
 * AUFRUFER: `tests/CMakeLists.txt` registriert diese Datei als ctest-Ziel.
 * BERUEHRTE API: nur lesend — `uft_td0_crc()`, `uft_disk_open()`,
 *   `uft_format_plugin_td0.read_track()`.
 * DATENSCHEMA: keines. Legt waehrend des Laufs Kopien der Korpusdatei mit
 *   je EINEM gekippten Bit an und loescht sie wieder.
 * ANWEISUNG (woertlich): "Rotbeweis mit der einen Datei, die wir committen
 *   duerfen: ein Byte kippen, drei Befunde erwarten."
 *
 * ── Der Anlass ───────────────────────────────────────────────────────
 *
 * TD0 traegt Pruefsummen, und dieser Baum rechnete keine nach.
 * `src/formats/td0/uft_td0.c` las den Spurkopf und liess sein viertes
 * Byte mit einem blossen Kommentar liegen; `crc_ok` je Sektor kam aus
 * dem FORMATFLAG `UFT_TD0_SEC_CRC` — also aus dem, was der Erzeuger
 * behauptet, nicht aus einer Messung.
 *
 * Das ist die billigste Verteidigung, die es gibt, und sie haette
 * MF-1284 an der Wurzel gefangen: dort kamen aus einem falsch gelesenen
 * Strom 188 Zylinder und null Sektoren heraus. Eine Spurkopf-Pruefsumme
 * sagt das beim ERSTEN falschen Byte, nicht beim 255. Muellzylinder.
 *
 * ── Was die Messung am Auftrag berichtigt hat ────────────────────────
 *
 * Der Auftrag nannte `sec_hdr[5]` die Sektorkopf-CRC — so stand es auch
 * in UFTs eigenem Header. **Es gibt keine Sektorkopf-CRC.** Zwei
 * unabhaengige Haende sagen, dass das Feld die DATEN deckt:
 *
 *   SAMdisk `src/samdisk/td0.cpp:53`   `uint8_t data_crc;`
 *                                      `// Low 8-bits of sector data CRC`
 *   SAMdisk `src/samdisk/td0.cpp:277`  CrcTd0Block(data, size) & 0xff
 *                                      gegen ts.data_crc
 *   libdsk  `lib/drvtele.c:133`        "buf[5] is the CRC, ignored on load"
 *   libdsk  `lib/drvtele.c:138`        Formatflag 0x02 -> ST2 0x20,
 *                                      "Data Error in Data Field"
 *
 * Gruppe 5 beweist es AM OBJEKT statt an einer zweiten Beschreibung: ein
 * gekipptes Byte im Sektorkopf laesst jede Pruefsumme der Datei
 * unberuehrt. Folge fuers Zentrum: `id_crc_known` bleibt fuer TD0
 * **false** — das Feld bedeutet dort woertlich "false = das Format
 * traegt keine Angabe".
 *
 * ── Die fuenf Gruppen ────────────────────────────────────────────────
 *
 *   1  Eichung AM OBJEKT: `uft_td0_crc()` trifft die beiden 16-Bit-
 *      Summen, die in der Datei stehen. Ohne sie waere alles Weitere
 *      eine Rechnung, die sich selbst bestaetigt.
 *   2  Die unveraenderte Korpusdatei: jeder Sektor ist GEPRUEFT und gut,
 *      kein Spurkopf faellt.
 *   3  Rotbeweis Sektordaten — ein Byte gekippt, genau ein Sektor faellt.
 *   4  Rotbeweis Spurkopf — ein Byte gekippt, die Spur wird BEENDET.
 *   5  Anti-Tautologie und der vierte Befund: ein Byte im SEKTORKOPF
 *      gekippt — nichts faellt, weil nichts ihn deckt.
 *
 * Gruppe 5 ist nicht Beiwerk. Ohne sie waeren 3 und 4 gruen aus dem
 * falschen Grund: ein Test, bei dem JEDE Aenderung rot wird, misst nicht
 * die Pruefsumme, sondern nur, dass sich etwas geaendert hat.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef _WIN32
#  include <process.h>
#  define UFT_EIGENE_KENNUNG() ((unsigned long)_getpid())
#else
#  include <unistd.h>
#  define UFT_EIGENE_KENNUNG() ((unsigned long)getpid())
#endif

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_error.h"
#include "uft/formats/uft_td0.h"
#include "uft/uft_core.h"
/* NICHT `#include "uft/uft_track.h"`: zusammen mit
 * `uft_format_plugin.h` meldet der Uebersetzer `conflicting types
 * for uft_track_get_sector` — die beiden Header deklarieren
 * dieselbe Funktion verschieden. Ein vorbestehender Konflikt, der
 * nicht in diesen Commit gehoert; festgehalten als Befund.
 * Die EINE Funktion, die hier gebraucht wird, steht deshalb als
 * Deklaration da — Quelle: `include/uft/uft_track.h:220`. */
extern void uft_track_release(uft_track_t *track);
/* `uft_track_release()` und NICHT `uft_track_free()`.
 *
 * Gemessen beim Bau dieses Tests: `uft_track_free()`
 * (`src/core/uft_unified_types.c:381`) ruft `free(track)` — es gibt die
 * STRUKTUR frei, nicht nur ihren Inhalt. Auf eine Spur auf dem STAPEL
 * angewandt endet der Lauf sofort (gemessen: rc 127, mitten in
 * Gruppe 2). `uft_track_release()` gibt den Inhalt frei und laesst die
 * Struktur stehen — das ist der richtige Partner fuer eine Spur, die
 * der Aufrufer selbst haelt.
 *
 * Der Schwestertest `test_td0_gepackt.c` befreit seine Spuren GAR
 * NICHT; "leckende Tests null halten" ist eine der vier
 * Release-Kennzahlen. */

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR fehlt — tests/CMakeLists.txt muss es fuer diesen Test setzen"
#endif

/* `uft_disk_open()` und `uft_disk_close()` kommen aus `uft_core.h`.
 * Sie hier noch einmal zu deklarieren war der erste Versuch und ist
 * schiefgegangen: der Uebersetzer meldete `conflicting types for
 * uft_disk_close`. Zwei Deklarationen einer Funktion sind die
 * Bauform aus §MF-1177 — eine Wahrheit, zwei Stellen. */
extern uft_error_t  uft_register_all_formats(void);
extern const uft_format_plugin_t uft_format_plugin_td0;

#define KORPUS UFT_CORPUS_DIR "/libdsk_uftk_pc720.td0"

/* Versaetze, AUS DER DATEI gemessen (nicht aus einer Beschreibung):
 *     0..11   Dateikopf, seine CRC-16 little-endian bei 10
 *    12..13   CRC-16 des Kommentarblocks
 *    14..15   Laenge des Kommentartextes (56)
 *    78..81   erster Spurkopf   (09 00 00 34)
 *    82..87   erster Sektorkopf (00 00 01 02 00 2F)
 *    88..     erster Datensatz                                        */
#define V_SPURKOPF_ZYL     79u    /* Zylinderbyte des ersten Spurkopfs   */
#define V_SEKTORKOPF_ZYL   82u    /* Zylinderbyte des ersten Sektorkopfs */
#define V_SEKTORDATEN      91u    /* im Datensatz des ersten Sektors     */

static int fehler = 0;
#define PRUEFE(bed, text) do { \
    if (!(bed)) { printf("    [ROT] %s (Zeile %d)\n", (text), __LINE__); fehler++; } \
} while (0)

/* ── Helfer ───────────────────────────────────────────────────────── */

static uint8_t *datei_lesen(const char *pfad, size_t *len_aus)
{
    FILE *f = fopen(pfad, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n <= 0) { fclose(f); return NULL; }
    rewind(f);
    uint8_t *b = (uint8_t *)malloc((size_t)n);
    if (!b) { fclose(f); return NULL; }
    size_t gelesen = fread(b, 1, (size_t)n, f);
    fclose(f);
    if (gelesen != (size_t)n) { free(b); return NULL; }
    *len_aus = gelesen;
    return b;
}

/** Legt eine Kopie an, in der GENAU EIN Bit gekippt ist.
 *
 *  Der Name traegt Versatz UND Prozesskennung: zwei `ctest`-Laeufe im
 *  selben Verzeichnis duerfen sich nicht die Pruefdatei wegnehmen
 *  (`docs/OPEN_ITEMS.md` P3-516). */
static bool kippe(const char *quelle, char *ziel, size_t ziel_len, size_t versatz)
{
    size_t len = 0;
    uint8_t *b = datei_lesen(quelle, &len);
    if (!b) return false;
    if (versatz >= len) { free(b); return false; }

    snprintf(ziel, ziel_len, "td0crc_%lu_%lu.td0",
             (unsigned long)versatz, UFT_EIGENE_KENNUNG());
    b[versatz] = (uint8_t)(b[versatz] ^ 0x01u);

    FILE *f = fopen(ziel, "wb");
    if (!f) { free(b); return false; }
    size_t geschrieben = fwrite(b, 1, len, f);
    fclose(f);
    free(b);
    return geschrieben == len;
}

/** Was die ganze Diskette ueber ihre Pruefsummen hergibt. */
typedef struct {
    int      offen;
    size_t   sektoren;
    size_t   geprueft;          /**< mit UFT_SECTOR_CRC_CHECKED         */
    size_t   crc_gut;
    size_t   crc_schlecht;
    size_t   id_crc_gesetzt;    /**< id.crc != 0 — muss 0 bleiben       */
    size_t   spuren_mit_hdr_crc;
    size_t   spuren_mit_sektoren;
} befund_t;

static befund_t lies_alles(const char *pfad)
{
    befund_t b;
    memset(&b, 0, sizeof(b));

    uft_disk_t *d = uft_disk_open(pfad, true);
    if (!d) return b;
    b.offen = 1;

    const unsigned zyl = (unsigned)d->geometry.cylinders;
    const unsigned kpf = (unsigned)d->geometry.heads;
    for (unsigned c = 0; c < zyl; c++) {
        for (unsigned h = 0; h < kpf; h++) {
            uft_track_t t;
            memset(&t, 0, sizeof(t));
            if (uft_format_plugin_td0.read_track(d, (int)c, (int)h, &t) != UFT_OK)
                continue;
            if (t.status & UFT_TRACK_HDR_CRC) b.spuren_mit_hdr_crc++;
            if (t.sector_count > 0)           b.spuren_mit_sektoren++;
            for (size_t i = 0; i < t.sector_count; i++) {
                const uft_sector_t *s = &t.sectors[i];
                b.sektoren++;
                if (s->status & UFT_SECTOR_CRC_CHECKED) b.geprueft++;
                if (s->crc_ok) b.crc_gut++;
                else           b.crc_schlecht++;
                if (s->id.crc != 0u) b.id_crc_gesetzt++;
            }
            uft_track_release(&t);
        }
    }
    uft_disk_close(d);
    return b;
}

/* ── 1. Eichung am Objekt ─────────────────────────────────────────── */
static void gruppe_1_eichung(void)
{
    printf("  [1] uft_td0_crc() trifft die Summen, die IN der Datei stehen\n");

    size_t len = 0;
    uint8_t *b = datei_lesen(KORPUS, &len);
    PRUEFE(b != NULL, "Korpusdatei nicht lesbar");
    if (!b) return;
    PRUEFE(len > 96u, "Korpusdatei zu kurz");
    if (len <= 96u) { free(b); return; }

    const uint16_t kopf_datei     = (uint16_t)(b[10] | ((uint16_t)b[11] << 8));
    const uint16_t kopf_gerechnet = uft_td0_crc(b, 10u, 0u);
    printf("      Dateikopf   Datei=0x%04X  gerechnet=0x%04X\n",
           kopf_datei, kopf_gerechnet);
    PRUEFE(kopf_datei == kopf_gerechnet,
           "die Kopf-CRC geht nicht auf — dann ist die Rechnung falsch "
           "und alles Weitere wertlos");

    /* Der Kommentarblock: seine CRC deckt alles AB dem Laengenfeld. */
    const uint16_t kom_datei = (uint16_t)(b[12] | ((uint16_t)b[13] << 8));
    const uint16_t kom_len   = (uint16_t)(b[14] | ((uint16_t)b[15] << 8));
    PRUEFE(kom_len > 0u && (size_t)(14u + 8u + kom_len) <= len,
           "Kommentarlaenge unglaubwuerdig");
    const uint16_t kom_gerechnet =
        uft_td0_crc(b + 14, (size_t)(8u + kom_len), 0u);
    printf("      Kommentar   Datei=0x%04X  gerechnet=0x%04X  (%u Byte Text)\n",
           kom_datei, kom_gerechnet, (unsigned)kom_len);
    PRUEFE(kom_datei == kom_gerechnet, "die Kommentar-CRC geht nicht auf");

    /* Gegenprobe: dieselbe Rechnung ueber eine ANDERE Spanne darf NICHT
     * denselben Wert liefern. Sonst misst Gruppe 1 nichts. */
    PRUEFE(uft_td0_crc(b, 9u, 0u) != kopf_datei,
           "CRC ueber 9 statt 10 Byte liefert denselben Wert — die "
           "Rechnung haengt nicht von ihrer Eingabe ab");

    free(b);
}

/* ── 2. Die unveraenderte Datei ───────────────────────────────────── */
static void gruppe_2_unveraendert(void)
{
    printf("  [2] unveraendert: jeder Sektor GEPRUEFT und gut\n");

    befund_t b = lies_alles(KORPUS);
    PRUEFE(b.offen, "Korpusdatei nicht zu oeffnen");
    if (!b.offen) return;

    printf("      %zu Sektoren, %zu geprueft, %zu gut, %zu schlecht; "
           "%zu Spuren mit Kopf-CRC-Fehler\n",
           b.sektoren, b.geprueft, b.crc_gut, b.crc_schlecht,
           b.spuren_mit_hdr_crc);

    PRUEFE(b.sektoren > 0u, "keine Sektoren gelesen");
    PRUEFE(b.geprueft == b.sektoren,
           "nicht jeder Sektor traegt UFT_SECTOR_CRC_CHECKED — dann sagt "
           "crc_ok wieder nicht, OB nachgesehen wurde");
    PRUEFE(b.crc_schlecht == 0u,
           "ein Sektor faellt, obwohl die Datei von libdsk stammt und "
           "unveraendert ist");
    PRUEFE(b.spuren_mit_hdr_crc == 0u,
           "ein Spurkopf faellt an der unveraenderten Datei");
    PRUEFE(b.id_crc_gesetzt == 0u,
           "ein Sektor traegt eine ID-CRC — TD0 hat keine, und eine "
           "erfundene macht id_crc_known im Zentrum wahr");
}

/* ── 3. Rotbeweis: ein Byte in den Sektordaten ────────────────────── */
static void gruppe_3_rot_daten(void)
{
    printf("  [3] Rotbeweis: EIN Byte in den Sektordaten gekippt\n");

    char ziel[256];
    if (!kippe(KORPUS, ziel, sizeof(ziel), V_SEKTORDATEN)) {
        PRUEFE(0, "Pruefdatei mit gekipptem Datenbyte nicht anlegbar");
        return;
    }
    befund_t b = lies_alles(ziel);
    printf("      %zu Sektoren, %zu gut, %zu schlecht\n",
           b.sektoren, b.crc_gut, b.crc_schlecht);

    PRUEFE(b.offen, "gekippte Datei nicht zu oeffnen");
    PRUEFE(b.crc_schlecht == 1u,
           "genau EIN Sektor muss fallen — mehr hiesse, die Pruefsumme "
           "deckt zu viel, weniger, sie deckt zu wenig");
    PRUEFE(b.geprueft == b.sektoren,
           "auch der gefallene Sektor muss als GEPRUEFT gelten");
    remove(ziel);
}

/* ── 4. Rotbeweis: ein Byte im Spurkopf ───────────────────────────── */
static void gruppe_4_rot_spurkopf(void)
{
    printf("  [4] Rotbeweis: EIN Byte im Spurkopf gekippt\n");

    char ziel[256];
    if (!kippe(KORPUS, ziel, sizeof(ziel), V_SPURKOPF_ZYL)) {
        PRUEFE(0, "Pruefdatei mit gekipptem Spurkopf nicht anlegbar");
        return;
    }
    befund_t b = lies_alles(ziel);
    printf("      %zu Spuren melden UFT_TRACK_HDR_CRC, %zu Sektoren gelesen\n",
           b.spuren_mit_hdr_crc, b.sektoren);

    PRUEFE(b.offen, "Datei mit gekipptem Spurkopf nicht zu oeffnen");
    PRUEFE(b.spuren_mit_hdr_crc > 0u,
           "kein Spurkopf faellt — dann wird die Spurkopf-Pruefsumme "
           "nicht nachgerechnet");
    PRUEFE(b.sektoren == 0u,
           "hinter einem gefallenen Spurkopf wurden Sektoren gelesen — "
           "genau die Gestalt von MF-1284");
    remove(ziel);
}

/* ── 5. Anti-Tautologie: der Sektorkopf ist ungedeckt ─────────────── */
static void gruppe_5_sektorkopf_ungedeckt(void)
{
    printf("  [5] Gegenprobe: ein Byte im SEKTORKOPF deckt keine Pruefsumme\n");

    char ziel[256];
    if (!kippe(KORPUS, ziel, sizeof(ziel), V_SEKTORKOPF_ZYL)) {
        PRUEFE(0, "Pruefdatei mit gekipptem Sektorkopf nicht anlegbar");
        return;
    }
    befund_t b = lies_alles(ziel);
    printf("      %zu Sektoren, %zu gut, %zu schlecht; %zu Spuren mit "
           "Kopf-CRC-Fehler\n",
           b.sektoren, b.crc_gut, b.crc_schlecht, b.spuren_mit_hdr_crc);

    PRUEFE(b.offen, "Datei mit gekipptem Sektorkopf nicht zu oeffnen");
    PRUEFE(b.crc_schlecht == 0u,
           "ein Sektor faellt, obwohl nur sein KOPF veraendert wurde — "
           "dann misst der Test nicht die Pruefsumme, sondern nur, dass "
           "sich etwas geaendert hat");
    PRUEFE(b.spuren_mit_hdr_crc == 0u,
           "ein Spurkopf faellt, obwohl ein SEKTORkopf veraendert wurde");
    PRUEFE(b.id_crc_gesetzt == 0u, "ein Sektor traegt eine ID-CRC");
    remove(ziel);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== TD0: die Pruefsummen werden nachgerechnet (MF-1296) ===\n");

    if (uft_register_all_formats() != UFT_OK) {
        printf("  [ROT] uft_register_all_formats() fehlgeschlagen\n");
        return 1;
    }

    gruppe_1_eichung();
    gruppe_2_unveraendert();
    gruppe_3_rot_daten();
    gruppe_4_rot_spurkopf();
    gruppe_5_sektorkopf_ungedeckt();

    if (fehler) { printf("\n%d Zusage(n) gefallen\n", fehler); return 1; }
    printf("\nalle Zusagen gehalten\n");
    return 0;
}
