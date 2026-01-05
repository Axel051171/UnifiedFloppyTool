/**
 * @file uft_format_registry.c
 * @brief Zentrale Registrierung aller Format-Plugins
 */

#include "uft/uft_format_plugin.h"

// Externe Plugin-Deklarationen
extern const uft_format_plugin_t uft_format_plugin_d71;
extern const uft_format_plugin_t uft_format_plugin_d80;
extern const uft_format_plugin_t uft_format_plugin_d81;
extern const uft_format_plugin_t uft_format_plugin_d82;
extern const uft_format_plugin_t uft_format_plugin_g71;
extern const uft_format_plugin_t uft_format_plugin_atr;
extern const uft_format_plugin_t uft_format_plugin_msa;
extern const uft_format_plugin_t uft_format_plugin_ipf;
extern const uft_format_plugin_t uft_format_plugin_td0;
extern const uft_format_plugin_t uft_format_plugin_imd;
extern const uft_format_plugin_t uft_format_plugin_dsk_cpc;
extern const uft_format_plugin_t uft_format_plugin_nib;
extern const uft_format_plugin_t uft_format_plugin_woz;
extern const uft_format_plugin_t uft_format_plugin_trd;
extern const uft_format_plugin_t uft_format_plugin_ssd;
extern const uft_format_plugin_t uft_format_plugin_d88;
extern const uft_format_plugin_t uft_format_plugin_sad;
extern const uft_format_plugin_t uft_format_plugin_dmk;
extern const uft_format_plugin_t uft_format_plugin_fdi;
extern const uft_format_plugin_t uft_format_plugin_cqm;

static const uft_format_plugin_t* all_plugins[] = {
    &uft_format_plugin_d71,
    &uft_format_plugin_d80,
    &uft_format_plugin_d81,
    &uft_format_plugin_d82,
    &uft_format_plugin_g71,
    &uft_format_plugin_atr,
    &uft_format_plugin_msa,
    &uft_format_plugin_ipf,
    &uft_format_plugin_td0,
    &uft_format_plugin_imd,
    &uft_format_plugin_dsk_cpc,
    &uft_format_plugin_nib,
    &uft_format_plugin_woz,
    &uft_format_plugin_trd,
    &uft_format_plugin_ssd,
    &uft_format_plugin_d88,
    &uft_format_plugin_sad,
    &uft_format_plugin_dmk,
    &uft_format_plugin_fdi,
    &uft_format_plugin_cqm,
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
