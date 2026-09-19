/*
 * @file test_sonde_sagt_ab_bei_gleichstand.c
 * @brief Rotbeweis: bei Gleichstand gewinnt heute der ZUERST
 *        REGISTRIERTE. Die Doktrin sagt: keiner.
 *
 * DER WIDERSPRUCH, WOERTLICH
 * --------------------------
 * `docs/SONDEN_DOKTRIN.md` (Eigentuemer-Entscheidung, MF-1153):
 *
 *     „Groesse allein — 0, nie hinreichend"
 *     „bei Gleichstand gewinnt der ENGERE Anspruch; bleibt es gleich,
 *      gewinnt KEINER („mehrdeutig" mit beiden Namen)"
 *
 * `include/uft/uft_format_plugin.h` ueber `uft_probe_buffer_ranked()`:
 *
 *     „Der Gewinner ist derselbe (deterministisch: bei Gleichstand der
 *      zuerst registrierte)."
 *
 * Regel und Code widersprechen sich, und der Code verliert.
 *
 * DIE MESSUNG, DIE ES SICHTBAR MACHT
 * ----------------------------------
 * `src/formats/dsk_generic/uft_dsk_generic.c` erzeugt ueber EIN Makro
 * 49 Plugins; sie unterscheiden sich nur durch einen Index in eine
 * Geometrietafel, und ihre Sonde prueft AUSSCHLIESSLICH
 * `file_size == expected` mit Konfidenz 40.
 *
 * An der TAFEL gemessen: 49 Geometrien, aber nur 20 verschiedene
 * Groessen; 38 Zeilen sind ueber die Groesse nicht unterscheidbar, und
 * die Gleichstaende tragen VERSCHIEDENE Anordnungen (327 680 Byte:
 * acht Mal 40x2x16x256 gegen `SAN` mit 40x2x8x512).
 *
 * AN DER REGISTRY GEMESSEN SIEHT ES ANDERS AUS, UND DAS IST DER PUNKT.
 * Die Tafel sagt, was mehrdeutig IST; sie sagt nicht, was daraus
 * FOLGT — ausserhalb der 49 beanspruchen weitere Plugins dieselben
 * Groessen, teils mit hoeherer Konfidenz. Gemessen ueber alle 20:
 *
 *     327 680   17 Beansprucher, KEIN Gleichstand: `TRD` 45 > DSK 40
 *     204 800   12 Beansprucher, FUENF gleichauf bei 40:
 *               `IMG` `DSK_ACE` `DSK_EIN` `DSK_LYN` ...
 *      92 160    8 Beansprucher, FUENF gleichauf bei 40:
 *               `XFD` `JVC` `DSK_SV` `DSK_VEC` ...
 *     634 880    4 Beansprucher, DREI gleichauf: `IMG` `DSK_HP` `DSK_RC`
 *     179 200    `NorthStar` 65 — eindeutig
 *     315 392    `Micropolis` 70 — eindeutig
 *
 *     -> 10 von 20 Groessen werden durch REGISTRIERUNGSREIHENFOLGE
 *        entschieden, und die Gleichstaende laufen quer durch die
 *        Systeme: Atari `XFD` gegen TRS-80 `JVC`, Commodore `D81`
 *        gegen `SAD`.
 *
 * Wer die falsche Anordnung bekommt, liest JEDEN Sektor am falschen
 * Versatz — die Klasse MF-1026 (`victor9k`) und MF-1039 (`cpm`, wo
 * eine CPC-Datendiskette alle 360 Sektornummern falsch bekommen
 * haette).
 *
 * WAS DIESER TEST HEUTE TUT
 * -------------------------
 * Er MISST den Gleichstand (das ist gruen, es ist eine Tatsache) und
 * verlangt das Verhalten der Doktrin (das faellt). Die zwei roten
 * Zusagen sind der Beweis, dass die Aenderung noetig ist; ohne sie
 * waere jede Behauptung ueber „behoben" unbelegt.
 *
 * Gegenprobe: eine Groesse, die nur EIN Plugin beansprucht, muss
 * weiterhin ohne Zwang aufgehen. Sonst waere die Behebung nur eine
 * Verweigerung.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
/* `uft_disk_open()`/`uft_disk_close()` stehen NICHT im Plugin-Kopf.
 * Ohne diese Zeile faengt `-Werror=implicit-function-declaration` es —
 * und genau das ist der Punkt: ohne die Sperre waere aus dem
 * Zeigerergebnis ein `int` geworden, NULL zu 0, und die Absage-Zusage
 * waere gruen gewesen, WEIL der Aufruf falsch war (Klasse
 * `test_gruen_weil_der_aufruf_scheiterte`). */
#include "uft/uft_core.h"   /* uft_disk_open() — die kanonische Form */
#include "uft/uft_disk.h"   /* uft_disk_close() */

static int gruen = 0;
static int rot = 0;

#define ZUSAGE(bed, text)                                                   \
    do {                                                                    \
        if (bed) { gruen++; printf("   [ok ] %s\n", (text)); }              \
        else     { rot++;   printf("   [ROT] %s\n", (text)); }              \
    } while (0)

/* Die Sonden sehen hoechstens 4096 Byte; `file_size` traegt die echte
 * Groesse (MF-1029: wer sie verwirft, macht seinen Groessenvergleich
 * zu totem Code). */
#define SICHT 4096

static uint8_t g_puffer[SICHT];

static void ranked_fuer(size_t dateigroesse, uft_probe_ranking_t *r)
{
    memset(g_puffer, 0, sizeof(g_puffer));
    memset(r, 0, sizeof(*r));
    (void)uft_probe_buffer_ranked(g_puffer, sizeof(g_puffer),
                                  dateigroesse, r);
}

int main(void)
{
    printf("== Sonde bei Gleichstand ==\n");

    ZUSAGE(uft_register_all_formats() == UFT_OK,
           "SPERRE: uft_register_all_formats() gelingt");

    /* 0. Erst das Bild, dann das Urteil. Die Tafel sagt, welche
     *    Groessen mehrdeutig SIND; die Registry sagt, was daraus
     *    folgt — und das ist nicht dasselbe. */
    static const size_t alle[] = {
        327680u, 737280u, 368640u, 163840u, 204800u, 819200u,
        92160u, 634880u, 184320u, 1261568u, 152320u, 177408u,
        179200u, 256256u, 286720u, 315392u, 512512u, 573440u,
        655360u, 696320u
    };
    printf("   %-9s %4s %4s %4s  %-14s %s\n",
           "Groesse", "bean", "glch", "konf", "Sieger", "gleichauf");
    size_t mit_gleichstand = 0;
    for (size_t i = 0; i < sizeof(alle) / sizeof(alle[0]); i++) {
        uft_probe_ranking_t e;
        ranked_fuer(alle[i], &e);
        printf("   %-9zu %4zu %4zu %4d  %-14s",
               alle[i], e.claimants, e.tied, e.confidence,
               e.winner ? e.winner->name : "-");
        for (size_t k = 0; k < e.tied_listed; k++)
            printf(" %s", e.tied_with[k] ? e.tied_with[k]->name : "?");
        printf("\n");
        if (e.tied > 1) mit_gleichstand++;
    }
    printf("   -> %zu von %zu Groessen mit echtem Gleichstand an der "
           "Spitze\n", mit_gleichstand,
           sizeof(alle) / sizeof(alle[0]));

    ZUSAGE(mit_gleichstand >= 8,
           "MESSUNG: mindestens ACHT dieser Groessen werden durch "
           "Registrierungsreihenfolge entschieden");

    /* 0b. „Eigene Groesse in der Tafel" ist NICHT „erreichbar" (MF-1256).
     *
     *     `docs/VERIFICATION_TIERS.md` teilt die 49 Makrozeilen in
     *     11 mit eigener Groesse IN DER TAFEL und 38 im Gleichstand.
     *     Die 11 koennten wie eine hebbare Menge aussehen — und genau
     *     das waere der Spiegelfehler zu dem, was der Auftrag
     *     verbietet ("den 38 eine Stufe geben, weil die Spalte leer
     *     aussieht"). Hier steht die Gegenmessung, und sie faellt
     *     anders aus als die Tafel vermuten laesst.
     *
     *     Die Liste steht ABSICHTLICH ein zweites Mal hier und wird
     *     nicht aus dem Generator geholt — sonst befragte der Test
     *     dieselbe Quelle wie der Prueflings-Text (Klasse MF-1000). */
    static const struct { size_t groesse; const char *name; } tafel_allein[] = {
        { 152320u,  "DSK_VIC"  }, { 177408u,  "DSK_CRO" },
        { 179200u,  "DSK_NS"   }, { 256256u,  "DSK_X820" },
        { 286720u,  "DSK_OLI"  }, { 315392u,  "DSK_NAS" },
        { 512512u,  "DSK_WNG"  }, { 573440u,  "DSK_SMC" },
        { 655360u,  "DSK_MZ"   }, { 696320u,  "DSK_ORC" },
        { 1261568u, "DSK_RLD"  }
    };
    const size_t N_TAFEL = sizeof(tafel_allein) / sizeof(tafel_allein[0]);
    size_t r_allein = 0, r_gleich = 0, r_geschlagen = 0;
    printf("   -- die 11 mit eigener Groesse in der TAFEL, an der "
           "REGISTRY gemessen --\n");
    for (size_t i = 0; i < N_TAFEL; i++) {
        uft_probe_ranking_t e;
        ranked_fuer(tafel_allein[i].groesse, &e);
        int dabei = (e.winner && strcmp(e.winner->name,
                                        tafel_allein[i].name) == 0);
        for (size_t k = 0; !dabei && k < e.tied_listed; k++)
            dabei = (e.tied_with[k] && strcmp(e.tied_with[k]->name,
                                              tafel_allein[i].name) == 0);
        const char *klasse;
        if (!dabei)          { klasse = "GESCHLAGEN";   r_geschlagen++; }
        else if (e.tied > 1) { klasse = "GLEICHSTAND";  r_gleich++;     }
        else                 { klasse = "allein";       r_allein++;     }
        printf("   %-9zu %-9s %-12s Sieger %s (%d%%)\n",
               tafel_allein[i].groesse, tafel_allein[i].name, klasse,
               e.winner ? e.winner->name : "-", e.confidence);
    }
    printf("   -> allein %zu · Gleichstand %zu · geschlagen %zu (von %zu)\n",
           r_allein, r_gleich, r_geschlagen, N_TAFEL);

    ZUSAGE(r_allein + r_gleich + r_geschlagen == N_TAFEL,
           "SPERRE: die drei Klassen sind erschoepfend");
    ZUSAGE(r_allein < N_TAFEL,
           "BEFUND: „eigene Groesse in der Tafel\" heisst NICHT "
           "„in der Registry erreichbar\"");
    ZUSAGE(r_allein == 3,
           "MESSUNG: genau DREI der elf stehen registryweit allein oben");
    ZUSAGE(r_geschlagen == 3,
           "MESSUNG: DREI werden ueberboten — ein Plugin mit mehr Beleg "
           "gewinnt (NorthStar 65, Micropolis 70, TRD/ADL 45)");

    /* 1. Der Gleichstand ist eine Tatsache, keine Vermutung.
     *
     *    GEWAEHLT: 204 800 Byte — fuenf Plugins gleichauf bei 40, und
     *    sie gehoeren zu VERSCHIEDENEN Systemen (`IMG`, `DSK_ACE`,
     *    `DSK_EIN`, `DSK_LYN`). Wer die falsche Anordnung bekommt,
     *    liest jeden Sektor am falschen Versatz.
     *
     *    BERICHTIGT WAEHREND DER MESSUNG: der erste Entwurf nahm
     *    327 680, weil die Geometrietafel dort neun Zeilen fuehrt.
     *    Gemessen gewinnt dort `TRD` mit 45 gegen die DSK-Zeilen mit
     *    40 — es gibt gar keinen Gleichstand an der Spitze. Die Tafel
     *    sagt, was mehrdeutig IST; die Registry sagt, was daraus
     *    FOLGT, und das ist nicht dasselbe. */
    const size_t STRITTIG = 204800u;
    uft_probe_ranking_t r;
    ranked_fuer(STRITTIG, &r);
    printf("   %zu Byte: %zu Beansprucher, %zu gleichauf bei %d%%\n",
           STRITTIG, r.claimants, r.tied, r.confidence);
    for (size_t i = 0; i < r.tied_listed; i++)
        printf("      gleichauf: %s\n",
               r.tied_with[i] ? r.tied_with[i]->name : "?");

    ZUSAGE(r.tied >= 2,
           "MESSUNG: 204800 Byte werden von mehreren Plugins mit "
           "derselben Konfidenz beansprucht");
    ZUSAGE(r.confidence <= 45,
           "MESSUNG: und zwar mit hoechstens 45 — ohne Kennung ist das "
           "die Obergrenze der Doktrin");

    /* 2. Was die Doktrin verlangt (faellt heute). */
    memset(g_puffer, 0, sizeof(g_puffer));
    const uft_format_plugin_t *sieger =
        uft_probe_buffer_format(g_puffer, sizeof(g_puffer), STRITTIG);
    if (sieger)
        printf("   heute gewaehlt: %s (durch Registrierungsreihenfolge)\n",
               sieger->name);
    ZUSAGE(sieger == NULL,
           "DOKTRIN: bei Gleichstand gibt die Sonde KEINEN Sieger "
           "zurueck");

    /* 3. Und der Oeffnungspfad sagt ab. */
    const char *pfad = "gleichstand_probe.bin";
    FILE *f = fopen(pfad, "wb");
    if (f) {
        static const uint8_t null[4096] = {0};
        for (size_t g = 0; g < STRITTIG; g += sizeof(null))
            fwrite(null, 1, sizeof(null), f);
        fclose(f);
    }
    ZUSAGE(f != NULL, "SPERRE: Pruefdatei mit 204800 Byte angelegt");

    uft_disk_t *d = uft_disk_open(pfad, true);
    if (d) {
        const uft_format_plugin_t *p = uft_disk_plugin(d);
        printf("   uft_disk_open() oeffnete als: %s\n",
               p ? p->name : "?");
        uft_disk_close(d);
    }
    ZUSAGE(d == NULL,
           "DOKTRIN: uft_disk_open() sagt bei Gleichstand ab, statt "
           "eine Anordnung zu raten");

    /* 3b. Der AUSWEG: wer das Format nennt, kommt hinein — und wer
     *     die Kandidaten braucht, bekommt sie. Ohne diese beiden
     *     waere die Absage nur eine Verweigerung. */
    uft_probe_ranking_t rr;
    memset(&rr, 0, sizeof(rr));
    uft_disk_t *dr = uft_disk_open_ranked(pfad, true, &rr);
    ZUSAGE(dr == NULL && rr.tied > 1,
           "AUSWEG: uft_disk_open_ranked() sagt ab UND nennt, wie "
           "viele gleichauf liegen");
    ZUSAGE(rr.tied_listed > 0 && rr.tied_with[0] != NULL,
           "AUSWEG: die Kandidatenliste ist gefuellt — die Auswahl "
           "steht dem Bediener zur Verfuegung");
    if (dr) uft_disk_close(dr);

    /* Mit genanntem Format muss es aufgehen. Genommen wird der, den
     * die Rangliste an erster Stelle fuehrt — nicht weil er richtig
     * WAERE, sondern weil hier nur der WEG geprueft wird. */
    if (rr.tied_with[0]) {
        uft_disk_t *dz = uft_disk_open_as(pfad, true, rr.tied_with[0]);
        ZUSAGE(dz != NULL,
               "AUSWEG: uft_disk_open_as() oeffnet mit genanntem "
               "Format");
        if (dz) uft_disk_close(dz);
    }

    remove(pfad);

    /* 4. GEGENPROBE: eindeutig bleibt eindeutig.
     *
     *    BERICHTIGT WAEHREND DER MESSUNG: die erste Fassung suchte
     *    `claimants == 1` — „nur EIN Plugin beansprucht die Groesse".
     *    Das gibt es in dieser Registry gar nicht; selbst
     *    `Micropolis` mit 70 hat fuenf Mitbewerber unter sich.
     *    Entscheidend ist nicht, wie viele ANSPRUCH erheben, sondern
     *    ob einer von ihnen ALLEIN OBEN steht — `tied == 1`. Die
     *    falsche Bedingung haette die Gegenprobe rot gelassen und
     *    damit den Eindruck erzeugt, es gebe keine eindeutigen
     *    Groessen. */
    static const size_t eindeutig[] = {
        179200u, 315392u, 1261568u, 152320u, 177408u
    };
    size_t gefunden = 0;
    for (size_t i = 0; i < sizeof(eindeutig) / sizeof(eindeutig[0]); i++) {
        uft_probe_ranking_t e;
        ranked_fuer(eindeutig[i], &e);
        if (e.tied == 1 && e.winner) {
            gefunden = eindeutig[i];
            printf("   eindeutig: %zu Byte -> %s (%d%%, %zu Beansprucher)\n",
                   gefunden, e.winner->name, e.confidence, e.claimants);
            break;
        }
    }
    ZUSAGE(gefunden != 0,
           "GEGENPROBE: es gibt Groessen, bei denen EINER allein oben "
           "steht");
    if (gefunden) {
        memset(g_puffer, 0, sizeof(g_puffer));
        ZUSAGE(uft_probe_buffer_format(g_puffer, sizeof(g_puffer),
                                       gefunden) != NULL,
               "GEGENPROBE: eine eindeutige Groesse liefert weiterhin "
               "einen Sieger — die Behebung ist keine Verweigerung");
    }

    printf("\n%d/%d\n", gruen, gruen + rot);
    return rot ? 1 : 0;
}
