/**
 * @file uft_samdisk_adapter.cpp
 * @brief C API Wrapper Implementation for SAMdisk
 * 
 * This file bridges UFT's C code to SAMdisk's C++ bitstream processing.
 * 
 * @copyright UFT Project 2025
 */

#include "uft/samdisk/uft_samdisk_adapter.h"

#ifdef UFT_ENABLE_SAMDISK

#include "SAMdisk.h"
#include "BitBuffer.h"
#include "CRC16.h"

#include <cstring>
#include <vector>

/*============================================================================
 * Internal Structures
 *============================================================================*/

struct uft_sam_bitbuffer_s {
    BitBuffer buffer;
    
    uft_sam_bitbuffer_s() = default;
    uft_sam_bitbuffer_s(const uint8_t *data, size_t size) 
        : buffer(data, data + size) {}
};

struct uft_sam_decoder_s {
    /* Reserved for future decoder state */
    int dummy;
};

/*============================================================================
 * BitBuffer API Implementation
 *============================================================================*/

extern "C" {

uft_sam_bitbuffer_t *uft_sam_bitbuffer_create(const uint8_t *data, size_t size)
{
    if (!data || size == 0) {
        return nullptr;
    }
    
    try {
        auto buf = new uft_sam_bitbuffer_s(data, size);
        return buf;
    } catch (...) {
        return nullptr;
    }
}

uft_sam_bitbuffer_t *uft_sam_bitbuffer_from_flux(const uint32_t *flux_times, 
                                                  size_t count,
                                                  uint32_t sample_rate)
{
    if (!flux_times || count == 0) {
        return nullptr;
    }
    
    try {
        auto buf = new uft_sam_bitbuffer_s();
        /* TODO: Implement flux-to-bitstream conversion using FluxDecoder */
        (void)sample_rate;
        return buf;
    } catch (...) {
        return nullptr;
    }
}

void uft_sam_bitbuffer_destroy(uft_sam_bitbuffer_t *buf)
{
    delete buf;
}

size_t uft_sam_bitbuffer_bits(const uft_sam_bitbuffer_t *buf)
{
    if (!buf) return 0;
    return buf->buffer.size() * 8;
}

/*============================================================================
 * Decoder API Implementation
 *============================================================================*/

uft_sam_decoder_t *uft_sam_decoder_create(void)
{
    try {
        return new uft_sam_decoder_s();
    } catch (...) {
        return nullptr;
    }
}

void uft_sam_decoder_destroy(uft_sam_decoder_t *dec)
{
    delete dec;
}

int uft_sam_decode_track(uft_sam_decoder_t *dec,
                         const uft_sam_bitbuffer_t *buf,
                         uft_sam_track_t *track)
{
    if (!dec || !buf || !track) {
        return UFT_SAM_ERROR_INVALID_PARAM;
    }
    
    /* TODO: Implement using BitstreamDecoder */
    memset(track, 0, sizeof(*track));
    
    return UFT_SAM_ERROR_NOT_INITIALIZED;
}

void uft_sam_track_free(uft_sam_track_t *track)
{
    if (!track) return;
    
    if (track->sectors) {
        for (size_t i = 0; i < track->sector_count; i++) {
            free(track->sectors[i].data);
        }
        free(track->sectors);
    }
    memset(track, 0, sizeof(*track));
}

/*============================================================================
 * CRC API Implementation
 *============================================================================*/

uint16_t uft_sam_crc_ccitt(const uint8_t *data, size_t size, uint16_t init)
{
    if (!data || size == 0) {
        return init;
    }
    
    /* Use SAMdisk's CRC implementation */
    return CRC16(data, size, init);
}

uint16_t uft_sam_crc16(const uint8_t *data, size_t size)
{
    return uft_sam_crc_ccitt(data, size, 0xFFFF);
}

/*============================================================================
 * Version API Implementation
 *============================================================================*/

const char *uft_sam_version(void)
{
    return "SAMdisk 4.0-uft (UFT Integration)";
}

bool uft_sam_available(void)
{
    return true;
}

} /* extern "C" */

#else /* !UFT_ENABLE_SAMDISK */

/*============================================================================
 * Stub Implementation when SAMdisk is disabled
 *============================================================================*/

extern "C" {

uft_sam_bitbuffer_t *uft_sam_bitbuffer_create(const uint8_t *data, size_t size)
{
    (void)data; (void)size;
    return NULL;
}

uft_sam_bitbuffer_t *uft_sam_bitbuffer_from_flux(const uint32_t *flux_times, 
                                                  size_t count,
                                                  uint32_t sample_rate)
{
    (void)flux_times; (void)count; (void)sample_rate;
    return NULL;
}

void uft_sam_bitbuffer_destroy(uft_sam_bitbuffer_t *buf) { (void)buf; }
size_t uft_sam_bitbuffer_bits(const uft_sam_bitbuffer_t *buf) { (void)buf; return 0; }

uft_sam_decoder_t *uft_sam_decoder_create(void) { return NULL; }
void uft_sam_decoder_destroy(uft_sam_decoder_t *dec) { (void)dec; }

int uft_sam_decode_track(uft_sam_decoder_t *dec,
                         const uft_sam_bitbuffer_t *buf,
                         uft_sam_track_t *track)
{
    (void)dec; (void)buf; (void)track;
    return UFT_SAM_ERROR_NOT_INITIALIZED;
}

void uft_sam_track_free(uft_sam_track_t *track) { (void)track; }

uint16_t uft_sam_crc_ccitt(const uint8_t *data, size_t size, uint16_t init)
{
    (void)data; (void)size;
    return init;
}

uint16_t uft_sam_crc16(const uint8_t *data, size_t size)
{
    (void)data; (void)size;
    return 0xFFFF;
}

const char *uft_sam_version(void) { return "SAMdisk (disabled)"; }
bool uft_sam_available(void) { return false; }

} /* extern "C" */

#endif /* UFT_ENABLE_SAMDISK */
