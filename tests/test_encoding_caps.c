/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_encoding_caps.c
 * @brief Die Faehigkeitstabelle sagt die Wahrheit — auch ueber sich selbst (MF-865)
 *
 * ── Was hier bewacht wird ────────────────────────────────────────────
 *
 * `uft_encoding_caps()` erklaert je Kodierung, ob dieses Werkzeug sie
 * erkennen und lesen kann. Die Erklaerung ist nur so viel wert wie ihre
 * Vollstaendigkeit: eine Kodierung, die in der Tabelle FEHLT, bekommt
 * den Rueckfallwert — und der sagt „nichts belegt".
 *
 * Das ist die richtige Vorgabe, aber es waere eine bequeme Luecke. Wer
 * eine neue Kodierung in die Aufzaehlung schreibt und die Tabelle
 * vergisst, bekaeme still „kann nichts" statt eines Fehlers. Deshalb
 * prueft der erste Fall unten die VOLLSTAENDIGKEIT gegen die
 * Aufzaehlung.
 *
 * Die zweite Haelfte — stimmt `can_decode` mit dem Verteiler ueberein? —
 * kann ein C-Test nicht sehen; das misst `scripts/audit_encoding_caps.py`
 * (Tor 54) am Quelltext von `flux_decode_track()`.
 *
 * ── Die Messung hinter den M2FM-Zeilen ───────────────────────────────
 *
 * `grep M2FM` ueber `src/flux/` findet null; `flux_encoding_t` kennt es
 * nicht; und keine Zuweisung im Baum setzt `UFT_ENC_M2FM` je als
 * Ergebnis. Alle Fundstellen sind `case`-Zweige, Namenstabellen oder ein
 * GUI-Eintrag (MF-865, ueber `git ls-files` gemessen).
 */
#include "uft/core/uft_encoding_caps.h"

#include <stdio.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last = 0;
#define RUN(n)  do { printf("  [TEST] %-46s ... ", #n); test_##n(); \
                     if (_last == _fail) { printf("OK\n"); _pass++; } \
                     _last = _fail; } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                       _fail++; return; } } while (0)

/* Jeder Wert aus `uft_track_encoding_t`. Waechst die Aufzaehlung, muss
 * diese Liste mitwachsen — und genau das faellt im ersten Fall auf. */
static const uft_track_encoding_t ALLE[] = {
    UFT_ENC_UNKNOWN, UFT_ENC_FM, UFT_ENC_MFM, UFT_ENC_GCR_C64,
    UFT_ENC_GCR_APPLE, UFT_ENC_AMIGA_MFM, UFT_ENC_GCR_VICTOR,
    UFT_ENC_M2FM, UFT_ENC_RAW,
};
#define ALLE_N (sizeof ALLE / sizeof ALLE[0])

TEST(jede_kodierung_der_aufzaehlung_ist_gefuehrt)
{
    /* Der Rueckfallwert traegt `UFT_ENC_UNKNOWN` als `enc`. Wer also
     * einen anderen Wert abfragt und `UNKNOWN` zurueckbekommt, ist durch
     * den Rueckfall gelaufen — die Kodierung fehlt in der Tabelle. */
    for (size_t i = 0; i < ALLE_N; i++) {
        const uft_encoding_caps_t *c = uft_encoding_caps(ALLE[i]);
        ASSERT(c != NULL);
        if (ALLE[i] != UFT_ENC_UNKNOWN && c->enc == UFT_ENC_UNKNOWN) {
            printf("\n      Kodierung %d fehlt in der Tabelle — sie wuerde"
                   " still als „nichts belegt\" gelten\n      ",
                   (int)ALLE[i]);
            _fail++;
            return;
        }
    }
    ASSERT(uft_encoding_caps_count() == ALLE_N);
}

TEST(eine_ungefuehrte_kodierung_bekommt_keine_faehigkeit)
{
    /* Gegenprobe zum Rueckfall: er darf nie etwas erlauben. */
    const uft_track_encoding_t ERFUNDEN = (uft_track_encoding_t)200;
    const uft_encoding_caps_t *c = uft_encoding_caps(ERFUNDEN);
    ASSERT(c != NULL);
    ASSERT(c->can_detect == false);
    ASSERT(c->can_decode == false);
    ASSERT(c->grenze != NULL);
    ASSERT(uft_encoding_can_decode(ERFUNDEN) == false);
}

TEST(m2fm_ist_weder_erkennbar_noch_dekodierbar)
{
    /* DER FALL, DER DEN AUFTRAG AUSLOESTE. Die GUI bot M2FM zur Auswahl
     * an und meldete es danach als „Encoding: M2FM" zurueck. */
    ASSERT(uft_encoding_can_decode(UFT_ENC_M2FM) == false);
    ASSERT(uft_encoding_can_detect(UFT_ENC_M2FM) == false);
    ASSERT(uft_encoding_grenze(UFT_ENC_M2FM) != NULL);
    /* Die Begruendung muss die Sache benennen, nicht nur „nicht
     * unterstuetzt" sagen. */
    ASSERT(strstr(uft_encoding_grenze(UFT_ENC_M2FM), "M2FM") != NULL);
}

TEST(die_fuenf_verdrahteten_dekoder_stehen_als_dekodierbar)
{
    /* Was `flux_decode_track()` an einen Dekoder routet, der Sektoren
     * anlegt. FM gehoert seit MF-864 dazu — davor war es der Fall, den
     * diese Tabelle sichtbar machen soll. */
    ASSERT(uft_encoding_can_decode(UFT_ENC_FM));
    ASSERT(uft_encoding_can_decode(UFT_ENC_MFM));
    ASSERT(uft_encoding_can_decode(UFT_ENC_GCR_C64));
    ASSERT(uft_encoding_can_decode(UFT_ENC_GCR_APPLE));
    ASSERT(uft_encoding_can_decode(UFT_ENC_AMIGA_MFM));
}

TEST(dekodierbar_heisst_nicht_erkennbar)
{
    /* Die zwei Spalten sind nicht dasselbe, und das ist der Kern der
     * Tabelle. Die Auto-Erkennung des Flusspfads liefert MFM oder FM —
     * GCR und Amiga muessen VORGEGEBEN werden, obwohl sie dekodiert
     * werden koennen. Wer das zusammenwirft, baut wieder eine Anzeige,
     * die eine Vorgabe als Befund ausgibt. */
    ASSERT(uft_encoding_can_decode(UFT_ENC_GCR_C64));
    ASSERT(uft_encoding_can_detect(UFT_ENC_GCR_C64) == false);
    ASSERT(uft_encoding_grenze(UFT_ENC_GCR_C64) != NULL);

    ASSERT(uft_encoding_can_decode(UFT_ENC_AMIGA_MFM));
    ASSERT(uft_encoding_can_detect(UFT_ENC_AMIGA_MFM) == false);
}

TEST(wer_etwas_kann_braucht_keine_begruendung)
{
    /* Und umgekehrt: wer beides kann, traegt KEINE Grenze. Sonst
     * verwaessert das Feld zu einem Kommentarfeld. */
    for (size_t i = 0; i < uft_encoding_caps_count(); i++) {
        const uft_encoding_caps_t *c = uft_encoding_caps_at(i);
        ASSERT(c != NULL);
        if (c->can_detect && c->can_decode && c->grenze != NULL) {
            printf("\n      Kodierung %d kann alles, traegt aber eine "
                   "Grenze: %s\n      ", (int)c->enc, c->grenze);
            _fail++;
            return;
        }
        if ((!c->can_detect || !c->can_decode) && c->grenze == NULL) {
            printf("\n      Kodierung %d fehlt etwas, sagt aber nicht "
                   "was\n      ", (int)c->enc);
            _fail++;
            return;
        }
    }
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Kodierungen: was koennen wir wirklich? (MF-865) ===\n");
    RUN(jede_kodierung_der_aufzaehlung_ist_gefuehrt);
    RUN(eine_ungefuehrte_kodierung_bekommt_keine_faehigkeit);
    RUN(m2fm_ist_weder_erkennbar_noch_dekodierbar);
    RUN(die_fuenf_verdrahteten_dekoder_stehen_als_dekodierbar);
    RUN(dekodierbar_heisst_nicht_erkennbar);
    RUN(wer_etwas_kann_braucht_keine_begruendung);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
