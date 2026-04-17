/**
 * @file uft_format_registry.c
 * @brief Plattform-basierte Format-Registry (130 Plugins in 15 Gruppen)
 *
 * Gruppen:
 *   CBM       — Commodore 8-bit (8)
 *   ATARI     — Atari 8-bit + ST (9)
 *   APPLE     — Apple II + Macintosh (7)
 *   AMIGA     — Commodore Amiga (3)
 *   IBM_PC    — IBM PC kompatibel + Generic (10)
 *   AMSTRAD   — Amstrad CPC + PCW (3)
 *   SPECTRUM  — ZX Spectrum + SAM Coupe + Jupiter Ace (8)
 *   ACORN     — BBC Micro + Acorn (3)
 *   TRS80     — Tandy TRS-80 + CoCo (6)
 *   MSX       — MSX Computer (2)
 *   CPM       — CP/M Generic (5)
 *   JAPAN     — PC-88/98, X68000, Sharp, Fujitsu, Toshiba (13)
 *   FLUX      — Universelle Flux-Container (4)
 *   TAPE      — Tape-Formate (1)
 *   OTHER     — Nischen, nicht-klassifizierte DSK-Varianten (48)
 *
 * Siehe FORMAT_GROUPS.md für vollständige Plugin-zu-Gruppe Zuordnung.
 */

#include "uft/uft_format_plugin.h"
#include <string.h>
#include <stdbool.h>

/* ============================================================================
 * Gruppe: CBM — Commodore 8-bit (8 Plugins)
 * ============================================================================ */
extern const uft_format_plugin_t uft_format_plugin_d64;
extern const uft_format_plugin_t uft_format_plugin_d67;
extern const uft_format_plugin_t uft_format_plugin_d71;
extern const uft_format_plugin_t uft_format_plugin_d80;
extern const uft_format_plugin_t uft_format_plugin_d81;
extern const uft_format_plugin_t uft_format_plugin_d82;
extern const uft_format_plugin_t uft_format_plugin_g71;
extern const uft_format_plugin_t uft_format_plugin_dsk_vic;

/* ============================================================================
 * Gruppe: ATARI — Atari 8-bit + ST (9 Plugins)
 * ============================================================================ */
extern const uft_format_plugin_t uft_format_plugin_atr;
extern const uft_format_plugin_t uft_format_plugin_atx;
extern const uft_format_plugin_t uft_format_plugin_xfd;
extern const uft_format_plugin_t uft_format_plugin_dcm;
extern const uft_format_plugin_t uft_format_plugin_pro;
extern const uft_format_plugin_t uft_format_plugin_st;
extern const uft_format_plugin_t uft_format_plugin_msa;
extern const uft_format_plugin_t uft_format_plugin_stx;
extern const uft_format_plugin_t uft_format_plugin_dim_atari;

/* ============================================================================
 * Gruppe: APPLE — Apple II + Macintosh (7 Plugins)
 * ============================================================================ */
extern const uft_format_plugin_t uft_format_plugin_do;
extern const uft_format_plugin_t uft_format_plugin_po;
extern const uft_format_plugin_t uft_format_plugin_woz;
extern const uft_format_plugin_t uft_format_plugin_nib;
extern const uft_format_plugin_t uft_format_plugin_2img;
extern const uft_format_plugin_t uft_format_plugin_d13;
extern const uft_format_plugin_t uft_format_plugin_dc42;

/* ============================================================================
 * Gruppe: AMIGA — Commodore Amiga (3 Plugins)
 * ============================================================================ */
extern const uft_format_plugin_t uft_format_plugin_adf;
extern const uft_format_plugin_t uft_format_plugin_dms;
extern const uft_format_plugin_t uft_format_plugin_ipf;

/* ============================================================================
 * Gruppe: IBM_PC — IBM PC kompatibel (10 Plugins)
 * ============================================================================ */
extern const uft_format_plugin_t uft_format_plugin_imd;
extern const uft_format_plugin_t uft_format_plugin_td0;
extern const uft_format_plugin_t uft_format_plugin_cqm;
extern const uft_format_plugin_t uft_format_plugin_fdi;
extern const uft_format_plugin_t uft_format_plugin_86f;
extern const uft_format_plugin_t uft_format_plugin_dsk_emu;
extern const uft_format_plugin_t uft_format_plugin_dsk_uni;
extern const uft_format_plugin_t uft_format_plugin_dsk_krg;
extern const uft_format_plugin_t uft_format_plugin_t1k;
extern const uft_format_plugin_t uft_format_plugin_dsk_dc42v;

/* ============================================================================
 * Gruppe: AMSTRAD — Amstrad CPC + PCW (3 Plugins)
 * ============================================================================ */
extern const uft_format_plugin_t uft_format_plugin_dsk_cpc;
extern const uft_format_plugin_t uft_format_plugin_edsk;
extern const uft_format_plugin_t uft_format_plugin_dsk_pcw;

/* ============================================================================
 * Gruppe: SPECTRUM — ZX Spectrum + SAM Coupe + Jupiter Ace (8 Plugins)
 * ============================================================================ */
extern const uft_format_plugin_t uft_format_plugin_trd;
extern const uft_format_plugin_t uft_format_plugin_scl;
extern const uft_format_plugin_t uft_format_plugin_udi;
extern const uft_format_plugin_t uft_format_plugin_mgt;
extern const uft_format_plugin_t uft_format_plugin_sam;
extern const uft_format_plugin_t uft_format_plugin_sad;
extern const uft_format_plugin_t uft_format_plugin_dsk_p3;
extern const uft_format_plugin_t uft_format_plugin_dsk_ace;

/* ============================================================================
 * Gruppe: ACORN — BBC Micro + Acorn (3 Plugins)
 * ============================================================================ */
extern const uft_format_plugin_t uft_format_plugin_ssd;
extern const uft_format_plugin_t uft_format_plugin_adl;
extern const uft_format_plugin_t uft_format_plugin_adf_arc;

/* ============================================================================
 * Gruppe: TRS80 — Tandy TRS-80 + CoCo (6 Plugins)
 * ============================================================================ */
extern const uft_format_plugin_t uft_format_plugin_jv1;
extern const uft_format_plugin_t uft_format_plugin_jv3;
extern const uft_format_plugin_t uft_format_plugin_jvc;
extern const uft_format_plugin_t uft_format_plugin_dmk;
extern const uft_format_plugin_t uft_format_plugin_vdk;
extern const uft_format_plugin_t uft_format_plugin_tan;

/* ============================================================================
 * Gruppe: MSX — MSX Computer (2 Plugins)
 * ============================================================================ */
extern const uft_format_plugin_t uft_format_plugin_dsk_msx;
extern const uft_format_plugin_t uft_format_plugin_msx_disk;

/* ============================================================================
 * Gruppe: CPM — CP/M Generic (5 Plugins)
 * ============================================================================ */
extern const uft_format_plugin_t uft_format_plugin_cpm;
extern const uft_format_plugin_t uft_format_plugin_rcpmfs;
extern const uft_format_plugin_t uft_format_plugin_myz80;
extern const uft_format_plugin_t uft_format_plugin_dsk_cro;
extern const uft_format_plugin_t uft_format_plugin_dsk_flex;

/* ============================================================================
 * Gruppe: JAPAN — NEC, Sharp, Fujitsu, Toshiba, Sony (13 Plugins)
 * ============================================================================ */
extern const uft_format_plugin_t uft_format_plugin_d77;
extern const uft_format_plugin_t uft_format_plugin_d88;
extern const uft_format_plugin_t uft_format_plugin_dim;
extern const uft_format_plugin_t uft_format_plugin_nfd;
extern const uft_format_plugin_t uft_format_plugin_fdi_pc98;
extern const uft_format_plugin_t uft_format_plugin_dsk_fm7;
extern const uft_format_plugin_t uft_format_plugin_dsk_nec;
extern const uft_format_plugin_t uft_format_plugin_dsk_cg;
extern const uft_format_plugin_t uft_format_plugin_dsk_xm;
extern const uft_format_plugin_t uft_format_plugin_dsk_mz;
extern const uft_format_plugin_t uft_format_plugin_dsk_sc3;
extern const uft_format_plugin_t uft_format_plugin_dsk_smc;
extern const uft_format_plugin_t uft_format_plugin_dsk_tok;

/* ============================================================================
 * Gruppe: FLUX — Universelle Flux-Container (4 Plugins)
 * ============================================================================ */
extern const uft_format_plugin_t uft_format_plugin_hfe;
extern const uft_format_plugin_t uft_format_plugin_kfx;
extern const uft_format_plugin_t uft_format_plugin_mfi;
extern const uft_format_plugin_t uft_format_plugin_pri;

/* ============================================================================
 * Gruppe: TAPE — Tape-Formate (1 Plugin)
 * ============================================================================ */
extern const uft_format_plugin_t uft_format_plugin_cas;

/* ============================================================================
 * Gruppe: OTHER — Nischen (48 Plugins)
 * ============================================================================ */
extern const uft_format_plugin_t uft_format_plugin_apridisk;
extern const uft_format_plugin_t uft_format_plugin_cfi;
extern const uft_format_plugin_t uft_format_plugin_dsk_agt;
extern const uft_format_plugin_t uft_format_plugin_dsk_ak;
extern const uft_format_plugin_t uft_format_plugin_dsk_aln;
extern const uft_format_plugin_t uft_format_plugin_dsk_aq;
extern const uft_format_plugin_t uft_format_plugin_dsk_bk;
extern const uft_format_plugin_t uft_format_plugin_dsk_bw;
extern const uft_format_plugin_t uft_format_plugin_dsk_ein;
extern const uft_format_plugin_t uft_format_plugin_dsk_eqx;
extern const uft_format_plugin_t uft_format_plugin_dsk_fp;
extern const uft_format_plugin_t uft_format_plugin_dsk_hk;
extern const uft_format_plugin_t uft_format_plugin_dsk_hp;
extern const uft_format_plugin_t uft_format_plugin_dsk_kc;
extern const uft_format_plugin_t uft_format_plugin_dsk_lyn;
extern const uft_format_plugin_t uft_format_plugin_dsk_m5;
extern const uft_format_plugin_t uft_format_plugin_dsk_mtx;
extern const uft_format_plugin_t uft_format_plugin_dsk_nas;
extern const uft_format_plugin_t uft_format_plugin_dsk_nb;
extern const uft_format_plugin_t uft_format_plugin_dsk_ns;
extern const uft_format_plugin_t uft_format_plugin_dsk_oli;
extern const uft_format_plugin_t uft_format_plugin_dsk_orc;
extern const uft_format_plugin_t uft_format_plugin_dsk_os9;
extern const uft_format_plugin_t uft_format_plugin_dsk_px;
extern const uft_format_plugin_t uft_format_plugin_dsk_rc;
extern const uft_format_plugin_t uft_format_plugin_dsk_rld;
extern const uft_format_plugin_t uft_format_plugin_dsk_san;
extern const uft_format_plugin_t uft_format_plugin_dsk_sv;
extern const uft_format_plugin_t uft_format_plugin_dsk_vec;
extern const uft_format_plugin_t uft_format_plugin_dsk_vt;
extern const uft_format_plugin_t uft_format_plugin_dsk_wng;
extern const uft_format_plugin_t uft_format_plugin_dsk_x820;
extern const uft_format_plugin_t uft_format_plugin_edk;
extern const uft_format_plugin_t uft_format_plugin_fds;
extern const uft_format_plugin_t uft_format_plugin_hardsector;
extern const uft_format_plugin_t uft_format_plugin_logical;
extern const uft_format_plugin_t uft_format_plugin_micropolis;
extern const uft_format_plugin_t uft_format_plugin_nanowasp;
extern const uft_format_plugin_t uft_format_plugin_northstar;
extern const uft_format_plugin_t uft_format_plugin_opus;
extern const uft_format_plugin_t uft_format_plugin_pdp;
extern const uft_format_plugin_t uft_format_plugin_posix;
extern const uft_format_plugin_t uft_format_plugin_qrst;
extern const uft_format_plugin_t uft_format_plugin_sap_thomson;
extern const uft_format_plugin_t uft_format_plugin_syn;
extern const uft_format_plugin_t uft_format_plugin_v9t9;
extern const uft_format_plugin_t uft_format_plugin_victor9k;
extern const uft_format_plugin_t uft_format_plugin_xdm86;

/* ============================================================================
 * Per-Group Plugin Arrays
 * ============================================================================ */

static const uft_format_plugin_t* g_cbm_plugins[] = {
    &uft_format_plugin_d64, &uft_format_plugin_d67, &uft_format_plugin_d71,
    &uft_format_plugin_d80, &uft_format_plugin_d81, &uft_format_plugin_d82,
    &uft_format_plugin_g71, &uft_format_plugin_dsk_vic,
};

static const uft_format_plugin_t* g_atari_plugins[] = {
    &uft_format_plugin_atr, &uft_format_plugin_atx, &uft_format_plugin_xfd,
    &uft_format_plugin_dcm, &uft_format_plugin_pro, &uft_format_plugin_st,
    &uft_format_plugin_msa, &uft_format_plugin_stx, &uft_format_plugin_dim_atari,
};

static const uft_format_plugin_t* g_apple_plugins[] = {
    &uft_format_plugin_do, &uft_format_plugin_po, &uft_format_plugin_woz,
    &uft_format_plugin_nib, &uft_format_plugin_2img, &uft_format_plugin_d13,
    &uft_format_plugin_dc42,
};

static const uft_format_plugin_t* g_amiga_plugins[] = {
    &uft_format_plugin_adf, &uft_format_plugin_dms, &uft_format_plugin_ipf,
};

static const uft_format_plugin_t* g_ibm_pc_plugins[] = {
    &uft_format_plugin_imd, &uft_format_plugin_td0, &uft_format_plugin_cqm,
    &uft_format_plugin_fdi, &uft_format_plugin_86f, &uft_format_plugin_dsk_emu,
    &uft_format_plugin_dsk_uni, &uft_format_plugin_dsk_krg,
    &uft_format_plugin_t1k, &uft_format_plugin_dsk_dc42v,
};

static const uft_format_plugin_t* g_amstrad_plugins[] = {
    &uft_format_plugin_dsk_cpc, &uft_format_plugin_edsk, &uft_format_plugin_dsk_pcw,
};

static const uft_format_plugin_t* g_spectrum_plugins[] = {
    &uft_format_plugin_trd, &uft_format_plugin_scl, &uft_format_plugin_udi,
    &uft_format_plugin_mgt, &uft_format_plugin_sam, &uft_format_plugin_sad,
    &uft_format_plugin_dsk_p3, &uft_format_plugin_dsk_ace,
};

static const uft_format_plugin_t* g_acorn_plugins[] = {
    &uft_format_plugin_ssd, &uft_format_plugin_adl, &uft_format_plugin_adf_arc,
};

static const uft_format_plugin_t* g_trs80_plugins[] = {
    &uft_format_plugin_jv1, &uft_format_plugin_jv3, &uft_format_plugin_jvc,
    &uft_format_plugin_dmk, &uft_format_plugin_vdk, &uft_format_plugin_tan,
};

static const uft_format_plugin_t* g_msx_plugins[] = {
    &uft_format_plugin_dsk_msx, &uft_format_plugin_msx_disk,
};

static const uft_format_plugin_t* g_cpm_plugins[] = {
    &uft_format_plugin_cpm, &uft_format_plugin_rcpmfs, &uft_format_plugin_myz80,
    &uft_format_plugin_dsk_cro, &uft_format_plugin_dsk_flex,
};

static const uft_format_plugin_t* g_japan_plugins[] = {
    &uft_format_plugin_d77, &uft_format_plugin_d88, &uft_format_plugin_dim,
    &uft_format_plugin_nfd, &uft_format_plugin_fdi_pc98,
    &uft_format_plugin_dsk_fm7, &uft_format_plugin_dsk_nec, &uft_format_plugin_dsk_cg,
    &uft_format_plugin_dsk_xm, &uft_format_plugin_dsk_mz, &uft_format_plugin_dsk_sc3,
    &uft_format_plugin_dsk_smc, &uft_format_plugin_dsk_tok,
};

static const uft_format_plugin_t* g_flux_plugins[] = {
    &uft_format_plugin_hfe, &uft_format_plugin_kfx,
    &uft_format_plugin_mfi, &uft_format_plugin_pri,
};

static const uft_format_plugin_t* g_tape_plugins[] = {
    &uft_format_plugin_cas,
};

static const uft_format_plugin_t* g_other_plugins[] = {
    &uft_format_plugin_apridisk, &uft_format_plugin_cfi,
    &uft_format_plugin_dsk_agt, &uft_format_plugin_dsk_ak, &uft_format_plugin_dsk_aln,
    &uft_format_plugin_dsk_aq, &uft_format_plugin_dsk_bk, &uft_format_plugin_dsk_bw,
    &uft_format_plugin_dsk_ein, &uft_format_plugin_dsk_eqx, &uft_format_plugin_dsk_fp,
    &uft_format_plugin_dsk_hk, &uft_format_plugin_dsk_hp, &uft_format_plugin_dsk_kc,
    &uft_format_plugin_dsk_lyn, &uft_format_plugin_dsk_m5, &uft_format_plugin_dsk_mtx,
    &uft_format_plugin_dsk_nas, &uft_format_plugin_dsk_nb, &uft_format_plugin_dsk_ns,
    &uft_format_plugin_dsk_oli, &uft_format_plugin_dsk_orc, &uft_format_plugin_dsk_os9,
    &uft_format_plugin_dsk_px, &uft_format_plugin_dsk_rc, &uft_format_plugin_dsk_rld,
    &uft_format_plugin_dsk_san, &uft_format_plugin_dsk_sv, &uft_format_plugin_dsk_vec,
    &uft_format_plugin_dsk_vt, &uft_format_plugin_dsk_wng, &uft_format_plugin_dsk_x820,
    &uft_format_plugin_edk, &uft_format_plugin_fds,
    &uft_format_plugin_hardsector, &uft_format_plugin_logical,
    &uft_format_plugin_micropolis, &uft_format_plugin_nanowasp,
    &uft_format_plugin_northstar, &uft_format_plugin_opus,
    &uft_format_plugin_pdp, &uft_format_plugin_posix, &uft_format_plugin_qrst,
    &uft_format_plugin_sap_thomson, &uft_format_plugin_syn,
    &uft_format_plugin_v9t9, &uft_format_plugin_victor9k, &uft_format_plugin_xdm86,
};

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

/* ============================================================================
 * Group Metadata Table
 * ============================================================================ */

static const uft_format_group_t g_groups[] = {
    { "CBM",      "Commodore 8-bit",
      g_cbm_plugins,      ARRAY_COUNT(g_cbm_plugins) },
    { "ATARI",    "Atari 8-bit + ST",
      g_atari_plugins,    ARRAY_COUNT(g_atari_plugins) },
    { "APPLE",    "Apple II + Macintosh",
      g_apple_plugins,    ARRAY_COUNT(g_apple_plugins) },
    { "AMIGA",    "Commodore Amiga",
      g_amiga_plugins,    ARRAY_COUNT(g_amiga_plugins) },
    { "IBM_PC",   "IBM PC kompatibel + Generic",
      g_ibm_pc_plugins,   ARRAY_COUNT(g_ibm_pc_plugins) },
    { "AMSTRAD",  "Amstrad CPC + PCW",
      g_amstrad_plugins,  ARRAY_COUNT(g_amstrad_plugins) },
    { "SPECTRUM", "ZX Spectrum + SAM Coupe + Jupiter Ace",
      g_spectrum_plugins, ARRAY_COUNT(g_spectrum_plugins) },
    { "ACORN",    "BBC Micro + Acorn Archimedes",
      g_acorn_plugins,    ARRAY_COUNT(g_acorn_plugins) },
    { "TRS80",    "Tandy TRS-80 + CoCo",
      g_trs80_plugins,    ARRAY_COUNT(g_trs80_plugins) },
    { "MSX",      "MSX Computer",
      g_msx_plugins,      ARRAY_COUNT(g_msx_plugins) },
    { "CPM",      "CP/M Generic",
      g_cpm_plugins,      ARRAY_COUNT(g_cpm_plugins) },
    { "JAPAN",    "NEC / Sharp / Fujitsu / Toshiba / Sony",
      g_japan_plugins,    ARRAY_COUNT(g_japan_plugins) },
    { "FLUX",     "Universelle Flux/Bitstream-Container",
      g_flux_plugins,     ARRAY_COUNT(g_flux_plugins) },
    { "TAPE",     "Tape-Formate",
      g_tape_plugins,     ARRAY_COUNT(g_tape_plugins) },
    { "OTHER",    "Nischen + nicht-klassifizierte DSK-Varianten",
      g_other_plugins,    ARRAY_COUNT(g_other_plugins) },
};

#define NUM_GROUPS (sizeof(g_groups) / sizeof(g_groups[0]))

/* ============================================================================
 * Globale Plugin-Tabelle (flach, alle Plugins aus allen Gruppen)
 * ============================================================================ */

static const uft_format_plugin_t* all_plugins[
    /* CBM */      8
    /* ATARI */  + 9
    /* APPLE */  + 7
    /* AMIGA */  + 3
    /* IBM_PC */ + 10
    /* AMSTRAD */+ 3
    /* SPECT. */ + 8
    /* ACORN */  + 3
    /* TRS80 */  + 6
    /* MSX */    + 2
    /* CPM */    + 5
    /* JAPAN */  + 13
    /* FLUX */   + 4
    /* TAPE */   + 1
    /* OTHER */  + 47
    /* NULL */   + 1
] = { NULL };

/* Initialisierung der flachen Liste einmalig */
static bool g_all_plugins_initialized = false;

static void init_all_plugins(void) {
    if (g_all_plugins_initialized) return;
    size_t idx = 0;
    for (size_t g = 0; g < NUM_GROUPS; g++) {
        for (size_t i = 0; i < g_groups[g].plugin_count; i++) {
            all_plugins[idx++] = g_groups[g].plugins[i];
        }
    }
    all_plugins[idx] = NULL;
    g_all_plugins_initialized = true;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

uft_error_t uft_register_all_formats(void) {
    init_all_plugins();
    uft_error_t err;
    for (int i = 0; all_plugins[i] != NULL; i++) {
        err = uft_register_format_plugin(all_plugins[i]);
        if (err != UFT_OK) return err;
    }
    return UFT_OK;
}

size_t uft_get_format_count(void) {
    init_all_plugins();
    size_t count = 0;
    while (all_plugins[count] != NULL) count++;
    return count;
}

const uft_format_plugin_t* uft_get_format_by_index(size_t index) {
    init_all_plugins();
    if (index >= uft_get_format_count()) return NULL;
    return all_plugins[index];
}

const uft_format_group_t* uft_format_get_groups(size_t *count) {
    if (count) *count = NUM_GROUPS;
    return g_groups;
}

const uft_format_group_t* uft_format_find_group(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < NUM_GROUPS; i++) {
        if (strcmp(g_groups[i].name, name) == 0)
            return &g_groups[i];
    }
    return NULL;
}
