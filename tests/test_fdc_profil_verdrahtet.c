/**
 * @file test_fdc_profil_verdrahtet.c
 * @brief Das FDC-Profilmodul war unerreichbar — und deshalb hat niemand
 *        bemerkt, dass eines seiner 17 Profile unmoeglich ist (MF-1168)
 *
 * ── Warum es diese Datei gibt ───────────────────────────────────────────
 *
 * `include/uft/formats/uft_fdc_gaps.h` fuehrt 17 Geometrieprofile mit
 * Drehzahl, Spurlaenge, Zellzahl und Lueckenwerten. Gemessen ueber
 * `git ls-files` kam `uft_fdc_detect_format` im ganzen Baum an genau zwei
 * Stellen vor: seiner Definition und seiner Deklaration. Null Aufrufer,
 * Klasse P3-204 — und ein Profil, das niemand befragt, kann nicht
 * widersprechen.
 *
 * MF-1168 verdrahtet die Tafel an ihrer einen echten Einbaustelle, dem
 * HFE-Wandler. Der Lauf hat dabei DREI Befunde am Pruefling selbst
 * gefunden; die zwei behobenen sind unten je mit ihrem Vorzustand
 * festgenagelt.
 *
 * ── Befund 1: der entspannte Durchlauf verwarf die SEITENZAHL ───────────
 *
 * Gemessen am Vorzustand:
 *
 *     detect_format(35, 1, 9, 512)  ->  „PC 360K (5.25" DD)"   Tafel 40/2
 *     detect_format( 1, 1, 9, 512)  ->  „PC 360K (5.25" DD)"   Tafel 40/2
 *
 * Eine EINSEITIGE Diskette bekam ein zweiseitiges Profil. Der Kommentar an
 * der Stelle sagte „Track count can vary" — die Bedingung liess aber auch
 * `sides` fallen, und dann gewann der erste Treffer. Seit MF-1168 wird bei
 * echter Mehrdeutigkeit abgesagt statt geraten (MF-1039, MF-1153).
 *
 * ── Befund 2: ein Profil, das kein Laufwerk formatieren kann ────────────
 *
 * `FM Single Density` fuehrt 26 x 128 Byte — 3328 Byte reine Sektordaten —
 * und stand mit `track_bytes = 3125`. Gemessen von der eigenen Funktion
 * dieses Moduls, seit MF-1167:
 *
 *     uft_fdc_calc_gaps(3125, 26, 128, false, &g4b)  ->  gap3 0, gap4b 0
 *
 * Die richtige Zahl steht im Profil SELBST: seine `gap3_fmt = 27` und
 * `gap4b = 247` verlangen gap_space = 949, und mit dem FM-Aufschlag des
 * Moduls (31 je Sektor, 73 je Spur) folgt track_bytes = 5208 — die
 * dokumentierte Spurkapazitaet der IBM-3740-Achtzoll-Diskette. Die 3125
 * waren die Zahl des Nachbarprofils `BBC DFS`.
 *
 * ── Befund 3: benannt, nicht behoben ────────────────────────────────────
 *
 * `uft_fdc_get_format()` vergleicht mit `strstr` statt auf Gleichheit —
 * `("PC")` liefert „PC 360K", `("2DD")` liefert „PC-98 2DD" und nicht
 * „MSX 2DD". Steht im Header; eine Verschaerfung waere eine
 * Eigentuemer-Entscheidung.
 *
 * ── Rotbeweis ───────────────────────────────────────────────────────────
 *
 * Vor MF-1168 fallen: `einseitig_bekommt_kein_zweiseitiges_profil`,
 * `mehrdeutigkeit_wird_gezaehlt` und `spurzahl_darf_abweichen_wenn_eindeutig`
 * (die Funktion mit dem Zaehler gab es nicht),
 * `fm_sd_passt_jetzt_in_ihre_spur` und `jedes_profil_passt_in_seine_spur`.
 * Gruen vor UND nach: `vier_groessen_treffen_genau`, `amiga_hat_kein_profil`
 * und `alte_schnittstelle_unveraendert` — sie belegen, dass die Korrektur
 * nicht einfach alles ablehnt.
 */
#include "uft/formats/uft_fdc_gaps.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* ── Die vier Groessen, die der HFE-Wandler entscheidet ──────────────── */

TEST(vier_groessen_treffen_genau)
{
    /* Gruen vor UND nach MF-1168. Die Zellzahlen sind hier UNABHAENGIG
     * hergeleitet und als Literal notiert, nicht aus der Tafel gelesen —
     * sonst befragte der Test dieselbe Quelle wie der Pruefling (Tor 64,
     * MF-1000). Eine Umdrehung dauert 60/rpm Sekunden, MFM legt je Datenbit
     * ein Taktbit:
     *
     *     250 kbit/s, 300 U/min:  250000 * 0,2       = 50000  -> 100000
     *     500 kbit/s, 360 U/min:  500000 * 60/360    = 83333  -> 166666
     *     500 kbit/s, 300 U/min:  500000 * 0,2       = 100000 -> 200000
     *
     * Die 83333 sind abgeschnitten, nicht gerundet — deshalb 166666. */
    const uft_fdc_format_t *p;
    unsigned n;

    p = uft_fdc_detect_format_counted(40, 2, 9, 512, &n);
    ASSERT(p != NULL);
    ASSERT(strcmp(p->name, "PC 360K (5.25\" DD)") == 0);
    ASSERT(p->rpm == 300u);
    ASSERT(p->raw_bits == 100000u);
    ASSERT(n == 1u);

    p = uft_fdc_detect_format_counted(80, 2, 9, 512, &n);
    ASSERT(p != NULL);
    ASSERT(strcmp(p->name, "PC 720K (3.5\" DD)") == 0);
    ASSERT(p->rpm == 300u);
    ASSERT(p->raw_bits == 100000u);
    /* DREI Profile sind 80/2/9x512: PC 720K, Atari ST DS, MSX 2DD. Die
     * haeufigste PC-Geometrie ist also schon im GENAUEN Durchlauf
     * dreifach mehrdeutig — sie stimmen in den Zeiten ueberein, und genau
     * das haelt `mehrdeutige_gruppen_stimmen_in_den_zeiten` fest. */
    ASSERT(n == 3u);

    p = uft_fdc_detect_format_counted(80, 2, 15, 512, &n);
    ASSERT(p != NULL);
    ASSERT(strcmp(p->name, "PC 1.2M (5.25\" HD)") == 0);
    ASSERT(p->rpm == 360u);
    ASSERT(p->raw_bits == 166666u);
    ASSERT(n == 1u);

    p = uft_fdc_detect_format_counted(80, 2, 18, 512, &n);
    ASSERT(p != NULL);
    ASSERT(strcmp(p->name, "PC 1.44M (3.5\" HD)") == 0);
    ASSERT(p->rpm == 300u);
    ASSERT(p->raw_bits == 200000u);
    ASSERT(n == 2u);        /* PC 1.44M und Atari ST HD */
}

/* ── ROTBEWEIS: einseitig bleibt einseitig ───────────────────────────── */

TEST(einseitig_bekommt_kein_zweiseitiges_profil)
{
    /* Vor MF-1168 antworteten beide mit „PC 360K (5.25" DD)", einem
     * Profil mit `sides = 2` und `tracks = 40`. Es gibt in der Tafel kein
     * einseitiges 35-Spur-Profil mit 9 x 512 — also gibt es keine Antwort,
     * und die Absage ist die richtige. */
    unsigned n = 99u;
    ASSERT(uft_fdc_detect_format_counted(35, 1, 9, 512, &n) == NULL);
    ASSERT(n == 0u);

    n = 99u;
    ASSERT(uft_fdc_detect_format_counted(1, 1, 9, 512, &n) == NULL);
    ASSERT(n == 0u);

    /* Gegenprobe in die andere Richtung: wo es ein einseitiges Profil GIBT,
     * kommt es auch. Sonst waere „lehnt alles Einseitige ab" eine genauso
     * gruene Zusage. */
    const uft_fdc_format_t *p = uft_fdc_detect_format_counted(80, 1, 9, 512, &n);
    ASSERT(p != NULL);
    ASSERT(p->sides == 1u);
    ASSERT(strcmp(p->name, "Atari ST SS (360K)") == 0);
    ASSERT(n == 2u);        /* Atari ST SS und MSX 1DD, beide 80/1/9x512 */

    p = uft_fdc_detect_format_counted(40, 1, 9, 512, &n);
    ASSERT(p != NULL);
    ASSERT(p->sides == 1u);
    ASSERT(strcmp(p->name, "Amstrad CPC Data") == 0);
    ASSERT(n == 2u);        /* CPC Data und CPC System, beide 40/1/9x512 */
}

/* ── Die Spurzahl darf abweichen — aber nur bei Eindeutigkeit ────────── */

TEST(spurzahl_darf_abweichen_wenn_eindeutig)
{
    /* Das ist der Fall, den der alte Kommentar gemeint hat und den es jetzt
     * wirklich gibt: PC-98 2HD steht mit 77 Spuren in der Tafel. Ein
     * 80-Spur-Abbild derselben Aufteilung trifft im GENAUEN Durchlauf
     * nichts und im zweiten genau EIN Profil. */
    unsigned n = 99u;
    const uft_fdc_format_t *p = uft_fdc_detect_format_counted(80, 2, 8, 1024, &n);
    ASSERT(p != NULL);
    ASSERT(strcmp(p->name, "PC-98 2HD (1.23M)") == 0);
    ASSERT(p->tracks == 77u);   /* die Tafel behaelt ihre eigene Spurzahl */
    ASSERT(n == 1u);

    /* Und bei MEHREREN Kandidaten im zweiten Durchlauf wird abgesagt:
     * 42/2/18x512 trifft genau nichts und im zweiten Durchlauf zwei
     * Profile (PC 1.44M und Atari ST HD). Vor MF-1168 kam „PC 1.44M"
     * heraus — geraten, nicht bestimmt. */
    n = 99u;
    ASSERT(uft_fdc_detect_format_counted(42, 2, 18, 512, &n) == NULL);
    ASSERT(n == 0u);
}

TEST(mehrdeutigkeit_wird_gezaehlt)
{
    /* Der Zaehler ist der ganze Zweck der neuen Schnittstelle: ein
     * Aufrufer, der nur die ZEITEN braucht, darf einen mehrfachen Treffer
     * nehmen; einer, der den NAMEN oder die LUECKEN braucht, nicht. */
    unsigned n = 0u;
    const uft_fdc_format_t *p = uft_fdc_detect_format_counted(80, 2, 18, 512, &n);
    ASSERT(p != NULL);
    ASSERT(n == 2u);

    /* und ein wirklich eindeutiges Profil meldet genau 1 */
    n = 0u;
    p = uft_fdc_detect_format_counted(80, 2, 36, 512, &n);
    ASSERT(p != NULL);
    ASSERT(strcmp(p->name, "PC 2.88M (3.5\" ED)") == 0);
    ASSERT(n == 1u);
}

/* ── Der Rueckfall im Wandler muss bleiben ───────────────────────────── */

TEST(amiga_hat_kein_profil)
{
    /* Gruen vor UND nach. Das ist die Zusage, auf die sich der Rueckfall im
     * HFE-Wandler stuetzt: fuer AmigaDOS fuehrt die Tafel nichts, also
     * muss dort die eigene Herleitung stehen bleiben. Waere das falsch,
     * traege der Wandler eine unerreichbare Codezeile. */
    ASSERT(uft_fdc_detect_format(80, 2, 11, 512) == NULL);
    ASSERT(uft_fdc_detect_format(80, 2, 22, 512) == NULL);
}

TEST(alte_schnittstelle_unveraendert)
{
    /* `uft_fdc_detect_format()` ist seit MF-1168 ein Aufruf der Fassung mit
     * Zaehler und NULL — dieselbe Bauform wie `uft_fdc_calc_gap3()` gegen
     * `uft_fdc_calc_gaps()` einen Commit vorher. */
    ASSERT(uft_fdc_detect_format(80, 2, 18, 512)
           == uft_fdc_detect_format_counted(80, 2, 18, 512, NULL));
    ASSERT(uft_fdc_detect_format(35, 1, 9, 512) == NULL);
}

/* ── Die mechanische Sicherung hinter dem Vertrag ────────────────────── */

TEST(mehrdeutige_gruppen_stimmen_in_den_zeiten)
{
    /* Der Header sagt: ein mehrfacher Treffer bestimmt die ZEITEN, nicht
     * Name und Luecken. Das ist eine Zusage ueber die TAFEL, und Zusagen
     * ueber Tafeln driften — also wird sie hier gehalten statt behauptet.
     *
     * Ununterscheidbar fuer den entspannten Durchlauf sind zwei Profile,
     * wenn `sides`, `sectors` und `sector_size` gleich sind. Gemessen gibt
     * es davon DREI Gruppen und damit 13 Paare:
     *
     *     2 / 9x512   PC 360K, PC 720K, Atari ST DS, MSX 2DD     -> 6 Paare
     *     1 / 9x512   Atari ST SS, CPC Data, CPC System, MSX 1DD -> 6 Paare
     *     2 /18x512   PC 1.44M, Atari ST HD                      -> 1 Paar
     *
     * Die 13 stehen ABSICHTLICH als Zahl da: ohne sie koennte der Test
     * gruen bleiben, weil er kein einziges Paar gefunden hat (Tor 64). Wer
     * ein Profil hinzufuegt, muss diese Zahl anfassen — und genau das ist
     * das Signal. */
    unsigned paare = 0u;
    for (int i = 0; UFT_FDC_FORMATS[i]; i++) {
        for (int j = i + 1; UFT_FDC_FORMATS[j]; j++) {
            const uft_fdc_format_t *a = UFT_FDC_FORMATS[i];
            const uft_fdc_format_t *b = UFT_FDC_FORMATS[j];
            if (a->sides != b->sides) continue;
            if (a->sectors != b->sectors) continue;
            if (a->sector_size != b->sector_size) continue;
            paare++;
            ASSERT(a->rpm == b->rpm);
            ASSERT(a->track_bytes == b->track_bytes);
            ASSERT(a->raw_bits == b->raw_bits);
        }
    }
    ASSERT(paare == 13u);
}

TEST(tafel_und_zaehler_driften_nicht)
{
    /* `UFT_FDC_FORMAT_COUNT` ist eine gepflegte Zahl neben einer
     * abzaehlbaren Tafel — die Klasse, die dieser Baum dreimal als
     * Zahlendrift gemessen hat. Hier kostet sie eine Zeile. */
    unsigned n = 0u;
    while (UFT_FDC_FORMATS[n]) n++;
    ASSERT(n == 17u);
    ASSERT(n == (unsigned)UFT_FDC_FORMAT_COUNT);
}

/* ── ROTBEWEIS: FM Single Density passt jetzt in ihre eigene Spur ────── */

TEST(fm_sd_passt_jetzt_in_ihre_spur)
{
    /* Die beiden Zahlen stehen als Literal da, unabhaengig hergeleitet:
     * 250 kbit/s bei 360 U/min sind 250000/6 = 41666,67 Datenbits je
     * Umdrehung, also 5208 Byte; als Zellenstrom 83333. Vor MF-1168 stand
     * dort 3125 / 50000 — die Zahlen des Nachbarprofils BBC DFS. */
    ASSERT(UFT_FDC_FM_SD.track_bytes == 5208u);
    ASSERT(UFT_FDC_FM_SD.raw_bits    == 83333u);

    /* Der zweite, unabhaengige Weg zu 5208, und er ist der schaerfere: die
     * Lueckenwerte DES PROFILS SELBST verlangen ihn. Der FM-Aufschlag des
     * Moduls ist 31 Byte je Sektor plus die 2 in `data_space`, und 73 je
     * Spur; beides hier bewusst als Literal, damit eine Aenderung dort
     * diesen Test rot macht statt ihn stillschweigend mitzuziehen. */
    const uint32_t gap_space = 5208u - 73u - 26u * (128u + 31u + 2u);
    ASSERT(gap_space == 949u);
    ASSERT((uint32_t)UFT_FDC_FM_SD.gaps.gap3_fmt * 26u
           + UFT_FDC_FM_SD.gaps.gap4b == 949u);

    /* Und die Rechnung des Moduls sagt jetzt „passt": 949/26 = 36, Rest 13.
     * Vor MF-1168 kam 0/0 heraus — MF-1167s Absage, zu Recht. */
    uint16_t g4b = 0xFFFFu;
    uint8_t  g3  = uft_fdc_calc_gaps(UFT_FDC_FM_SD.track_bytes, 26u, 128u,
                                     false, &g4b);
    ASSERT(g3  == 36u);
    ASSERT(g4b == 13u);

    /* Gegenprobe: mit der alten Spurlaenge sagt dieselbe Funktion weiter
     * ab. Ohne sie waere nicht belegt, dass die 5208 den Unterschied
     * machen und nicht eine Aenderung an der Rechnung. */
    g4b = 0xFFFFu;
    ASSERT(uft_fdc_calc_gaps(3125u, 26u, 128u, false, &g4b) == 0u);
    ASSERT(g4b == 0u);
}

TEST(jedes_profil_passt_in_seine_spur)
{
    /* Die Sicherung gegen den naechsten unmoeglichen Eintrag: jedes Profil
     * muss in seine eigene `track_bytes` passen. Vor MF-1168 fiel hier
     * genau eines — FM Single Density mit 3328 Byte Nutzdaten in 3125 Byte
     * Spur. Die Sektordaten allein werden zusaetzlich geprueft, weil sie
     * die Bedingung ohne jede Annahme ueber den Aufschlag stellen. */
    unsigned geprueft = 0u;
    for (int i = 0; UFT_FDC_FORMATS[i]; i++) {
        const uft_fdc_format_t *f = UFT_FDC_FORMATS[i];
        uint32_t nutz = (uint32_t)f->sectors * f->sector_size;
        uint16_t g4b = 0xFFFFu;
        uint8_t  g3;

        if (nutz >= f->track_bytes) {
            printf("\n    %s: %u Byte Nutzdaten in %u Byte Spur\n",
                   f->name, (unsigned)nutz, (unsigned)f->track_bytes);
        }
        ASSERT(nutz < f->track_bytes);

        g3 = uft_fdc_calc_gaps(f->track_bytes, f->sectors, f->sector_size,
                               f->mfm, &g4b);
        if (g3 == 0u) {
            printf("\n    %s: kein gueltiger Zwischenraum\n", f->name);
        }
        ASSERT(g3 != 0u);
        geprueft++;
    }
    ASSERT(geprueft == 17u);
}

int main(void)
{
    printf("=== FDC-Profiltafel: verdrahtet und nachgerechnet (MF-1168) ===\n");
    RUN(vier_groessen_treffen_genau);
    RUN(einseitig_bekommt_kein_zweiseitiges_profil);
    RUN(spurzahl_darf_abweichen_wenn_eindeutig);
    RUN(mehrdeutigkeit_wird_gezaehlt);
    RUN(amiga_hat_kein_profil);
    RUN(alte_schnittstelle_unveraendert);
    printf("--- mechanische Sicherungen ---\n");
    RUN(mehrdeutige_gruppen_stimmen_in_den_zeiten);
    RUN(tafel_und_zaehler_driften_nicht);
    printf("--- FM Single Density (Befund 2) ---\n");
    RUN(fm_sd_passt_jetzt_in_ihre_spur);
    RUN(jedes_profil_passt_in_seine_spur);
    printf("\nErgebnis: %d bestanden, %d gefallen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
