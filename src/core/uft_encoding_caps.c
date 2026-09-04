/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_encoding_caps.c
 * @brief Die Faehigkeitstabelle (MF-865). Begruendung im Header.
 *
 * Jede Zeile unten ist eine MESSUNG, keine Absicht. Wie sie zustande kam
 * und was sie bewacht, steht je Eintrag daneben.
 *
 * BENANNTE Initialisierer, nicht positionelle. Zwei Gruende, und der
 * zweite ist der wichtigere:
 *
 *  1. Das Tor „Tote Header-Felder" zaehlt ein Feld als tot, wenn es
 *     nirgends NAMENTLICH geschrieben wird — bei positioneller
 *     Schreibweise waeren das hier drei auf einen Schlag.
 *  2. Wer spaeter ein Feld in die Struktur einschiebt, verschiebt
 *     positionell still ALLE Werte. Genau diese Fehlerklasse ist der
 *     Grund, warum es in diesem Baum ueberhaupt Tore gibt.
 */
#include "uft/core/uft_encoding_caps.h"

#include <stddef.h>

/* Stand der Messung: 2026-09-04, MF-865.
 *
 * `can_decode` stammt aus den `switch`-Zweigen von `flux_decode_track()`
 * (`src/flux/uft_flux_decoder.c`) — und wird von
 * `scripts/audit_encoding_caps.py` dagegen geprueft, damit die Tabelle
 * nicht vom Verteiler abdriften kann.
 *
 * `can_detect` stammt aus der Frage „setzt irgendeine Zuweisung im Baum
 * diesen Wert als Ergebnis?", gemessen ueber `git ls-files`. */
static const uft_encoding_caps_t TABELLE[] = {
    { .enc = UFT_ENC_UNKNOWN,   .can_detect = false, .can_decode = false,
      .grenze =
      "Unbekannt ist kein Ergebnis, sondern das Ausbleiben eines "
      "Ergebnisses." },

    { .enc = UFT_ENC_FM,        .can_detect = true,  .can_decode = true,
      .grenze = NULL },
    /* FM: seit MF-864 legt `flux_decode_fm()` Sektoren an. Davor war
     * genau dieser Eintrag der Fall, den die Tabelle sichtbar machen
     * soll — erreichbar, benannt, ohne Wirkung. */

    { .enc = UFT_ENC_MFM,       .can_detect = true,  .can_decode = true,
      .grenze = NULL },
    { .enc = UFT_ENC_GCR_C64,   .can_detect = false, .can_decode = true,
      .grenze =
      "GCR (C64) wird dekodiert, aber von keiner Erkennung als Ergebnis "
      "gesetzt — es muss vorgegeben werden." },
    { .enc = UFT_ENC_GCR_APPLE, .can_detect = false, .can_decode = true,
      .grenze =
      "GCR (Apple) wird dekodiert, aber von keiner Erkennung als Ergebnis "
      "gesetzt — es muss vorgegeben werden." },
    { .enc = UFT_ENC_AMIGA_MFM, .can_detect = false, .can_decode = true,
      .grenze =
      "Amiga-MFM wird dekodiert, aber von keiner Erkennung als Ergebnis "
      "gesetzt — es muss vorgegeben werden." },

    { .enc = UFT_ENC_GCR_VICTOR, .can_detect = false, .can_decode = false,
      .grenze =
      "Victor-9000-GCR ist nur ein Aufzaehlungseintrag: der Bezeichner "
      "kommt in keiner Quelldatei des Baums vor." },

    { .enc = UFT_ENC_M2FM,      .can_detect = false, .can_decode = false,
      .grenze =
      "M2FM ist im Baum benannt, aber weder erkannt noch dekodiert: die "
      "Aufzaehlung des Flusspfads kennt es nicht, und keine Zuweisung "
      "setzt es je als Ergebnis." },

    { .enc = UFT_ENC_RAW,       .can_detect = false, .can_decode = false,
      .grenze =
      "Rohbits werden durchgereicht, nicht dekodiert — der Verteiler "
      "hat fuer sie keinen Zweig." },
};

#define TABELLE_N (sizeof TABELLE / sizeof TABELLE[0])

/* Rueckfallwert. Absichtlich beide Faehigkeiten `false`: eine Kodierung,
 * die hier fehlt, ist NICHT stillschweigend nutzbar. Ein Test prueft,
 * dass die Tabelle vollstaendig ist, damit dieser Zweig nicht zur
 * bequemen Luecke wird. */
static const uft_encoding_caps_t UNGEFUEHRT = {
    .enc = UFT_ENC_UNKNOWN, .can_detect = false, .can_decode = false,
    .grenze = "Diese Kodierung ist in der Faehigkeitstabelle nicht gefuehrt — "
    "ueber sie ist nichts belegt."
};

const uft_encoding_caps_t *uft_encoding_caps(uft_track_encoding_t enc)
{
    for (size_t i = 0; i < TABELLE_N; i++)
        if (TABELLE[i].enc == enc)
            return &TABELLE[i];
    return &UNGEFUEHRT;
}

bool uft_encoding_can_detect(uft_track_encoding_t enc)
{
    return uft_encoding_caps(enc)->can_detect;
}

bool uft_encoding_can_decode(uft_track_encoding_t enc)
{
    return uft_encoding_caps(enc)->can_decode;
}

const char *uft_encoding_grenze(uft_track_encoding_t enc)
{
    return uft_encoding_caps(enc)->grenze;
}

size_t uft_encoding_caps_count(void)
{
    return TABELLE_N;
}

const uft_encoding_caps_t *uft_encoding_caps_at(size_t i)
{
    return (i < TABELLE_N) ? &TABELLE[i] : NULL;
}
