/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_hfe_spurende.c
 * @brief HFE: der Spurschluss gehoert beiden Seiten (MF-1125)
 *
 * ── Was gemessen wurde ────────────────────────────────────────────────────
 *
 * Eine HFE-v1-Spur liegt in 512-Byte-Paaren: 256 Byte Seite 0, dann
 * 256 Byte Seite 1. `track_len` in der Spurtabelle ist die Summe der
 * ECHTEN Bytes BEIDER Seiten — nicht die Laenge des belegten
 * Dateibereichs. Ist sie kein Vielfaches von 512, traegt das letzte Paar
 * je Seite einen kurzen echten Teil und dahinter Polster.
 *
 * Gemessen an `tests/corpus_free/gw_amigados.hfe`, dem einzigen freien
 * HFE im Korpus, erzeugt von Greaseweazle. Alle 80 Zylinder melden
 * `track_len = 25336`, und das letzte 512er-Paar sieht in ALLEN 80 so
 * aus — nicht in einem, in allen:
 *
 *     Blockbyte [  0..124)  124 x 0x55   Seite 0, echte Daten
 *     Blockbyte [124..256)  132 x 0x88   Seite 0, Polster
 *     Blockbyte [256..380)  124 x 0x55   Seite 1, echte Daten
 *     Blockbyte [380..512)  132 x 0x88   Seite 1, Polster
 *
 * und 25336 = 49 * 512 + 2 * 124. Je Seite also 12668 Byte = 101344
 * Zellen; bei 1976 ns je Zelle (bitRate 253 kbit/s ist die DATENrate,
 * die Zelle ist 1/(2*253 kHz)) sind das 200,26 ms — eine Umdrehung bei
 * 300 U/min. Die Zahl ist keine Annahme, sie faellt aus der Datei.
 *
 * ── Der Befund: 992 Zellen verloren UND 992 fremde Zellen gemeldet ────────
 *
 * `deinterleave_track()` lief die Paare mit `pos += 512` ab und behandelte
 * den Rest so:
 *
 *     if (remaining >= 256) { Seite 0 += 256; remaining -= 256; ... }
 *     if (remaining >  0  ) { Seite 1 += remaining; }
 *
 * Bei `remaining = 248` fiel der erste Zweig aus, und Seite 1 bekam alle
 * 248 Byte — das sind Blockbyte [0..248), also Seite 0s 124 echte Bytes
 * plus 124 Byte von Seite 0s Polster. Gemessen am Vorzustand:
 *
 *              Seite 0            Seite 1
 *     vorher   12544 Byte         12792 Byte
 *     richtig  12668 Byte         12668 Byte
 *
 * Seite 0 verlor je Spur ihre letzten 124 echten Bytes (992 Zellen),
 * still, mit `UFT_OK`. Seite 1 bekam 248 fremde Bytes angehaengt, davon
 * 124 Byte ECHTER FLUSS DER ANDEREN SEITE. Ueber 80 Zylinder: 79 360
 * Zellen weg und 79 360 fremde Zellen als eigene gemeldet — beide
 * Grundsaetze des Werkzeugs auf einmal, „kein Bit verloren" und „keine
 * erfundenen Daten".
 *
 * ── Zweiter Befund: die echten Bytes der Seite 1 lagen nie im Speicher ────
 *
 * `hfe_read_track()` las `malloc(track_len)` und `fread(…, track_len, …)`.
 * Seite 1s echter Schluss steht aber bei Blockbyte [256..380) des letzten
 * Paares, also bei Dateiversatz `49*512 + 256` bis `+380` — **hinter** dem
 * deklarierten Ende 25336. Wer nur `track_len` Byte einliest, kann diese
 * Bytes gar nicht sehen. Gelesen wird deshalb jetzt der auf 512
 * aufgerundete Bereich; er liegt vollstaendig im Blockplatz der Spur
 * (die Spuren dieses Abbilds stehen 50 Bloecke auseinander und
 * beanspruchen 50).
 *
 * ── Dritter Befund: die Schreibseite lief 264 Byte ueber den Puffer ───────
 *
 * `interleave_track()` schreibt IMMER ganze 512er-Paare
 * (`out_pos += 256` zweimal je Runde) und lief, solange eine der beiden
 * Seiten noch Bytes hatte. Ziel war `malloc(track_len)`. Bei den alten
 * Laengen 12544/12792 endete es bei `out_pos = 25600` in einem 25336
 * Byte grossen Puffer: **264 Byte Heap-Ueberlauf bei jedem Schreiben**
 * einer HFE, deren Spurlaenge kein 512er-Vielfaches ist. Die
 * Schreibseite baut den Bereich deshalb nicht mehr neu, sondern ersetzt
 * nur die echten Bytes der genannten Seite im eingelesenen Bereich —
 * damit bleibt das Polster Byte fuer Byte stehen (MF-931 Regel 4: die
 * Schreibseite gegen die Leseseite halten).
 *
 * ── Warum es niemand gesehen hat ──────────────────────────────────────────
 *
 * `tests/fixtures/hfe_v1_fixture.h` setzt `track_len = 512`, ein exaktes
 * Vielfaches. Die Fehlerklasse ist dort **unerreichbar** — der Prueffall
 * kann nicht rot werden (MF-1000 / Tor 64). Das Abbild unten hat deshalb
 * mit Absicht einen kurzen Schlussblock, und seine Bytes BENENNEN IHRE
 * SEITE: die oberen zwei Bits sagen 01 fuer Seite 0 und 11 fuer Seite 1,
 * die unteren sechs die Position. Ein Byte der falschen Seite oder an
 * der falschen Stelle faellt damit auf, nicht nur eine falsche Laenge.
 * Das Polster ist 0x88 wie im echten Abbild und in keinem der beiden
 * Markenbaender.
 *
 * ── Die vierte Umsetzung im Baum hatte es richtig ─────────────────────────
 *
 * `hfe_deinterleave_track()` in `include/uft/uft_hfe_format.h` nimmt die
 * Laenge JE SEITE und holt aus jedem 512er-Block die passende Haelfte —
 * das ist das richtige Gesetz, und die Wandlerpfade benutzen es seit
 * MF-526 mit `head_len = track_len / 2`. Dessen Kommentar nannte dabei
 * ausdruecklich `src/formats/hfe/uft_hfe.c::deinterleave_track` als
 * „den Leser, der es richtig macht" — also genau den, der den
 * Spurschluss falsch zerlegt. Fuer die vollen Bloecke stimmte der Satz,
 * fuer den Rest nicht; er ist mit MF-1125 berichtigt. Ein Kommentar, der
 * eine Stelle fuer geprueft erklaert, ist eine Aussage ueber eine ANDERE
 * Datei.
 *
 * ── Drei fremde Haende, drei verschiedene Antworten ───────────────────────
 *
 * Entschieden hat es die DATEI, nicht ein Orakel, und zwar mit Grund:
 *
 *   hxcfe (HFE ist HxCs eigenes Format, `hfe_loader.c`, Kanal *Spec*)
 *       rundet die Laenge auf 512 auf — `(len & ~0x1FF) + 0x200` — und
 *       gibt jeder Seite 12800 Byte. Es liest damit 132 Byte Polster je
 *       Seite als Fluss.
 *   fdc_bitstream (`disk_image/image_hfe.cpp`, MIT)
 *       laeuft `for (blk_id = 0; blk_id <= num_blocks; blk_id++)` und
 *       liest im letzten Durchgang **hinter den eigenen Puffer**.
 *   MAME (`hxchfe_dsk.cpp`, BSD-3, nur gelesen)
 *       nimmt an, Seite 0s Schlussblock sei auf 256 gepolstert und
 *       Seite 1s nicht — `if (head == 0) track_end -= 0x100;`. Fuer
 *       25336 ergibt dieses Modell eine Restlaenge von **-8**; es passt
 *       auf dieses Abbild nicht.
 *
 * Drei Umsetzungen, drei Antworten, keine als Beweis brauchbar. Was
 * bleibt, ist der Bytebefund oben und zwei Eichungen am Objekt: die
 * Laengengleichung `25336 = 49*512 + 2*124` und die MFM-Laufregel an der
 * Naht (Test unten).
 *
 * ── Was dieser Test NICHT prueft ──────────────────────────────────────────
 *
 * Eine HFE mit UNGERADER `track_len`. Die Aufteilung ist dann nicht
 * darstellbar; `hfe_seitenlaengen()` gibt das Mehr an Seite 0 und sagt
 * das im Kopf — belegt ist der Fall nicht, im Korpus liegt kein solches
 * Abbild. Ebenso nicht geprueft: HFE v3 (eigener Opcode-Strom) und eine
 * Spur, deren Blockplatz in der Datei kleiner ist als ihr aufgerundeter
 * Bereich.
 */

#include "uft/core/uft_zellregel.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_hfe;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-38s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

/* ── das synthetische Abbild ───────────────────────────────────────────── */

#define BLOCK      512u
#define HALB       256u
#define SPUREN       2u
#define VOLLE        3u     /* volle 512er-Paare je Spur                */
#define REST       100u     /* echte Bytes je Seite im letzten Paar     */
#define JE_SEITE   (VOLLE * HALB + REST)               /* 868  */
#define TRACK_LEN  (2u * JE_SEITE)                     /* 1736 */
#define PAARE      ((TRACK_LEN + BLOCK - 1u) / BLOCK)  /* 4    */
#define POLSTER    0x88u

/** Seite 0 -> 0x40..0x7F, Seite 1 -> 0xC0..0xFF. 0x88 liegt in keinem. */
static uint8_t marke(int seite, size_t lauf)
{
    return (uint8_t)((seite ? 0xC0u : 0x40u) | (uint8_t)(lauf & 0x3Fu));
}

static uint8_t spiegel(uint8_t b)
{
    uint8_t r = 0;
    for (int i = 0; i < 8; i++) r = (uint8_t)((r << 1) | ((b >> i) & 1u));
    return r;
}

/** Schreibt das Abbild; 1 bei Erfolg. */
static int baue(const char *pfad)
{
    const size_t bloecke = 2u + SPUREN * PAARE;
    uint8_t *datei = calloc(bloecke, BLOCK);
    if (!datei) return 0;

    memcpy(datei, "HXCPICFE", 8);
    datei[8]  = 0;                    /* format_revision: v1      */
    datei[9]  = (uint8_t)SPUREN;
    datei[10] = 2;                    /* zwei Seiten              */
    datei[11] = 0x00;                 /* ISOIBM_MFM               */
    datei[12] = 250; datei[13] = 0;
    datei[14] = 44;  datei[15] = 1;   /* 300 U/min                */
    datei[16] = 0x07;                 /* Generic Shugart          */
    datei[17] = 0x01;
    datei[18] = 1;   datei[19] = 0;   /* Spurtabelle in Block 1   */
    datei[20] = 0xFF;                 /* Schreiben erlaubt        */
    datei[21] = 0xFF;
    datei[22] = 0xFF; datei[23] = 0xFF;   /* kein Spur-0-Ersatz   */
    datei[24] = 0xFF; datei[25] = 0xFF;

    uint8_t *lut = datei + BLOCK;
    for (unsigned t = 0; t < SPUREN; t++) {
        const unsigned blk = 2u + t * PAARE;
        lut[t * 4 + 0] = (uint8_t)(blk & 0xFF);
        lut[t * 4 + 1] = (uint8_t)(blk >> 8);
        lut[t * 4 + 2] = (uint8_t)(TRACK_LEN & 0xFF);
        lut[t * 4 + 3] = (uint8_t)(TRACK_LEN >> 8);
    }

    for (unsigned t = 0; t < SPUREN; t++) {
        uint8_t *spur = datei + (size_t)(2u + t * PAARE) * BLOCK;
        for (unsigned s = 0; s < 2u; s++) {
            size_t hin = 0;
            for (unsigned b = 0; b < PAARE; b++) {
                uint8_t *ziel = spur + (size_t)b * BLOCK + s * HALB;
                size_t echt = JE_SEITE - hin;
                if (echt > HALB) echt = HALB;
                for (size_t i = 0; i < echt; i++)
                    ziel[i] = marke((int)s, hin + i);
                memset(ziel + echt, POLSTER, HALB - echt);
                hin += echt;
            }
        }
    }

    FILE *f = fopen(pfad, "wb");
    if (!f) { free(datei); return 0; }
    const size_t n = fwrite(datei, 1, bloecke * BLOCK, f);
    fclose(f);
    free(datei);
    return n == bloecke * BLOCK;
}

/** Liest eine Seite; gibt 1 zurueck und setzt `aus` und `len`
 *  (der Aufrufer befreit den Speicher). */
static int lies(const char *pfad, int zyl, int kopf, uint8_t **aus, size_t *len)
{
    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    if (uft_format_plugin_hfe.open(&disk, pfad, true) != UFT_OK) return 0;
    uft_track_t t;
    memset(&t, 0, sizeof(t));
    const int ok = (uft_format_plugin_hfe.read_track(&disk, zyl, kopf, &t) == UFT_OK)
                   && t.raw_data && t.raw_size;
    if (ok) { *aus = t.raw_data; *len = t.raw_size; }
    else if (t.raw_data) free(t.raw_data);
    if (t.weak_mask) free(t.weak_mask);
    uft_format_plugin_hfe.close(&disk);
    return ok;
}

/** Prueft jeden Byte gegen seine Marke; gibt den Index des ersten Fehlers
 *  oder (size_t)-1. Der Leser spiegelt die Bits, also zurueckspiegeln. */
static size_t erster_fremder(const uint8_t *d, size_t n, int seite)
{
    const uint8_t band = seite ? 0xC0u : 0x40u;
    for (size_t i = 0; i < n; i++) {
        const uint8_t o = spiegel(d[i]);
        if ((o & 0xC0u) != band) return i;
        if ((o & 0x3Fu) != (uint8_t)(i & 0x3Fu)) return i;
    }
    return (size_t)-1;
}

static const char *tmp_pfad(void)
{
    static char p[512];
#ifdef _WIN32
    const char *t = getenv("TEMP");
    snprintf(p, sizeof(p), "%s\\uft_hfe_spurende.hfe", t ? t : ".");
#else
    snprintf(p, sizeof(p), "/tmp/uft_hfe_spurende.hfe");
#endif
    return p;
}

/* ── Zusagen am synthetischen Abbild ───────────────────────────────────── */

TEST(seite0_bekommt_ihre_schlussbytes)
{
    const char *p = tmp_pfad();
    ASSERT(baue(p));
    uint8_t *d = NULL; size_t n = 0;
    ASSERT(lies(p, 0, 0, &d, &n));
    const size_t bad = erster_fremder(d, n, 0);
    if (bad != (size_t)-1)
        printf("\n      erstes falsches Byte bei %zu: 0x%02X ",
               bad, spiegel(d[bad]));
    free(d);
    /* Vor MF-1125: 768 Byte — die letzten 100 echten Bytes fielen weg. */
    ASSERT(n == JE_SEITE);
    ASSERT(bad == (size_t)-1);
    remove(p);
}

TEST(seite1_traegt_keine_fremden_bytes)
{
    const char *p = tmp_pfad();
    ASSERT(baue(p));
    uint8_t *d = NULL; size_t n = 0;
    ASSERT(lies(p, 0, 1, &d, &n));
    const size_t bad = erster_fremder(d, n, 1);
    if (bad != (size_t)-1)
        printf("\n      erstes falsches Byte bei %zu: 0x%02X ",
               bad, spiegel(d[bad]));
    free(d);
    /* Vor MF-1125: 968 Byte, die letzten 200 aus Seite 0 und ihrem Polster. */
    ASSERT(n == JE_SEITE);
    ASSERT(bad == (size_t)-1);
    remove(p);
}

TEST(polster_steht_in_keinem_strom)
{
    const char *p = tmp_pfad();
    ASSERT(baue(p));
    for (int kopf = 0; kopf < 2; kopf++) {
        uint8_t *d = NULL; size_t n = 0;
        ASSERT(lies(p, 0, kopf, &d, &n));
        size_t treffer = 0;
        for (size_t i = 0; i < n; i++)
            if (spiegel(d[i]) == (uint8_t)POLSTER) treffer++;
        if (treffer) printf("\n      Kopf %d: %zu Polsterbytes im Strom ",
                            kopf, treffer);
        free(d);
        ASSERT(treffer == 0);
    }
    remove(p);
}

TEST(zweite_spur_liegt_nicht_auf_der_ersten)
{
    /* Die Spurtabelle nennt Bloecke 2 und 6. Wer den aufgerundeten
     * Bereich liest, darf die Nachbarspur nicht anfassen — und weil jede
     * Spur dieselben Marken traegt, faellt eine Verwechslung nur ueber
     * die Laenge auf. Geprueft wird deshalb beides. */
    const char *p = tmp_pfad();
    ASSERT(baue(p));
    for (int zyl = 0; zyl < (int)SPUREN; zyl++) {
        for (int kopf = 0; kopf < 2; kopf++) {
            uint8_t *d = NULL; size_t n = 0;
            ASSERT(lies(p, zyl, kopf, &d, &n));
            const size_t bad = erster_fremder(d, n, kopf);
            free(d);
            ASSERT(n == JE_SEITE);
            ASSERT(bad == (size_t)-1);
        }
    }
    remove(p);
}

TEST(schreiben_laesst_die_andere_seite_stehen)
{
    const char *p = tmp_pfad();
    ASSERT(baue(p));

    /* Seite 1 der Spur 0 mit einem erkennbaren Muster ueberschreiben. */
    uint8_t neu[JE_SEITE];
    for (size_t i = 0; i < JE_SEITE; i++) neu[i] = spiegel(0x3Cu);

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    ASSERT(uft_format_plugin_hfe.open(&disk, p, false) == UFT_OK);
    uft_track_t t;
    memset(&t, 0, sizeof(t));
    uft_track_init(&t, 0, 1);
    t.raw_data = neu;
    t.raw_size = JE_SEITE;
    const uft_error_t wrc = uft_format_plugin_hfe.write_track
                            ? uft_format_plugin_hfe.write_track(&disk, 0, 1, &t)
                            : UFT_ERROR_NOT_SUPPORTED;
    uft_format_plugin_hfe.close(&disk);
    ASSERT(wrc == UFT_OK);

    /* Seite 0 muss Byte fuer Byte stehen — vor MF-1125 nullte die
     * Neuverschraenkung ihre letzten 100 echten Bytes. */
    uint8_t *d = NULL; size_t n = 0;
    ASSERT(lies(p, 0, 0, &d, &n));
    const size_t bad = erster_fremder(d, n, 0);
    free(d);
    ASSERT(n == JE_SEITE);
    ASSERT(bad == (size_t)-1);

    /* Und das Polster der Spur muss unberuehrt sein. */
    FILE *f = fopen(p, "rb");
    ASSERT(f != NULL);
    uint8_t blk[BLOCK];
    ASSERT(fseek(f, (long)((2u + (PAARE - 1u)) * BLOCK), SEEK_SET) == 0);
    const size_t gelesen = fread(blk, 1, BLOCK, f);
    fclose(f);
    ASSERT(gelesen == BLOCK);
    size_t heil = 0;
    for (size_t i = REST; i < HALB; i++) if (blk[i] == (uint8_t)POLSTER) heil++;
    if (heil != HALB - REST)
        printf("\n      Polster der Seite 0 nur %zu von %u Byte heil ",
               heil, HALB - REST);
    ASSERT(heil == HALB - REST);

    remove(p);
}

/* ── die kurze Datei: liefern, nicht erfinden ─────────────────────────── */

/**
 * Kuerzt die Datei auf `n` Byte. Der interessante Punkt ist genau
 * `Spurbeginn + TRACK_LEN`: die deklarierte Laenge ist dann vollstaendig
 * vorhanden, das Polster dahinter nicht — und Seite 1s echte
 * Schlussbytes liegen im Polsterbereich. Eine Datei dieser Gestalt
 * entsteht, wenn ein Schreiber die letzte Spur nicht auf 512 auffuellt.
 */
static int kuerze(const char *pfad, size_t n)
{
    FILE *f = fopen(pfad, "rb");
    if (!f) return 0;
    uint8_t *d = malloc(n);
    if (!d) { fclose(f); return 0; }
    const size_t gelesen = fread(d, 1, n, f);
    fclose(f);
    if (gelesen != n) { free(d); return 0; }
    f = fopen(pfad, "wb");
    if (!f) { free(d); return 0; }
    const size_t n2 = fwrite(d, 1, n, f);
    fclose(f);
    free(d);
    return n2 == n;
}

/** Dateilaenge, wenn die LETZTE Spur bei ihrer deklarierten Laenge endet. */
#define KURZ_LEN  ((2u + (SPUREN - 1u) * PAARE) * BLOCK + TRACK_LEN)

TEST(kurze_datei_liefert_kurz_statt_erfunden)
{
    const char *p = tmp_pfad();
    ASSERT(baue(p));
    ASSERT(kuerze(p, KURZ_LEN));

    /* Seite 0 ist vollstaendig: ihr echter Schluss liegt bei Blockbyte
     * [0..REST) des letzten Paares, also VOR dem deklarierten Ende. */
    uint8_t *d = NULL; size_t n = 0;
    ASSERT(lies(p, (int)SPUREN - 1, 0, &d, &n));
    size_t bad = erster_fremder(d, n, 0);
    free(d);
    ASSERT(n == JE_SEITE);
    ASSERT(bad == (size_t)-1);

    /* Seite 1 nicht: ihr echter Schluss liegt bei Blockbyte
     * [256..256+REST) und damit hinter dem Dateiende. Geliefert werden
     * deshalb nur die vollen Bloecke — kurz, aber kein erfundenes Byte.
     * Ohne die Schranke in `hfe_seite_lesen()` kaeme hier die volle
     * Laenge mit 100 Byte aus dem uninitialisierten Puffer. */
    d = NULL; n = 0;
    ASSERT(lies(p, (int)SPUREN - 1, 1, &d, &n));
    bad = erster_fremder(d, n, 1);
    if (n != VOLLE * HALB)
        printf("\n      Seite 1: %zu Byte statt %u ", n, VOLLE * HALB);
    free(d);
    ASSERT(n == VOLLE * HALB);
    ASSERT(bad == (size_t)-1);

    remove(p);
}

TEST(schreiben_verlaengert_die_kurze_datei_nicht)
{
    const char *p = tmp_pfad();
    ASSERT(baue(p));
    ASSERT(kuerze(p, KURZ_LEN));

    uint8_t neu[JE_SEITE];
    for (size_t i = 0; i < JE_SEITE; i++) neu[i] = spiegel(0x3Cu);

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    ASSERT(uft_format_plugin_hfe.open(&disk, p, false) == UFT_OK);
    uft_track_t t;
    memset(&t, 0, sizeof(t));
    uft_track_init(&t, (int)SPUREN - 1, 1);
    t.raw_data = neu;
    t.raw_size = JE_SEITE;
    const uft_error_t wrc = uft_format_plugin_hfe.write_track
                            ? uft_format_plugin_hfe.write_track(&disk, (int)SPUREN - 1, 1, &t)
                            : UFT_ERROR_NOT_SUPPORTED;
    uft_format_plugin_hfe.close(&disk);
    ASSERT(wrc == UFT_OK);

    /* Die Datei darf nicht gewachsen sein — geschrieben wird nur, was
     * auch gelesen wurde. */
    FILE *f = fopen(p, "rb");
    ASSERT(f != NULL);
    ASSERT(fseek(f, 0, SEEK_END) == 0);
    const long gross = ftell(f);
    fclose(f);
    if (gross != (long)KURZ_LEN)
        printf("\n      Datei %ld Byte statt %u ", gross, (unsigned)KURZ_LEN);
    ASSERT(gross == (long)KURZ_LEN);

    /* Und Seite 0 steht unberuehrt. */
    uint8_t *d = NULL; size_t n = 0;
    ASSERT(lies(p, (int)SPUREN - 1, 0, &d, &n));
    const size_t bad = erster_fremder(d, n, 0);
    free(d);
    ASSERT(n == JE_SEITE);
    ASSERT(bad == (size_t)-1);

    remove(p);
}

/* ── Zusagen am echten Abbild ──────────────────────────────────────────── */

#ifdef UFT_CORPUS_DIR
/** Die Zahlen stehen in der Datei; sie sind oben im Kopf hergeleitet. */
#define GW_TRACK_LEN  25336u
#define GW_JE_SEITE   (GW_TRACK_LEN / 2u)     /* 12668  */

static const char *korpus(void)
{
    static char p[512];
    snprintf(p, sizeof(p), "%s/gw_amigados.hfe", UFT_CORPUS_DIR);
    return p;
}

static int korpus_da(void)
{
    FILE *f = fopen(korpus(), "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

TEST(korpus_beide_seiten_sind_gleich_lang)
{
    if (!korpus_da()) { printf("SKIP (kein Korpus) "); return; }
    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    ASSERT(uft_format_plugin_hfe.open(&disk, korpus(), true) == UFT_OK);
    int gelesen = 0, schief = 0;
    for (int c = 0; c < 80; c++) {
        for (int h = 0; h < 2; h++) {
            uft_track_t t;
            memset(&t, 0, sizeof(t));
            if (uft_format_plugin_hfe.read_track(&disk, c, h, &t) != UFT_OK)
                continue;
            if (t.raw_data && t.raw_size != GW_JE_SEITE) {
                if (!schief)
                    printf("\n      Spur %d/%d: %zu Byte statt %u ",
                           c, h, t.raw_size, GW_JE_SEITE);
                schief++;
            }
            gelesen++;
            if (t.raw_data) free(t.raw_data);
            if (t.weak_mask) free(t.weak_mask);
        }
    }
    uft_format_plugin_hfe.close(&disk);
    /* Vor MF-1125: Seite 0 je 12544, Seite 1 je 12792 — 160 von 160 schief. */
    ASSERT(gelesen == 160);
    ASSERT(schief == 0);
}

TEST(korpus_naht_haelt_die_mfm_regel)
{
    if (!korpus_da()) { printf("SKIP (kein Korpus) "); return; }
    /* In einem MFM-Strom folgt auf eine 1-Zelle nie unmittelbar eine
     * zweite, und zwischen zwei Einsen liegen nie mehr als drei Nullen.
     * Geprueft wird der Bereich um die Naht bei Byte 49*256; der
     * Schlusslauf der Spur bleibt aussen vor, weil die letzte Zelle
     * mitten im Byte endet.
     *
     * Die Regel gilt in der PHYSISCHEN Bitfolge. Der Leser spiegelt jedes
     * Byte, also wird hier zurueckgespiegelt — eine Spiegelung zerreisst
     * die Nachbarschaft ueber Bytegrenzen hinweg. */
    const int spuren[] = { 0, 1, 40, 79 };
    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    ASSERT(uft_format_plugin_hfe.open(&disk, korpus(), true) == UFT_OK);
    int brueche = 0;
    for (unsigned k = 0; k < sizeof(spuren) / sizeof(spuren[0]); k++) {
        for (int h = 0; h < 2; h++) {
            uft_track_t t;
            memset(&t, 0, sizeof(t));
            if (uft_format_plugin_hfe.read_track(&disk, spuren[k], h, &t) != UFT_OK)
                continue;
            if (t.raw_data && t.raw_size >= GW_JE_SEITE) {
                /* MF-1128: hier stand eine eigene Zaehlschleife. Sie war
                 * eine von drei Kopien derselben Regel im Baum; gemessen
                 * wird jetzt von `uft_zellregel_messen()`. Der Leser
                 * spiegelt jedes Byte in die logische Domaene, also wird
                 * fuer diese Messung LSB-ZUERST gelesen — damit ist die
                 * Nachbarschaft der Zellen wieder die PHYSISCHE, und
                 * genau darauf bezieht sich die MFM-Regel. Das spart
                 * zugleich die Rueckspiegelung von Hand. */
                const size_t ab = 49u * 256u;      /* Byte der Naht */
                const size_t von = (ab > 8u) ? ab - 8u : 0u;
                uft_zellregel_t r;
                if (uft_zellregel_messen(t.raw_data + von,
                                         (GW_JE_SEITE - von) * 8u,
                                         UFT_ZELL_LSB_ZUERST,
                                         UFT_ZELL_MFM_MAX_NULL,
                                         UFT_ZELL_MFM_MAX_EINS, &r)
                    && (r.paare || r.max_null > UFT_ZELL_MFM_MAX_NULL)) {
                    printf("\n      Spur %d/%d: %zu 11-Paare, laengster "
                           "Nulllauf %u ", spuren[k], h, r.paare,
                           r.max_null);
                    brueche++;
                }
            }
            if (t.raw_data) free(t.raw_data);
            if (t.weak_mask) free(t.weak_mask);
        }
    }
    uft_format_plugin_hfe.close(&disk);
    /* Vor MF-1125: Seite 1 jeder Spur mit einem Nulllauf 4 an der Naht. */
    ASSERT(brueche == 0);
}
#endif /* UFT_CORPUS_DIR */

int main(void)
{
    /* Ungepuffert, weil der Vorzustand mit STATUS_HEAP_CORRUPTION
     * (0xC0000374) abbricht und gepufferte Zeilen dabei verloren gehen —
     * dann sagt der Lauf nicht, WELCHE Zusage ihn ausgeloest hat. */
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("HFE: der Spurschluss gehoert beiden Seiten (MF-1125)\n");
    RUN(seite0_bekommt_ihre_schlussbytes);
    RUN(seite1_traegt_keine_fremden_bytes);
    RUN(polster_steht_in_keinem_strom);
    RUN(zweite_spur_liegt_nicht_auf_der_ersten);
    RUN(schreiben_laesst_die_andere_seite_stehen);
    RUN(kurze_datei_liefert_kurz_statt_erfunden);
    RUN(schreiben_verlaengert_die_kurze_datei_nicht);
#ifdef UFT_CORPUS_DIR
    RUN(korpus_beide_seiten_sind_gleich_lang);
    RUN(korpus_naht_haelt_die_mfm_regel);
#endif
    printf("\n  %d bestanden, %d gefallen\n", _pass, _fail);
    return _fail ? 1 : 0;
}
