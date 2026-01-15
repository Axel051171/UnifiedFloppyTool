/**
 * @file uft_sector_parser.c
 * @brief Sector parsing and extraction
 * @version 3.8.0
 */
/*
 * uft_sector_parser.c - IBM FM/MFM Sector Parser Implementation
 *
 * Part of UnifiedFloppyTool (UFT) v3.3.0
 */

#include "uft/uft_sector_parser.h"
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Public: Length from N
 * ═══════════════════════════════════════════════════════════════════════════ */

uint16_t uft_sector_length_from_n(uint8_t size_n) {
    if (size_n > 7) return 0;
    return (uint16_t)(128u << size_n);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Public: CRC16-CCITT
 * ═══════════════════════════════════════════════════════════════════════════ */

uint16_t uft_sector_crc16(const uint8_t *buf, size_t len, uint16_t init) {
    uint16_t crc = init;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)buf[i] << 8;
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000u) 
                ? (uint16_t)((crc << 1) ^ 0x1021u) 
                : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: Mark Mask Helper
 * ═══════════════════════════════════════════════════════════════════════════ */

static int mask_is_mark(const uft_sector_config_t *cfg, size_t pos) {
    if (!cfg || !cfg->mark_mask) return 0;
    if (pos >= cfg->mark_mask_len) return 0;
    return (cfg->mark_mask[pos] & 0x01u) != 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: Sync Detection
 * ═══════════════════════════════════════════════════════════════════════════ */

static int accept_sync(const uft_sector_config_t *cfg,
                       const uint8_t *stream, size_t stream_len,
                       size_t sync_pos, uft_sector_encoding_t enc,
                       uint32_t *status_out)
{
    if (!stream || sync_pos + 3 > stream_len) return 0;

    if (enc == UFT_ENC_MFM) {
        /* MFM: Look for 0xA1 0xA1 0xA1 sync pattern */
        if (stream[sync_pos] == 0xA1 && 
            stream[sync_pos+1] == 0xA1 && 
            stream[sync_pos+2] == 0xA1) {
            
            if (cfg && cfg->require_mark_mask) {
                if (mask_is_mark(cfg, sync_pos) && 
                    mask_is_mark(cfg, sync_pos+1) && 
                    mask_is_mark(cfg, sync_pos+2)) {
                    return 1;
                }
                if (status_out) *status_out |= UFT_SECTOR_WEAK_SYNC;
                return 0;
            }
            if (cfg && !cfg->mark_mask && status_out) {
                *status_out |= UFT_SECTOR_WEAK_SYNC;
            }
            return 1;
        }
        return 0;
    }

    if (enc == UFT_ENC_FM) {
        /* FM: Look for 0x00 0x00 0x00 sync pattern */
        if (stream[sync_pos] == 0x00 && 
            stream[sync_pos+1] == 0x00 && 
            stream[sync_pos+2] == 0x00) {
            return 1;
        }
        return 0;
    }

    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: Address Mark Detection
 * ═══════════════════════════════════════════════════════════════════════════ */

static int is_idam(uint8_t b) { return b == 0xFE; }
static int is_dam(uint8_t b)  { return (b == 0xFB) || (b == 0xF8); }

static int find_next_record(const uft_sector_config_t *cfg,
                            const uint8_t *stream, size_t stream_len,
                            size_t start_pos,
                            uft_sector_encoding_t enc,
                            size_t *out_sync_pos,
                            size_t *out_mark_pos,
                            uint8_t *out_mark,
                            uint32_t *out_status)
{
    if (!stream || start_pos >= stream_len) return 0;
    
    for (size_t i = start_pos; i + 4 <= stream_len; i++) {
        uint32_t st = 0;
        if (!accept_sync(cfg, stream, stream_len, i, enc, &st)) continue;
        
        size_t mark_pos = i + 3;
        if (mark_pos >= stream_len) continue;
        
        uint8_t m = stream[mark_pos];
        if (is_idam(m) || is_dam(m)) {
            if (out_sync_pos) *out_sync_pos = i;
            if (out_mark_pos) *out_mark_pos = mark_pos;
            if (out_mark) *out_mark = m;
            if (out_status) *out_status |= st;
            return 1;
        } else {
            if (out_status) *out_status |= (st | UFT_SECTOR_UNUSUAL_MARK);
        }
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: ID Comparison
 * ═══════════════════════════════════════════════════════════════════════════ */

static int id_equals(const uft_sector_id_t *a, const uft_sector_id_t *b) {
    return a && b && 
           a->cylinder == b->cylinder && 
           a->head == b->head && 
           a->sector == b->sector && 
           a->size_code == b->size_code;
}

static int find_sector_by_id(uft_parsed_sector_t *sectors, size_t count, 
                             const uft_sector_id_t *id) {
    if (!sectors || !id) return -1;
    for (size_t i = 0; i < count; i++) {
        if (id_equals(&sectors[i].id_rec.id, id)) return (int)i;
    }
    return -1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Public: Parse Track
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_sector_parse_track(const uft_sector_config_t *cfg,
                           const uint8_t *stream, size_t stream_len,
                           uft_parsed_sector_t *sectors, size_t sectors_cap,
                           uft_sector_result_t *out)
{
    if (!cfg || !stream || !sectors) return -1;
    if (sectors_cap == 0) return -2;

    /* Auto-detect encoding if unknown */
    uft_sector_encoding_t enc = cfg->encoding;
    if (enc == UFT_ENC_UNKNOWN) {
        enc = UFT_ENC_FM;  /* Default to FM */
        for (size_t i = 0; i + 3 < stream_len; i++) {
            if (stream[i] == 0xA1 && stream[i+1] == 0xA1 && stream[i+2] == 0xA1) {
                enc = UFT_ENC_MFM;
                break;
            }
        }
    }

    /* Initialize sectors */
    for (size_t i = 0; i < sectors_cap; i++) {
        memset(&sectors[i].id_rec, 0, sizeof(sectors[i].id_rec));
        memset(&sectors[i].data_rec, 0, sizeof(sectors[i].data_rec));
    }

    uft_sector_result_t res = {0};
    size_t pos = 0;
    size_t sector_count = 0;

    /* ─────────────────────────────────────────────────────────────────────
     * Phase 1: Find all ID records
     * ───────────────────────────────────────────────────────────────────── */
    while (pos < stream_len && 
           sector_count < sectors_cap && 
           sector_count < cfg->max_sectors) {
        
        size_t sync_pos = 0, mark_pos = 0;
        uint8_t mark = 0;
        uint32_t st = 0;
        
        if (!find_next_record(cfg, stream, stream_len, pos, enc, 
                              &sync_pos, &mark_pos, &mark, &st)) {
            break;
        }
        pos = mark_pos + 1;

        if (!is_idam(mark)) continue;
        res.ids_found++;

        /* Check if enough data for ID record */
        if (mark_pos + 1 + 4 + 2 > stream_len) {
            uft_parsed_sector_t *s = &sectors[sector_count++];
            s->id_rec.sync_offset = sync_pos;
            s->id_rec.offset = mark_pos;
            s->id_rec.status = st | UFT_SECTOR_TRUNCATED;
            res.warnings++;
            continue;
        }

        /* Parse ID fields (CHRN) */
        uft_sector_id_t id = {
            .cylinder    = stream[mark_pos + 1],
            .head   = stream[mark_pos + 2],
            .sector    = stream[mark_pos + 3],
            .size_code = stream[mark_pos + 4],
        };

        /* Read and calculate CRC */
        uint16_t crc_read = (uint16_t)((uint16_t)stream[mark_pos + 5] << 8) | 
                           (uint16_t)stream[mark_pos + 6];

        uint8_t tmp[8];
        tmp[0] = stream[sync_pos + 0];
        tmp[1] = stream[sync_pos + 1];
        tmp[2] = stream[sync_pos + 2];
        tmp[3] = stream[mark_pos];
        tmp[4] = id.cylinder;
        tmp[5] = id.head;
        tmp[6] = id.sector;
        tmp[7] = id.size_code;
        uint16_t crc_calc = uft_sector_crc16(tmp, sizeof(tmp), 0xFFFF);

        /* Check for duplicate */
        int existing = find_sector_by_id(sectors, sector_count, &id);
        if (existing >= 0) {
            sectors[existing].id_rec.status |= (UFT_SECTOR_DUPLICATE_ID | st);
            res.duplicates++;
            res.warnings++;
            continue;
        }

        /* Store ID record */
        uft_parsed_sector_t *s = &sectors[sector_count++];
        s->id_rec.sync_offset = sync_pos;
        s->id_rec.offset = mark_pos;
        s->id_rec.id = id;
        s->id_rec.crc_read = crc_read;
        s->id_rec.crc_calc = crc_calc;
        s->id_rec.status = st;
        
        if (crc_read != crc_calc) {
            s->id_rec.status |= UFT_SECTOR_CRC_ID_BAD;
            res.warnings++;
        }
    }

    res.sectors_found = (uint16_t)sector_count;

    /* ─────────────────────────────────────────────────────────────────────
     * Phase 2: Find data records for each sector
     * ───────────────────────────────────────────────────────────────────── */
    for (size_t si = 0; si < sector_count; si++) {
        uft_parsed_sector_t *s = &sectors[si];
        uint16_t expected_len = uft_sector_length_from_n(s->id_rec.id.size_code);
        s->data_rec.expected_len = expected_len;

        /* Search range: after ID record */
        size_t start = s->id_rec.offset;
        if (start + 7 < stream_len) start += 7;
        
        size_t end = stream_len;
        if (cfg->max_search_gap > 0) {
            size_t lim = start + cfg->max_search_gap;
            if (lim < end) end = lim;
        }

        /* Find DAM */
        size_t sync_pos = 0, mark_pos = 0;
        uint8_t mark = 0;
        uint32_t st = 0;
        int found = 0;

        for (size_t p = start; p < end; p++) {
            if (!find_next_record(cfg, stream, stream_len, p, enc, 
                                  &sync_pos, &mark_pos, &mark, &st)) {
                break;
            }
            p = mark_pos;
            if (!is_dam(mark)) continue;
            found = 1;
            break;
        }

        if (!found) {
            s->data_rec.status |= UFT_SECTOR_MISSING_DATA;
            res.warnings++;
            continue;
        }

        res.data_records_found++;
        s->data_rec.sync_offset = sync_pos;
        s->data_rec.offset = mark_pos;
        s->data_rec.dam = mark;
        s->data_rec.status |= st;

        if (expected_len == 0) {
            s->data_rec.status |= UFT_SECTOR_SIZE_MISMATCH;
            expected_len = s->data_capacity;
            res.warnings++;
        }

        /* Check if data is truncated */
        size_t need = (size_t)mark_pos + 1 + (size_t)expected_len + 2;
        if (need > stream_len) {
            s->data_rec.status |= UFT_SECTOR_TRUNCATED;
            res.warnings++;
            
            size_t available = (mark_pos + 1 < stream_len) 
                             ? (stream_len - (mark_pos + 1)) : 0;
            size_t copy_len = available;
            if (copy_len > s->data_capacity) copy_len = s->data_capacity;
            if (s->data && copy_len) {
                memcpy(s->data, &stream[mark_pos + 1], copy_len);
            }
            s->data_rec.data_len = (uint16_t)copy_len;
            continue;
        }

        /* Copy data */
        size_t copy_len = expected_len;
        if (copy_len > s->data_capacity) {
            s->data_rec.status |= UFT_SECTOR_SIZE_MISMATCH;
            res.warnings++;
            copy_len = s->data_capacity;
        }
        if (s->data && copy_len) {
            memcpy(s->data, &stream[mark_pos + 1], copy_len);
        }
        s->data_rec.data_len = (uint16_t)copy_len;

        /* Calculate and verify CRC */
        size_t crc_pos = (size_t)mark_pos + 1 + (size_t)expected_len;
        uint16_t crc_read = (uint16_t)((uint16_t)stream[crc_pos] << 8) | 
                           (uint16_t)stream[crc_pos + 1];

        uint16_t crc_calc = 0xFFFF;
        crc_calc = uft_sector_crc16(&stream[sync_pos], 3, crc_calc);
        crc_calc = uft_sector_crc16(&stream[mark_pos], 1, crc_calc);
        crc_calc = uft_sector_crc16(&stream[mark_pos + 1], expected_len, crc_calc);

        s->data_rec.crc_read = crc_read;
        s->data_rec.crc_calc = crc_calc;

        if (crc_read != crc_calc) {
            s->data_rec.status |= UFT_SECTOR_CRC_DATA_BAD;
            res.warnings++;
        } else {
            res.sectors_with_data++;
        }
    }

    if (out) *out = res;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Public: Status String
 * ═══════════════════════════════════════════════════════════════════════════ */

const char *uft_sector_status_str(uint32_t status) {
    if (status == UFT_SECTOR_OK) return "OK";
    if (status & UFT_SECTOR_CRC_ID_BAD) return "CRC Error (ID)";
    if (status & UFT_SECTOR_CRC_DATA_BAD) return "CRC Error (Data)";
    if (status & UFT_SECTOR_MISSING_DATA) return "Missing Data";
    if (status & UFT_SECTOR_DUPLICATE_ID) return "Duplicate ID";
    if (status & UFT_SECTOR_SIZE_MISMATCH) return "Size Mismatch";
    if (status & UFT_SECTOR_TRUNCATED) return "Truncated";
    if (status & UFT_SECTOR_WEAK_SYNC) return "Weak Sync";
    if (status & UFT_SECTOR_UNUSUAL_MARK) return "Unusual Mark";
    return "Unknown Error";
}
