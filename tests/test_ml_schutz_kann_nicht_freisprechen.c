/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_ml_schutz_kann_nicht_freisprechen.c
 * @brief Ein Erkenner, der niemals „nein" sagen kann, ist kein Erkenner
 *        (MF-882)
 *
 * -- Der Befund --------------------------------------------------------
 *
 * `src/analysis/uft_ml_protection.c` vergleicht einen 8-stelligen
 * Merkmalsvektor per Cosinus-Aehnlichkeit gegen 15 handgesetzte
 * „reference signatures" und meldet bei >= 0.70 einen PRODUKTNAMEN mit
 * Prozentzahl. Der Zweig `is_protected = false` (:369, „No copy
 * protection detected") wurde gemessen: er ist fuer JEDE Eingabe
 * unerreichbar.
 *
 * Gemessen am uebersetzten Modul, drei unabhaengige Abtastungen:
 *
 *   |                                        | Punkte  | „kein Schutz" |
 *   |----------------------------------------|---------|---------------|
 *   | Eingaberaum, den die GUI erzeugen kann | 441     | **0**         |
 *   | Raster ueber den ganzen 8D-Raum (3^8)  | 6561    | **0**         |
 *   | Zufallsabtastung ueber den 8D-Raum     | 500 000 | **0**         |
 *
 * Kleinste je erreichte beste Aehnlichkeit: **0.5134** — die Schwelle ist
 * 0.70, und was darunter faellt, faengt der `weirdness`-Zweig (:365,
 * Schwelle 0.4). Ein leerer Vektor — also „nichts gemessen" — ergibt
 * „Unknown protection scheme suspected (weirdness: 50.0%)", weil
 * `fabsf(blended[1] - 1.0f) * 5.0f` eine ungemessene Null als maximale
 * Abweichung vom Nennwert liest.
 *
 * -- Warum der Cosinus das tut ----------------------------------------
 *
 * Alle 15 Signaturen tragen an Stelle [1] (Laengenverhaeltnis) einen Wert
 * um 1.0 — die groesste Komponente jedes Vektors. Der Cosinus ist
 * skaleninvariant und misst danach im Wesentlichen die gemeinsame
 * Grosskomponente gegen sich selbst. Eine mustergueltig SAUBERE Diskette
 * (Nennlaenge, kein Sync, kein schlechtes GCR, kaum Jitter) liegt damit
 * geometrisch neben „Long Track Generic" und wird gemessen mit
 * **99.1 %** so benannt.
 *
 * -- Warum es zaehlt ---------------------------------------------------
 *
 * Der Pfad ist ERREICHBAR und der einzige Aufrufer ist die Oberflaeche:
 * `src/gui/uft_otdr_panel.cpp:1018` faerbt das Ergebnis bei
 * `is_protected` fett rot. Jede geoeffnete Diskette bekam damit einen
 * Schutz-Befund mit Herstellernamen und Prozentzahl.
 *
 * Erschwerend: dieselbe Aufrufstelle uebergibt **sechs der acht**
 * Merkmale als Festwerte (`:1006-1012`: `1.0f`, `0`, `0`, `0`, `false`,
 * `false`). Nur Entropie und Jitter stammen aus Messdaten. Der Vergleich
 * lief also gegen die eigenen Platzhalter.
 *
 * -- Warum die Tabelle nicht nachgebessert wurde -----------------------
 *
 * Die 15 Signaturen haben **keine Quelle**: kein Spezifikationsverweis,
 * kein Korpus, kein Trainingslauf — der Dateikopf sagt nur „Known
 * protection reference signatures" und `@author UFT Project`. Schwellen
 * oder Geometrie zu justieren hiesse, einen zweiten Satz unbelegter
 * Zahlen ueber den ersten zu legen. Das ist die Klasse, an der dieser
 * Baum fuenfmal verbrannt ist (FMT-2/3/10/11/12).
 *
 * Der Baum hat fuer diese Lage bereits ein Vorbild, dreimal:
 * `uft_protection_extended.c:522` („NOT IMPLEMENTED — returns an error,
 * not 'not detected'"), `uft_speedlock.c:385` und
 * `uft_c64_protection_enhanced.c:655` („A missing implementation must
 * not look like a measurement.").
 *
 * -- Was dieser Test festhaelt ----------------------------------------
 *
 * 1. Eine mustergueltig saubere Diskette darf keinen Produktnamen
 *    bekommen. (Rot gegen den Vorzustand: „Long Track Generic 99.1 %".)
 * 2. Die Absage muss von einem Erfolg UNTERSCHEIDBAR sein — sonst waere
 *    sie ein stiller Freispruch, also derselbe Fehler in die andere
 *    Richtung.
 * 3. Es darf kein Kandidat genannt werden.
 * 4. Die Begruendung muss beim Aufrufer ankommen, nicht nur im Log.
 *
 * Gegenprobe (von Hand gefahren, MF-882): wird die Absage durch
 * `return 0` mit leerem `summary` ersetzt — also durch einen stillen
 * Freispruch —, faellt Pruefung 2 und 4. Wird der alte Rumpf
 * zurueckgeholt, faellt Pruefung 1 und 3. Jede Mutation faellt genau die
 * Pruefungen, die sie betrifft.
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "uft/analysis/uft_ml_protection.h"

static int fehler = 0;

static void pruefe(int bedingung, const char *was)
{
    printf("  [%s] %s\n", bedingung ? "ok" : "ROT", was);
    if (!bedingung)
        fehler++;
}

int main(void)
{
    printf("MF-882: der Signaturvergleich darf nicht benennen, was er "
           "nicht freisprechen kann\n\n");

    /* Eine mustergueltig saubere Diskette: Nennlaenge, kein Sync, kein
     * schlechtes GCR, keine doppelten IDs, kaum Jitter, keine Halbspur. */
    float merkmale[1][UFT_ML_PROT_FEATURES] = {
        { 0.40f, 1.00f, 0.00f, 0.00f, 0.00f, 0.05f, 0.00f, 0.00f }
    };

    uft_ml_prot_result_t r;
    memset(&r, 0, sizeof(r));
    int rc = uft_ml_detect_protection(merkmale, 1, &r);

    printf("  rc=%d  is_protected=%s  count=%d\n", rc,
           r.is_protected ? "true" : "false", r.count);
    printf("  summary=\"%s\"\n\n", r.summary);

    /* 1. Kein Produktname fuer eine saubere Diskette. */
    pruefe(!r.is_protected,
           "eine mustergueltig saubere Diskette wird nicht als geschuetzt "
           "gemeldet");

    /* 2. Die Absage ist von einem Erfolg unterscheidbar. Ein Rueckgabewert
     *    von 0 hiesse „klassifiziert" — und `is_protected == false` waere
     *    dann ein FREISPRUCH, den dieses Modul gemessen nicht aussprechen
     *    kann. */
    pruefe(rc == UFT_ML_PROT_NICHT_GEPRUEFT,
           "der Rueckgabewert sagt NICHT GEPRUEFT, nicht Erfolg");

    /* 3. Kein Kandidat wird genannt. */
    pruefe(r.count == 0,
           "es wird kein Kandidat aus einer unbelegten Tabelle genannt");

    /* 4. Die Begruendung erreicht den Aufrufer. */
    pruefe(r.summary[0] != '\0',
           "die Begruendung steht im Ergebnis, nicht nur im Quelltext");

    /* 5. Auch ein AUFFAELLIGER Vektor bekommt keine Nennung — die Absage
     *    darf nicht von der Eingabe abhaengen, sonst waere sie wieder ein
     *    Urteil aus derselben Tabelle. */
    float auffaellig[1][UFT_ML_PROT_FEATURES] = {
        { 0.90f, 1.25f, 0.80f, 0.40f, 0.30f, 0.70f, 1.00f, 1.00f }
    };
    uft_ml_prot_result_t r2;
    memset(&r2, 0, sizeof(r2));
    int rc2 = uft_ml_detect_protection(auffaellig, 1, &r2);
    pruefe(rc2 == UFT_ML_PROT_NICHT_GEPRUEFT && r2.count == 0 &&
               !r2.is_protected,
           "auch ein auffaelliger Vektor bekommt keine Nennung");

    /* 6. Argumentfehler bleiben Argumentfehler und werden nicht mit der
     *    Absage verwechselt. */
    uft_ml_prot_result_t r3;
    memset(&r3, 0, sizeof(r3));
    pruefe(uft_ml_detect_protection(NULL, 1, &r3) == -1,
           "ein Argumentfehler meldet -1, nicht NICHT GEPRUEFT");
    pruefe(uft_ml_detect_protection(merkmale, 0, &r3) == -1,
           "n_tracks == 0 meldet -1");

    /* 7. Die Merkmalsextraktion bleibt unangetastet — sie normiert nur und
     *    behauptet nichts. Entropie eines Ein-Gipfel-Histogramms ist 0. */
    float hist[256];
    memset(hist, 0, sizeof(hist));
    hist[42] = 1000.0f;
    float m[UFT_ML_PROT_FEATURES];
    int rcf = uft_ml_extract_features(hist, 1.0f, 0, 0, 0, 0.0f, false, false, m);
    pruefe(rcf == 0 && m[0] == 0.0f && m[1] == 1.0f,
           "die Merkmalsextraktion normiert weiterhin (Ein-Gipfel: "
           "Entropie 0)");

    printf("\n%s: %d Pruefung(en) rot\n", fehler ? "FEHLGESCHLAGEN" : "OK",
           fehler);
    return fehler ? 1 : 0;
}
