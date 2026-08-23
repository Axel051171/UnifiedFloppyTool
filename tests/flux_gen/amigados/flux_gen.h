/**
 * @file tests/flux_gen/amigados/flux_gen.h
 * @brief Synthetischer AmigaDOS-MFM-Flux fuer Tests (MF-478/MF-480).
 *
 * Anders als die uebrigen Generatoren unter tests/flux_gen/ bildet dieser
 * keinen CONTROLLER nach, sondern ein FORMAT: er erzeugt eine echte
 * AmigaDOS-Spur mit Sync-Marken, Odd/Even-Feldern und gueltigen
 * Pruefsummen — also etwas, das `flux_decode_amiga()` als 11 Sektoren
 * liest.
 *
 * ── Warum es das gibt ────────────────────────────────────────────────────
 *
 * `tests/test_convert_scp_adf.c` deckt den SCP->ADF-Pfad gegen eine echte
 * Greaseweazle-Aufnahme ab — und SKIPt in CI, weil die Datei 32 MB gross und
 * gitignored ist. Ein Test, der nur lokal laeuft, schuetzt keine
 * Verdrahtung.
 *
 * ── Warum man ihm glauben darf ───────────────────────────────────────────
 *
 * Der Kodierer ist die exakte Umkehrung von `decode_amiga_sector()`
 * (src/flux/uft_flux_decoder.c) und **prueft sich selbst**: der erste Test in
 * `test_convert_scp_adf_multirev.c` verlangt, dass der Dekoder alle 11
 * Sektoren byteidentisch zurueckgibt. Dieser Dekoder ist gegen die echte
 * Aufnahme belegt (MF-438). Ein Kodierer, den der belegte Dekoder liest, ist
 * damit kein zweites unbelegtes Stueck Code.
 *
 * ── Was er NICHT nachbildet ──────────────────────────────────────────────
 *
 * Jitter, Signalabfall, Bandbreitenbegrenzung des Kopfes. Der Flux ist exakt
 * — jede Zelle genau @p cell_ns lang. Ein Test hierauf sagt deshalb nichts
 * ueber das Verhalten an einer marginalen Diskette aus; das braucht eine
 * reale Aufnahme.
 */
#ifndef UFT_TESTS_AMIGADOS_FLUX_GEN_H
#define UFT_TESTS_AMIGADOS_FLUX_GEN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Geometrie ──────────────────────────────────────────────────────── */

#define UFT_AMIGADOS_SPT          11
#define UFT_AMIGADOS_SECSZ        512
#define UFT_AMIGADOS_HEADS        2
#define UFT_AMIGADOS_CYLS         80
/** 80 x 2 x 11 x 512 = 901120 */
#define UFT_AMIGADOS_ADF_SIZE     901120u

/** Nennwert einer DD-Zelle. */
#define UFT_AMIGADOS_CELL_NS      2000u
/** Eine Umdrehung bei 300 min^-1. */
#define UFT_AMIGADOS_REV_NS       200000000u
/** 500 kbit/s bei 300 min^-1 — die Zellenzahl, mit der das Medienprofil
 *  rechnet (uft_media_profile.c: UFT_MEDIA_AMIGA_DD). */
#define UFT_AMIGADOS_CELLS_PER_REV 100000u

/* ─── Defekt fuer genau eine Sektorposition ──────────────────────────── */

typedef struct {
    /** 0..10, oder -1 fuer „betrifft keinen Sektor". */
    int     sector;
    /** Datenpruefsumme absichtlich falsch schreiben — genau das, was ein
     *  Kopierschutz auf die Diskette bringt. */
    bool    bad_checksum;
    /** Wenn != 0: Datenbyte 0 dieses Sektors durch diesen Wert ersetzen. */
    uint8_t overwrite;
} uft_amigados_defect_t;

/* ─── Zellenstrom ────────────────────────────────────────────────────── */

typedef struct {
    uint8_t *cells;   /**< MSB-first Zellenstrom, (cap+7)/8+1 Byte gross */
    size_t   cap;     /**< Kapazitaet in Zellen */
    size_t   n;       /**< belegte Zellen */
    int      prev;    /**< letztes DATENbit, fuer die MFM-Taktregel */
} uft_amigados_cells_t;

/* ─── API ────────────────────────────────────────────────────────────── */

/**
 * Eine ganze AmigaDOS-Spur als Zellenstrom bauen.
 *
 * Fuellt bis @p c->cap auf: die Spur traegt danach genau eine Umdrehung.
 * Wer eine andere Zellendauer benutzt, setzt @p c->cap entsprechend
 * (Umdrehungsdauer / Zellendauer) — die Anzahl Zellen je Umdrehung haengt
 * an der Zellendauer, die Umdrehungsdauer nicht.
 *
 * @param c       Zielpuffer; @p cells und @p cap muessen gesetzt sein
 * @param adf     Quell-ADF, mindestens (track+1) * 11 * 512 Byte
 * @param track   AmigaDOS-Spurnummer (cyl*2 + head)
 * @param defect  Defekt fuer eine Sektorposition, oder NULL
 */
void uft_amigados_build_track(uft_amigados_cells_t *c,
                              const uint8_t *adf,
                              uint8_t track,
                              const uft_amigados_defect_t *defect);

/**
 * Zellenstrom -> ns-Intervalle zwischen den Flusswechseln.
 *
 * @param cell_ns  Dauer EINER Zelle. Weicht sie vom Nennwert ab, entsteht
 *                 genau der Fall, fuer den es den Feineinsteller gibt
 *                 (MF-480): die Diskette wurde mit einer anderen Datenrate
 *                 beschrieben, als das Medienprofil annimmt.
 * @return Anzahl geschriebener Intervalle
 */
size_t uft_amigados_cells_to_intervals(const uft_amigados_cells_t *c,
                                       unsigned cell_ns,
                                       uint32_t *out, size_t cap);

/**
 * Ein ADF mit erkennbarem, nicht konstantem Inhalt fuellen.
 *
 * Konstante Fuellung wuerde verschweigen, wenn Sektoren vertauscht landen.
 */
void uft_amigados_fill_pattern(uint8_t *adf, size_t bytes);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TESTS_AMIGADOS_FLUX_GEN_H */
