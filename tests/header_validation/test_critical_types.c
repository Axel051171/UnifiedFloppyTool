/**
 * @file test_critical_types.c
 * @brief Validiert dass alle kritischen Typen definiert sind
 * 
 * Dieser Test kompiliert nur wenn alle benötigten Typen vorhanden sind.
 * Er wird automatisch in CI ausgeführt.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Teste kritische Header-Includes */
#include "uft/uft_error.h"
#include "uft/uft_error_compat.h"
#include "uft/uft_types.h"
#include "uft/uft_mfm_flux.h"
#include "uft/uft_atomics.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST 1: Error-Codes
 * ═══════════════════════════════════════════════════════════════════════════ */

void test_error_codes(void)
{
    /* Diese Zeilen kompilieren nur wenn die Codes definiert sind */
    int e1 = UFT_OK;
    int e2 = UFT_ERROR;
    int e3 = UFT_ERR_INVALID_ARG;
    int e4 = UFT_ERR_IO;
    int e5 = UFT_ERR_NOMEM;
    
    /* Legacy-Codes über Compat-Header */
    int e6 = UFT_ERROR_FORMAT_NOT_SUPPORTED;
    int e7 = UFT_ERR_NULL_PTR;
    int e8 = UFT_ERROR_NO_DATA;
    
    (void)e1; (void)e2; (void)e3; (void)e4;
    (void)e5; (void)e6; (void)e7; (void)e8;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST 2: CRC-Konstanten
 * ═══════════════════════════════════════════════════════════════════════════ */

void test_crc_constants(void)
{
    uint16_t c1 = UFT_CRC16_INIT;
    uint16_t c2 = UFT_CRC16_POLY;
    
    (void)c1; (void)c2;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST 3: MFM-Konstanten
 * ═══════════════════════════════════════════════════════════════════════════ */

void test_mfm_constants(void)
{
    uint8_t m1 = UFT_MFM_MARK_IDAM;
    uint8_t m2 = UFT_MFM_MARK_DAM;
    uint8_t m3 = UFT_MFM_MARK_DDAM;
    
    (void)m1; (void)m2; (void)m3;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST 4: Atomic-Typen (MSVC-Kompatibilität)
 * ═══════════════════════════════════════════════════════════════════════════ */

void test_atomic_types(void)
{
    atomic_int ai = 0;
    atomic_bool ab = 0;
    
    (void)ai; (void)ab;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * MAIN - Wenn dieser Code kompiliert, sind alle Tests bestanden
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void)
{
    test_error_codes();
    test_crc_constants();
    test_mfm_constants();
    test_atomic_types();
    
    return 0;  /* Erfolg */
}
