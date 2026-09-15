/**
 * @file test_scp_kopf_seitenzahl.c
 * @brief Die Seitenzahl kam aus der Indexmarke (MF-1162)
 *
 * ── Der Befund ──────────────────────────────────────────────────────────
 *
 * `uft_scp_parser_v3.c` rechnete die Seitenzahl so:
 *
 *     disk->side_count = (disk->flags & 0x01) ? 2 : 1;
 *
 * und `SCP_FLAG_INDEX` ist in derselben Datei bei :85 genau dieses Bit —
 * „Index mark stored". Zwei Falschaussagen in einer Zeile:
 *
 *   - jede INDEXLOSE Aufnahme meldete eine EINSEITIGE Diskette,
 *   - jede indexgefuehrte eine ZWEISEITIGE,
 *
 * beides ohne jeden Bezug zum tatsaechlichen Inhalt. Und es wirkte: die
 * Geometrie rechnet mit `side_count` (`scp_v3_geometrie`, :1810).
 * Verschaerfend setzt `uft_scp_writer.c:159` `flags = SCP_FLAG_INDEX |
 * SCP_FLAG_RW` — jede von UFT geschriebene SCP las sich als zweiseitig
 * zurueck.
 *
 * Richtig ist Kopfbyte 0x0A, das :1135 schon einliest und das bis MF-1162
 * nirgends benutzt wurde. Quelle: Jim Drew, „SuperCard Pro Image
 * Specification" v1.9 (17.09.2019) — 0 = beide, 1 = nur Seite 0,
 * 2 = nur Seite 1; gegengelesen in pySuperCardPro (`scpfile.py`) und
 * a8rawconv (`rawdiskscp.cpp`).
 *
 * ── Warum dieser Test ueberhaupt entsteht ───────────────────────────────
 *
 * Im Zerleger gibt es einen Selbsttest, und er prueft den Kopf sogar — aber
 * er steht hinter `#ifdef SCP_V3_TEST`, und **kein Bausystem definiert
 * dieses Makro**. Das Tor „Selbsttest ohne Uebersetzung" hat es gemeldet:
 * 35 Zusicherungen, nie uebersetzt, koennen nicht rot werden (Klasse
 * MF-1000). Darin steckt der Lesefehler selbst:
 *
 *     minimal_scp[10] = 2;    (Kommentar dort: „2 heads")
 *
 * Nach der Spezifikation heisst 2 aber „nur Seite 1". Ein toter Selbsttest
 * mit einer falschen Annahme darin ist der klarste Fall fuer das, was das
 * Tor vorschlaegt: die Zusagen nach `tests/` heben, damit sie fallen
 * koennen (Muster MF-851). Dieser Test hebt den Teil, den MF-1162
 * angefasst hat — nicht alle 35; der Rest bleibt benannt in der Grundlinie.
 *
 * ── Zugang ──────────────────────────────────────────────────────────────
 *
 * Ueber die vorgesehenen Eingaenge, nicht ueber das Feldlayout.
 * `uft_v3_parsers.h` sagt ausdruecklich: „Die Strukturen bleiben
 * dateilokal … wer die Felder braucht, ruft eine Funktion. Die Bruecke
 * kommt ohnehin nur ueber `*_disk_sizeof()` an ihren Puffer." Genau so
 * hier: `scp_disk_sizeof()` fuer den Speicher, `scp_v3_geometrie()` fuer
 * die Antwort.
 *
 * ── Rotbeweis gegen den Vorzustand ──────────────────────────────────────
 *
 * Fall 2 und Fall 3 fielen vor MF-1162. Fall 1 war zufaellig richtig, und
 * genau deshalb ist der Fehler nie aufgefallen.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/uft_scp_integrity.h"

/* Opake Zeiger, wie `uft_v3_parsers.h` sie fuehrt. */
struct scp_disk;
struct scp_params;

extern size_t scp_disk_sizeof(void);
extern void   scp_get_default_params(struct scp_params *params);
extern bool   scp_parse(const uint8_t *data, size_t size,
                        struct scp_disk *disk, struct scp_params *params);
extern void   scp_disk_free(struct scp_disk *disk);
extern bool   scp_v3_geometrie(const struct scp_disk *disk,
                               int *spuren, int *koepfe, int *max_sektoren);

/* Fuer `scp_params` gibt es kein `sizeof`-Tor; die Bruecke haelt dafuer
 * einen festen Puffer (`V3_PARAMS_BUFFER_SIZE` in `uft_v3_bridge.c`).
 * Dieselbe Groesse hier, damit es EINE Zahl bleibt und nicht zwei. */
#define PARAMS_PUFFER 4096u

static int _fail = 0;
static int _run  = 0;

#define ASSERT(c) do { \
    if (!(c)) { printf("[ROT] %s:%d: %s\n", __FILE__, __LINE__, #c); _fail++; } \
} while (0)
#define LAUF(name) do { _run++; printf("  %-50s", #name); \
    int vor = _fail; name(); \
    printf("%s\n", (_fail == vor) ? "ok" : "ROT"); } while (0)

/* Kleinster gueltiger SCP-Kopf plus leere Spurtabelle.
 * `heads` ist Byte 0x0A, `flags` Byte 0x08 — die zwei Groessen, deren
 * Verwechslung der Befund ist. */
static uint8_t *scp_bauen(uint8_t heads, uint8_t flags, size_t *out_size)
{
    const size_t groesse = UFT_SCP_MIN_FILE_SIZE + 64u;
    uint8_t *b = calloc(1, groesse);
    if (!b) return NULL;

    memcpy(b, "SCP", 3);
    b[UFT_SCP_OFF_VERSION]   = 0x19;   /* v1.9              */
    b[UFT_SCP_OFF_DISKTYPE]  = 0x00;   /* Commodore 64      */
    b[UFT_SCP_OFF_REVS]      = 3;
    b[UFT_SCP_OFF_START_TRK] = 0;
    b[UFT_SCP_OFF_END_TRK]   = 83;
    b[UFT_SCP_OFF_FLAGS]     = flags;
    b[UFT_SCP_OFF_HEADS]     = heads;

    *out_size = groesse;
    return b;
}

/* Gibt die von der Geometrie gemeldete Kopfzahl zurueck, oder -1. */
static int koepfe_von(uint8_t heads, uint8_t flags)
{
    size_t groesse = 0;
    uint8_t *b = scp_bauen(heads, flags, &groesse);
    if (!b) return -1;

    struct scp_disk   *disk  = calloc(1, scp_disk_sizeof());
    struct scp_params *parms = calloc(1, PARAMS_PUFFER);
    if (!disk || !parms) { free(b); free(disk); free(parms); return -1; }

    scp_get_default_params(parms);

    int ergebnis = -1;
    if (scp_parse(b, groesse, disk, parms)) {
        int spuren = 0, koepfe = 0, sekt = 0;
        if (scp_v3_geometrie(disk, &spuren, &koepfe, &sekt))
            ergebnis = koepfe;
        scp_disk_free(disk);
    }
    free(b); free(disk); free(parms);
    return ergebnis;
}

/* ── Die vier Faelle ─────────────────────────────────────────────────── */

static void beide_seiten_mit_indexmarke(void)
{
    /* heads = 0 heisst „beide". Die Indexmarke ist gesetzt — hier kam der
     * alte Code zufaellig auf die richtige Zahl. Genau deshalb ist er nie
     * aufgefallen, und genau deshalb steht dieser Fall zuerst. */
    ASSERT(koepfe_von(UFT_SCP_HEADS_BOTH, 0x01) == 2);
}

static void beide_seiten_OHNE_indexmarke(void)
{
    /* ROTBEWEIS 1. Derselbe Inhalt, nur ohne Indexmarke: der alte Code
     * meldete EINE Seite. Die Aufnahme ist dieselbe. */
    ASSERT(koepfe_von(UFT_SCP_HEADS_BOTH, 0x00) == 2);
}

static void nur_seite_0_mit_indexmarke(void)
{
    /* ROTBEWEIS 2. heads = 1 heisst „nur Seite 0", also EIN Kopf. Der alte
     * Code meldete zwei, weil die Indexmarke gesetzt war. */
    ASSERT(koepfe_von(UFT_SCP_HEADS_SIDE0, 0x01) == 1);
}

static void nur_seite_1_die_rueckseite(void)
{
    /* heads = 2 heisst „nur Seite 1" — EIN Kopf, nicht „zwei Koepfe".
     * Genau diese Verwechslung steht im toten Selbsttest des Zerlegers,
     * und weil der Block nie uebersetzt wurde, hat sie niemand bemerkt. */
    ASSERT(koepfe_von(UFT_SCP_HEADS_SIDE1, 0x01) == 1);
}

int main(void)
{
    printf("=== SCP: die Seitenzahl kommt aus Byte 0x0A (MF-1162) ===\n");
    LAUF(beide_seiten_mit_indexmarke);
    LAUF(beide_seiten_OHNE_indexmarke);
    LAUF(nur_seite_0_mit_indexmarke);
    LAUF(nur_seite_1_die_rueckseite);
    printf("=== %d von %d gruen ===\n", _run - _fail, _run);
    return _fail ? 1 : 0;
}
