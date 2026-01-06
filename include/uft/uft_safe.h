/**
 * @file uft_safe.h
 * @brief UnifiedFloppyTool - Sichere Makros und Hilfsfunktionen
 * 
 * Defense-in-Depth: Alle kritischen Operationen abgesichert
 * 
 * @author UFT Team - ELITE QA Mode
 * @date 2025
 */

#ifndef UFT_SAFE_H
#define UFT_SAFE_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include "uft_error.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Memory Allocation (NULL-Check erzwungen)
// ============================================================================

/**
 * @brief Sichere malloc - gibt UFT_ERROR_NO_MEMORY bei Fehler zurück
 * @note Erfordert dass die Funktion uft_error_t zurückgibt
 */
#define UFT_MALLOC(ptr, size) do { \
    (ptr) = malloc(size); \
    if (!(ptr)) { \
        return UFT_ERROR_NO_MEMORY; \
    } \
} while(0)

/**
 * @brief Sichere calloc
 */
#define UFT_CALLOC(ptr, count, size) do { \
    (ptr) = calloc((count), (size)); \
    if (!(ptr)) { \
        return UFT_ERROR_NO_MEMORY; \
    } \
} while(0)

/**
 * @brief Sichere malloc mit Cleanup-Block
 * @param cleanup Code der bei Fehler ausgeführt wird (z.B. fclose(f))
 */
#define UFT_MALLOC_OR(ptr, size, cleanup) do { \
    (ptr) = malloc(size); \
    if (!(ptr)) { \
        cleanup; \
        return UFT_ERROR_NO_MEMORY; \
    } \
} while(0)

#define UFT_CALLOC_OR(ptr, count, size, cleanup) do { \
    (ptr) = calloc((count), (size)); \
    if (!(ptr)) { \
        cleanup; \
        return UFT_ERROR_NO_MEMORY; \
    } \
} while(0)

/**
 * @brief Sichere realloc (behält Original bei Fehler)
 */
#define UFT_REALLOC(ptr, new_ptr, size) do { \
    void* _tmp = realloc((ptr), (size)); \
    if (!_tmp) { \
        return UFT_ERROR_NO_MEMORY; \
    } \
    (new_ptr) = _tmp; \
} while(0)

// ============================================================================
// File I/O (Return-Check erzwungen)
// ============================================================================

/**
 * @brief Sichere fread - prüft vollständiges Lesen
 * @param cleanup Code bei Fehler
 */
#define UFT_FREAD(buf, elem_size, count, stream, cleanup) do { \
    size_t _read = fread((buf), (elem_size), (count), (stream)); \
    if (_read != (size_t)(count)) { \
        cleanup; \
        return UFT_ERROR_FILE_READ; \
    } \
} while(0)

/**
 * @brief Sichere fread ohne Cleanup (für einfache Fälle)
 */
#define UFT_FREAD_SIMPLE(buf, elem_size, count, stream) do { \
    if (fread((buf), (elem_size), (count), (stream)) != (size_t)(count)) { \
        return UFT_ERROR_FILE_READ; \
    } \
} while(0)

/**
 * @brief Sichere fwrite
 */
#define UFT_FWRITE(buf, elem_size, count, stream, cleanup) do { \
    if (fwrite((buf), (elem_size), (count), (stream)) != (size_t)(count)) { \
        cleanup; \
        return UFT_ERROR_FILE_WRITE; \
    } \
} while(0)

/**
 * @brief Sichere fseek
 */
#define UFT_FSEEK(stream, offset, whence, cleanup) do { \
    if (fseek((stream), (long)(offset), (whence)) != 0) { \
        cleanup; \
        return UFT_ERROR_FILE_SEEK; \
    } \
} while(0)

#define UFT_FSEEK_SIMPLE(stream, offset, whence) do { \
    if (fseek((stream), (long)(offset), (whence)) != 0) { \
        return UFT_ERROR_FILE_SEEK; \
    } \
} while(0)

// ============================================================================
// Integer Overflow Prevention
// ============================================================================

/**
 * @brief Sichere Multiplikation (Size)
 * @return false bei Overflow
 */
static inline bool uft_safe_mul_size(size_t a, size_t b, size_t* result) {
    if (a > 0 && b > SIZE_MAX / a) {
        return false;
    }
    *result = a * b;
    return true;
}

/**
 * @brief Sichere Addition (Size)
 */
static inline bool uft_safe_add_size(size_t a, size_t b, size_t* result) {
    if (a > SIZE_MAX - b) {
        return false;
    }
    *result = a + b;
    return true;
}

/**
 * @brief Sichere Multiplikation (uint32)
 */
static inline bool uft_safe_mul_u32(uint32_t a, uint32_t b, uint32_t* result) {
    if (a > 0 && b > UINT32_MAX / a) {
        return false;
    }
    *result = a * b;
    return true;
}

/**
 * @brief Makro für sichere Allokation mit Multiplikation
 */
#define UFT_MALLOC_MUL(ptr, count, elem_size, cleanup) do { \
    size_t _total; \
    if (!uft_safe_mul_size((count), (elem_size), &_total)) { \
        cleanup; \
        return UFT_ERROR_OVERFLOW; \
    } \
    (ptr) = malloc(_total); \
    if (!(ptr)) { \
        cleanup; \
        return UFT_ERROR_NO_MEMORY; \
    } \
} while(0)

// ============================================================================
// Buffer Safety
// ============================================================================

/**
 * @brief Sichere memcpy mit Bounds-Check
 */
#define UFT_MEMCPY_SAFE(dst, dst_size, src, copy_size) do { \
    if ((copy_size) > (dst_size)) { \
        return UFT_ERROR_BUFFER_TOO_SMALL; \
    } \
    memcpy((dst), (src), (copy_size)); \
} while(0)

/**
 * @brief Sichere String-Kopie (ersetzt strcpy)
 */
static inline size_t uft_strlcpy(char* dst, const char* src, size_t dst_size) {
    if (dst_size == 0) return 0;
    
    size_t src_len = strlen(src);
    size_t copy_len = (src_len < dst_size - 1) ? src_len : (dst_size - 1);
    
    memcpy(dst, src, copy_len);
    dst[copy_len] = '\0';
    
    return src_len;  // Wie BSD strlcpy
}

/**
 * @brief Sichere String-Konkatenation (ersetzt strcat)
 */
static inline size_t uft_strlcat(char* dst, const char* src, size_t dst_size) {
    size_t dst_len = strlen(dst);
    
    if (dst_len >= dst_size) {
        return dst_len + strlen(src);
    }
    
    return dst_len + uft_strlcpy(dst + dst_len, src, dst_size - dst_len);
}

// ============================================================================
// Bounds Checking
// ============================================================================

/**
 * @brief Prüft ob Index innerhalb Bounds
 */
#define UFT_CHECK_BOUNDS(index, max, cleanup) do { \
    if ((size_t)(index) >= (size_t)(max)) { \
        cleanup; \
        return UFT_ERROR_BOUNDS; \
    } \
} while(0)

/**
 * @brief Prüft Track/Sector Bounds
 */
#define UFT_CHECK_TRACK(cyl, head, max_cyl, max_head) do { \
    if ((cyl) < 0 || (cyl) >= (max_cyl) || \
        (head) < 0 || (head) >= (max_head)) { \
        return UFT_ERROR_INVALID_ARG; \
    } \
} while(0)

/**
 * @brief Prüft Sektor-Bounds
 */
#define UFT_CHECK_SECTOR(sector, max_sector) do { \
    if ((sector) < 0 || (sector) >= (max_sector)) { \
        return UFT_ERROR_INVALID_ARG; \
    } \
} while(0)

// ============================================================================
// Resource Management (RAII-Style)
// ============================================================================

/**
 * @brief Cleanup-Guard für FILE*
 */
typedef struct {
    FILE** fp;
} uft_file_guard_t;

static inline void uft_file_guard_cleanup(uft_file_guard_t* guard) {
    if (guard && guard->fp && *guard->fp) {
        fclose(*guard->fp);
        *guard->fp = NULL;
    }
}

#define UFT_FILE_GUARD(fp) \
    uft_file_guard_t _guard_##fp __attribute__((cleanup(uft_file_guard_cleanup))) = { &(fp) }

/**
 * @brief Cleanup-Guard für malloc'd memory
 */
typedef struct {
    void** ptr;
} uft_mem_guard_t;

static inline void uft_mem_guard_cleanup(uft_mem_guard_t* guard) {
    if (guard && guard->ptr && *guard->ptr) {
        free(*guard->ptr);
        *guard->ptr = NULL;
    }
}

#define UFT_MEM_GUARD(ptr) \
    uft_mem_guard_t _guard_##ptr __attribute__((cleanup(uft_mem_guard_cleanup))) = { (void**)&(ptr) }

// ============================================================================
// Error Propagation
// ============================================================================

/**
 * @brief Prüft Fehler und propagiert
 */
#define UFT_CHECK(expr) do { \
    uft_error_t _err = (expr); \
    if (_err != UFT_OK) return _err; \
} while(0)

/**
 * @brief Prüft Fehler mit Cleanup
 */
#define UFT_CHECK_OR(expr, cleanup) do { \
    uft_error_t _err = (expr); \
    if (_err != UFT_OK) { \
        cleanup; \
        return _err; \
    } \
} while(0)

// ============================================================================
// Validation Helpers
// ============================================================================

/**
 * @brief Prüft Pointer auf NULL
 */
#define UFT_REQUIRE_NOT_NULL(ptr) do { \
    if (!(ptr)) return UFT_ERROR_NULL_POINTER; \
} while(0)

/**
 * @brief Prüft multiple Pointer
 */
#define UFT_REQUIRE_NOT_NULL2(p1, p2) do { \
    if (!(p1) || !(p2)) return UFT_ERROR_NULL_POINTER; \
} while(0)

#define UFT_REQUIRE_NOT_NULL3(p1, p2, p3) do { \
    if (!(p1) || !(p2) || !(p3)) return UFT_ERROR_NULL_POINTER; \
} while(0)

/**
 * @brief Prüft positive Größe
 */
#define UFT_REQUIRE_POSITIVE(val) do { \
    if ((val) <= 0) return UFT_ERROR_INVALID_ARG; \
} while(0)

#ifdef __cplusplus
}
#endif

#endif // UFT_SAFE_H
