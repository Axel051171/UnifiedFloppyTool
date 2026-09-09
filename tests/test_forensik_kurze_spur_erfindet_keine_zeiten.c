/**
 * @file test_forensik_kurze_spur_erfindet_keine_zeiten.c
 * @brief Eine Spur mit zu wenig Flusswechseln darf keine Zeiten liefern (MF-995).
 *
 * ── Woher die Frage kommt ────────────────────────────────────────────
 *
 * Zwei `-Wmaybe-uninitialized` aus dem Linux-Bau:
 *
 *   uft_forensic_track.c:324: 'timing.rotation_time_ms' may be used uninitialized
 *   uft_forensic_track.c:339: 'timing.timing_histogram' may be used uninitialized
 *
 * Dahinter steht dieselbe Bauart wie bei MF-987, nur eine Ebene ernster.
 * `uft_forensic_recover_track()` legt sich eine **uninitialisierte**
 * `timing_analysis_t` auf den Stapel und fuellt sie so:
 *
 *     timing_analysis_t timing;
 *     analyze_timing(flux_timestamps[0], flux_counts[0], &timing, session);
 *     out_track->rotation_time_ms = timing.rotation_time_ms;
 *
 * Der Rueckgabewert wird **nicht geprueft**. Und `analyze_timing()` hat
 * einen Rueckweg, der VOR seinem eigenen `memset` liegt:
 *
 *     if (!flux_timestamps || flux_count < 10 || !out_analysis) return -1;
 *     memset(out_analysis, 0, sizeof(*out_analysis));
 *
 * Unter zehn Flusswechseln bleibt die Struktur also **unberuehrt**, und
 * der Aufrufer benutzt sie an vier Stellen weiter:
 *
 *   Z. 324  `rotation_time_ms`  wird als gemessene Umdrehungszeit ausgegeben
 *   Z. 366  `cell_time_ns`      steht im NENNER einer Division
 *   Z. 460  `timing_anomalies`  wird als Befund ausgegeben
 *   Z. 469  `timing_histogram`  wird an `free()` uebergeben
 *
 * Die letzte ist keine Ungenauigkeit mehr: **ein Stapelwert wird als
 * Zeiger freigegeben**, auf dem normalen Pfad, nicht nur bei
 * Speichermangel.
 *
 * ── Warum genau dieser Fall zaehlt ───────────────────────────────────
 *
 * „Weniger als zehn Flusswechsel" ist kein Kunstgriff. Das ist eine
 * **leere oder schwer beschaedigte Spur** — also genau der Fall, fuer den
 * es ein forensisches Wiederherstellungsmodul ueberhaupt gibt. Der
 * Fehlerweg liegt dort, wo das Werkzeug seinen Zweck hat.
 *
 * ── Reichweite, ehrlich ──────────────────────────────────────────────
 *
 * `src/recovery/uft_forensic_track.c` steht in `docs/orphan_baseline.txt`
 * (Z. 239): kein Produktions-Aufrufer. Wie bei LDBS (MF-987) ist der
 * Fehler damit latent — und wie dort ist er eine geladene Waffe, weil
 * `uft_forensic_recover_track()` in
 * `include/uft/recovery/uft_forensic_types.h:262` oeffentlich erklaert
 * ist. Dieser Test ruft die veroeffentlichte API.
 *
 * ── Zum Stapelfaerben ────────────────────────────────────────────────
 *
 * Wie in MF-987: ohne Faerbung waere „Stapelmuell" ein Zufallswert, und
 * ein Test auf Zufall taugt nichts. Gefaerbt wird mit 0x00 NICHT — eine
 * genullte Struktur waere ja gerade der harmlose Fall. Gefaerbt wird mit
 * einem Muster, das als Gleitkommazahl und als Zeiger gleichermassen
 * auffaellt: als uint32_t ergibt 0x7F7F7F7F rund 25 Tage
 * Umdrehungszeit, und als Zeiger eine Adresse, die kein
 * Allokator je zurueckgibt.
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "uft/recovery/uft_forensic_types.h"

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

/* 0x7F als Byte ergibt als float ein grosses, unverwechselbares Muster
 * und als Zeiger eine Adresse, die kein Allokator je zurueckgibt. */
#define MUSTER 0x7F

static void stapel_faerben(void)
{
    volatile uint8_t puffer[16384];
    for (size_t i = 0; i < sizeof(puffer); i++)
        puffer[i] = MUSTER;
}

/* Genau neun Flusswechsel — einer unter der Schranke `flux_count < 10`. */
#define ZU_WENIG 9
/* Genug fuer den Gegenzweig. */
#define GENUG    64

/* Eine ECHTE Sitzung, kein NULL.
 *
 * `uft_forensic_recover_track()` weist `session == NULL` in seiner
 * Eingangspruefung sofort ab. Mit NULL waere dieser Test aus dem falschen
 * Grund gruen geworden — er haette die Eingangspruefung belegt statt der
 * Zeitmessung. Dieselbe Falle wie „ein Rotbeweis, der nicht feuert",
 * einen Schritt frueher: hier feuert er, aber auf das falsche Ziel.
 *
 * `verbosity` steht auf 0 -- der Kopf nennt das „silent". Meldungen der
 * Stufe 0 (CRITICAL) kommen dann noch durch, und das ist gewollt: wenn
 * das Modul in diesem Fall etwas Kritisches zu sagen hat, will ich es
 * im Testprotokoll sehen. */
static void lauf(size_t anzahl, int *rc_out, uft_forensic_track_t *spur)
{
    uint64_t *zeiten = calloc(anzahl, sizeof(uint64_t));
    if (!zeiten) { *rc_out = -99; return; }
    for (size_t i = 0; i < anzahl; i++)
        zeiten[i] = (uint64_t)i * 4000u;      /* 4 us, gleichmaessig */

    uft_forensic_config_t cfg = uft_forensic_config_default();
    cfg.verbosity = 0;

    uft_forensic_session_t sitzung;
    if (uft_forensic_session_init(&sitzung, &cfg) != 0) {
        free(zeiten); *rc_out = -99; return;
    }

    const uint64_t *umdrehungen[1] = { zeiten };
    const size_t    zaehler[1]     = { anzahl };

    memset(spur, 0, sizeof(*spur));
    *rc_out = uft_forensic_recover_track(umdrehungen, zaehler, 1,
                                         /*cylinder*/ 0, /*head*/ 0,
                                         /*format_hint*/ NULL,
                                         &sitzung, spur);
    free(zeiten);
}

/* Der Gegenzweig: eine Spur mit genug Wechseln muss weiterhin durchlaufen.
 * Ohne ihn waere „gib immer -1 zurueck" ein gruener Fix. */
static void test_spur_mit_genug_wechseln_laeuft_durch(void)
{
    stapel_faerben();

    uft_forensic_track_t spur;
    int rc = 0;
    lauf(GENUG, &rc, &spur);
    ASSERT(rc != -99);

    /* Der Rueckgabewert ist hier nicht die Zusage — die Zusage ist, dass
     * die Zeiten aus der EINGABE stammen. Bei 4 us gleichmaessigem Abstand
     * darf keine Zeit das Stapelmuster tragen. */
    if (rc == 0) {
        /* `rotation_time_ms` ist uint32_t -- ein `>= 0` waere hier selbst
         * eine -Wtype-limits-Warnung, also genau die Klasse aus MF-988.
         * Geprueft wird deshalb nur die OBERE Schranke: 10 Sekunden fuer
         * eine Umdrehung ist bereits absurd, das Faerbemuster 0x7F7F7F7F
         * ergibt 2 139 062 143 ms -- knapp 25 Tage. */
        ASSERT(spur.rotation_time_ms < 10000u);
    }
    free(spur.sectors);
}

static void test_zu_kurze_spur_liefert_keine_erfundenen_zeiten(void)
{
    stapel_faerben();

    uft_forensic_track_t spur;
    int rc = 0;
    lauf(ZU_WENIG, &rc, &spur);
    ASSERT(rc != -99);

    /* Die ZUSAGE: entweder das Modul sagt ab, oder es liefert Zeiten, die
     * es gemessen hat. Was nicht sein darf, ist Erfolg mit einer Zahl aus
     * dem Stapel — und ein `free()` auf einen Stapelwert.
     *
     * Vor MF-995 kam der Test hier gar nicht an: Zeile 469 gibt
     * `timing.timing_histogram` frei, und das war das Faerbemuster. */
    if (rc == 0) {
        printf("\n        meldet Erfolg fuer %d Flusswechsel;"
               " rotation_time_ms = %u\n        ",
               ZU_WENIG, (unsigned)spur.rotation_time_ms);
    }
    ASSERT(rc != 0);
    free(spur.sectors);
}

/* ═══ Der zweite Fund: eine Zahl, die niemand gemessen hat ══════════
 *
 * Hier stand `out_analysis->rotation_time_ms = 200.0f;` mit dem
 * Kommentar „In real implementation, this would use index pulses".
 *
 * Der Wert hing an **keiner Eingabe**. Er ging als
 * `uft_forensic_track_t.rotation_time_ms` hinaus, und dessen Kopfzeile
 * nannte ihn „observed rotation". 200 ms sind 300 U/min — eine voellig
 * plausible Zahl, und genau das macht sie gefaehrlich.
 *
 * Aus dieser Eingabe LAESST sich keine Umdrehungszeit bestimmen:
 * `analyze_timing()` bekommt Zeitstempel von Flusswechseln, keine
 * Indexmarken. Ohne Index gibt es keinen Umlaufanfang.
 *
 * Die Zusage lautet deshalb: **0 heisst „nicht gemessen"**. Dieselbe
 * Regel wie `uft_write_protect_t` in MF-986, wo die Null UNKNOWN
 * bedeutet, damit eine genullte Struktur ehrlich statt plausibel ist.
 *
 * Der Fall braucht eine Spur mit GENUG Wechseln — sonst faellt er schon
 * an der Absage aus dem ersten Fund und beweist nichts ueber die
 * Konstante.                                                            */
static void test_umdrehungszeit_wird_nicht_erfunden(void)
{
    stapel_faerben();

    uft_forensic_track_t spur;
    int rc = 0;
    lauf(GENUG, &rc, &spur);
    ASSERT(rc != -99);
    ASSERT(rc == 0);            /* diese Spur ist auswertbar */

    if (spur.rotation_time_ms != 0)
        printf("\n        rotation_time_ms = %u — aber die Eingabe traegt"
               " keine Indexmarken, also ist nichts gemessen\n        ",
               (unsigned)spur.rotation_time_ms);
    ASSERT(spur.rotation_time_ms == 0);

    free(spur.sectors);
}

int main(void)
{
    printf("=== Forensik: eine zu kurze Spur erfindet keine Zeiten (MF-995) ===\n");
    RUN(spur_mit_genug_wechseln_laeuft_durch);
    RUN(umdrehungszeit_wird_nicht_erfunden);
    RUN(zu_kurze_spur_liefert_keine_erfundenen_zeiten);
    printf("\n%d Pruefungen gruen, %d rot\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
