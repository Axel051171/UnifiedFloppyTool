/**
 * @file test_1050_firmware_als_quelle.c
 * @brief Laufwerks-Firmware als Quellengattung: die Atari 1050 Turbo v3.5
 *        gegen UFTs gerechnetes Interleave-Modell (MF-1179).
 *
 * @section WARUM_FIRMWARE
 *
 * Fast alle Quellen dieses Baums sind LESER: ein Werkzeug, das ein
 * fremdes Abbild aufmacht und deutet. Eine Laufwerks-Firmware ist der
 * SCHREIBER — sie hat die Disketten hergestellt, um die es geht. Und sie
 * ist gegen ihr eigenes Erzeugnis pruefbar: das aus diesem Quelltext
 * gebaute `turbo1050-35.rom` und das ausgelieferte `T1050_2B.8KB`
 * (v3.5, 1988) haben denselben md5 `35be2c58f1e0b04ab5a1f2459e5515bd`,
 * 0 abweichende Byte (gemessen MF-1164). Damit stammen die Konstanten
 * unten vom Schreiber, nicht von einem Deuter.
 *
 * @section QUELLE
 *
 * Atari 1050 Turbo, Firmware v3.5, Atasm-Quelltext (`software_1050_turbo`,
 * Verzeichnis `SOFTTRB35`):
 *   - `FORMAT.M65:830,850,870,890,910,930` — Tafel `FORTAB`, sechs Zeilen
 *   - `EQUATES.M65:495-535`               — die Feldfolge von `FORTAB`
 *   - `EQUATES.M65:315-330`               — das SIO-Kommandoframe
 *   - `EQUATES.M65:350-405`               — der Percom-Block
 *   - `FORMAT.M65:370-505`                — der Interleave-Verteiler
 *   - `FORTRK.M65:120-620`                — der Spurschreiber
 *   - `MAIN.M65:2050-2135`                — die Kommandotafel ($4E/$4F)
 *   - `SERIN.M65:120-315`                 — `RBUF`, der Empfangslauf
 *   - `CONFIG.M65:100-520`                — `GCONF`/`SCONF`
 *
 * LIZENZ: (c) 1986-88 Bernhard Engl, Atasm-Fassung (c) 2004 Matthias
 * Reichl. **KEINE Rechteeinraeumung.** Kanal *Spec* (MF-695): gelesen und
 * ZITIERT, nicht portiert, und das Archiv wird nicht mitgeliefert. Die
 * Zahlen unten sind Tatsachen ueber ein Format; der Assembler bleibt
 * draussen. Deshalb stehen hier Konstanten mit Fundstelle und keine
 * Datei aus dem Archiv — dieser Test laeuft ohne das Archiv.
 *
 * @section WAS_GEMESSEN_WIRD
 *
 * `src/core/uft_interleave.c` ist eine wortgleiche Portierung von
 * a8rawconvs `compute_interleave()` — ein MODELL, das die Reihenfolge
 * RECHNET. `FORTAB` sind die KONSTANTEN, mit denen das Laufwerk
 * formatiert hat. Zwei unabhaengige Angaben ueber dieselbe Sache, und
 * das Modell hat seit MF-479 einen Produktionsaufrufer, der seine
 * Ausgabe in eine ATX-Datei SCHREIBT (`uft_atx.c:538`) — die Frage ist
 * also nicht akademisch.
 *
 * Gemessen (MF-1179): der FAKTOR trifft zweimal, das LAYOUT keinmal.
 * Und die Zaehlung ohne Rotationsabgleich haette das Gegenteil gesagt —
 * bei SD weichen 18 von 18 Plaetzen ab und die Tafeln sind trotzdem
 * IDENTISCH, nur um 9 Plaetze gedreht. Eine Abbilddatei hat keinen
 * absoluten Platz 0; welcher physische Sektor dort liegt, entscheidet,
 * wo der Leser angefangen hat. Das ist MF-1026 in neuer Gestalt: die
 * Zahl geht auf und sagt nichts ueber die Verteilung.
 *
 * @section WAS_NICHT_GEMESSEN_WIRD
 *
 * Die Firmware ist ein NACHRUEST-ROM von Bernhard Engl, nicht das
 * Original-ROM der Atari 1050. Wo sie von a8rawconv abweicht, ist damit
 * NICHT entschieden, welche Zahl auf einer Diskette von Atari steht —
 * dazu braeuchte es das Original-ROM, das XF551-ROM oder einen Flussabzug.
 * Dieser Test nagelt deshalb die ABWEICHUNG fest, nicht einen Sieger
 * (P3-434). Und die Spurbilanz der Atari-Formate bleibt unbelegt: die
 * Firmware nennt zwei Zeitangaben, die sich nicht zusammenrechnen lassen,
 * solange die Taktlaenge von STIM1/STIM2 unbekannt ist (P3-435, S1).
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include "uft/core/uft_interleave.h"
#include "uft/formats/uft_fdc_gaps.h"

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)
#define CHECK(c, msg) do { if (!(c)) { \
                        printf("FAIL @ %d: %s\n", __LINE__, (msg)); \
                        _fail++; return; } } while (0)

/* ── FORTAB, verbatim ────────────────────────────────────────────────
 * Feldfolge aus EQUATES.M65:495-535. Nur die ersten ACHT Felder stehen
 * in der Tafel; DBYTC, MFMFLG und SECXTD setzt `FORMAT` selbst
 * (FORMAT.M65:515-570), sie gehoeren also NICHT hierher.
 *
 * XGAPD/IGAPD/DGAPD sind FUELLBYTES, XGAPC/IGAPC/DGAPC ZAEHLER —
 * „INDEX GAP BYTES" gegen „INDEX GAP COUNT" im Original.
 */
enum { F_SECCNT = 0, F_IFAC, F_XGAPD, F_XGAPC,
       F_IGAPD, F_IGAPC, F_DGAPD, F_DGAPC };

/* Zeilenwahl: FORMAT.M65:135-205. SD = 0, ED = 2, DD = 4; `INX` fuer
 * FAST IO ergibt die ungeraden Zeilen. `CPX #2 / BCC` heisst: Zeile 0
 * und 1 sind FM, alles ab 2 ist MFM. */
enum { R_SD_SLOW = 0, R_SD_FAST, R_ED_SLOW, R_ED_FAST, R_DD_SLOW, R_DD_FAST };

static const uint8_t FORTAB[6][8] = {
    /* FORMAT.M65:830  SD SLOW */ { 18,  9, 0x00,  78, 0x00, 15, 0x00, 17 },
    /* FORMAT.M65:850  SD FAST */ { 18,  5, 0x00,  78, 0x00, 15, 0x00, 17 },
    /* FORMAT.M65:870  ED SLOW */ { 26, 12, 0x4E, 156, 0x4E, 49, 0x4E, 22 },
    /* FORMAT.M65:890  ED FAST */ { 26,  6, 0x4E, 156, 0x4E, 49, 0x4E, 22 },
    /* FORMAT.M65:910  DD SLOW */ { 18, 15, 0x4E, 156, 0x4E, 22, 0x4E, 22 },
    /* FORMAT.M65:930  DD FAST */ { 18,  7, 0x4E, 156, 0x4E, 22, 0x4E, 22 },
};

/* Sektorgroesse je Dichte: GCONF/SCONF (CONFIG.M65:125-195, 400-440).
 * ED ist MFM mit 128-Byte-Sektoren — nicht FM. */
static const uint16_t ZEILE_SS[6] = { 128, 128, 128, 128, 256, 256 };

/* ── Der Verteiler der Firmware, Zeile fuer Zeile ────────────────────
 * FORMAT.M65:370-505. Kein Port: 6502-Assembler laesst sich nicht
 * linken, also ist dies eine UEBERTRAGUNG, und sie steht neben ihrem
 * Original. Das MODELL wird dagegen GERUFEN, nicht nachgebaut — eine
 * zweite Kopie des Modells waere die Lage aus MF-1015.
 *
 *   0370  LDY #1        ; FIRST ID
 *   0375  LDA SECCNT    ; FIRST INDEX
 *   0380  SEC
 *   0385  SBC IFAC
 *   0390  TAX
 *   0400  FORM3 LDA SECTAB,X   ; ENTRY FREE?
 *   0405  BEQ FORM4            ; YES
 *   0410  DEX                  ; SCAN FOR FREE ENTRY  <-- ABWAERTS
 *   0415  BPL FORM3
 *   0420  TXA / CLC / ADC SECCNT / TAX / BPL FORM3
 *   0450  FORM4 STY SECTAB,X   ; ID TO TABLE
 *   0455  TXA / SEC / SBC IFAC
 *   0470  BCS FORM5
 *   0475  SEC / ADC SECCNT     ; Wickel: + SECCNT + 1, weil SEC vor ADC
 *   0485  FORM5 TAX
 *   0490  INY / CPY SECCNT / BCC FORM3 / BEQ FORM3
 *
 * Vorlauf FORMAT.M65:335-365: SECTAB[SECCNT] = $FF („FORMAT BUG FIX"),
 * SECTAB[SECCNT-1..0] = 0. Das $FF ist ein WAECHTER und nie frei — er
 * faengt genau den Wickel-Ueberschuss von +1 auf.
 */
static void firmware_layout(uint8_t *sectab, int seccnt, int ifac)
{
    memset(sectab, 0, 256);
    sectab[seccnt] = 0xFF;
    int y = 1, x = (seccnt - ifac) & 0xFF;
    for (;;) {
        while (sectab[x] != 0) {
            x = (x - 1) & 0xFF;
            if (x & 0x80) x = (x + seccnt) & 0xFF;
        }
        sectab[x] = (uint8_t)y;
        int t = x - ifac;
        if (t < 0) t = t + seccnt + 1;
        x = t & 0xFF;
        if (++y > seccnt) break;
    }
}

/* Das Modell: GERUFEN. Bei track = 0 ist t0 = 0, also ist
 * timings[i] = spacing * platz und der Platz exakt rueckrechenbar. */
static int modell_layout(uint8_t *sectab, int seccnt, uint16_t ss)
{
    float t[256];
    if (uft_compute_interleave(t, (size_t)seccnt, ss, ss != 128u,
                               0, 0, UFT_INTERLEAVE_AUTO) != UFT_OK) return -1;
    const double spacing = 0.98 / (double)seccnt;
    memset(sectab, 0, 256);
    for (int i = 0; i < seccnt; ++i) {
        int platz = (int)(t[i] / spacing + 0.5);
        if (platz < 0 || platz >= seccnt || sectab[platz] != 0) return -2;
        sectab[platz] = (uint8_t)(i + 1);
    }
    return 0;
}

/* Der Faktor, den das Modell waehlt. Er steht NICHT in der API.
 *
 * BERICHTIGT waehrend der Rot-Probe: hier stand zuerst die Formel aus
 * `uft_interleave.c:53-56` nachgerechnet — und gegen ein absichtlich
 * gestoertes Modell blieb die Zusage GRUEN, weil sie eine Konstante
 * gegen eine Konstante hielt. Eine Tautologie im eigenen Test, dieselbe
 * Gestalt wie MF-1014/MF-1026/MF-1031.
 *
 * Jetzt wird der Faktor GEMESSEN: das Modell legt Sektor 1 auf Platz 0
 * und Sektor 2 genau `interleave` Plaetze weiter — fuer den zweiten
 * Sektor kann die Freiplatzsuche noch nicht zugeschlagen haben, weil
 * nur Platz 0 belegt ist. Der Abstand IST also der Faktor.
 * Gibt -1 zurueck, wenn das Layout nicht ermittelbar ist. */
static int modell_layout(uint8_t *sectab, int seccnt, uint16_t ss);

static int modell_faktor_gemessen(int seccnt, uint16_t ss)
{
    uint8_t s[256];
    if (modell_layout(s, seccnt, ss) != 0) return -1;
    int p1 = -1, p2 = -1;
    for (int i = 0; i < seccnt; ++i) {
        if (s[i] == 1) p1 = i;
        if (s[i] == 2) p2 = i;
    }
    if (p1 < 0 || p2 < 0) return -1;
    return ((p2 - p1) % seccnt + seccnt) % seccnt;
}

/* Kleinste Abweichung ueber alle Rotationen. Ohne das waere jede Zahl
 * hier eine Aussage darueber, wo der Leser angefangen hat. */
static int min_rot(const uint8_t *a, const uint8_t *b, int n, int *rot_out)
{
    int best = n + 1, best_r = -1;
    for (int r = 0; r < n; ++r) {
        int d = 0;
        for (int i = 0; i < n; ++i) if (a[i] != b[(i + r) % n]) ++d;
        if (d < best) { best = d; best_r = r; }
    }
    if (rot_out) *rot_out = best_r;
    return best;
}

/* jhallen/atari-tools, `atr2imd.c:56-65`, Kommentar „Interleave map for
 * 90K/130K/180K disks". GPL v1 oder spaeter, (c) 2011 Joseph H. Allen,
 * Quellstand 835d5a6fc1258921949fe92400adb789c398b9c3. Nur gelesen.
 * Beachte: DIESELBE Tafel fuer 90 K und 180 K — jhallen unterscheidet
 * SD und DD nicht. */
static const uint8_t JH18[18] = {
    1,3,5,7,9,11,13,15,17,2,4,6,8,10,12,14,16,18 };
static const uint8_t JH26[26] = {
    1,3,5,7,9,11,13,15,17,19,21,23,25,2,4,6,8,10,12,14,16,18,20,22,24,26 };

/* ══════════════════════════════════════════════════════════════════ */

TEST(fortab_traegt_sechs_zeilen_und_zwei_unratbare_werte)
{
    /* (1) Das Fuellbyte der FM-Zwischenraeume ist $00, nicht $4E und
     * nicht $FF. Beide FM-Zeilen, alle drei Fuellbytes. */
    for (int r = R_SD_SLOW; r <= R_SD_FAST; ++r) {
        ASSERT(FORTAB[r][F_XGAPD] == 0x00);
        ASSERT(FORTAB[r][F_IGAPD] == 0x00);
        ASSERT(FORTAB[r][F_DGAPD] == 0x00);
    }
    /* und $4E in allen vier MFM-Zeilen — sonst waere die Aussage oben
     * keine Unterscheidung, sondern ein Zufall. */
    for (int r = R_ED_SLOW; r <= R_DD_FAST; ++r) {
        ASSERT(FORTAB[r][F_XGAPD] == 0x4E);
        ASSERT(FORTAB[r][F_IGAPD] == 0x4E);
        ASSERT(FORTAB[r][F_DGAPD] == 0x4E);
    }

    /* (2) Der Interleave haengt an der SIO-GESCHWINDIGKEIT, nicht am
     * Format. Je Dichte zwei verschiedene Werte, und der schnelle ist
     * jedes Mal kleiner. */
    ASSERT(FORTAB[R_SD_SLOW][F_IFAC] == 9  && FORTAB[R_SD_FAST][F_IFAC] == 5);
    ASSERT(FORTAB[R_ED_SLOW][F_IFAC] == 12 && FORTAB[R_ED_FAST][F_IFAC] == 6);
    ASSERT(FORTAB[R_DD_SLOW][F_IFAC] == 15 && FORTAB[R_DD_FAST][F_IFAC] == 7);
    for (int d = 0; d < 3; ++d) {
        const int slow = FORTAB[d * 2][F_IFAC], fast = FORTAB[d * 2 + 1][F_IFAC];
        CHECK(slow != fast, "der Interleave haengt doch nur am Format — "
                            "dann ist die Aussage dieses Tests falsch");
        ASSERT(fast < slow);
    }

    /* (3) Sektorzahl je Spur — dieselbe Zahl, die drei andere Quellen
     * fuer ED nennen (docs/spec_verification.json, atr-Zeile). */
    ASSERT(FORTAB[R_SD_SLOW][F_SECCNT] == 18);
    ASSERT(FORTAB[R_ED_SLOW][F_SECCNT] == 26);
    ASSERT(FORTAB[R_DD_SLOW][F_SECCNT] == 18);
    /* SD und DD haben dieselbe Sektorzahl und verschiedene Groessen —
     * die Sektorzahl allein unterscheidet sie also NICHT (D4). */
    ASSERT(FORTAB[R_SD_SLOW][F_SECCNT] == FORTAB[R_DD_SLOW][F_SECCNT]);
    ASSERT(ZEILE_SS[R_SD_SLOW] != ZEILE_SS[R_DD_SLOW]);
}

TEST(der_faktor_trifft_bei_sd_und_dd_und_verfehlt_ed)
{
    const int f_sd = modell_faktor_gemessen(18, 128);
    const int f_ed = modell_faktor_gemessen(26, 128);
    const int f_dd = modell_faktor_gemessen(18, 256);

    printf("\n      Faktor:  SD Modell %d / fw %d %s | ED %d / %d %s | "
           "DD %d / %d %s\n",
           f_sd, FORTAB[R_SD_SLOW][F_IFAC],
           f_sd == FORTAB[R_SD_SLOW][F_IFAC] ? "=" : "!=",
           f_ed, FORTAB[R_ED_SLOW][F_IFAC],
           f_ed == FORTAB[R_ED_SLOW][F_IFAC] ? "=" : "!=",
           f_dd, FORTAB[R_DD_SLOW][F_IFAC],
           f_dd == FORTAB[R_DD_SLOW][F_IFAC] ? "=" : "!=");

    /* Zweimal trifft es — und das ist kein Zufall, sondern eine
     * unabhaengige Bestaetigung einer a8rawconv-Formel durch das ROM
     * des Laufwerks. */
    ASSERT(f_sd == 9  && f_sd == FORTAB[R_SD_SLOW][F_IFAC]);
    ASSERT(f_dd == 15 && f_dd == FORTAB[R_DD_SLOW][F_IFAC]);

    /* Einmal nicht: 13 gegen 12. Das ist der Befund, und er wird hier
     * FESTGENAGELT, nicht behoben — welche der beiden Zahlen auf einer
     * Atari-Diskette steht, ist offen (P3-434). */
    ASSERT(f_ed == 13);
    ASSERT(FORTAB[R_ED_SLOW][F_IFAC] == 12);
    CHECK(f_ed != FORTAB[R_ED_SLOW][F_IFAC],
          "ROT-PROBE verfehlt: Modell und Firmware sind bei ED doch gleich — "
          "dann ist P3-434 erledigt und dieser Test gehoert umgeschrieben");

    /* Und KEIN Modellwert trifft eine der drei schnellen Zeilen. Das
     * Modell hat kein Geschwindigkeitsargument, kann also gar nicht. */
    for (int d = 0; d < 3; ++d) {
        const int n = FORTAB[d * 2][F_SECCNT];
        const int f = modell_faktor_gemessen(n, ZEILE_SS[d * 2]);
        ASSERT(f != FORTAB[d * 2 + 1][F_IFAC]);
    }
}

TEST(das_layout_ist_bei_sd_identisch_und_nur_gedreht)
{
    uint8_t fw[256], mo[256];
    int rot = -1;

    firmware_layout(fw, 18, FORTAB[R_SD_SLOW][F_IFAC]);
    ASSERT(modell_layout(mo, 18, 128) == 0);

    /* Ungedreht weichen ALLE 18 ab ... */
    int d_roh = 0;
    for (int i = 0; i < 18; ++i) if (mo[i] != fw[i]) ++d_roh;
    ASSERT(d_roh == 18);

    /* ... und rotationsbereinigt KEINER. Dieselbe Diskette, anderer
     * Anfangsplatz. Die Rotation ist 9 = IFAC, weil die Firmware bei
     * SECCNT - IFAC beginnt und das Modell bei 0. */
    const int d = min_rot(mo, fw, 18, &rot);
    printf("\n      SD: roh %d von 18, rotationsbereinigt %d bei Rotation %d\n",
           d_roh, d, rot);
    ASSERT(d == 0);
    ASSERT(rot == 9);
    ASSERT(rot == FORTAB[R_SD_SLOW][F_IFAC]);

    /* ROT-PROBE (D1). Vor MF-479 stand in uft_atx.c `s / n` — gleiche
     * Abstaende, Sektor k auf Platz k-1. Wenn DIESE Anordnung die
     * Firmware auch trifft, sagt der Vergleich oben nichts. */
    uint8_t naiv[256];
    int r0 = -1;
    memset(naiv, 0, sizeof(naiv));
    for (int i = 0; i < 18; ++i) naiv[i] = (uint8_t)(i + 1);
    const int d_naiv = min_rot(naiv, fw, 18, &r0);
    printf("      ROT-PROBE: die naive Gleichverteilung weicht "
           "rotationsbereinigt in %d von 18 ab\n", d_naiv);
    CHECK(d_naiv > 0, "ROT-PROBE verfehlt: die naive Gleichverteilung trifft "
                      "die Firmware ZUFAELLIG — dann ist dieser Vergleich "
                      "wertlos");
}

TEST(das_layout_weicht_bei_ed_und_dd_wirklich_ab)
{
    uint8_t fw[256], mo[256];
    int rot;

    /* ED: 26 Sektoren, Faktor 13 gegen IFAC 12. */
    firmware_layout(fw, 26, FORTAB[R_ED_SLOW][F_IFAC]);
    ASSERT(modell_layout(mo, 26, 128) == 0);
    const int d_ed = min_rot(mo, fw, 26, &rot);
    printf("\n      ED: rotationsbereinigt %d von 26 (Rotation %d)\n", d_ed, rot);
    ASSERT(d_ed == 24);
    CHECK(d_ed > 0, "ROT-PROBE verfehlt: bei ED ist es doch nur eine Drehung — "
                    "dann ist der Befund kein Befund");

    /* DD: derselbe Faktor 15, und trotzdem ein anderes Layout — weil
     * die Firmware ABWAERTS schreitet und das Modell AUFWAERTS.
     * Gleicher Zahlenwert, gegenlaeufige Richtung. */
    firmware_layout(fw, 18, FORTAB[R_DD_SLOW][F_IFAC]);
    ASSERT(modell_layout(mo, 18, 256) == 0);
    const int d_dd = min_rot(mo, fw, 18, &rot);
    printf("      DD: Faktor BEIDE 15, rotationsbereinigt %d von 18 "
           "(Rotation %d)\n", d_dd, rot);
    ASSERT(modell_faktor_gemessen(18, 256) == FORTAB[R_DD_SLOW][F_IFAC]);
    ASSERT(d_dd == 16);
    CHECK(d_dd > 0, "ROT-PROBE verfehlt: gleicher Faktor UND gleiches Layout — "
                    "dann ist die Richtungsaussage falsch");
}

TEST(drei_quellen_und_bei_dd_stimmt_keine_mit_keiner)
{
    uint8_t fw[256], mo[256];

    /* SD: alle drei identisch (bis auf die Drehung). Drei Haende —
     * a8rawconvs Formel, jhallens Tafel, das ROM des Laufwerks. */
    firmware_layout(fw, 18, FORTAB[R_SD_SLOW][F_IFAC]);
    ASSERT(modell_layout(mo, 18, 128) == 0);
    ASSERT(min_rot(mo, JH18, 18, NULL) == 0);
    ASSERT(min_rot(fw, JH18, 18, NULL) == 0);
    ASSERT(min_rot(mo, fw,   18, NULL) == 0);

    /* ED: Modell und jhallen gleich, Firmware allein. */
    firmware_layout(fw, 26, FORTAB[R_ED_SLOW][F_IFAC]);
    ASSERT(modell_layout(mo, 26, 128) == 0);
    ASSERT(min_rot(mo, JH26, 26, NULL) == 0);
    ASSERT(min_rot(fw, JH26, 26, NULL) == 24);

    /* DD: drei Angaben, keine zwei gleich — die Gestalt aus MF-1015
     * (drei Pruefsummen) und MF-1026 (drei Victor-Geometrien).
     * jhallen benutzt fuer 180 K dieselbe Tafel wie fuer 90 K. */
    firmware_layout(fw, 18, FORTAB[R_DD_SLOW][F_IFAC]);
    ASSERT(modell_layout(mo, 18, 256) == 0);
    ASSERT(min_rot(mo, JH18, 18, NULL) == 16);
    ASSERT(min_rot(fw, JH18, 18, NULL) == 16);
    ASSERT(min_rot(mo, fw,   18, NULL) == 16);
    printf("\n      DD: Modell/jhallen 16, Firmware/jhallen 16, "
           "Modell/Firmware 16 — von 18\n");
    CHECK(!(min_rot(mo, JH18, 18, NULL) == 0 || min_rot(fw, JH18, 18, NULL) == 0),
          "ROT-PROBE verfehlt: bei DD stimmt doch eine mit jhallen ueberein — "
          "dann ist die Dreier-Aussage falsch");
}

TEST(die_firmware_belegt_ihre_eigene_datenrate)
{
    /* FORTRK.M65:185 schreibt XGAPC Byte und nennt sie „5MS INDEX GAP".
     * Zwei unabhaengige Angaben in derselben Zeile: eine Byteanzahl und
     * eine Zeit. Zusammen ergeben sie die Datenrate — und die muss die
     * von FM sein, sonst irrt eine der beiden. */
    const double bytes = (double)FORTAB[R_SD_SLOW][F_XGAPC];  /* 78 */
    const double ms    = 5.0;
    const double gemessen = bytes / (ms / 1000.0);            /* Byte/s */
    const double fm_125k  = 125000.0 / 8.0;                   /* 15625  */
    const double abw = fabs(gemessen - fm_125k) / fm_125k;
    printf("\n      %.0f Byte / %.0f ms = %.0f B/s, FM 125 kbit/s = %.0f B/s, "
           "Abweichung %.2f %%\n", bytes, ms, gemessen, fm_125k, abw * 100.0);
    ASSERT(abw < 0.01);

    /* Gegenprobe: mit der MFM-Zahl derselben Spalte (156) geht es NICHT
     * auf — die Uebereinstimmung oben ist also keine Rundungstoleranz,
     * die alles durchlaesst. */
    const double mfm_zahl = (double)FORTAB[R_ED_SLOW][F_XGAPC] / (ms / 1000.0);
    CHECK(fabs(mfm_zahl - fm_125k) / fm_125k > 0.01,
          "ROT-PROBE verfehlt: auch die MFM-Zahl trifft die FM-Rate — dann "
          "prueft diese Zusage nur, dass zwei Zahlen ungefaehr gleich sind");

    /* Und 156 = 2 x 78: MFM packt doppelt so viele Byte in dieselbe Zeit.
     * Das ist die Selbstkonsistenz, die die Tafel mitbringt. */
    ASSERT(FORTAB[R_ED_SLOW][F_XGAPC] == 2 * FORTAB[R_SD_SLOW][F_XGAPC]);
    ASSERT(FORTAB[R_DD_SLOW][F_XGAPC] == 2 * FORTAB[R_SD_SLOW][F_XGAPC]);
}

TEST(der_percom_block_liegt_rueckwaerts_im_speicher)
{
    /* EQUATES.M65:350-405 erklaert zwoelf Byte, und zwar in dieser
     * Reihenfolge — PCRES2 zuerst, PCTRCK zuletzt:
     *
     *   Offset  0  1  2  3  4  5  6  7  8  9 10 11
     *   Feld  RES2 RES1 BAUD ACTV BYTL BYTH DENS SIDE SECL SECH RATE TRCK
     *
     * Das ist die UMKEHRUNG der bekannten Percom-Reihenfolge. Belegt
     * ist die Umkehrung NICHT durch Zutrauen, sondern durch zwei
     * unabhaengige Messungen im selben Archiv:
     *
     * (a) `SERIN.M65:300-310`: RBUF speichert nach `BUFF,X` und macht
     *     dann `DEX` — der ERSTE Draht-Byte landet also am HOECHSTEN
     *     Index. BUFF = $FF und die Nullseite wickelt, also ist
     *     Offset = X - 1.
     * (b) `EQUATES.M65:315-330` erklaert das SIO-Kommandoframe mit
     *     DAUX2 = 0, DAUX1 = 1, DCMD = 2, DID = 3 — und auf dem Draht
     *     kommt DID ZUERST. Ein Frame, dessen Reihenfolge bekannt ist,
     *     eicht damit die Richtung von RBUF.
     *
     * Unter dieser Richtung ist Draht-Byte 0 des Percom-Blocks das Feld
     * mit Offset 11 = PCTRCK = „Spuren je Seite" — genau das, was der
     * kanonische Percom-Block als Byte 0 fuehrt. Zwei Frames, eine
     * Regel, beide gehen auf. */
    enum { N_PC = 12 };
    static const char *FELD[N_PC] = {  /* nach Speicheroffset */
        "PCRES2", "PCRES1", "PCBAUD", "PCACTV", "PCBYTL", "PCBYTH",
        "PCDENS", "PCSIDE", "PCSECL", "PCSECH", "PCRATE", "PCTRCK" };

    /* Die Eichung am Kommandoframe: 4 Byte, DID zuerst auf dem Draht,
     * DID auf Offset 3. */
    const int frame_n = 4, did_offset = 3;
    ASSERT(frame_n - 1 - did_offset == 0);   /* Draht-Byte 0 == DID */

    /* Dieselbe Regel auf zwoelf Byte: Draht = 11 - Offset. */
    for (int draht = 0; draht < N_PC; ++draht) {
        const int offset = N_PC - 1 - draht;
        ASSERT(offset >= 0 && offset < N_PC);
        if (draht == 0) ASSERT(strcmp(FELD[offset], "PCTRCK") == 0);
        if (draht == 1) ASSERT(strcmp(FELD[offset], "PCRATE") == 0);
        if (draht == 3) ASSERT(strcmp(FELD[offset], "PCSECL") == 0);
        if (draht == 4) ASSERT(strcmp(FELD[offset], "PCSIDE") == 0);
        if (draht == 5) ASSERT(strcmp(FELD[offset], "PCDENS") == 0);
    }

    /* ZWEITE QUELLE, und hier liegt die Falle. Erwin Reuss, „Die Formate
     * der XF551", Compy-Shop-Magazin 4/88, nennt „Byte 4" als
     * Sektoren/Track. Nullbasiert waere Byte 4 = PCSIDE — „Seiten minus
     * eins", nicht Sektoren. EINSBASIERT ist Byte 4 = Draht-Byte 3 =
     * PCSECL, und das stimmt. Die messbare Quelle entscheidet also die
     * Zaehlweise der Prosa-Quelle, nicht umgekehrt. */
    const int reuss_1basiert = 4;
    const int draht_aus_reuss = reuss_1basiert - 1;
    ASSERT(strcmp(FELD[N_PC - 1 - draht_aus_reuss], "PCSECL") == 0);
    CHECK(strcmp(FELD[N_PC - 1 - reuss_1basiert], "PCSECL") != 0,
          "ROT-PROBE verfehlt: die nullbasierte Lesart trifft PCSECL AUCH — "
          "dann entscheidet dieser Test die Zaehlweise nicht");
    ASSERT(strcmp(FELD[N_PC - 1 - reuss_1basiert], "PCSIDE") == 0);
}

TEST(die_drei_unbekannten_percom_bytes_werden_genullt)
{
    /* Der Auftrag lautete, die Firmware FUELLE die drei Byte, die der
     * Artikel als „???" fuehrt. Gemessen ist das SO NICHT richtig, und
     * das gehoert gesagt.
     *
     * `CONFIG.M65:350-374` (SCONF, Befehl $4E): der ganze Zwoelfer wird
     * mit 0 ueberschrieben, dann werden GENAU VIER Groessen gesetzt —
     * PCTRCK = 40, PCACTV = 1, PCBYTH/PCBYTL und PCDENS/PCSECL nach
     * Dichte. PCRATE, PCSECH, PCBAUD, PCRES1 und PCRES2 bleiben 0.
     *
     * Was die Firmware WIRKLICH beitraegt, ist ein NAME: Draht-Byte 9
     * heisst dort `PCBAUD`. Ein Name ist mehr als „???" und weniger als
     * ein Wert. */
    static const int gesetzt[] = { 0 /*TRCK*/, 3 /*SECL*/, 5 /*DENS*/,
                                   6 /*BYTH*/, 7 /*BYTL*/, 8 /*ACTV*/ };
    static const int genullt[] = { 1 /*RATE*/, 2 /*SECH*/, 4 /*SIDE*/,
                                   9 /*BAUD*/, 10 /*RES1*/, 11 /*RES2*/ };
    const size_t n_ges = sizeof(gesetzt) / sizeof(gesetzt[0]);
    const size_t n_nul = sizeof(genullt) / sizeof(genullt[0]);
    ASSERT(n_ges + n_nul == 12u);

    /* Die drei „???"-Byte des Artikels sind die letzten drei, und alle
     * drei stehen in der genullt-Liste. */
    for (int i = 9; i <= 11; ++i) {
        bool ist_genullt = false;
        for (size_t k = 0; k < n_nul; ++k) if (genullt[k] == i) ist_genullt = true;
        CHECK(ist_genullt, "die Firmware fuellt eines der drei unbekannten "
                           "Percom-Byte doch — dann ist die Berichtigung "
                           "in diesem Test falsch");
    }
    /* Gegenprobe: die vier Groessen, die SCONF WIRKLICH setzt, stehen
     * NICHT in der genullt-Liste — sonst pruefte die Zusage oben nur,
     * dass eine Liste eine Zahl enthaelt. */
    for (size_t k = 0; k < n_ges; ++k) {
        bool kollision = false;
        for (size_t j = 0; j < n_nul; ++j)
            if (genullt[j] == gesetzt[k]) kollision = true;
        CHECK(!kollision, "ROT-PROBE verfehlt: ein Feld steht in beiden "
                          "Listen — dann ist die Aufteilung falsch");
    }

    /* Und die Werte, die SCONF schreibt (CONFIG.M65:380-500). */
    const int sconf_tracks = 40;      /* CONFIG.M65:380 LDA #40 */
    const int sconf_actv   = 1;       /* CONFIG.M65:390 INC PCACTV */
    const int sconf_sec_sd = 18;      /* CONFIG.M65:480 LDA #18 */
    const int sconf_sec_ed = 26;      /* CONFIG.M65:495 LDA #26 */
    ASSERT(sconf_tracks == 40 && sconf_actv == 1);
    ASSERT(sconf_sec_sd == FORTAB[R_SD_SLOW][F_SECCNT]);
    ASSERT(sconf_sec_ed == FORTAB[R_ED_SLOW][F_SECCNT]);
    /* SCONF nennt nur EINE Spurzahl fuer alle drei Dichten — 40 — und
     * PCSIDE bleibt 0. Die XF551-Quad-Density mit 40 Spuren auf ZWEI
     * Seiten (MF-1175) kann dieses Laufwerk also nicht melden, und
     * GCONF weist jede Diskette mit PCSIDE != 0 ausdruecklich ab
     * (CONFIG.M65:115-120). Der Percom-Block KANN zwei Seiten tragen;
     * diese Firmware nutzt es nicht. */
    ASSERT(sconf_tracks == 40);
}

TEST(die_atari_geometrie_hat_kein_fdc_profil_und_das_ist_richtig)
{
    /* FORTRK.M65 zeigt den Spuraufbau: $FE als ID-AM (Z. 350), $FB als
     * Daten-AM (Z. 610) — und KEIN $FC. Die Atari-Formate haben also
     * kein Index-Address-Mark, genau das Feld, das MF-1177 eingefuehrt
     * hat. Im FM-Fall wird auch kein eigenes Sync-Feld geschrieben: der
     * MFM-Zweig legt 12 x $00 und 3 x $F5 vor das AM (Z. 275-340), der
     * FM-Zweig springt darueber hinweg. IGAPC Byte $00 sind Gap UND Sync.
     *
     * Trotzdem kommen hier KEINE Atari-Zeilen in die Profiltafel: ohne
     * `track_bytes` liesse sich `uft_fdc_gap_space()` nicht rechnen, und
     * eine Spurbilanz hat keine Quelle — die Firmware nennt „1 MS TO
     * INDEX" mit 16 Takten (FORTRK.M65:120) und „201+5MS DATA" mit 201
     * Takten (Z. 215), und solange die Taktlaenge von STIM1/STIM2
     * unbekannt ist, widersprechen sich die beiden. S1, P3-435.
     *
     * Gemessen wird deshalb der IST-Zustand: die Tafel kennt diese
     * Geometrie nicht. Diese Zusage wird rot, sobald jemand eine
     * Atari-Zeile hinzufuegt — und dann ist P3-435 zu beantworten. */
    unsigned kandidaten = 99u;
    const uft_fdc_format_t *f =
        uft_fdc_detect_format_counted(40, 1, 18, 128, &kandidaten);
    printf("\n      40x1x18x128 (Atari SD): %u Kandidaten in der Tafel\n",
           kandidaten);
    ASSERT(f == NULL);
    ASSERT(kandidaten == 0u);

    /* Gegenprobe: eine Geometrie, die die Tafel SEHR WOHL kennt — sonst
     * prueft die Zusage oben nur, dass die Funktion immer NULL gibt. */
    unsigned k2 = 0u;
    const uft_fdc_format_t *pc =
        uft_fdc_detect_format_counted(80, 2, 18, 512, &k2);
    CHECK(pc != NULL && k2 >= 1u,
          "ROT-PROBE verfehlt: uft_fdc_detect_format_counted findet auch "
          "80x2x18x512 nicht — dann sagt die Atari-Absage oben nichts");

    /* Und die Zahl der Profile ist unveraendert 17 (MF-1177) — gemessen
     * gegen die TAFEL, nicht nur gegen das Makro. Zwei Aussagen ueber
     * dieselbe Sache liegen im Header nebeneinander (`UFT_FDC_FORMATS[]`
     * Z. 690 und `UFT_FDC_FORMAT_COUNT` Z. 711), und dieser Baum hat
     * dreimal gemessen, was daraus wird, wenn niemand sie vergleicht
     * (MF-1015).
     *
     * Die Tafel ist NULL-terminiert, hat also COUNT + 1 Elemente. Was
     * hier geprueft wird, ist die Stelle des Waechters: er muss GENAU
     * dort sitzen, wo COUNT ihn erwartet. Faellt das auseinander, sehen
     * die beiden Laufarten im Baum verschiedene Mengen — `i < COUNT`
     * uebergeht den neuen Eintrag still, `bis NULL` nimmt ihn mit.
     * Genau diese Lage hat MF-1177 bei den Zwischenraumzahlen gemessen. */
    ASSERT(UFT_FDC_FORMAT_COUNT == 17);
    ASSERT(sizeof(UFT_FDC_FORMATS) / sizeof(UFT_FDC_FORMATS[0])
           == (size_t)UFT_FDC_FORMAT_COUNT + 1u);
    ASSERT(UFT_FDC_FORMATS[UFT_FDC_FORMAT_COUNT] == NULL);
    ASSERT(UFT_FDC_FORMATS[UFT_FDC_FORMAT_COUNT - 1] != NULL);
    for (int i = 0; i < UFT_FDC_FORMAT_COUNT; ++i)
        ASSERT(UFT_FDC_FORMATS[i] != NULL);
}

int main(void)
{
    printf("=== 1050-Turbo-Firmware als Quellengattung (MF-1179) ===\n");
    RUN(fortab_traegt_sechs_zeilen_und_zwei_unratbare_werte);
    RUN(der_faktor_trifft_bei_sd_und_dd_und_verfehlt_ed);
    RUN(das_layout_ist_bei_sd_identisch_und_nur_gedreht);
    RUN(das_layout_weicht_bei_ed_und_dd_wirklich_ab);
    RUN(drei_quellen_und_bei_dd_stimmt_keine_mit_keiner);
    RUN(die_firmware_belegt_ihre_eigene_datenrate);
    RUN(der_percom_block_liegt_rueckwaerts_im_speicher);
    RUN(die_drei_unbekannten_percom_bytes_werden_genullt);
    RUN(die_atari_geometrie_hat_kein_fdc_profil_und_das_ist_richtig);
    printf("\n=== %d bestanden, %d gefallen ===\n", _pass, _fail);
    return _fail ? 1 : 0;
}
