/**
 * @file test_sondendoktrin_engerer_anspruch.c
 * @brief MYZ80 gegen CP/M: der Ueberanspruch ist weg, der Gleichstand
 *        bleibt — und damit ist Regel 2 der Doktrin erst halb erledigt
 *        (MF-1182, P3-406, P3-439).
 *
 * @section DER_BEFUND
 *
 * MYZ80s ganze Erkennung ist „die ersten 256 Byte sind alle 0xE5"
 * (libdsk `drvmyz80.c:87-91`, uebernommen MF-1029), und es nahm kurze
 * Dateien an — die Groesse konnte also nichts abweisen. Eine CP/M-
 * Diskette mit unbeschriebener Systemspur beginnt genau so. Gemessen an
 * `tests/corpus_free/cpmtools_cf2dd_720k.cpm` (737 280 Byte, von
 * cpmtools und libdsk erzeugt, MF-1149) gewann MYZ80 mit **70** gegen
 * `cpm`s **40** — gelesen wuerde 64x1x128x1024 statt 80x2x9x512. Das
 * traf nicht die Pruefdatei allein, sondern **jede** frisch formatierte
 * CP/M-Diskette (P3-406).
 *
 * **Und der Kommentar an der Stelle hat es vorhergesagt.** In
 * `uft_myz80_probe()` stand woertlich: „eine Datei, die durchgehend
 * 0xE5 ist — etwa eine leer formatierte Diskette eines anderen
 * Formats — erfuellt die Bedingung ebenfalls. libdsk hat dasselbe
 * Problem und lebt damit." Ein Kommentar, der einen Defekt harmlos
 * nennt, ist eine Aussage — und diese war falsch.
 *
 * @section WAS_MF_1182_BEHEBT_UND_WAS_NICHT
 *
 * Beide Sonden vergaben ihre Zahl von Hand. Seit MF-1182 kommen beide
 * aus `uft_probe_konfidenz()`, und die Leiter gibt beiden dasselbe:
 * **25** — Struktur (eine geprueft Stelle) plus Geometrie, ohne Kennung.
 *
 *   MYZ80  vorher 70 von Hand   nachher 25 aus der Leiter
 *   cpm    vorher 40 von Hand   nachher 25 aus der Leiter
 *
 * **Behoben ist damit der Ueberanspruch, nicht der Gleichstand.** 30
 * Punkte Vorsprung fuer eine Sonde, die 256 von 737 280 Byte erklaert,
 * sind weg. Uebrig bleibt ein echter Gleichstand, und den entscheidet
 * Regel 2 der Doktrin: der ENGERE Anspruch gewinnt. Fuer diese Regel
 * fehlt das Mass im Sondenvertrag — „wie viel der Datei erklaert der
 * Anspruch" —, und die Doktrin nennt es selbst die „naechste
 * Vertragsfrage". Gefuehrt als P3-439.
 *
 * @section EIN_EIGENER_FEHLGRIFF_UND_WIE_ER_AUFFIEL
 *
 * Ein erster Entwurf gab `cpm` (und MYZ80 auf passender Groesse)
 * `UFT_BELEG_SELBSTKONSISTENZ`, weil `cpm_waehle()` die Gesamtgroesse
 * EXAKT prueft. Das ergab cpm 45 gegen MYZ80 25 und sah wie die
 * Loesung aus. Es war falsch: die Doktrin definiert den Beleg woertlich
 * als „der Kopf sagt eine Groesse, und die Datei hat sie — die Datei
 * bestaetigt sich selbst", und beide Formate sind KOPFLOS. Eine
 * Groesse, die zu einer Tafelgeometrie passt, ist „Groesse allein" und
 * damit **0**.
 *
 * **Gefangen hat es eine zweite Testzeile, nicht das Nachdenken:** mit
 * dem Zugestaendnis gewann `cpm` ploetzlich auch das Rennen um
 * `nwasp_spec_400k.nanowasp`, und `test_oeffentliche_api_am_korpus.c`
 * meldete „gemessen war IMG, jetzt CP/M". Eine Zahl, die an einer
 * Stelle passt und an der naechsten einen fremden Sieger erzeugt, ist
 * keine Messung.
 *
 * @section WAS_HIER_NICHT_GEPRUEFT_WIRD
 *
 * **Nicht, wer das volle Rennen gewinnt.** Dieser Test vergleicht das
 * PAAR MYZ80/cpm. Das Rennen ueber alle registrierten Plugins messt
 * `tests/test_oeffentliche_api_am_korpus.c`; dort ist der Sieger seit
 * MF-1182 **MSX bei gleichauf 2**, weil MSX-2DD ebenfalls 80x2x9x512
 * ist. Auch das ist ein Gleichstand, und dort hilft das Mass aus
 * P3-439 nicht — beide erklaeren alles.
 *
 * Auch nicht, dass `cpm` die Datei richtig LIEST — das tut
 * `tests/test_cpm_gegen_libdsk.c` mit 960 von 960 Sektoren (MF-1149).
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/formats/uft_myz80.h"

/* Die beiden Plugin-Strukturen. Der Test ruft die Sonden DURCH sie,
 * damit die echten `static`-Funktionen laufen und nicht eine zweite
 * Kopie ihrer Rechnung (Klasse MF-1015). */
extern const uft_format_plugin_t uft_format_plugin_myz80;
extern const uft_format_plugin_t uft_format_plugin_cpm;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-48s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)
#define CHECK(c, msg) do { if (!(c)) { \
                        printf("FAIL @ %d: %s\n", __LINE__, (msg)); \
                        _fail++; return; } } while (0)

/* Der Korpuspfad kommt aus der Bauumgebung, nicht aus einem relativen
 * Pfad — sonst haengt der Test daran, aus welchem Verzeichnis ctest
 * ihn startet. Konvention des Baums: `UFT_CORPUS_DIR`. */
#ifndef UFT_CORPUS_DIR
#  define UFT_CORPUS_DIR "tests/corpus_free"
#endif
#define KORPUS UFT_CORPUS_DIR "/cpmtools_cf2dd_720k.cpm"
#define KORPUS_GROESSE 737280u

/* Die Zahlen, die BIS MF-1182 von Hand vergeben wurden. Sie stehen hier
 * und nicht im Produktivcode — genau das ist der Befund. */
#define ALT_MYZ80 70
#define ALT_CPM   40

/* MYZ80: 256 Byte reserviert, dann Zylinder von 131 072 Byte
 * (libdsk `drvmyz80.c:178`); voll 256 + 64 * 131 072. */
#define MYZ80_RES   256u
#define MYZ80_SPUR  131072u

static uint8_t *lade(const char *pfad, size_t *n_out)
{
    FILE *f = fopen(pfad, "rb");
    if (!f) return NULL;
    if (fseek(f, 0L, SEEK_END) != 0) { fclose(f); return NULL; }
    const long ende = ftell(f);
    if (ende <= 0L) { fclose(f); return NULL; }
    rewind(f);
    uint8_t *p = (uint8_t *)malloc((size_t)ende);
    if (!p) { fclose(f); return NULL; }
    const size_t gelesen = fread(p, 1u, (size_t)ende, f);
    fclose(f);
    if (gelesen != (size_t)ende) { free(p); return NULL; }
    *n_out = gelesen;
    return p;
}

/* ══════════════════════════════════════════════════════════════════ */

TEST(der_ueberanspruch_ist_weg)
{
    size_t n = 0;
    uint8_t *d = lade(KORPUS, &n);
    CHECK(d != NULL, "Korpusdatei fehlt — sie ist eingecheckt, also ist "
                     "das ein echter Fehlschlag und kein Skip");
    ASSERT(n == KORPUS_GROESSE);

    /* Die Praemisse am Objekt, nicht behauptet: die ersten 256 Byte
     * sind alle 0xE5, also feuert MYZ80s GANZE Erkennung. */
    for (size_t i = 0; i < MYZ80_RES; ++i) ASSERT(d[i] == 0xE5u);
    /* Und die Datei ist genau eine `pcw-720`: 80 x 2 x 9 x 512. */
    ASSERT(KORPUS_GROESSE == 80u * 2u * 9u * 512u);

    int k_myz80 = -1, k_cpm = -1;
    const bool a_myz80 = uft_format_plugin_myz80.probe(d, n, n, &k_myz80);
    const bool a_cpm   = uft_format_plugin_cpm.probe(d, n, n, &k_cpm);
    free(d);

    printf("\n      MYZ80 %s mit %d (vorher %d)  |  CPM %s mit %d "
           "(vorher %d)\n",
           a_myz80 ? "beansprucht" : "sagt ab", k_myz80, ALT_MYZ80,
           a_cpm ? "beansprucht" : "sagt ab", k_cpm, ALT_CPM);

    /* Beide beanspruchen die Datei — das ist richtig und bleibt so.
     * MYZ80s Bedingung IST erfuellt; falsch war der Vorsprung. */
    ASSERT(a_myz80 && a_cpm);

    /* VORZUSTAND: 30 Punkte Vorsprung fuer den breiteren Anspruch. */
    CHECK(ALT_MYZ80 - ALT_CPM == 30,
          "ROT-PROBE verfehlt: der alte Vorsprung war nicht 30 — dann "
          "beschreibt dieser Test den Befund nicht");
    CHECK(ALT_MYZ80 > ALT_CPM,
          "ROT-PROBE verfehlt: die alten Zahlen gaben CPM schon den "
          "Zuschlag — dann ist P3-406 kein Befund");

    /* NACHZUSTAND: beide 25, der Vorsprung ist weg. */
    ASSERT(k_myz80 == 25);
    ASSERT(k_cpm == 25);
    CHECK(k_myz80 == k_cpm,
          "ROT-PROBE verfehlt: es steht kein Gleichstand — dann ist die "
          "Aussage dieses Tests falsch und P3-439 beschreibt etwas "
          "anderes");
    CHECK(k_myz80 < ALT_MYZ80,
          "ROT-PROBE verfehlt: MYZ80 meldet weiter 70 — dann ist der "
          "Ueberanspruch nicht behoben");

    /* Und das Band hat sich mitbewegt: 70 lag im Band „Struktur
     * gelesen" (50..79), 25 liegt in „kein Anspruch" (0..29). Eine
     * kopflose Erkennung an 256 Konventionsbyte gehoert dorthin. */
    ASSERT(ALT_MYZ80 >= 50 && k_myz80 < 30);
}

TEST(myz80_beansprucht_seine_eigene_datei_weiter)
{
    /* Die wichtigste Gegenprobe: eine Korrektur, die MYZ80 seine
     * eigenen Dateien kostet, waere keine Korrektur. Ein Zylinder
     * reicht — libdsk laesst kurze Dateien ausdruecklich zu. */
    const size_t n = MYZ80_RES + MYZ80_SPUR;   /* 131 328 */
    uint8_t *d = (uint8_t *)malloc(n);
    ASSERT(d != NULL);
    memset(d, 0xE5, n);

    int k_myz80 = -1, k_cpm = -1;
    const bool a_myz80 = uft_format_plugin_myz80.probe(d, n, n, &k_myz80);
    const bool a_cpm   = uft_format_plugin_cpm.probe(d, n, n, &k_cpm);
    free(d);

    printf("\n      eigene Datei (%zu Byte): MYZ80 %s mit %d, CPM %s\n",
           n, a_myz80 ? "beansprucht" : "sagt ab", k_myz80,
           a_cpm ? "beansprucht" : "sagt ab");

    CHECK(a_myz80, "ROT-PROBE verfehlt: MYZ80 beansprucht seine EIGENE "
                   "Datei nicht mehr — dann hat die Korrektur das Format "
                   "lahmgelegt statt es ehrlich zu machen");
    ASSERT(k_myz80 == 25);

    /* Und keine CP/M-Definition hat diese Groesse, also gibt es hier
     * gar kein Rennen — die Gegenprobe zur Zusage oben. */
    CHECK(!a_cpm, "ROT-PROBE verfehlt: CPM beansprucht auch 131 328 Byte — "
                  "dann pruefte die Zusage oben nicht, was sie behauptet");
}

TEST(ohne_kennung_bleibt_die_klemme_bei_45)
{
    /* Beide Formate sind kopflos. Ohne Kennung ist die Obergrenze 45,
     * und Struktur plus Geometrie ergeben 25 — das ist die ganze
     * Strecke, die diese zwei Sonden belegen koennen. */
    const int struk_geo = uft_probe_konfidenz(UFT_BELEG_STRUKTUR
                                              | UFT_BELEG_GEOMETRIE);
    ASSERT(struk_geo == 25);
    CHECK(ALT_MYZ80 > struk_geo,
          "ROT-PROBE verfehlt: MYZ80s alte 70 lagen nicht ueber dem, was "
          "die Leiter hergibt — dann war die Zahl kein Ueberanspruch");
    CHECK(ALT_CPM > struk_geo,
          "ROT-PROBE verfehlt: auch cpms alte 40 waren also gedeckt — "
          "dann ist die Haelfte dieses Commits unnoetig");

    /* Die Klemme selbst: drei Belege ohne Kennung kommen auf 45, nicht
     * auf 50. */
    ASSERT(uft_probe_konfidenz(UFT_BELEG_SELBSTKONSISTENZ
                               | UFT_BELEG_STRUKTUR
                               | UFT_BELEG_GEOMETRIE) == 45);

    /* Gegenprobe: MIT Kennung geht es sehr wohl darueber, sonst pruefte
     * die Zusage oben nur, dass die Funktion immer klemmt. */
    ASSERT(uft_probe_konfidenz(UFT_BELEG_KENNUNG
                               | UFT_BELEG_SELBSTKONSISTENZ
                               | UFT_BELEG_STRUKTUR
                               | UFT_BELEG_GEOMETRIE) == 100);
}

TEST(das_mass_fuer_regel_2_ist_gerechnet_aber_nicht_verdrahtet)
{
    /* Regel 2 der Doktrin: der ENGERE Anspruch gewinnt. Das Mass dafuer
     * — wie viel der Datei erklaert der Anspruch — ist fuer diesen Fall
     * AUSRECHENBAR, und die Zahlen stehen hier, damit P3-439 nicht von
     * einer Schaetzung ausgeht:
     *
     *   MYZ80 prueft 256 Byte von 737 280            =  0,03 %
     *   cpm   prueft die Gesamtgroesse und ein Byte  = 100 %
     *
     * Kein Plugin liefert diese Zahl heute, und dieser Test fuegt sie
     * NICHT hinzu — eine Vertragsaenderung an 137 Sonden ist eine
     * Eigentuemer-Entscheidung, keine Nebenwirkung. */
    const double anteil_myz80 = (double)MYZ80_RES / (double)KORPUS_GROESSE;
    printf("\n      MYZ80 erklaert %u von %u Byte (%.4f %%), cpm alle\n",
           (unsigned)MYZ80_RES, KORPUS_GROESSE, anteil_myz80 * 100.0);
    ASSERT(anteil_myz80 < 0.001);

    /* Die Groessenregel, die MYZ80 dem Mass anzubieten haette, ist
     * gemessen — und die Korpusdatei erfuellt sie NICHT, was genau der
     * Grund ist, warum MYZ80 dort der breitere Anspruch ist. */
    ASSERT((KORPUS_GROESSE - MYZ80_RES) % MYZ80_SPUR != 0u);
    ASSERT((MYZ80_RES + 64u * MYZ80_SPUR) == 8388864u);
    ASSERT(((MYZ80_RES + 64u * MYZ80_SPUR) - MYZ80_RES) % MYZ80_SPUR == 0u);

    /* Und die Leiter KANN den Gleichstand nicht aufloesen: dieselben
     * Belege ergeben dieselbe Zahl. Das ist keine Schwaeche der
     * Umsetzung, sondern die Stelle, an der der Vertrag fehlt. */
    ASSERT(uft_probe_konfidenz(UFT_BELEG_STRUKTUR | UFT_BELEG_GEOMETRIE)
           == uft_probe_konfidenz(UFT_BELEG_STRUKTUR | UFT_BELEG_GEOMETRIE));
}

TEST(nullpuffer_und_ein_gekipptes_byte_werden_abgewiesen)
{
    /* Die Eichung aus MF-729: auf einem Nullpuffer darf nichts >= 50
     * melden — hier darf ueberhaupt nichts beanspruchen, weil 0x00
     * nicht 0xE5 ist und keine CP/M-Definition 4096 Byte gross ist. */
    uint8_t nullen[4096];
    memset(nullen, 0, sizeof(nullen));
    int k = -1;
    ASSERT(!uft_format_plugin_myz80.probe(nullen, sizeof(nullen),
                                          sizeof(nullen), &k));
    ASSERT(k == 0);
    ASSERT(!uft_format_plugin_cpm.probe(nullen, sizeof(nullen),
                                        sizeof(nullen), &k));

    /* Ein Puffer, der bis auf EIN Byte 0xE5 ist, faellt bei MYZ80
     * durch — die Pruefung ist vollstaendig, nicht stichprobenartig. */
    uint8_t fast[MYZ80_RES + 16u];
    memset(fast, 0xE5, sizeof(fast));
    fast[MYZ80_RES - 1u] = 0xE4u;
    ASSERT(!uft_format_plugin_myz80.probe(fast, sizeof(fast),
                                          sizeof(fast), &k));

    /* Gegenprobe: mit allen 256 Byte 0xE5 beansprucht es sehr wohl —
     * sonst pruefte die Zusage oben nur, dass es immer absagt. */
    fast[MYZ80_RES - 1u] = 0xE5u;
    int k_ja = -1;
    CHECK(uft_format_plugin_myz80.probe(fast, sizeof(fast), sizeof(fast),
                                        &k_ja),
          "ROT-PROBE verfehlt: MYZ80 sagt auch bei vollstaendigen 256 "
          "Byte 0xE5 ab — dann misst der Test nicht das gekippte Byte");
    ASSERT(k_ja == 25);
}

int main(void)
{
    printf("=== Doktrin: der Ueberanspruch ist weg (MF-1182) ===\n");
    RUN(der_ueberanspruch_ist_weg);
    RUN(myz80_beansprucht_seine_eigene_datei_weiter);
    RUN(ohne_kennung_bleibt_die_klemme_bei_45);
    RUN(das_mass_fuer_regel_2_ist_gerechnet_aber_nicht_verdrahtet);
    RUN(nullpuffer_und_ein_gekipptes_byte_werden_abgewiesen);
    printf("\n=== %d bestanden, %d gefallen ===\n", _pass, _fail);
    return _fail ? 1 : 0;
}
