/**
 * @file uft_format_registry.c
 * @brief Zentrale Registrierung aller Format-Plugins (72 Plugins)
 */

#include "uft/uft_format_plugin.h"

/* ============================================================================
 * Tier 1: Core Floppy Formats (38 plugins, previously registered)
 * ============================================================================ */

/* Commodore */
extern const uft_format_plugin_t uft_format_plugin_d64;
extern const uft_format_plugin_t uft_format_plugin_d71;
extern const uft_format_plugin_t uft_format_plugin_d80;
extern const uft_format_plugin_t uft_format_plugin_d81;
extern const uft_format_plugin_t uft_format_plugin_d82;
extern const uft_format_plugin_t uft_format_plugin_g71;

/* Atari */
extern const uft_format_plugin_t uft_format_plugin_atr;
extern const uft_format_plugin_t uft_format_plugin_atx;
extern const uft_format_plugin_t uft_format_plugin_stx;
extern const uft_format_plugin_t uft_format_plugin_pro;
extern const uft_format_plugin_t uft_format_plugin_msa;
extern const uft_format_plugin_t uft_format_plugin_dim_atari;

/* Apple */
extern const uft_format_plugin_t uft_format_plugin_do;
extern const uft_format_plugin_t uft_format_plugin_po;
extern const uft_format_plugin_t uft_format_plugin_nib;
extern const uft_format_plugin_t uft_format_plugin_woz;
extern const uft_format_plugin_t uft_format_plugin_d13;

/* IBM PC / Generic */
extern const uft_format_plugin_t uft_format_plugin_td0;
extern const uft_format_plugin_t uft_format_plugin_imd;
extern const uft_format_plugin_t uft_format_plugin_cqm;
extern const uft_format_plugin_t uft_format_plugin_dmk;
extern const uft_format_plugin_t uft_format_plugin_86f;

/* Amstrad / Spectrum */
extern const uft_format_plugin_t uft_format_plugin_dsk_cpc;
extern const uft_format_plugin_t uft_format_plugin_trd;
extern const uft_format_plugin_t uft_format_plugin_scl;

/* BBC / Acorn */
extern const uft_format_plugin_t uft_format_plugin_ssd;
extern const uft_format_plugin_t uft_format_plugin_adl;
extern const uft_format_plugin_t uft_format_plugin_adf;

/* Japanese */
extern const uft_format_plugin_t uft_format_plugin_d88;
extern const uft_format_plugin_t uft_format_plugin_dim;
extern const uft_format_plugin_t uft_format_plugin_fdi_pc98;

/* Flux / Bitstream */
extern const uft_format_plugin_t uft_format_plugin_hfe;
extern const uft_format_plugin_t uft_format_plugin_ipf;
extern const uft_format_plugin_t uft_format_plugin_fdi;

/* Other */
extern const uft_format_plugin_t uft_format_plugin_sad;
extern const uft_format_plugin_t uft_format_plugin_v9t9;
extern const uft_format_plugin_t uft_format_plugin_vdk;
extern const uft_format_plugin_t uft_format_plugin_sap_thomson;

/* ============================================================================
 * Tier 2: Newly registered real Plugin-B implementations (34 plugins)
 * ============================================================================ */

/* Atari additional */
extern const uft_format_plugin_t uft_format_plugin_st;
extern const uft_format_plugin_t uft_format_plugin_dcm;
extern const uft_format_plugin_t uft_format_plugin_xfd;

/* Apple additional */
extern const uft_format_plugin_t uft_format_plugin_2img;
extern const uft_format_plugin_t uft_format_plugin_dc42;

/* Amstrad / Spectrum additional */
extern const uft_format_plugin_t uft_format_plugin_edsk;
extern const uft_format_plugin_t uft_format_plugin_mgt;

/* Japanese additional */
extern const uft_format_plugin_t uft_format_plugin_d77;

/* TRS-80 */
extern const uft_format_plugin_t uft_format_plugin_jv1;
extern const uft_format_plugin_t uft_format_plugin_jv3;

/* Nintendo */
extern const uft_format_plugin_t uft_format_plugin_fds;

/* MSX / Tape */
extern const uft_format_plugin_t uft_format_plugin_cas;

/* Flux additional */
extern const uft_format_plugin_t uft_format_plugin_kfx;
extern const uft_format_plugin_t uft_format_plugin_pri;
extern const uft_format_plugin_t uft_format_plugin_mfi;

/* Acorn additional */
extern const uft_format_plugin_t uft_format_plugin_adf_arc;

/* SAM Coupe */
extern const uft_format_plugin_t uft_format_plugin_sam;

/* Ensoniq / Synth */
extern const uft_format_plugin_t uft_format_plugin_edk;
extern const uft_format_plugin_t uft_format_plugin_syn;

/* TI-99 additional */
extern const uft_format_plugin_t uft_format_plugin_xdm86;

/* Tandy */
extern const uft_format_plugin_t uft_format_plugin_t1k;
extern const uft_format_plugin_t uft_format_plugin_tan;

/* DEC */
extern const uft_format_plugin_t uft_format_plugin_pdp;

/* CP/M */
extern const uft_format_plugin_t uft_format_plugin_cpm;
extern const uft_format_plugin_t uft_format_plugin_rcpmfs;
extern const uft_format_plugin_t uft_format_plugin_myz80;

/* Exotic / Niche */
extern const uft_format_plugin_t uft_format_plugin_apridisk;
extern const uft_format_plugin_t uft_format_plugin_cfi;
extern const uft_format_plugin_t uft_format_plugin_hardsector;
extern const uft_format_plugin_t uft_format_plugin_logical;
extern const uft_format_plugin_t uft_format_plugin_nanowasp;
extern const uft_format_plugin_t uft_format_plugin_opus;
extern const uft_format_plugin_t uft_format_plugin_posix;
extern const uft_format_plugin_t uft_format_plugin_qrst;

/* ============================================================================
 * Tier 3: New implementations (9 plugins)
 * ============================================================================ */

/* TRS-80 additional */
extern const uft_format_plugin_t uft_format_plugin_jvc;

/* Amiga additional */
extern const uft_format_plugin_t uft_format_plugin_dms;

/* Commodore additional */
extern const uft_format_plugin_t uft_format_plugin_d67;

/* MSX */
extern const uft_format_plugin_t uft_format_plugin_msx_disk;

/* Spectrum additional */
extern const uft_format_plugin_t uft_format_plugin_udi;

/* Japanese additional */
extern const uft_format_plugin_t uft_format_plugin_nfd;

/* Exotic hardware */
extern const uft_format_plugin_t uft_format_plugin_victor9k;
extern const uft_format_plugin_t uft_format_plugin_micropolis;
extern const uft_format_plugin_t uft_format_plugin_northstar;

/* ============================================================================
 * Plugin Array (81 entries)
 * ============================================================================ */

static const uft_format_plugin_t* all_plugins[] = {
    /* Tier 1: Core (38) */
    &uft_format_plugin_d64,
    &uft_format_plugin_d71,
    &uft_format_plugin_d80,
    &uft_format_plugin_d81,
    &uft_format_plugin_d82,
    &uft_format_plugin_g71,
    &uft_format_plugin_atr,
    &uft_format_plugin_atx,
    &uft_format_plugin_stx,
    &uft_format_plugin_pro,
    &uft_format_plugin_msa,
    &uft_format_plugin_dim_atari,
    &uft_format_plugin_do,
    &uft_format_plugin_po,
    &uft_format_plugin_nib,
    &uft_format_plugin_woz,
    &uft_format_plugin_d13,
    &uft_format_plugin_td0,
    &uft_format_plugin_imd,
    &uft_format_plugin_cqm,
    &uft_format_plugin_dmk,
    &uft_format_plugin_86f,
    &uft_format_plugin_dsk_cpc,
    &uft_format_plugin_trd,
    &uft_format_plugin_scl,
    &uft_format_plugin_ssd,
    &uft_format_plugin_adl,
    &uft_format_plugin_adf,
    &uft_format_plugin_d88,
    &uft_format_plugin_dim,
    &uft_format_plugin_fdi_pc98,
    &uft_format_plugin_hfe,
    &uft_format_plugin_ipf,
    &uft_format_plugin_fdi,
    &uft_format_plugin_sad,
    &uft_format_plugin_v9t9,
    &uft_format_plugin_vdk,
    &uft_format_plugin_sap_thomson,

    /* Tier 2: Newly registered (34) */
    &uft_format_plugin_st,
    &uft_format_plugin_dcm,
    &uft_format_plugin_xfd,
    &uft_format_plugin_2img,
    &uft_format_plugin_dc42,
    &uft_format_plugin_edsk,
    &uft_format_plugin_mgt,
    &uft_format_plugin_d77,
    &uft_format_plugin_jv1,
    &uft_format_plugin_jv3,
    &uft_format_plugin_fds,
    &uft_format_plugin_cas,
    &uft_format_plugin_kfx,
    &uft_format_plugin_pri,
    &uft_format_plugin_mfi,
    &uft_format_plugin_adf_arc,
    &uft_format_plugin_sam,
    &uft_format_plugin_edk,
    &uft_format_plugin_syn,
    &uft_format_plugin_xdm86,
    &uft_format_plugin_t1k,
    &uft_format_plugin_tan,
    &uft_format_plugin_pdp,
    &uft_format_plugin_cpm,
    &uft_format_plugin_rcpmfs,
    &uft_format_plugin_myz80,
    &uft_format_plugin_apridisk,
    &uft_format_plugin_cfi,
    &uft_format_plugin_hardsector,
    &uft_format_plugin_logical,
    &uft_format_plugin_nanowasp,
    &uft_format_plugin_opus,
    &uft_format_plugin_posix,
    &uft_format_plugin_qrst,

    /* Tier 3: New implementations (9) */
    &uft_format_plugin_jvc,
    &uft_format_plugin_dms,
    &uft_format_plugin_d67,
    &uft_format_plugin_msx_disk,
    &uft_format_plugin_udi,
    &uft_format_plugin_nfd,
    &uft_format_plugin_victor9k,
    &uft_format_plugin_micropolis,
    &uft_format_plugin_northstar,
    NULL
};

uft_error_t uft_register_all_formats(void) {
    uft_error_t err;
    for (int i = 0; all_plugins[i] != NULL; i++) {
        err = uft_register_format_plugin(all_plugins[i]);
        if (err != UFT_OK) return err;
    }
    return UFT_OK;
}

size_t uft_get_format_count(void) {
    size_t count = 0;
    while (all_plugins[count] != NULL) count++;
    return count;
}

const uft_format_plugin_t* uft_get_format_by_index(size_t index) {
    if (index >= uft_get_format_count()) return NULL;
    return all_plugins[index];
}
