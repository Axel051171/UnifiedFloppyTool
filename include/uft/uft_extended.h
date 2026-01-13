/**
 * @file uft_extended.h
 * @brief UFT Extended API - All Advanced Features
 * @version 3.8.0
 * 
 * "Bei uns geht kein Bit verloren" - UFT Preservation Philosophy
 * 
 * This is the master header for UFT's extended functionality.
 * 
 * QUICK START - Advanced Mode:
 * 
 *   #include "uft/uft_extended.h"
 *   
 *   // Enable advanced mode
 *   uft_advanced_init();
 *   
 *   // Open with automatic v3 parser, protection detection, and God-Mode
 *   uft_advanced_handle_t* h;
 *   if (uft_advanced_open("disk.d64", &h) == UFT_OK) {
 *       printf("Format: %d, Confidence: %d%%\n", h->format_id, h->detection_confidence);
 *       if (h->protection_detected)
 *           printf("Protection: %s\n", h->protection_name);
 *       if (h->using_v3)
 *           printf("Using advanced v3 parser\n");
 *       uft_advanced_close(h);
 *   }
 */

#ifndef UFT_EXTENDED_H
#define UFT_EXTENDED_H

/* ═══════════════════════════════════════════════════════════════════════════════
 * ADVANCED MODE - Automatic Integration
 * ═══════════════════════════════════════════════════════════════════════════════ */
#include "uft/uft_advanced_mode.h"

/* ═══════════════════════════════════════════════════════════════════════════════
 * FORMAT DETECTION - Probe Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */
#include "uft/uft_format_probes.h"

/* ═══════════════════════════════════════════════════════════════════════════════
 * PROTECTION DETECTION
 * ═══════════════════════════════════════════════════════════════════════════════ */
#include "uft/uft_protection.h"
#include "uft/uft_protection_detect.h"

/* ═══════════════════════════════════════════════════════════════════════════════
 * GOD-MODE ALGORITHMS
 * ═══════════════════════════════════════════════════════════════════════════════ */
#include "uft/uft_god_mode.h"

/* ═══════════════════════════════════════════════════════════════════════════════
 * V3 PARSER BRIDGE
 * ═══════════════════════════════════════════════════════════════════════════════ */
#include "uft/uft_v3_bridge.h"

#endif /* UFT_EXTENDED_H */
