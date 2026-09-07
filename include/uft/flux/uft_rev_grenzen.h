/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_rev_grenzen.h
 * @brief Umdrehungsgrenzen in einem flachen Flussstrom (MF-951)
 *
 * ── Warum es das gibt ────────────────────────────────────────────────
 *
 * MF-950 hat gezeigt: ein Umdrehungsvergleich ohne Ausrichtung meldet
 * auf einer gesunden Spur 99,71 % schwache Bits. Wer Umdrehungen
 * vergleichen will, braucht sie **einzeln** — also die Stelle im
 * Flussstrom, an der jede beginnt.
 *
 * Die Geraete liefern das. Der Baum wirft es weg, und zwar an DREI
 * Stellen (gemessen MF-951):
 *
 *   `src/hal/greaseweazle_backend.c:249`   kopiert nur `fd->samples`,
 *       dann `uft_gw_flux_free(fd)` — `index_times` sterben dort
 *   `src/hal/uft_hal_unified.c` (KryoFlux) holt sie ausdruecklich und
 *       gibt sie sofort frei:
 *       „Free index array (not used in HAL interface currently)"
 *   `src/hal/uft_hal_unified.c` (SCP)      liest je Umdrehung
 *       `rev_hdr[8]` = [index_time(4), data_len(4)] und benutzt nur
 *       die Laenge
 *
 * `uft_hal_read_flux()` (`include/uft/hal/uft_hal.h:187`) hat kein Feld
 * dafuer — eine flache Liste und ein Zaehler.
 *
 * ── Der Fehler, den dieses Modul verhindert ──────────────────────────
 *
 * Ein Bericht (57. Durchgang) schlug vor:
 *
 *     rev_offsets[i] = fd->index_times[i];   // Sample-Index je Umdrehung
 *
 * Das ist falsch, und zwar still. Gemessen an
 * `uft_gw_decode_flux_index_times()` (`uft_greaseweazle_full.c:1350`):
 * `index_times[k]` ist die **Tick-Dauer** der Umdrehung k, kein
 * Abtast-Index. Die beiden haben nicht einmal dieselbe Einheit — bei
 * 72 MHz und 200 ms Umdrehung stuenden dort rund 14 400 000, waehrend
 * der Strom vielleicht 50 000 Abtastungen hat.
 *
 * Ein Verbraucher, der das eine fuer das andere haelt, schneidet die
 * Umdrehungen an willkuerlichen Stellen — und der Vergleich danach
 * misst wieder den Schnitt statt des Mediums.
 *
 * ── Was hier gerechnet wird ──────────────────────────────────────────
 *
 * `samples[i]` sind **Intervalle** in Ticks (Abstand zum vorigen
 * Wechsel). Die Summe der ersten n ist die verstrichene Zeit. Eine
 * Umdrehung endet, wenn diese Summe ihre Dauer erreicht.
 *
 * Das ist geraeteunabhaengig: Greaseweazle, KryoFlux und SuperCard Pro
 * liefern alle Intervalle plus Umdrehungsdauern.
 */
#ifndef UFT_FLUX_REV_GRENZEN_H
#define UFT_FLUX_REV_GRENZEN_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Rechnet Umdrehungs-DAUERN in Abtast-VERSAETZE um.
 *
 * @param samples       Intervalle in Ticks, @p sample_count Eintraege
 * @param sample_count  Anzahl der Intervalle
 * @param dauern        Dauer je Umdrehung in Ticks (z. B. `index_times`)
 * @param dauer_count   Anzahl der Umdrehungen
 * @param versaetze     Ausgabe: Index in @p samples, an dem Umdrehung k
 *                      BEGINNT. `versaetze[0]` ist immer 0.
 * @param max_versaetze Platz in @p versaetze
 *
 * @return Zahl der geschriebenen Versaetze — also der Umdrehungen, die
 *         im Strom BEGINNEN. Das kann weniger sein als @p dauer_count:
 *         verspricht das Geraet drei Umdrehungen und endet der Strom
 *         vorher, wird die dritte nicht erfunden.
 *
 *         **Was diese Zahl NICHT sagt: ob die letzte vollstaendig ist.**
 *         Eine abgebrochene Aufnahme endet mitten in ihrer letzten
 *         Umdrehung; die wird gemeldet, weil sie beginnt, und ihre
 *         wahre — kuerzere — Laenge steht in
 *         uft_rev_laengen_aus_grenzen(). Sie zu verschweigen waere
 *         Datenverlust: fuer einen Umdrehungsvergleich ist auch ein
 *         Teilstueck brauchbar, weil ohnehin nur ueber die gemeinsame
 *         Laenge verglichen wird (MF-949/950).
 *
 *         Der Aufrufer muss die Laengen also ansehen, nicht nur die
 *         Anzahl. Das steht hier, weil eine erste Fassung dieses
 *         Kommentars das Gegenteil behauptete und der Test denselben
 *         Namen trug — die Messung hat beides berichtigt.
 *
 * Gibt 0 zurueck, wenn die Eingabe unbrauchbar ist. Eine Null ist ein
 * Befund, kein Nebenweg: ohne Grenzen darf kein Umdrehungsvergleich
 * laufen (MF-950).
 */
size_t uft_rev_grenzen_aus_dauern(const uint32_t *samples,
                                  size_t sample_count,
                                  const uint32_t *dauern,
                                  size_t dauer_count,
                                  size_t *versaetze,
                                  size_t max_versaetze);

/**
 * @brief Laenge jeder Umdrehung in Abtastungen, aus den Versaetzen.
 *
 * @param versaetze     wie von uft_rev_grenzen_aus_dauern() geliefert
 * @param anzahl        Zahl der Umdrehungen
 * @param sample_count  Gesamtzahl der Abtastungen
 * @param laengen       Ausgabe, @p anzahl Eintraege
 */
void uft_rev_laengen_aus_grenzen(const size_t *versaetze, size_t anzahl,
                                 size_t sample_count, size_t *laengen);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLUX_REV_GRENZEN_H */
