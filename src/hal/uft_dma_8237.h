#ifndef UFT_DMA_8237_H
#define UFT_DMA_8237_H
/**
 * @file uft_dma_8237.h
 * @brief Intel 8237 DMA Controller Definitions
 *
 * For UFT Hardware Abstraction Layer - FDC DMA support.
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * DMA Mode Register Values
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_DMA_MODE_DEMAND     0x00
#define UFT_DMA_MODE_SINGLE     0x40
#define UFT_DMA_MODE_BLOCK      0x80
#define UFT_DMA_MODE_CASCADE    0xC0

#define UFT_DMA_XFER_VERIFY     0x00
#define UFT_DMA_XFER_WRITE      0x04    /* Write to memory */
#define UFT_DMA_XFER_READ       0x08    /* Read from memory */

#define UFT_DMA_AUTOINIT        0x10
#define UFT_DMA_DECREMENT       0x20

/* ═══════════════════════════════════════════════════════════════════════════
 * DMA Channel for Floppy (typically channel 2)
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_DMA_FLOPPY_CHANNEL  2

/* ═══════════════════════════════════════════════════════════════════════════
 * Backend I/O Interface
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct uft_dma_io {
    uint8_t (*in8)(void *ctx, uint16_t port);
    void    (*out8)(void *ctx, uint16_t port, uint8_t value);
    void    *ctx;
} uft_dma_io_t;

#ifdef __cplusplus
}
#endif

#endif /* UFT_DMA_8237_H */
