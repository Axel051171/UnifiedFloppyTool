/**
 * @file uft_decoder_plugin.c
 * @brief UnifiedFloppyTool - Decoder Plugin Registry
 * 
 * Verwaltet Encoding/Decoding Plugins (MFM, GCR, etc.)
 * 
 * @author UFT Team
 * @date 2025
 */

#include "uft/uft_decoder_plugin.h"
#include "uft/uft_compiler.h"
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Plugin Registry
// ============================================================================

#define MAX_DECODER_PLUGINS 16

static const uft_decoder_plugin_t* g_decoder_plugins[MAX_DECODER_PLUGINS] = {0};
static size_t g_decoder_plugin_count = 0;

// ============================================================================
// Registration
// ============================================================================

uft_error_t uft_register_decoder_plugin(const uft_decoder_plugin_t* plugin) {
    if (!plugin) return UFT_ERROR_NULL_POINTER;
    if (!plugin->name) return UFT_ERROR_INVALID_ARG;
    
    // Duplikate prüfen
    for (size_t i = 0; i < g_decoder_plugin_count; i++) {
        if (g_decoder_plugins[i]->encoding == plugin->encoding) {
            return UFT_ERROR_PLUGIN_LOAD;
        }
    }
    
    if (g_decoder_plugin_count >= MAX_DECODER_PLUGINS) {
        return UFT_ERROR_BUFFER_TOO_SMALL;
    }
    
    // Plugin initialisieren
    if (plugin->init) {
        uft_error_t err = plugin->init();
        if (UFT_FAILED(err)) {
            return err;
        }
    }
    
    g_decoder_plugins[g_decoder_plugin_count++] = plugin;
    
    return UFT_OK;
}

uft_error_t uft_unregister_decoder_plugin(uft_encoding_t encoding) {
    for (size_t i = 0; i < g_decoder_plugin_count; i++) {
        if (g_decoder_plugins[i]->encoding == encoding) {
            if (g_decoder_plugins[i]->shutdown) {
                g_decoder_plugins[i]->shutdown();
            }
            
            for (size_t j = i; j + 1 < g_decoder_plugin_count; j++) {
                g_decoder_plugins[j] = g_decoder_plugins[j + 1];
            }
            g_decoder_plugin_count--;
            g_decoder_plugins[g_decoder_plugin_count] = NULL;
            
            return UFT_OK;
        }
    }
    
    return UFT_ERROR_PLUGIN_NOT_FOUND;
}

// ============================================================================
// Lookup
// ============================================================================

const uft_decoder_plugin_t* uft_get_decoder_plugin(uft_encoding_t encoding) {
    for (size_t i = 0; i < g_decoder_plugin_count; i++) {
        if (g_decoder_plugins[i]->encoding == encoding) {
            return g_decoder_plugins[i];
        }
    }
    return NULL;
}

const uft_decoder_plugin_t* uft_find_decoder_plugin_for_flux(
    const uint32_t* flux, size_t count) 
{
    if (!flux || count == 0) return NULL;
    
    const uft_decoder_plugin_t* best_plugin = NULL;
    int best_confidence = 0;
    
    for (size_t i = 0; i < g_decoder_plugin_count; i++) {
        if (!g_decoder_plugins[i]->detect) continue;
        
        int confidence = 0;
        bool matches = g_decoder_plugins[i]->detect(flux, count, &confidence);
        
        if (matches && confidence > best_confidence) {
            best_plugin = g_decoder_plugins[i];
            best_confidence = confidence;
        }
    }
    
    return best_plugin;
}

size_t uft_list_decoder_plugins(const uft_decoder_plugin_t** plugins, size_t max) {
    if (!plugins || max == 0) return 0;
    
    size_t count = 0;
    for (size_t i = 0; i < g_decoder_plugin_count && count < max; i++) {
        plugins[count++] = g_decoder_plugins[i];
    }
    
    return count;
}

// ============================================================================
// High-Level Decode/Encode
// ============================================================================

uft_error_t uft_decode_flux(const uint32_t* flux, size_t count,
                             uft_sector_t* sectors, size_t max_sectors,
                             size_t* sector_count, uft_decode_stats_t* stats) {
    if (!flux || !sectors || !sector_count) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    // Plugin auto-detecten
    const uft_decoder_plugin_t* plugin = uft_find_decoder_plugin_for_flux(flux, count);
    if (!plugin) {
        return UFT_ERROR_UNKNOWN_ENCODING;
    }
    
    uft_decode_options_t opts = uft_default_decode_options();
    
    return plugin->decode(flux, count, &opts, sectors, max_sectors, 
                          sector_count, stats);
}

uft_error_t uft_encode_sectors(const uft_sector_t* sectors, size_t sector_count,
                                uft_encoding_t encoding, int cylinder, int head,
                                uint32_t** flux, size_t* flux_count) {
    if (!sectors || !flux || !flux_count) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    const uft_decoder_plugin_t* plugin = uft_get_decoder_plugin(encoding);
    if (!plugin) {
        return UFT_ERROR_UNKNOWN_ENCODING;
    }
    
    if (!plugin->encode) {
        return UFT_ERROR_NOT_SUPPORTED;
    }
    
    uft_encode_options_t opts = uft_default_encode_options();
    
    return plugin->encode(sectors, sector_count, cylinder, head, &opts,
                          flux, flux_count);
}

// ============================================================================
// PLL Utilities
// ============================================================================

void uft_pll_init(uft_pll_t* pll, double nominal_period_ns, double adjust_pct) {
    if (!pll) return;
    
    pll->nominal_period = nominal_period_ns;
    pll->current_period = nominal_period_ns;
    pll->adjust_rate = adjust_pct / 100.0;
    pll->phase = 0.0;
    pll->lock_count = 0;
    pll->slip_count = 0;
}

bool uft_pll_process(uft_pll_t* pll, uint32_t delta, uint8_t* bits, int* bit_count) {
    if (!pll || !bits || !bit_count) return false;
    
    *bit_count = 0;
    bool locked = true;
    
    // Anzahl Bits berechnen (wie viele Perioden passen rein)
    double periods = (double)delta / pll->current_period;
    int num_bits = (int)(periods + 0.5);  // Runden
    
    if (num_bits < 1) num_bits = 1;
    if (num_bits > 4) {
        num_bits = 4;  // Max 4 Nullen in Folge (typisch für MFM)
        locked = false;
        pll->slip_count++;
    }
    
    // Bits ausgeben
    for (int i = 0; i < num_bits - 1; i++) {
        bits[(*bit_count)++] = 0;
    }
    bits[(*bit_count)++] = 1;  // Flux-Transition = 1
    
    // PLL anpassen
    double error = delta - (num_bits * pll->current_period);
    pll->current_period += error * pll->adjust_rate / num_bits;
    
    // Grenzen einhalten (±20% vom Nominal)
    double min_period = pll->nominal_period * 0.8;
    double max_period = pll->nominal_period * 1.2;
    if (pll->current_period < min_period) pll->current_period = min_period;
    if (pll->current_period > max_period) pll->current_period = max_period;
    
    if (locked) pll->lock_count++;
    
    return locked;
}

void uft_pll_reset(uft_pll_t* pll) {
    if (!pll) return;
    pll->current_period = pll->nominal_period;
    pll->phase = 0.0;
}

// ============================================================================
// CRC Utilities
// ============================================================================

/**
 * @brief CRC-16 CCITT Lookup Table
 */
static const uint16_t crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
    0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
    0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
    0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
    0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
    0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
    0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
    0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
    0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
    0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
    0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
    0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
    0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
    0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
    0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
    0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
    0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
    0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
    0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
    0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
    0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
    0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
    0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

uint16_t uft_crc16_ccitt(const uint8_t* data, size_t len, uint16_t init) {
    uint16_t crc = init;
    
    while (len--) {
        crc = (crc << 8) ^ crc16_table[((crc >> 8) ^ *data++) & 0xFF];
    }
    
    return crc;
}

uint16_t uft_crc16_mfm_idam(const uint8_t* id, size_t len) {
    // MFM IDAM startet mit drei A1 Sync-Bytes
    uint16_t crc = 0xFFFF;
    
    // Drei $A1 Bytes
    crc = uft_crc16_ccitt((const uint8_t*)"\xA1\xA1\xA1", 3, crc);
    
    // ID-Feld
    crc = uft_crc16_ccitt(id, len, crc);
    
    return crc;
}

uint8_t uft_checksum_gcr_cbm(const uint8_t* data, size_t len) {
    uint8_t checksum = 0;
    while (len--) {
        checksum ^= *data++;
    }
    return checksum;
}

uint32_t uft_checksum_amiga(const uint32_t* data, size_t count) {
    uint32_t checksum = 0;
    while (count--) {
        checksum ^= *data++;
    }
    return checksum;
}

// ============================================================================
// Built-in Decoder Registration
// ============================================================================

// Forward declarations (MFM ist immer verfügbar)
extern const uft_decoder_plugin_t uft_decoder_plugin_mfm;

// GCR Decoder (jetzt auch verfügbar)
extern const uft_decoder_plugin_t uft_decoder_plugin_gcr;

// Optional (wenn kompiliert)
extern const uft_decoder_plugin_t uft_decoder_plugin_fm UFT_WEAK;
extern const uft_decoder_plugin_t uft_decoder_plugin_amiga_mfm UFT_WEAK;

uft_error_t uft_register_builtin_decoder_plugins(void) {
    uft_error_t err;
    
    // MFM Decoder (immer verfügbar)
    err = uft_register_decoder_plugin(&uft_decoder_plugin_mfm);
    if (UFT_FAILED(err) && err != UFT_ERROR_PLUGIN_LOAD) {
        return err;
    }
    
    // GCR Decoder (Commodore 64)
    err = uft_register_decoder_plugin(&uft_decoder_plugin_gcr);
    if (UFT_FAILED(err) && err != UFT_ERROR_PLUGIN_LOAD) {
        // Nicht kritisch
    }
    
    // Optionale Decoder (nur wenn kompiliert)
    if (&uft_decoder_plugin_fm) {
        err = uft_register_decoder_plugin(&uft_decoder_plugin_fm);
        if (UFT_FAILED(err) && err != UFT_ERROR_PLUGIN_LOAD) {
            // Nicht kritisch
        }
    }
    
    if (&uft_decoder_plugin_amiga_mfm) {
        err = uft_register_decoder_plugin(&uft_decoder_plugin_amiga_mfm);
        if (UFT_FAILED(err) && err != UFT_ERROR_PLUGIN_LOAD) {
            // Nicht kritisch
        }
    }
    
    return UFT_OK;
}
