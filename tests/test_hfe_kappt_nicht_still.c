/**
 * @file test_hfe_kappt_nicht_still.c
 * @brief Ein ZUVIEL war still — bei einer 2,88-M-Diskette die Haelfte
 *        (MF-1170)
 *
 * ── Der Befund ──────────────────────────────────────────────────────────
 *
 * `uftc_convert_sectors_to_hfe()` leitet die IMG-Geometrie aus der
 * Dateigroesse ab, und der letzte Zweig ist ein `else` OHNE obere Schranke:
 * alles ueber 1 228 800 Byte wird 80 x 2 x 18 x 512. Geprueft wurde danach
 * nur
 *
 *     if (src_size < expected_size)  -> Warnung „padding with zeros"
 *
 * Ein ZUVIEL sieht diese Bedingung nicht. Die Sektorschleife rechnet
 *
 *     src_offset = ((cyl * heads * sectors) + hd * sectors + sec)
 *                  * sector_size,        mit cyl < cylinders
 *
 * erreicht also hoechstens `expected_size`. Gemessen an einer 2,88-M-Datei
 * (80 x 2 x 36 x 512 = 2 949 120 Byte): **1 474 560 Byte wurden nie
 * angefasst — genau die Haelfte, weil 36 = 2 x 18 — und `UFT_OK` kam
 * zurueck.** Klasse MF-1001/1022/1038/1040.
 *
 * ── Die schaerfste Form: derselbe Fehler war einen Zweig weiter behoben ─
 *
 * Die ADF-Seite DERSELBEN Funktion prueft das seit MF-1081 ausdruecklich —
 * dort steht „Der REST, nicht nur der Quotient", weil 901 119 Byte
 * ganzzahlig 79 Zylinder ergaben und still eine Spur verwarfen. Die
 * IMG-Seite hat diese Pruefung nie bekommen. Zwei Zweige einer Funktion,
 * einer gehaertet: Klasse MF-519/529/1026, und MF-1164 hat dieselbe Gestalt
 * schon einmal INNERHALB einer Datei gefunden.
 *
 * ── Und warum der naheliegende Fix verworfen ist ────────────────────────
 *
 * Der erste Entwurf von MF-1170 war ein 2,88-M-Zweig mit den Zahlen aus der
 * Profiltafel (`uft_fdc_detect_format(80, 2, 36, 512)` liefert eindeutig
 * „PC 2.88M" mit rpm 300 und raw_bits 400 000, und `HFE_IF_IBMPC_ED = 0x08`
 * liegt ungenutzt im Header). Die Messung hat ihn verworfen:
 * `hfe_track_entry_t.length` ist ein `uint16_t` und traegt die Laenge eines
 * SPURPAARS.
 *
 *      250 kbps / 300 U/min  ->  12 544 je Seite  ->  Paar  25 088  passt
 *      500 kbps / 360 U/min  ->  20 992           ->  Paar  41 984  passt
 *      500 kbps / 300 U/min  ->  25 088           ->  Paar  50 176  passt
 *     1000 kbps / 300 U/min  ->  50 176           ->  Paar 100 352  NICHT
 *
 * `(uint16_t)100352` ist **34 816** — eine Spurtabelle, die auf ein Drittel
 * der Daten zeigt, also ein ZWEITER stiller Verlust ueber dem ersten.
 * **HFE v1 kann eine 2,88-M-Spur nicht tragen**; die Grenze liegt bei
 * 32 767 Byte je Seite, etwa 655 kbps bei 300 U/min. Der Weg dorthin ist
 * HFE v3, nicht ein Zweig im Wandler.
 *
 * ── Zugang ──────────────────────────────────────────────────────────────
 *
 * Der Kopf wird BYTEWEISE nach der HxC-Feldlage gelesen, nicht ueber
 * `hfe_header_t`. Ein Test, der seine Erwartung aus derselben Struktur holt
 * wie der Pruefling, kann die Struktur nicht pruefen (MF-1000).
 *
 * ── Rotbeweis ───────────────────────────────────────────────────────────
 *
 * Vor MF-1170 fallen `zweikommaachtundachtzig_wird_abgesagt` und
 * `ein_byte_zu_viel_wird_abgesagt` (beide gaben `UFT_OK`), und
 * `absage_laesst_keine_datei_zurueck` fiel mit ihnen, weil eine Datei
 * entstand. Gruen vor UND nach: `eineinhalb_geht_weiterhin`,
 * `zu_kleine_datei_wird_weiter_gepolstert` und
 * `spurtabelle_grenze_ist_arithmetisch_belegt` — die ersten zwei belegen,
 * dass die Absage nicht alles ablehnt, die dritte ist reine Arithmetik.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_format_convert.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern uft_error_t uftc_convert_sectors_to_hfe(const uint8_t *src_data,
                                               size_t src_size,
                                               const char *dst_path,
                                               uft_format_t src_format,
                                               const uft_convert_options_ext_t *opts,
                                               uft_convert_result_t *result);

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* HxC-HFE-v1-Kopf, Feldlage aus der Formatbeschreibung — bewusst als
 * Versatzkonstanten und nicht ueber `hfe_header_t`. */
#define HFE_OFF_BITRATE      12u   /* LE16, kbit/s */
#define HFE_OFF_RPM          14u   /* LE16          */
#define HFE_OFF_IFACE        16u
#define HFE_OFF_LUT_BLOCK    18u   /* LE16, in 512-Byte-Bloecken */

/* Die Groessen, um die es geht — je aus ihrer Geometrie gerechnet und nicht
 * als nackte Zahl hingeschrieben. */
#define GR_1440K  ((size_t)80u * 2u * 18u * 512u)   /* 1 474 560 */
#define GR_1200K  ((size_t)80u * 2u * 15u * 512u)   /* 1 228 800 */
#define GR_2880K  ((size_t)80u * 2u * 36u * 512u)   /* 2 949 120 */

static uint8_t g_img[GR_2880K];        /* groesster Fall */
static uint8_t g_kopf[2048];           /* Kopf + erster LUT-Block */

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

/* Fuellt `g_img` bis `n` mit einem Muster, das JEDE Stelle unterscheidbar
 * macht — sonst saehe eine halb gelesene Datei wie eine ganze aus. */
static void muster(size_t n)
{
    for (size_t i = 0; i < n; i++)
        g_img[i] = (uint8_t)(i * 7u + (i >> 9) * 13u + 1u);
}

static int datei_existiert(const char *pfad)
{
    FILE *f = fopen(pfad, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

/* Wandelt und gibt den Rueckgabewert zurueck; liest bei Erfolg die
 * Kopfbytes nach `g_kopf`. */
static uft_error_t wandeln(size_t n, const char *pfad, size_t *gelesen)
{
    uft_convert_options_ext_t o;
    uft_convert_result_t r;

    memset(&o, 0, sizeof(o));
    o.accept_data_loss = true;        /* Sektor->Bitstrom ist verlustbehaftet */
    memset(&r, 0, sizeof(r));

    remove(pfad);
    uft_error_t rc = uftc_convert_sectors_to_hfe(g_img, n, pfad,
                                                 UFT_FORMAT_IMG, &o, &r);
    if (gelesen) *gelesen = 0;
    if (rc == UFT_OK && gelesen) {
        FILE *f = fopen(pfad, "rb");
        if (f) {
            *gelesen = fread(g_kopf, 1, sizeof(g_kopf), f);
            fclose(f);
        }
    }
    return rc;
}

/* ── ROTBEWEIS 1: die 2,88-M-Diskette ────────────────────────────────── */

TEST(zweikommaachtundachtzig_wird_abgesagt)
{
    /* 2 949 120 Byte. Vor MF-1170 kam UFT_OK zurueck und die Ausgabe war
     * eine 1,44-M-HFE: 1 474 560 Byte — genau die Haelfte — waren still
     * weg. Die Absage nennt jetzt den Grund. */
    muster(GR_2880K);
    const char *pfad = "uft_hfe_2880_absage.hfe";
    size_t gelesen = 0;
    uft_error_t rc = wandeln(GR_2880K, pfad, &gelesen);

    ASSERT(rc != UFT_OK);
    /* und die Haelfte ist keine Rundung: 36 = 2 x 18 */
    ASSERT(GR_2880K - GR_1440K == GR_1440K);
    remove(pfad);
}

/* ── ROTBEWEIS 2: EIN Byte zu viel ───────────────────────────────────── */

TEST(ein_byte_zu_viel_wird_abgesagt)
{
    /* Die schaerfere Fassung desselben Befunds. 1 474 561 Byte fallen in
     * denselben `else`-Zweig, `expected_size` ist 1 474 560 — vor MF-1170
     * verschwand das letzte Byte ohne ein Wort. */
    muster(GR_1440K + 1u);
    const char *pfad = "uft_hfe_plus1_absage.hfe";
    size_t gelesen = 0;
    ASSERT(wandeln(GR_1440K + 1u, pfad, &gelesen) != UFT_OK);
    remove(pfad);
}

TEST(absage_laesst_keine_datei_zurueck)
{
    /* Eine Absage, die eine halbe Datei hinterlaesst, ist ein zweiter
     * Befund (vgl. `test_convert_leaves_no_ghost.c`). Die Schranke greift
     * VOR dem Bau des Behaelters, also darf nichts entstehen. */
    muster(GR_2880K);
    const char *pfad = "uft_hfe_2880_geist.hfe";
    remove(pfad);
    ASSERT(wandeln(GR_2880K, pfad, NULL) != UFT_OK);
    ASSERT(!datei_existiert(pfad));
    remove(pfad);
}

/* ── Gegenprobe: was passt, geht weiterhin ───────────────────────────── */

TEST(eineinhalb_geht_weiterhin)
{
    /* Gruen vor UND nach MF-1170. 1 474 560 Byte gehen restlos in
     * 80 x 2 x 18 x 512 auf. Die Kopfwerte sind hier unabhaengig
     * hergeleitet: 500 kbit/s bei 300 U/min sind 500000 * 0,2 = 100 000
     * Datenbits, verdoppelt 200 000 Zellen, aufgerundet 25 000 Byte, auf
     * 256 ausgerichtet 25 088 — und die Spurtabelle fuehrt das PAAR, also
     * 50 176. */
    muster(GR_1440K);
    const char *pfad = "uft_hfe_1440_ok.hfe";
    size_t gelesen = 0;
    ASSERT(wandeln(GR_1440K, pfad, &gelesen) == UFT_OK);
    ASSERT(gelesen >= 1024u);
    ASSERT(memcmp(g_kopf, "HXCPICFE", 8) == 0);
    ASSERT(le16(g_kopf + HFE_OFF_BITRATE) == 500u);
    ASSERT(le16(g_kopf + HFE_OFF_RPM)     == 300u);

    /* Eintrag 0 der Spurtabelle: Versatz (LE16, in 512-Byte-Bloecken),
     * dann Laenge (LE16, in Byte). */
    unsigned lut_block = le16(g_kopf + HFE_OFF_LUT_BLOCK);
    ASSERT(lut_block >= 1u);
    size_t lut_off = (size_t)lut_block * 512u;
    ASSERT(lut_off + 4u <= gelesen);
    ASSERT(le16(g_kopf + lut_off + 2u) == 50176u);
    remove(pfad);
}

TEST(vier_groessen_gehen_weiterhin)
{
    /* Gruen vor UND nach MF-1174, und die eigentliche Absicherung: die vier
     * Groessen, die die Bereiche EXAKT treffen, gehen weiter durch. Ohne
     * diesen Fall waere „lehnt alles ab, was nicht genau passt" eine
     * genauso gruene Zusage. */
    struct { size_t n; unsigned rpm; const char *pfad; } f[] = {
        { (size_t)40u * 2u *  9u * 512u, 300u, "uft_hfe_360_ok.hfe"  },
        { (size_t)80u * 2u *  9u * 512u, 300u, "uft_hfe_720_ok.hfe"  },
        { GR_1200K,                      360u, "uft_hfe_1200_ok.hfe" },
        { GR_1440K,                      300u, "uft_hfe_1440b_ok.hfe"},
    };
    for (unsigned i = 0; i < sizeof(f) / sizeof(f[0]); i++) {
        size_t gelesen = 0;
        muster(f[i].n);
        ASSERT(wandeln(f[i].n, f[i].pfad, &gelesen) == UFT_OK);
        ASSERT(gelesen >= 1024u);
        /* 1,2 M laeuft im 5,25-Zoll-HD-Laufwerk mit 360 U/min (MF-1166) */
        ASSERT(le16(g_kopf + HFE_OFF_RPM) == f[i].rpm);
        remove(f[i].pfad);
    }
}

/* ── ROTBEWEIS 4 (MF-1174): zu kurz ist auch eine falsche Geometrie ──── */

TEST(zu_kurze_datei_wird_abgesagt)
{
    /* Bis MF-1174 stand hier das Gegenteil: dieser Test hat in MF-1170
     * festgenagelt, dass eine zu kurze Datei ANGENOMMEN wird. Gemessen war
     * das der einzige Abhaengige der Fuellfaehigkeit im ganzen Baum — und
     * die Messung dazu hat gezeigt, dass „zu kurz" fast immer eine FALSCHE
     * GEOMETRIE ist und nicht eine kurze Diskette: von zwoelf echten
     * Groessen treffen die vier Bereiche nur vier exakt.
     *
     * 1 228 288 Byte sind 512 weniger als der 1,2-M-Zweig erwartet. Vorher
     * kamen UFT_OK und 512 erfundene 0xE5-Byte heraus, als Sektordaten
     * gemeldet. */
    muster(GR_1200K - 512u);
    const char *pfad = "uft_hfe_kurz_absage.hfe";
    ASSERT(wandeln(GR_1200K - 512u, pfad, NULL) != UFT_OK);
    ASSERT(!datei_existiert(pfad));
    remove(pfad);

    /* Und die schaerfere Fassung, symmetrisch zum Byte zu viel oben:
     * EIN Byte zu wenig. */
    muster(GR_1440K - 1u);
    pfad = "uft_hfe_minus1_absage.hfe";
    ASSERT(wandeln(GR_1440K - 1u, pfad, NULL) != UFT_OK);
    remove(pfad);

    /* Die Klasse, die die Messung gefunden hat: eine 160-K-Diskette
     * (40 x 1 x 8 x 512) wurde als 40 x 2 x 9 x 512 gelesen — Kopfzahl und
     * Sektorzahl falsch — und 204 800 der 368 640 Byte waren erfunden,
     * mehr als die Diskette selbst hat. */
    muster((size_t)40u * 1u * 8u * 512u);
    pfad = "uft_hfe_160_absage.hfe";
    ASSERT(wandeln((size_t)40u * 1u * 8u * 512u, pfad, NULL) != UFT_OK);
    remove(pfad);
}

/* ── Die Grenze der Spurtabelle, arithmetisch ────────────────────────── */

TEST(spurtabelle_grenze_ist_arithmetisch_belegt)
{
    /* Die Absage im Wandler ist heute UNERREICHBAR, weil er nie mehr als
     * 500 kbps setzt. Getestet wird darum nicht der Zweig, sondern seine
     * AUSSAGE — und zwar in beide Richtungen, sonst waere „passt immer"
     * eine genauso gruene Zusage.
     *
     * Die Rechnung steht hier eigenstaendig, nicht aus dem Pruefling
     * geholt (MF-1000): Zellen = kbps * 1000 * 60 / rpm * 2, Byte
     * aufgerundet, auf 256 ausgerichtet, und die Tabelle fuehrt das PAAR. */
    struct { unsigned kbps, rpm, paar; int passt; } f[] = {
        {  250, 300,  25088, 1 },   /* DD, beide Seiten        */
        {  500, 360,  41984, 1 },   /* 1,2 M im 5,25-Zoll-HD   */
        {  500, 300,  50176, 1 },   /* 1,44 M                  */
        { 1000, 300, 100352, 0 },   /* 2,88 M ED — passt NICHT */
    };
    unsigned passend = 0u, unpassend = 0u;

    for (unsigned i = 0; i < sizeof(f) / sizeof(f[0]); i++) {
        unsigned zellen = f[i].kbps * 1000u * 60u / f[i].rpm * 2u;
        unsigned bytes  = (zellen + 7u) / 8u;
        unsigned ausg   = ((bytes + 255u) / 256u) * 256u;
        unsigned paar   = ausg * 2u;

        ASSERT(paar == f[i].paar);
        ASSERT((paar <= 0xFFFFu) == (f[i].passt != 0));
        if (f[i].passt) passend++; else unpassend++;
    }
    /* Beide Zahlen stehen absichtlich da: ohne die zweite koennte der Test
     * gruen bleiben, weil er keinen unpassenden Fall geprueft hat. */
    ASSERT(passend   == 3u);
    ASSERT(unpassend == 1u);

    /* Und die Kuerzung, die ohne die Schranke passieren wuerde. */
    ASSERT((uint16_t)100352u == 34816u);
}

int main(void)
{
    printf("=== HFE-Wandler: ein Zuviel ist keine Rundung (MF-1170) ===\n");
    RUN(zweikommaachtundachtzig_wird_abgesagt);
    RUN(ein_byte_zu_viel_wird_abgesagt);
    RUN(absage_laesst_keine_datei_zurueck);
    RUN(zu_kurze_datei_wird_abgesagt);
    printf("--- Gegenproben: was passt, geht weiterhin ---\n");
    RUN(eineinhalb_geht_weiterhin);
    RUN(vier_groessen_gehen_weiterhin);
    printf("--- Grenze der HFE-v1-Spurtabelle ---\n");
    RUN(spurtabelle_grenze_ist_arithmetisch_belegt);
    printf("\nErgebnis: %d bestanden, %d gefallen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
