/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_disk2_priv.h
 * @brief Die innere Gestalt des Zentrums — geteilt zwischen Modell und
 *        Behaelter, sonst von niemandem gelesen.
 *
 * Dieser Kopf liegt in `src/core/` und NICHT in `include/`, weil er kein
 * Vertrag ist: wer ihn einbindet, haengt an der Anordnung der Felder. Genau
 * zwei Dateien duerfen das — `uft_disk2.c` (das Modell) und der
 * Behaelter-Leser/-Schreiber. Jeder andere geht ueber `uft_disk2.h`.
 *
 * Warum es ihn ueberhaupt gibt: ein Behaelter, der ALLES traegt, muss auch
 * an die Zaehler kommen, die kein oeffentlicher Leser hat — die Zahl der
 * Befunde, die gar nicht erst gespeichert wurden, zum Beispiel. Eine
 * Sicherung, die diese Zahl verliert, ist stiller als das Modell.
 */

#ifndef UFT_DISK2_PRIV_H
#define UFT_DISK2_PRIV_H

#include "uft/core/uft_disk2.h"

struct uft_disk2 {
    /* ── Ableitungsregister. Platz 0 bleibt leer: 0 ist UFT_D2_DERIV_NONE,
     *    und eine Kennung, die „keine" heisst, darf kein Objekt treffen. */
    uft_d2_derivation_t *derivs;
    size_t               nderiv, deriv_cap;

    /* ── Spuren. NUMMERN statt Zeiger im Index, weil `tracks` bei jedem
     *    Wachsen umzieht und ein gemerkter Zeiger danach ins Leere sieht. */
    uft_d2_track_t *tracks;
    size_t          ntracks, track_cap;
    uint32_t       *tidx;        /**< [cyl * idx_heads + head] + 1, 0 = keine */
    uint32_t        idx_cyls;
    uint32_t        idx_heads;

    /* ── Dateisysteme. In MF-1272 war es genau eines. */
    uft_d2_fs_t *fs;
    size_t       nfs, fs_cap;

    /* ── Metadaten, dynamisch bis UFT_D2_MAX_META. */
    uft_d2_meta_t *meta;
    size_t         nmeta, meta_cap;
    size_t         meta_hidden;  /**< ueber der Grenze abgewiesen           */

    /* ── Befunde. Feste Zahl, letzter Platz fuer die Ueberlaufmeldung. */
    uft_d2_diag_t diag[UFT_D2_MAX_DIAG];
    size_t        ndiag;
    size_t        diag_hidden;     /**< gar nicht erst gespeichert          */
    size_t        diag_hidden_err; /**< davon Fehler — sonst waere der
                                        Ueberlauf eine stille Kuerzung      */

    /* ── Merkmalcache. `features()` ist ein Lauf ueber alle Sektoren; ihn
     *    bei jedem Bericht zu wiederholen ist Arbeit ohne Aussage. */
    uint32_t feat_cache;
    bool     feat_dirty;

    /** Beim Laden aus einem Behaelter stehen die Einspeise-BEFUNDE schon
     *  in der Datei. Ohne dieses Flag erzeugt jedes `add_*` sie ein
     *  zweites Mal, und der Befundblock waechst bei jeder Sicherung.
     *  FEHLER werden nie unterdrueckt — siehe INGEST_DIAG in uft_disk2.c. */
    bool suppress_ingest_diag;
};

#endif /* UFT_DISK2_PRIV_H */
