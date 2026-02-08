/**
 * @file uft_sector_status.c
 * @brief Sector status tracking implementation.
 */
#include "uft/uft_sector_status.h"

static int rank_state(uft_sector_state_t s)
{
    switch (s) {
    case UFT_SECTOR_OK:        return 4;
    case UFT_SECTOR_RECOVERED: return 3;
    case UFT_SECTOR_PARTIAL:   return 2;
    case UFT_SECTOR_BAD_CRC:   return 1;
    case UFT_SECTOR_MISSING:   return 0;
    default:                   return 0;
    }
}

void uft_sector_status_init(uft_sector_status_t *s, uint16_t track, uint8_t head, uint16_t sector, uint16_t size)
{
    if (!s) return;
    s->track = track;
    s->head = head;
    s->sector = sector;
    s->size = size;
    s->state = UFT_SECTOR_MISSING;
    s->confidence = 0;
    s->retries = 0;
    s->flags = UFT_SECTOR_FLAG_NONE;
    s->crc = 0;
}

void uft_sector_status_mark(uft_sector_status_t *s, uft_sector_state_t state, uint8_t confidence, uint32_t flags, uint32_t crc)
{
    if (!s) return;
    s->state = state;
    s->confidence = confidence;
    s->flags = flags;
    s->crc = crc;
}

void uft_sector_status_merge(uft_sector_status_t *dst, const uft_sector_status_t *src)
{
    if (!dst || !src) return;

    dst->retries = (uint8_t)(dst->retries + 1);

    /* Prefer better state by rank */
    if (rank_state(src->state) > rank_state(dst->state)) {
        dst->state = src->state;
        dst->crc = src->crc;
    }

    /* Confidence: keep the best */
    if (src->confidence > dst->confidence)
        dst->confidence = src->confidence;

    /* Flags: union */
    dst->flags |= src->flags;
}
