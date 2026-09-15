/**
 * @file test_xfd_ed_gegen_atr.c
 * @brief Enhanced Density: der XFD-Leser rechnet 520 x 256 statt 1040 x 128 (MF-1164)
 *
 * ── Der Befund ──────────────────────────────────────────────────────────
 *
 * `src/formats/xfd/uft_xfd.c` leitet die Sektorgroesse aus der Teilbarkeit
 * der Dateigroesse ab:
 *
 *     p->ss    = ((size_t)fs % 256 == 0 && fs > 92160) ? 256 : 128;
 *     p->total = (uint32_t)fs / p->ss;
 *
 * Eine Enhanced-Density-Diskette hat **1040 Sektoren von 128 Byte** —
 * 133 120 Byte. Diese Zahl ist durch 256 teilbar und groesser als 92 160,
 * also nimmt der Leser 256 und meldet **520 Sektoren**. Dazu stehen in
 * derselben Datei achtmal die Zahl 18 als Sektoren je Spur (:46 :48 :70
 * :73 :74 :117 :118 :119), womit aus 520 Sektoren 29 Zylinder werden.
 *
 * Gemessen am Vorzustand, je Groesse:
 *
 *     SD  720 x 128    92 160  ->  128 /  720 / 40 Zyl   richtig
 *     ED 1040 x 128   133 120  ->  256 /  520 / 29 Zyl   FALSCH
 *     DD  720 x 256   184 320  ->  256 /  720 / 40 Zyl   richtig
 *
 * Der SD-Fall geht nur durch, weil die Schranke `> 92160` und nicht
 * `>= 92160` lautet — 92 160 ist selbst durch 256 teilbar.
 *
 * ── Warum das die teuerste Fehlerklasse dieses Baums ist ────────────────
 *
 * Es gibt keine Absage und keine Warnung. Die Datei oeffnet, die
 * Sektorzahl klingt plausibel, und jeder Sektor traegt die Bytes zweier
 * anderer. Klasse MF-1016 / MF-1026 / MF-1038.
 *
 * ── Der Nachbar hatte es schon behoben ──────────────────────────────────
 *
 * `src/formats/atr/uft_atr.c:369` traegt woertlich:
 *
 *     MF-834: Sektoren je Spur aus der Standardgroesse, nicht fest 18.
 *     ... 8320 (130 K ED, **26** Sektoren) ...
 *     Mit fest 18 meldete Enhanced Density 58 Zylinder statt 40 — die
 *     Sektorzahl stimmte, die Spuraufteilung nicht.
 *
 * ATR und XFD sind dieselbe Diskette; `atr[16:] == xfd` ist ein
 * **registrierter verlustfreier Wandlungspfad** (MF-655, src/core/
 * uft_roundtrip.c:393). Die beiden Leser muessen also konstruktionsbedingt
 * uebereinstimmen — und taten es nicht. Vierter Fall von „Leseseite
 * geholt, Nachbarin uebersehen" nach MF-519, MF-529 und MF-1026, und der
 * erste, bei dem die beiden Dateien die zwei Haelften eines zugesagten
 * Wandlungspfads sind. Beim XFD war der Schaden groesser als damals beim
 * ATR: dort stimmte die Sektorzahl, hier stimmt keine der vier Zahlen.
 *
 * ── Warum der vorhandene Test es nicht gefangen hat ─────────────────────
 *
 * `tests/test_corpus_xfd.c` hat genau die richtige Zusage und sagt es
 * selbst: „the strong assertion is not 'the file parses' but 'BOTH
 * containers yield byte-identical sector data for the same disk'". Nur
 * wurde sie ausschliesslich bei **einer** Groesse gefahren — der Korpus
 * hat genau ein XFD/ATR-Paar, und das ist SD (92 160 / 92 176). Der
 * Entwurf war richtig, die Abdeckung war eine Groesse.
 *
 * ── Quellen fuer die 26 ─────────────────────────────────────────────────
 *
 * Drei, unabhaengig voneinander:
 *
 *  1. Der eigene ATR-Leser (oben), gegen SIO2PCs Groessentabelle
 *     `2SIOTEXT.S:1125` abgenommen: 8320 Absaetze = 130 K ED = 26.
 *  2. Die Firmware der Atari 1050 Turbo, `FORMAT.M65` `FORTAB` Z. 870/890,
 *     Feldfolge `EQUATES.M65` Z. 495-530 — erstes Feld `SECCNT`:
 *         SD  .BYTE 18,9,0,78,0,15,0,17
 *         ED  .BYTE 26,12,$4E,156,$4E,49,$4E,22
 *         DD  .BYTE 18,15,$4E,156,$4E,22,$4E,22
 *     Diese Quelle ist gegen ihr eigenes Erzeugnis geprueft: das aus dem
 *     Quelltext gebaute `turbo1050-35.rom` und das ausgelieferte
 *     `T1050_2B.8KB` (v3.5, 1988) haben denselben md5
 *     35be2c58f1e0b04ab5a1f2459e5515bd, 0 abweichende Byte. Sie ist
 *     ausdruecklich NUR GELESEN — (c) 1986-88 Bernhard Engl, kein Grant.
 *  3. `jhallen/atari-tools`, readme.md: „133,136 bytes (16 byte .atr
 *     header + 40 tracks * 26 sectors per track * 128 bytes per sector)".
 *     Ebenfalls nur gelesen, Kanal *Spec*.
 *
 * ── Rotbeweis gegen den Vorzustand ──────────────────────────────────────
 *
 * Faelle 1 und 2 sind gruen vor MF-1164 (die Rechnung und der ATR-Leser
 * waren richtig). Faelle 3 und 4 fallen.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_xfd;
extern const uft_format_plugin_t uft_format_plugin_atr;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* Enhanced Density, die vier Zahlen. Sie stehen hier ausgeschrieben, damit
 * der Test sie NENNT und nicht aus derselben Quelle holt wie der Prueflin
 * (Klasse MF-1000: ein Tor, das seinen Prueflin befragt, kann nicht rot
 * werden). */
#define ED_SS       128u
#define ED_SPT       26u
#define ED_CYLS      40u
#define ED_SECTORS (ED_SPT * ED_CYLS)          /* 1040 */
#define ED_BYTES   (ED_SECTORS * ED_SS)        /* 133120 */
#define ATR_HDR      16u

static void temp_pfad(char *p, size_t n, const char *endung)
{
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    snprintf(p, n, "%s/uft_ed_%d.%s", d, rand() % 100000, endung);
}

/* Jeder Sektor benennt sich selbst: logische Nummer, Zylinder und
 * Sektornummer innerhalb der Spur. Damit sagt ein Leseergebnis nicht nur,
 * DASS etwas kam, sondern ob die richtige Stelle getroffen wurde — und
 * eine falsche SEKTORGROESSE faellt ebenfalls auf, weil das Fuellbyte je
 * Sektor wechselt. */
static uint8_t *ed_nutzlast_bauen(void)
{
    uint8_t *b = (uint8_t *)malloc(ED_BYTES);
    if (!b) return NULL;
    for (unsigned ls = 1; ls <= ED_SECTORS; ls++) {
        uint8_t *s = b + (size_t)(ls - 1) * ED_SS;
        unsigned cyl = (ls - 1) / ED_SPT;
        unsigned sec = (ls - 1) % ED_SPT + 1;
        memset(s, (uint8_t)(ls & 0xFF), ED_SS);
        snprintf((char *)s, ED_SS, "UFT-ED L%04u C%02u S%02u", ls, cyl, sec);
    }
    return b;
}

static int schreiben(const char *pfad, const uint8_t *daten, size_t n)
{
    FILE *f = fopen(pfad, "wb");
    if (!f) return 0;
    size_t w = fwrite(daten, 1, n, f);
    fclose(f);
    return w == n;
}

/* ATR-Kopf nach der Beschreibung: Kennung 0x0296 LE, Groesse in
 * 16-Byte-Absaetzen (LE16 bei 2, hohes Byte bei 6), Sektorgroesse LE16
 * bei 4, Rest null. 133120 / 16 = 8320 passt in 16 Bit. */
static int atr_schreiben(const char *pfad, const uint8_t *nutz)
{
    uint8_t kopf[ATR_HDR];
    memset(kopf, 0, sizeof(kopf));
    unsigned absaetze = ED_BYTES / 16u;              /* 8320 */
    kopf[0] = 0x96; kopf[1] = 0x02;
    kopf[2] = (uint8_t)(absaetze & 0xFF);
    kopf[3] = (uint8_t)(absaetze >> 8);
    kopf[4] = (uint8_t)(ED_SS & 0xFF);
    kopf[5] = (uint8_t)(ED_SS >> 8);
    kopf[6] = 0;                                     /* hohes Absatzbyte */

    FILE *f = fopen(pfad, "wb");
    if (!f) return 0;
    int ok = (fwrite(kopf, 1, ATR_HDR, f) == ATR_HDR)
          && (fwrite(nutz, 1, ED_BYTES, f) == ED_BYTES);
    fclose(f);
    return ok;
}

static void spur_freigeben(uft_track_t *t)
{
    for (size_t s = 0; s < t->sector_count; s++) free(t->sectors[s].data);
    free(t->sectors);
    free(t->raw_data);
}

/* ── Fall 1: die Rechnung selbst ─────────────────────────────────────── */

TEST(die_ED_groesse_ist_1040_mal_128)
{
    /* Gruen vor MF-1164. Steht zuerst, weil jede weitere Zusage auf ihr
     * ruht: waere die Groesse anders, pruefte der Test ein anderes Format. */
    ASSERT(ED_SECTORS == 1040u);
    ASSERT(ED_BYTES   == 133120u);
    ASSERT(ED_BYTES / 16u == 8320u);          /* SIO2PCs Absatzzahl */
    /* und die Falle, die den Leser gestellt hat: */
    ASSERT(ED_BYTES % 256u == 0u);            /* deshalb griff die Heuristik */
    ASSERT(ED_BYTES > 92160u);
}

/* ── Fall 2: der ATR-Leser, der es seit MF-834 richtig macht ─────────── */

TEST(der_ATR_leser_meldet_die_ED_geometrie)
{
    uint8_t *nutz = ed_nutzlast_bauen();
    ASSERT(nutz != NULL);
    char pa[512]; temp_pfad(pa, sizeof(pa), "atr");
    ASSERT(atr_schreiben(pa, nutz));
    free(nutz);

    uft_disk_t da; memset(&da, 0, sizeof(da)); da.read_only = true;
    ASSERT(uft_format_plugin_atr.open(&da, pa, true) == UFT_OK);

    ASSERT(da.geometry.sector_size   == ED_SS);
    ASSERT(da.geometry.total_sectors == ED_SECTORS);
    ASSERT(da.geometry.sectors       == ED_SPT);
    ASSERT(da.geometry.cylinders     == ED_CYLS);
    ASSERT(da.geometry.heads         == 1u);

    uft_format_plugin_atr.close(&da);
    remove(pa);
}

/* ── Fall 3: ROTBEWEIS — derselbe Datentraeger als XFD ───────────────── */

TEST(der_XFD_leser_meldet_dieselbe_geometrie)
{
    /* Vor MF-1164 gemessen: 256 / 520 / 18 / 29 statt 128 / 1040 / 26 / 40. */
    uint8_t *nutz = ed_nutzlast_bauen();
    ASSERT(nutz != NULL);
    char px[512]; temp_pfad(px, sizeof(px), "xfd");
    ASSERT(schreiben(px, nutz, ED_BYTES));
    free(nutz);

    uft_disk_t dx; memset(&dx, 0, sizeof(dx)); dx.read_only = true;
    ASSERT(uft_format_plugin_xfd.open(&dx, px, true) == UFT_OK);

    ASSERT(dx.geometry.sector_size   == ED_SS);
    ASSERT(dx.geometry.total_sectors == ED_SECTORS);
    ASSERT(dx.geometry.sectors       == ED_SPT);
    ASSERT(dx.geometry.cylinders     == ED_CYLS);

    uft_format_plugin_xfd.close(&dx);
    remove(px);
}

/* ── Fall 4: ROTBEWEIS — eine Diskette, zwei Behaelter, zwei Leser ───── */

TEST(beide_behaelter_liefern_dieselben_sektoren)
{
    /* Die Zusage, die `test_corpus_xfd.c` fuer SD fuehrt, bei ED-Groesse.
     * Sie prueft mehr als die Geometrie: jeder Sektor benennt seine eigene
     * Stelle, also faellt auch eine um eins verschobene Spur auf. */
    uint8_t *nutz = ed_nutzlast_bauen();
    ASSERT(nutz != NULL);
    char px[512], pa[512];
    temp_pfad(px, sizeof(px), "xfd");
    temp_pfad(pa, sizeof(pa), "atr");
    ASSERT(schreiben(px, nutz, ED_BYTES));
    ASSERT(atr_schreiben(pa, nutz));
    free(nutz);

    uft_disk_t dx, da;
    memset(&dx, 0, sizeof(dx)); dx.read_only = true;
    memset(&da, 0, sizeof(da)); da.read_only = true;
    ASSERT(uft_format_plugin_xfd.open(&dx, px, true) == UFT_OK);
    ASSERT(uft_format_plugin_atr.open(&da, pa, true) == UFT_OK);

    unsigned verglichen = 0, marken = 0;
    for (unsigned cyl = 0; cyl < ED_CYLS; cyl++) {
        uft_track_t tx, ta;
        memset(&tx, 0, sizeof(tx)); memset(&ta, 0, sizeof(ta));
        uft_error_t ex = uft_format_plugin_xfd.read_track(&dx, (int)cyl, 0, &tx);
        uft_error_t ea = uft_format_plugin_atr.read_track(&da, (int)cyl, 0, &ta);
        if (ex != ea || ex != UFT_OK) {
            printf("\n    Zyl %u: xfd=%d atr=%d ", cyl, (int)ex, (int)ea);
            _fail++;
            spur_freigeben(&tx); spur_freigeben(&ta);
            break;
        }
        if (tx.sector_count != ta.sector_count) {
            printf("\n    Zyl %u: %zu gegen %zu Sektoren ",
                   cyl, tx.sector_count, ta.sector_count);
            _fail++;
            spur_freigeben(&tx); spur_freigeben(&ta);
            break;
        }
        for (size_t s = 0; s < tx.sector_count; s++) {
            if (tx.sectors[s].data_len != ta.sectors[s].data_len) { _fail++; break; }
            if (!tx.sectors[s].data || !ta.sectors[s].data) continue;
            if (memcmp(tx.sectors[s].data, ta.sectors[s].data,
                       tx.sectors[s].data_len) != 0) { _fail++; break; }
            verglichen++;
            /* und die Stelle selbst: die Marke muss diesen Zylinder nennen */
            char erwartet[32];
            snprintf(erwartet, sizeof(erwartet), "C%02u S%02u",
                     cyl, (unsigned)s + 1u);
            if (strstr((const char *)tx.sectors[s].data, erwartet)) marken++;
        }
        spur_freigeben(&tx); spur_freigeben(&ta);
    }

    /* Ein leerer Durchlauf waere wertlos (MF-1021). */
    ASSERT(verglichen == ED_SECTORS);
    ASSERT(marken     == ED_SECTORS);

    uft_format_plugin_xfd.close(&dx);
    uft_format_plugin_atr.close(&da);
    remove(px); remove(pa);
}

int main(void)
{
    printf("=== XFD gegen ATR bei Enhanced Density (MF-1164) ===\n");
    RUN(die_ED_groesse_ist_1040_mal_128);
    RUN(der_ATR_leser_meldet_die_ED_geometrie);
    RUN(der_XFD_leser_meldet_dieselbe_geometrie);
    RUN(beide_behaelter_liefern_dieselben_sektoren);
    printf("\nErgebnis: %d bestanden, %d gefallen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
