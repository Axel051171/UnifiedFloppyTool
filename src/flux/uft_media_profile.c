/**
 * @file uft_media_profile.c
 * @brief Bitzellendauer aus Medienprofil + gemessener Umdrehungsdauer (MF-471).
 *
 * Herleitung, Quellenangaben und die Grenzen des Verfahrens stehen im Header.
 */

#include "uft/flux/uft_media_profile.h"

#include <math.h>

/* Feineinsteller-Grenzen wie a8rawconv `-p` (a8rawconv.cpp:1158-1161:
 * "adjust clock period by percentage (50-200)"). Ausserhalb davon ist es
 * kein Feineinsteller mehr, sondern ein anderes Format. */
#define UFT_MEDIA_ADJUST_MIN_PCT   50.0
#define UFT_MEDIA_ADJUST_MAX_PCT  200.0

#define NS_PER_SEC  1000000000.0
#define SEC_PER_MIN 60.0

/*
 * Nominale Zellendauer = 1e9 / Datenrate. Sie steht redundant in der
 * Tabelle, damit die Zahl beim Lesen sofort erkennbar ist ("4000 ns = 4 µs
 * FM"). Damit die beiden Spalten nicht auseinanderlaufen koennen, rechnet
 * tests/test_media_profile.c::table_is_internally_consistent sie fuer JEDES
 * Profil gegeneinander.
 *
 * Belege:
 *   Atari FM   250 kHz @ 288 min⁻¹ — a8rawconv.cpp:133-135 ("Atari disk
 *              timing produces 250,000 clocks per second at 288 RPM"),
 *              analyze.cpp:37 (secs_per_rot_288rpm × cells_per_sec_4us),
 *              encode.cpp:4 ("4us @ 288 RPM").
 *   Atari MFM  500 kHz @ 288 min⁻¹ — analyze.cpp:42, a8rawconv.cpp:1143
 *              ("atari-mfm  Calibrate for 288 RPM, 2us bit cell").
 *   PC 360K    500 kHz @ 300 min⁻¹ — analyze.cpp:47-49.
 *   PC 1.2M      1 MHz @ 360 min⁻¹ — 5,25"-HD-Laufwerke drehen mit 360.
 *   PC 720K    500 kHz @ 300 min⁻¹.
 *   PC 1.44M     1 MHz @ 300 min⁻¹.
 *   Amiga DD   500 kHz @ 300 min⁻¹ — 11 Sektoren à 512 B je Spur.
 *   Apple II   250 kHz @ 300 min⁻¹ — encode.cpp:5 ("4us @ 300 RPM
 *              (Apple II GCR)").
 */
static const uft_media_profile_t MEDIA_PROFILES[] = {
    { UFT_MEDIA_ATARI_FM,   "Atari FM (288 min^-1, 4 us)",   288.0,  250000.0, 4000.0 },
    { UFT_MEDIA_ATARI_MFM,  "Atari MFM (288 min^-1, 2 us)",  288.0,  500000.0, 2000.0 },
    { UFT_MEDIA_PC_360K,    "PC 360K (300 min^-1, 2 us)",    300.0,  500000.0, 2000.0 },
    { UFT_MEDIA_PC_12M,     "PC 1.2M (360 min^-1, 1 us)",    360.0, 1000000.0, 1000.0 },
    { UFT_MEDIA_PC_720K,    "PC 720K (300 min^-1, 2 us)",    300.0,  500000.0, 2000.0 },
    { UFT_MEDIA_PC_144M,    "PC 1.44M (300 min^-1, 1 us)",   300.0, 1000000.0, 1000.0 },
    { UFT_MEDIA_AMIGA_DD,   "AmigaDOS DD (300 min^-1, 2 us)",300.0,  500000.0, 2000.0 },
    { UFT_MEDIA_APPLE2_GCR, "Apple II GCR (300 min^-1, 4 us)",300.0, 250000.0, 4000.0 },
};

#define PROFILE_COUNT (sizeof(MEDIA_PROFILES) / sizeof(MEDIA_PROFILES[0]))

const uft_media_profile_t *uft_media_profile(uft_media_kind_t kind)
{
    for (size_t i = 0; i < PROFILE_COUNT; i++) {
        if (MEDIA_PROFILES[i].kind == kind)
            return &MEDIA_PROFILES[i];
    }
    return NULL;
}

size_t uft_media_profile_count(void)
{
    return PROFILE_COUNT;
}

double uft_media_cells_per_rev(uft_media_kind_t kind)
{
    const uft_media_profile_t *p = uft_media_profile(kind);
    if (!p || p->rpm <= 0.0)
        return 0.0;

    /* a8rawconv.cpp:135 — Datenrate / (min^-1 / 60) */
    return p->data_rate_hz * SEC_PER_MIN / p->rpm;
}

bool uft_media_cell_ns_from_rev(uft_media_kind_t kind,
                                double rev_ns,
                                double adjust_pct,
                                double *out_cell_ns)
{
    if (!out_cell_ns)
        return false;
    if (!(rev_ns > 0.0) || !isfinite(rev_ns))
        return false;
    if (!isfinite(adjust_pct) ||
        adjust_pct < UFT_MEDIA_ADJUST_MIN_PCT ||
        adjust_pct > UFT_MEDIA_ADJUST_MAX_PCT)
        return false;

    const double cells = uft_media_cells_per_rev(kind);
    if (!(cells > 0.0))
        return false;

    /* a8rawconv.cpp:136 — gemessene Umdrehung / nominale Zellenzahl,
     * mal Feineinsteller. */
    *out_cell_ns = rev_ns / cells * (adjust_pct / 100.0);
    return true;
}

bool uft_media_rev_ns_from_index(const uint32_t *index_ticks,
                                 size_t count,
                                 uint32_t sample_rate_hz,
                                 double *out_rev_ns)
{
    if (!index_ticks || !out_rev_ns || count < 2 || sample_rate_hz == 0)
        return false;

    /* Aufsteigend muss die Folge sein — sonst ist es keine Index-Reihe,
     * und der Mittelwert unten waere eine Zahl ohne Bedeutung. */
    for (size_t i = 1; i < count; i++) {
        if (index_ticks[i] <= index_ticks[i - 1])
            return false;
    }

    /* rawdiskkf.cpp:204 — ueber ALLE Zwischenraeume mitteln, nicht das
     * erste Paar nehmen: ein einzelner traegt den vollen Motor-Jitter. */
    const double span_ticks = (double)index_ticks[count - 1]
                            - (double)index_ticks[0];
    const double revs = (double)(count - 1);
    const double ticks_per_rev = span_ticks / revs;

    *out_rev_ns = ticks_per_rev * NS_PER_SEC / (double)sample_rate_hz;
    return true;
}

double uft_media_rpm_from_rev_ns(double rev_ns)
{
    if (!(rev_ns > 0.0) || !isfinite(rev_ns))
        return 0.0;
    return NS_PER_SEC * SEC_PER_MIN / rev_ns;
}
