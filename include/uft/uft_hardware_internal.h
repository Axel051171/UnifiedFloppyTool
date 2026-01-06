/**
 * @file uft_hardware_internal.h
 * @brief Interne Definitionen für Hardware-Backends
 * 
 * Dieser Header ist NUR für Backend-Implementierungen!
 * Anwendungscode sollte nur uft_hardware.h verwenden.
 * 
 * @author UFT Team
 * @date 2025
 */

#ifndef UFT_HARDWARE_INTERNAL_H
#define UFT_HARDWARE_INTERNAL_H

#include "uft/uft_hardware.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Device Structure (intern)
// ============================================================================

/**
 * @brief Interne Device-Struktur
 * 
 * Backends greifen über diese Struktur auf das Handle zu.
 */
struct uft_hw_device {
    const uft_hw_backend_t* backend;    ///< Backend für dieses Gerät
    uft_hw_info_t           info;       ///< Geräte-Info
    void*                   handle;     ///< Backend-spezifisches Handle
    
    // Aktueller Status (vom Core verwaltet)
    uint8_t                 current_track;
    uint8_t                 current_head;
    bool                    motor_running;
};

// ============================================================================
// Backend Registration (intern)
// ============================================================================

/**
 * @brief Registriert das Greaseweazle Backend
 */
uft_error_t uft_hw_register_greaseweazle(void);

/**
 * @brief Registriert das OpenCBM/Nibtools Backend
 */
uft_error_t uft_hw_register_opencbm(void);

/**
 * @brief Registriert das FC5025 Backend
 */
uft_error_t uft_hw_register_fc5025(void);

/**
 * @brief Registriert das KryoFlux Backend
 */
uft_error_t uft_hw_register_kryoflux(void);

/**
 * @brief Registriert das SuperCard Pro Backend
 */
uft_error_t uft_hw_register_supercard(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_HARDWARE_INTERNAL_H */
