/**
 * @file test_ldbs_erfindet_keine_spur.c
 * @brief Eine abgeschnittene LDBS-Datei darf keine Spur erzeugen (MF-987).
 *
 * ── Woher die Frage kommt ────────────────────────────────────────────
 *
 * Der Linux-Bau meldete zwei Warnungen:
 *
 *   uft_ldbs.c:187: ignoring return value of 'fread' [-Wunused-result]
 *   uft_ldbs.c:234: ignoring return value of 'fread' [-Wunused-result]
 *
 * Die erste ist folgenlos — aber nicht, weil jemand den Fehlerfall
 * behandelt haette: `geom` ist `= {0}`, und ein Nullwert faellt danach
 * durch die Plausibilitaetspruefung (`geom.cylinders == 0` ->
 * `UFT_ERR_FORMAT`). Null-Initialisierung und Plausibilitaetspruefung
 * passen zufaellig zusammen.
 *
 * Das ist gemessen, nicht angenommen: Mutation M2 — die geom-Pruefung
 * wieder entfernen — faellt **keine** Zusage dieses Tests. Die Pruefung
 * an Zeile 187 ist also VORSORGE, kein Bugfix, und sie wird hier auch
 * nicht als einer ausgegeben. Was sie kauft, haelt der dritte Fall
 * unten fest: heute stuetzt sich die Absage auf „Zylinder 0 ist
 * unplausibel". Bekaeme die Geometrie je ein Feld, dessen Null gueltig
 * ist, kippte das still — der Vertrag steht seitdem als Test da und
 * nicht als Zufall.
 *
 * Die zweite ist echt. `ldbs_track_header_t th;` ist NICHT initialisiert.
 * Schlaegt der `fread` fehl — abgeschnittene Datei, Lesefehler —, enthaelt
 * `th` beliebigen Stapelspeicher, und der Code legt daraus eine Spur an:
 *
 *     int idx = th.cylinder * disk->heads + th.head;
 *     if (idx >= 0 && idx < track_count) {
 *         track = uft_track_alloc(th.sector_count, 0);
 *         track->track_num = th.cylinder;   ...
 *         disk->track_data[idx] = track;    // steht da wie gelesen
 *     }
 *
 * Die Bereichspruefung verhindert einen Speicherfehler; sie verhindert
 * nicht, dass eine **erfundene Spur** entsteht. Das ist die Klasse aus
 * MF-980 („23 Leser gaben erfundene Bytes als gelesene Sektoren aus").
 *
 * ── Warum dieser Test deterministisch ist ────────────────────────────
 *
 * „Stapelmuell" klingt nach Zufall, und ein Test auf Zufall taugt nichts:
 * ein zufaelliger Wert koennte ausserhalb des Bereichs liegen, der Test
 * waere gruen und wuerde nichts beweisen.
 *
 * Darum faerbt dieser Test den Stapel vorher selbst — `stapel_faerben()`
 * schreibt ein bekanntes Muster ueber den Bereich, in dem der Rahmen von
 * `uft_ldbs_read()` gleich liegen wird. Damit ist der Muell ein
 * **Messwert**: ohne den Fix traegt `th` genau MUSTER, die Spur landet
 * an einem vorhersagbaren Index, und der Test sagt beim Fehlschlag,
 * welches Muster er wiedergefunden hat.
 *
 * Das ist zugleich der Grund, warum der Fehler ernst ist: in der Praxis
 * ist der Rahmen nicht gefaerbt, sondern traegt die Reste des vorigen
 * Aufrufs — also die Werte der **zuletzt gelesenen Diskette**. In einem
 * forensischen Werkzeug ist das der denkbar schlechteste Wert, weil er
 * plausibel aussieht.
 *
 * Ehrlich zur Grenze: dass ein tiefer Rahmen den flacheren ueberdeckt,
 * ist Aufrufkonvention, keine Sprachgarantie. Sollte die Faerbung eines
 * Tages nicht mehr greifen, wird der Test **gruen ohne zu pruefen** —
 * darum ist der Rotbeweis gegen den Vorzustand Teil der Abnahme und in
 * MF-987 protokolliert, nicht bloss behauptet. Die ZUSAGE des Tests
 * bleibt davon unberuehrt und gilt immer: eine Datei ohne Spurdaten
 * ergibt keine Spur.
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "uft/formats/uft_ldbs.h"
#include "uft/core/uft_unified_types.h"
#include "uft/uft_error.h"

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

/* Feste Groessen des Formats (alles `#pragma pack(push,1)`, siehe
 * uft_ldbs.c). Nachgerechnet, nicht uebernommen:
 *   Kopf      4+4+4+4+4+12 = 32
 *   Blockkopf 2+2+4+4       = 12
 *   Geometrie 1*6 + 2 + 1 + 7 = 16
 *   Spurkopf  1*4 + 2 + 2   =  8                                     */
#define H_LEN   32u
#define BH_LEN  12u
#define GEO_LEN 16u
#define TH_LEN   8u

/* Geometrie der Pruefdatei: 4 Zylinder x 2 Koepfe = 8 Spurplaetze. */
#define ZYLINDER  4
#define KOEPFE    2
#define PLAETZE   (ZYLINDER * KOEPFE)

/* Die Spur, die in der GUELTIGEN Datei steht; Index 3*2+1 = 7. */
#define GUELTIG_CYL   3
#define GUELTIG_HEAD  1
#define GUELTIG_SECS  9

/* Das Stapelmuster. 0x01 ist mit Bedacht gewaehlt: als `th.cylinder` und
 * `th.head` gelesen ergibt es Index 1*2+1 = 3 — INNERHALB der 8 Plaetze.
 * Ein Muster, das aus dem Bereich faellt, wuerde vom Bereichstest
 * abgefangen und der Test waere gruen, ohne etwas zu zeigen. */
#define MUSTER        0x01
#define MUSTER_IDX    (MUSTER * KOEPFE + MUSTER)

static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;         p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

/* Faerbt den Stapelbereich, in dem gleich der Rahmen von uft_ldbs_read()
 * liegen wird. `volatile` ist nicht Zierde: ohne es darf der Uebersetzer
 * die Schleife streichen, weil niemand das Feld liest — und dann prueft
 * der Test nichts mehr. */
static void stapel_faerben(void)
{
    volatile uint8_t puffer[8192];
    for (size_t i = 0; i < sizeof(puffer); i++)
        puffer[i] = MUSTER;
}

static const char *temp_pfad(char *buf, size_t n, const char *name)
{
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    snprintf(buf, n, "%s/uft_ldbs_%s.ldbs", d, name);
    return buf;
}

/* Wie weit die gebaute Datei reicht. Abgeschnitten wird jeweils direkt
 * HINTER einem Blockkopf: der Kopf kuendigt Nutzdaten an, die Datei hat
 * sie nicht — genau die Lage, die den `fread` fehlschlagen laesst. */
typedef enum {
    DATEI_VOLL,            /* alles da                                   */
    DATEI_OHNE_SPURKOPF,   /* TRACK-Blockkopf da, Spurkopf fehlt  (:234) */
    DATEI_OHNE_GEOMETRIE,  /* GEOM-Blockkopf da, Geometrie fehlt  (:187) */
} datei_umfang_t;

static int baue(const char *pfad, datei_umfang_t umfang)
{
    uint8_t f[H_LEN + BH_LEN + GEO_LEN + BH_LEN + TH_LEN];
    memset(f, 0, sizeof(f));

    const uint32_t geo_block   = H_LEN;                        /* 32 */
    const uint32_t track_block = geo_block + BH_LEN + GEO_LEN; /* 60 */

    memcpy(f, "LDB\1", 4);
    put32(f + 4,  1);             /* version     */
    put32(f + 8,  2);             /* block_count */
    put32(f + 12, geo_block);     /* first_block */

    uint8_t *b = f + geo_block;   /* Geometrie-Block */
    put16(b + 0, 0x0002);         /* LDBS_BT_GEOM */
    put32(b + 4, GEO_LEN);
    put32(b + 8, track_block);    /* next */
    uint8_t *g = b + BH_LEN;
    g[0] = ZYLINDER;
    g[1] = KOEPFE;
    g[2] = 9;                     /* sectors          */
    g[3] = 2;                     /* size code -> 512 */

    uint8_t *t = f + track_block; /* Spur-Block */
    put16(t + 0, 0x0003);         /* LDBS_BT_TRACK */
    put32(t + 4, TH_LEN);         /* length: kuendigt einen Spurkopf an */
    put32(t + 8, 0);              /* next = 0 -> Ende der Kette */
    uint8_t *th = t + BH_LEN;
    th[0] = GUELTIG_CYL;
    th[1] = GUELTIG_HEAD;
    th[2] = GUELTIG_SECS;
    th[3] = 1;                    /* encoding = MFM */

    size_t laenge = sizeof(f);
    if (umfang == DATEI_OHNE_SPURKOPF)  laenge = (size_t)(track_block + BH_LEN);
    if (umfang == DATEI_OHNE_GEOMETRIE) laenge = (size_t)(geo_block + BH_LEN);

    FILE *fp = fopen(pfad, "wb");
    if (!fp) return 0;
    int ok = (fwrite(f, 1, laenge, fp) == laenge);
    fclose(fp);
    return ok;
}

static int spuren_gezaehlt(const uft_disk_image_t *d)
{
    int n = 0;
    if (!d || !d->track_data) return 0;
    for (size_t i = 0; i < d->track_count; i++)
        if (d->track_data[i]) n++;
    return n;
}

/* uft_ldbs_read() hat keinen Partner zum Freigeben — der ganze Leser hat
 * heute keinen Aufrufer (siehe Kopf von uft_ldbs.c). Der Test raeumt
 * deshalb selbst auf; ASan/UBSan laufen ueber diese Datei. */
static void gib_frei(uft_disk_image_t *d)
{
    if (!d) return;
    if (d->track_data) {
        for (size_t i = 0; i < d->track_count; i++)
            if (d->track_data[i]) uft_track_free(d->track_data[i]);
        free(d->track_data);
    }
    free(d);
}

static void test_gueltige_datei_ergibt_genau_eine_spur(void)
{
    char p[300];
    temp_pfad(p, sizeof(p), "gut");
    ASSERT(baue(p, DATEI_VOLL));

    /* Der Gegenzweig. Ohne ihn koennte der Leser schlicht NIE eine Spur
     * anlegen — dann waere die Zusage unten wertlos erfuellt. */
    uft_disk_image_t *d = NULL;
    ASSERT(uft_ldbs_read(p, &d) == UFT_OK);
    ASSERT(d != NULL);
    ASSERT(d->track_count == PLAETZE);
    ASSERT(spuren_gezaehlt(d) == 1);

    const int idx = GUELTIG_CYL * KOEPFE + GUELTIG_HEAD;
    ASSERT(d->track_data[idx] != NULL);
    ASSERT(d->track_data[idx]->track_num == GUELTIG_CYL);
    ASSERT(d->track_data[idx]->head == GUELTIG_HEAD);

    gib_frei(d);
    remove(p);
}

static void test_abgeschnittene_datei_ergibt_keine_spur(void)
{
    char p[300];
    temp_pfad(p, sizeof(p), "kurz");
    ASSERT(baue(p, DATEI_OHNE_SPURKOPF));

    /* Muell mit bekanntem Inhalt hinterlegen — siehe Kopf. */
    stapel_faerben();

    uft_disk_image_t *d = NULL;
    ASSERT(uft_ldbs_read(p, &d) == UFT_OK);
    ASSERT(d != NULL);
    ASSERT(d->track_count == PLAETZE);

    if (spuren_gezaehlt(d) != 0) {
        const uft_track_t *erfunden = d->track_data[MUSTER_IDX];
        printf("\n        erfundene Spur an Index %d", MUSTER_IDX);
        if (erfunden)
            printf(" — track_num=%d head=%d (Stapelmuster 0x%02X)",
                   (int)erfunden->track_num, (int)erfunden->head, MUSTER);
        printf("\n        ");
    }
    ASSERT(spuren_gezaehlt(d) == 0);

    gib_frei(d);
    remove(p);
}

/* Der Vertrag zur ERSTEN Warnung (uft_ldbs.c:187).
 *
 * Dieser Fall ist heute mit UND ohne die Pruefung gruen — Mutation M2
 * faellt ihn nicht. Er steht trotzdem hier, und zwar als Vertrag statt
 * als Beweis: er haelt fest, DASS eine Datei ohne Geometrieblock
 * abgelehnt wird, ohne festzulegen, WORAUS die Ablehnung folgt. Heute
 * folgt sie aus „Zylinder 0 ist unplausibel"; kaeme je ein Feld hinzu,
 * dessen Null gueltig ist, faellt dieser Test — und nicht erst der
 * Anwender. */
static void test_datei_ohne_geometrie_wird_abgelehnt(void)
{
    char p[300];
    temp_pfad(p, sizeof(p), "ohnegeo");
    ASSERT(baue(p, DATEI_OHNE_GEOMETRIE));

    stapel_faerben();

    uft_disk_image_t *d = NULL;
    const int rc = uft_ldbs_read(p, &d);
    ASSERT(rc == UFT_ERR_FORMAT);
    ASSERT(d == NULL);   /* keine halbe Diskette zurueckgeben */

    remove(p);
}

int main(void)
{
    printf("=== LDBS: eine Datei ohne Spurdaten ergibt keine Spur (MF-987) ===\n");
    RUN(gueltige_datei_ergibt_genau_eine_spur);
    RUN(abgeschnittene_datei_ergibt_keine_spur);
    RUN(datei_ohne_geometrie_wird_abgelehnt);
    printf("\n%d Pruefungen gruen, %d rot\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
