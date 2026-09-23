/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * test_convert_g64_buchfuehrung.c — was der Wandler nicht wandelt, muss
 * er als gescheitert melden (MF-1332).
 *
 * ROTBEWEIS ZUERST. Gegen den Vorzustand faellt die Zusage fuer
 * G64->SCP; G64->HFE haelt sie und dient als Gegenprobe im selben Lauf.
 *
 * GEMESSEN am Vorzustand ueber `test_convert_roundtrip_measured`, selbe
 * Quelle `tests/corpus_free/vice_c1541_35trk.g64` (278 234 Byte):
 *
 *   G64->D64    35 Spuren gewandelt, 0 gescheitert, 683 Sektoren
 *   G64->SCP    34 Spuren gewandelt, 0 gescheitert   <-- 34 + 0 = 34
 *
 * Eine Spur ist bei G64->SCP weder gewandelt noch gescheitert. Sie ist
 * verschwunden, und der Bericht meldet Erfolg — eine stille
 * Veraenderung im Sinne von `docs/DESIGN_PRINCIPLES.md`.
 *
 * DIE URSACHE IST EINE ZEILE, und sie steht neben ihrem eigenen
 * Gegenbeispiel in derselben Datei
 * (`src/formats/uft_format_convert_bitstream.c`):
 *
 *   :327   if (rc != 0 || !track_data || track_len == 0) continue;
 *   :547   if (rc != 0 || !track_data || track_len == 0) {
 *   :548       result->tracks_failed++;
 *   :549       continue;
 *   :550   }
 *
 * Dieselbe Bedingung, zwei Verhalten. Das ist die Bauform aus MF-1177
 * (eine Groesse, zwei Rechnungen) verschraenkt mit MF-519/MF-529 (eine
 * Stelle geholt, den Nachbarn uebersehen).
 *
 * WARUM DIE ZAHL 35 NICHT VON MIR KOMMT: im selben Korpus liegt
 * `vice_c1541_35trk.d64` mit 174 848 Byte = 683 Sektoren zu 256 Byte —
 * die kanonische Groesse einer 35-Spur-D64, von VICEs `c1541` erzeugt.
 * Der Anker ist also eine zweite, unabhaengig erzeugte Datei und nicht
 * der Dateiname (Klasse MF-1000: der Test befragt nicht dieselbe Quelle
 * wie der Pruefling).
 *
 * WAS DIESER TEST NICHT PRUEFT: WELCHE Spur ausfaellt und warum. Er
 * prueft allein, dass die Buchfuehrung aufgeht. Der Grund des Ausfalls
 * — `g64_get_track()` liefert fuer eine Halbspur nichts — ist eine
 * eigene Frage und bleibt offen.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_core.h"
#include "uft/uft_format_convert.h"
#include "uft/uft_format_plugin.h"

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "tests/corpus_free"
#endif

#define QUELLE UFT_CORPUS_DIR "/vice_c1541_35trk.g64"
#define ANKER  UFT_CORPUS_DIR "/vice_c1541_35trk.d64"

/* 174 848 / 256 = 683 Sektoren = 35 Spuren nach der CBM-Zonentafel.
 * Steht hier als Zahl, wird unten aber an der Dateigroesse des Ankers
 * nachgerechnet statt geglaubt. */
#define ANKER_BYTE      174848u
#define SPUREN_ERWARTET 35

/* Die Wandler direkt, wie `tests/test_convert_roundtrip_measured.c:74-88`
 * es tut. Ueber `uft_convert_file()` sind beide Wege vom Preflight-Tor
 * gesperrt (siehe `pruefe_weg()`), die Buchfuehrung ist also nur hier
 * beobachtbar — und genau hier ist der Defekt auch entstanden. */
extern uft_error_t uftc_convert_g64_to_hfe(const uint8_t *src_data,
                                           size_t src_size,
                                           const char *dst_path,
                                           const uft_convert_options_ext_t *opts,
                                           uft_convert_result_t *result);
extern uft_error_t uftc_convert_g64_to_scp(const uint8_t *src_data,
                                           size_t src_size,
                                           const char *dst_path,
                                           const uft_convert_options_ext_t *opts,
                                           uft_convert_result_t *result);

static int fehler = 0;

static void zusage(int ok, const char *was)
{
    printf("  %-64s %s\n", was, ok ? "ok" : "[ROT]");
    if (!ok) fehler++;
}

static long groesse(const char *pfad)
{
    FILE *f = fopen(pfad, "rb");
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long g = ftell(f);
    fclose(f);
    return g;
}

/* Ein Wandlungslauf, dessen Buchfuehrung geprueft wird. */
static void pruefe_weg(const char *name, uft_format_t ziel_format,
                       const char *ziel_datei)
{
    remove(ziel_datei);

    uft_convert_options_t opt = uft_convert_default_options();
    /* Ohne Zustimmung weist das Preflight-Tor ein Paar ohne
     * Matrix-Eintrag als UNGEPRUEFT ab; dann misst dieser Test nichts
     * (Klasse MF-1000/Tor 64), was unten ausdruecklich rot wird. */
    opt.accept_data_loss = true;

    uft_convert_result_t res;
    memset(&res, 0, sizeof res);

    const uft_error_t rc = uft_convert_file(QUELLE, ziel_datei,
                                            ziel_format, &opt, &res);

    char was[192];

    if (rc != UFT_OK) {
        /* BERICHTIGT gegenueber der ersten Fassung dieses Tests. Dort
         * stand die Zusage „Pfad ist erreichbar, der Vorzustand belegt
         * ihn" — und sie war falsch begruendet: belegt hatte
         * `test_convert_roundtrip_measured` die WANDLERFUNKTION, die es
         * laut seinem eigenen Kopf „direkt, nicht ueber
         * `uft_convert_file()`" ruft. Ueber den oeffentlichen Weg sagt
         * das Preflight-Tor ab, und zwar mit Grund:
         *
         *   G64->SCP  rc=-40  „Flux timing will be synthesized from
         *                      GCR data"      (Fabrikation verweigert)
         *   G64->HFE  rc=-40  „conversion pair is UNTESTED"
         *
         * Beide Absagen sind RICHTIG. Die Zusage lautet deshalb nicht
         * „es muss durchgehen", sondern „es darf nicht stumm
         * scheitern". Das kann rot werden, wenn jemand die Absage
         * wortlos macht. */
        printf("  %-10s ABSAGE rc=%d: %s\n", name, (int)rc,
               res.warning_count ? res.warnings[0] : "(KEIN GRUND GENANNT)");
        snprintf(was, sizeof was, "%s: Absage nennt einen Grund", name);
        zusage(res.warning_count > 0, was);

        /* Und sie darf nichts hinterlassen, was wie ein Ergebnis
         * aussieht. */
        snprintf(was, sizeof was, "%s: Absage laesst keine Datei zurueck",
                 name);
        zusage(groesse(ziel_datei) < 0, was);

        remove(ziel_datei);
        return;
    }

    const int summe = res.tracks_converted + res.tracks_failed;

    printf("  %-10s gewandelt %d, gescheitert %d, Summe %d, erwartet %d\n",
           name, res.tracks_converted, res.tracks_failed, summe,
           SPUREN_ERWARTET);

    /* KERNZUSAGE. Was der Wandler nicht wandelt, muss er als gescheitert
     * melden. Eine Spur, die weder in der einen noch in der anderen
     * Zahl steht, ist still verschwunden. */
    snprintf(was, sizeof was,
             "%s: gewandelt + gescheitert = vorhandene Spuren", name);
    zusage(summe == SPUREN_ERWARTET, was);

    /* Und der Bericht darf nicht mehr Spuren behaupten, als die
     * Diskette hat. */
    snprintf(was, sizeof was,
             "%s: gewandelte Spuren uebersteigen die Diskette nicht", name);
    zusage(res.tracks_converted <= SPUREN_ERWARTET, was);

    remove(ziel_datei);
}

/* Der eigentliche Rotbeweis. Beide Wandler laufen ueber DIESELBE Datei
 * und dieselbe Schleifengrenze (`track <= 42`), also muessen sie gleich
 * viele Spuren VERSUCHT haben. Die Zusage braucht damit keinen aeusseren
 * Anker — sie vergleicht zwei Nachbarn miteinander.
 *
 * Gemessen VOR der Korrektur: SCP 34 + 0 = 34, HFE 34 + 8 = 42.
 * Gemessen DANACH:            SCP 34 + 8 = 42, HFE 34 + 8 = 42. */
static void buchfuehrung_direkt(void)
{
    FILE *f = fopen(QUELLE, "rb");
    if (!f) { zusage(0, "direkt: Quelle lesbar"); return; }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); zusage(0, "direkt: seek"); return; }
    long g = ftell(f);
    if (g <= 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f); zusage(0, "direkt: Groesse"); return;
    }
    uint8_t *d = (uint8_t *)malloc((size_t)g);
    if (!d) { fclose(f); zusage(0, "direkt: Speicher"); return; }
    if (fread(d, 1, (size_t)g, f) != (size_t)g) {
        free(d); fclose(f); zusage(0, "direkt: vollstaendig gelesen"); return;
    }
    fclose(f);

    uft_convert_options_ext_t ext;
    memset(&ext, 0, sizeof ext);

    uft_convert_result_t r_scp, r_hfe;
    memset(&r_scp, 0, sizeof r_scp);
    memset(&r_hfe, 0, sizeof r_hfe);

    const char *z_scp = "uft_g64_direkt.scp";
    const char *z_hfe = "uft_g64_direkt.hfe";
    remove(z_scp);
    remove(z_hfe);

    uftc_convert_g64_to_scp(d, (size_t)g, z_scp, &ext, &r_scp);
    uftc_convert_g64_to_hfe(d, (size_t)g, z_hfe, &ext, &r_hfe);

    const int summe_scp = r_scp.tracks_converted + r_scp.tracks_failed;
    const int summe_hfe = r_hfe.tracks_converted + r_hfe.tracks_failed;

    printf("  direkt  SCP %d+%d=%d   HFE %d+%d=%d\n",
           r_scp.tracks_converted, r_scp.tracks_failed, summe_scp,
           r_hfe.tracks_converted, r_hfe.tracks_failed, summe_hfe);

    /* Eine Summe von 0 waere gruen aus dem falschen Grund. */
    zusage(summe_scp > 0 && summe_hfe > 0,
           "direkt: beide Wandler haben ueberhaupt Spuren versucht");

    /* KERNZUSAGE — ohne aeusseren Anker: gleiche Quelle, gleiche
     * Schleifengrenze, also gleiche Zahl versuchter Spuren. */
    zusage(summe_scp == summe_hfe,
           "direkt: G64->SCP und G64->HFE versuchen gleich viele Spuren");

    /* ZWEITE KERNZUSAGE (MF-1332): die Diskette hat 35 Spuren, und der
     * D64-Weg wandelt sie auch alle — gemessen „35 Spuren gewandelt,
     * 0 gescheitert". Die Bitstrom-Wege muessen auf dieselbe Zahl
     * kommen.
     *
     * Gegen den Vorzustand sind es 34, und der Grund ist die
     * Halbspur-Abbildung: `uft_format_convert_bitstream.c` rechnet an
     * drei Stellen `halftrack = track * 2`, waehrend MF-928 die
     * Konvention auf `G64_TRACK_TO_HALFTRACK(t) = ((t)-1)*2` umgestellt
     * hat („Eintrag i IST Platz i"). Spur 1 liegt damit auf Platz 0 und
     * wird nie gefragt; gelesen werden die echten Spuren 2..35 unter
     * den Namen 1..34. MF-928 hat die eigene Datei an drei Stellen
     * nachgezogen und die drei Stellen nebenan uebersehen —
     * MF-519/MF-529. */
    zusage(r_scp.tracks_converted == SPUREN_ERWARTET,
           "direkt: G64->SCP wandelt alle 35 Spuren der Diskette");
    zusage(r_hfe.tracks_converted == SPUREN_ERWARTET,
           "direkt: G64->HFE wandelt alle 35 Spuren der Diskette");

    remove(z_scp);
    remove(z_hfe);
    free(d);
}

int main(void)
{
    printf("G64-Wandlung: die Buchfuehrung muss aufgehen (MF-1332)\n");

    /* MF-446/447: erst `main()` fuellt die Registry. Ohne diesen Aufruf
     * ist sie zur Laufzeit LEER, `uft_probe_format()` erkennt nichts,
     * und jede Wandlung sagt mit „Could not detect source format" ab —
     * der Test haette dann den Werkzeugfehler als Produktdefekt
     * gemeldet (Klasse MF-1125). Gemessen: genau das ist beim ersten
     * Lauf dieses Tests passiert. */
    if (uft_register_all_formats() != UFT_OK) {
        printf("  UEBERSPRUNGEN: Formatregistrierung fehlgeschlagen\n");
        return 77;
    }

    const long qg = groesse(QUELLE);
    const long ag = groesse(ANKER);
    if (qg <= 0 || ag <= 0) {
        printf("  UEBERSPRUNGEN: Korpus fehlt (%s / %s)\n", QUELLE, ANKER);
        return 77;                              /* benannter Skip */
    }

    /* Der Anker wird nachgerechnet, nicht geglaubt: 683 Sektoren zu 256
     * Byte sind die kanonische 35-Spur-D64. Stimmt die Groesse nicht,
     * taugt die erwartete Spurzahl nicht und der Test sagt das. */
    printf("  Quelle %ld Byte, Anker %ld Byte = %ld Sektoren a 256\n",
           qg, ag, ag / 256);
    zusage(ag == (long)ANKER_BYTE,
           "Anker ist die kanonische 35-Spur-D64 (174 848 Byte)");

    pruefe_weg("G64->SCP", UFT_FORMAT_SCP, "uft_g64_buchfuehrung.scp");
    /* Gegenprobe im selben Lauf: derselbe Leser, dieselbe Quelle, nur
     * der Nachbarzweig — er zaehlt seine Ausfaelle und muss gruen sein.
     * Ohne ihn waere nicht zu unterscheiden, ob die Zusage ueberhaupt
     * erfuellbar ist. */
    pruefe_weg("G64->HFE", UFT_FORMAT_HFE, "uft_g64_buchfuehrung.hfe");

    buchfuehrung_direkt();

    printf("%s\n", fehler ? "FEHLGESCHLAGEN" : "BESTANDEN (0 Fehler)");
    return fehler ? 1 : 0;
}
