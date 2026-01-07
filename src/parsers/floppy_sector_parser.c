#include "uft_floppy_sector_parser.h"
#include <string.h>

uint16_t fps_expected_length_from_N(uint8_t sizeN) {
    if (sizeN > 7) return 0;
    return (uint16_t)(128u << sizeN);
}

uint16_t fps_crc16_ccitt(const uint8_t *buf, size_t len, uint16_t init) {
    uint16_t crc = init;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)buf[i] << 8;
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

static int mask_is_mark(const fps_config_t *cfg, size_t pos) {
    if (!cfg || !cfg->mark_mask) return 0;
    if (pos >= cfg->mark_mask_len) return 0;
    return (cfg->mark_mask[pos] & 0x01u) != 0;
}

static int accept_sync(const fps_config_t *cfg,
                       const uint8_t *stream, size_t stream_len,
                       size_t sync_pos, fps_encoding_t enc,
                       uint32_t *status_out)
{
    if (!stream || sync_pos + 3 > stream_len) return 0;

    if (enc == FPS_ENC_MFM) {
        if (stream[sync_pos] == 0xA1 && stream[sync_pos+1] == 0xA1 && stream[sync_pos+2] == 0xA1) {
            if (cfg && cfg->require_mark_mask) {
                if (mask_is_mark(cfg, sync_pos) && mask_is_mark(cfg, sync_pos+1) && mask_is_mark(cfg, sync_pos+2)) {
                    return 1;
                }
                if (status_out) *status_out |= FPS_WARN_WEAK_SYNC;
                return 0;
            }
            if (cfg && !cfg->mark_mask && status_out) *status_out |= FPS_WARN_WEAK_SYNC;
            return 1;
        }
        return 0;
    }

    if (enc == FPS_ENC_FM) {
        if (stream[sync_pos] == 0x00 && stream[sync_pos+1] == 0x00 && stream[sync_pos+2] == 0x00) {
            return 1;
        }
        return 0;
    }

    return 0;
}

static int is_idam(uint8_t b) { return b == 0xFE; }
static int is_dam(uint8_t b)  { return (b == 0xFB) || (b == 0xF8); }

static int find_next_record(const fps_config_t *cfg,
                            const uint8_t *stream, size_t stream_len,
                            size_t start_pos,
                            fps_encoding_t enc,
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
            if (out_status) *out_status |= (st | FPS_WARN_UNUSUAL_MARK);
        }
    }
    return 0;
}

static int id_equals(const fps_id_fields_t *a, const fps_id_fields_t *b) {
    return a && b && a->cyl == b->cyl && a->head == b->head && a->sec == b->sec && a->sizeN == b->sizeN;
}

static int find_sector_by_id(fps_sector_t *sectors, size_t count, const fps_id_fields_t *id) {
    if (!sectors || !id) return -1;
    for (size_t i = 0; i < count; i++) {
        if (id_equals(&sectors[i].idrec.id, id)) return (int)i;
    }
    return -1;
}

int fps_parse_track(const fps_config_t *cfg,
                    const uint8_t *stream, size_t stream_len,
                    fps_sector_t *sectors, size_t sectors_cap,
                    fps_result_t *out)
{
    if (!cfg || !stream || !sectors) return -1;
    if (sectors_cap == 0) return -2;

    fps_encoding_t enc = cfg->encoding;
    if (enc == FPS_ENC_UNKNOWN) {
        enc = FPS_ENC_FM;
        for (size_t i = 0; i + 3 < stream_len; i++) {
            if (stream[i] == 0xA1 && stream[i+1] == 0xA1 && stream[i+2] == 0xA1) { enc = FPS_ENC_MFM; break; }
        }
    }

    for (size_t i = 0; i < sectors_cap; i++) {
        memset(&sectors[i].idrec, 0, sizeof(sectors[i].idrec));
        memset(&sectors[i].datarec, 0, sizeof(sectors[i].datarec));
    }

    fps_result_t res = {0};
    size_t pos = 0;
    size_t sector_count = 0;

    while (pos < stream_len && sector_count < sectors_cap && sector_count < cfg->max_sectors) {
        size_t sync_pos = 0, mark_pos = 0;
        uint8_t mark = 0;
        uint32_t st = 0;
        if (!find_next_record(cfg, stream, stream_len, pos, enc, &sync_pos, &mark_pos, &mark, &st)) break;
        pos = mark_pos + 1;

        if (!is_idam(mark)) continue;
        res.ids_found++;

        if (mark_pos + 1 + 4 + 2 > stream_len) {
            fps_sector_t *s = &sectors[sector_count++];
            s->idrec.sync_offset = sync_pos;
            s->idrec.offset = mark_pos;
            s->idrec.status = st | FPS_WARN_TRUNCATED_RECORD;
            res.warnings++;
            continue;
        }

        fps_id_fields_t id = {
            .cyl   = stream[mark_pos + 1],
            .head  = stream[mark_pos + 2],
            .sec   = stream[mark_pos + 3],
            .sizeN = stream[mark_pos + 4],
        };

        uint16_t crc_read = (uint16_t)((uint16_t)stream[mark_pos + 5] << 8) | (uint16_t)stream[mark_pos + 6];

        uint8_t tmp[8];
        tmp[0] = stream[sync_pos+0];
        tmp[1] = stream[sync_pos+1];
        tmp[2] = stream[sync_pos+2];
        tmp[3] = stream[mark_pos];
        tmp[4] = id.cyl;
        tmp[5] = id.head;
        tmp[6] = id.sec;
        tmp[7] = id.sizeN;
        uint16_t crc_calc = fps_crc16_ccitt(tmp, sizeof(tmp), 0xFFFF);

        int existing = find_sector_by_id(sectors, sector_count, &id);
        if (existing >= 0) {
            sectors[existing].idrec.status |= (FPS_WARN_DUPLICATE_ID | st);
            res.duplicates++;
            res.warnings++;
            continue;
        }

        fps_sector_t *s = &sectors[sector_count++];
        s->idrec.sync_offset = sync_pos;
        s->idrec.offset = mark_pos;
        s->idrec.id = id;
        s->idrec.crc_read = crc_read;
        s->idrec.crc_calc = crc_calc;
        s->idrec.status = st;
        if (crc_read != crc_calc) {
            s->idrec.status |= FPS_WARN_CRC_ID_BAD;
            res.warnings++;
        }
    }

    res.sectors_found = (uint16_t)sector_count;

    for (size_t si = 0; si < sector_count; si++) {
        fps_sector_t *s = &sectors[si];
        uint16_t expected_len = fps_expected_length_from_N(s->idrec.id.sizeN);
        s->datarec.expected_len = expected_len;

        size_t start = s->idrec.offset;
        if (start + 7 < stream_len) start += 7;
        size_t end = stream_len;
        if (cfg->max_search_gap > 0) {
            size_t lim = start + cfg->max_search_gap;
            if (lim < end) end = lim;
        }

        size_t sync_pos=0, mark_pos=0;
        uint8_t mark=0;
        uint32_t st=0;
        int found = 0;

        for (size_t p = start; p < end; p++) {
            if (!find_next_record(cfg, stream, stream_len, p, enc, &sync_pos, &mark_pos, &mark, &st)) break;
            p = mark_pos;
            if (!is_dam(mark)) continue;
            found = 1;
            break;
        }

        if (!found) {
            s->datarec.status |= FPS_WARN_MISSING_DATA;
            res.warnings++;
            continue;
        }

        res.data_records_found++;
        s->datarec.sync_offset = sync_pos;
        s->datarec.offset = mark_pos;
        s->datarec.dam = mark;
        s->datarec.status |= st;

        if (expected_len == 0) {
            s->datarec.status |= FPS_WARN_SIZE_MISMATCH;
            expected_len = s->data_capacity;
            res.warnings++;
        }

        size_t need = (size_t)mark_pos + 1 + (size_t)expected_len + 2;
        if (need > stream_len) {
            s->datarec.status |= FPS_WARN_TRUNCATED_RECORD;
            res.warnings++;
            size_t available = (mark_pos + 1 < stream_len) ? (stream_len - (mark_pos + 1)) : 0;
            size_t copy_len = available;
            if (copy_len > s->data_capacity) copy_len = s->data_capacity;
            if (s->data && copy_len) memcpy(s->data, &stream[mark_pos + 1], copy_len);
            s->datarec.data_len = (uint16_t)copy_len;
            continue;
        }

        size_t copy_len = expected_len;
        if (copy_len > s->data_capacity) {
            s->datarec.status |= FPS_WARN_SIZE_MISMATCH;
            res.warnings++;
            copy_len = s->data_capacity;
        }
        if (s->data && copy_len) memcpy(s->data, &stream[mark_pos + 1], copy_len);
        s->datarec.data_len = (uint16_t)copy_len;

        size_t crc_pos = (size_t)mark_pos + 1 + (size_t)expected_len;
        uint16_t crc_read = (uint16_t)((uint16_t)stream[crc_pos] << 8) | (uint16_t)stream[crc_pos + 1];

        uint16_t crc_calc = 0xFFFF;
        crc_calc = fps_crc16_ccitt(&stream[sync_pos], 3, crc_calc);
        crc_calc = fps_crc16_ccitt(&stream[mark_pos], 1, crc_calc);
        crc_calc = fps_crc16_ccitt(&stream[mark_pos + 1], expected_len, crc_calc);

        s->datarec.crc_read = crc_read;
        s->datarec.crc_calc = crc_calc;

        if (crc_read != crc_calc) {
            s->datarec.status |= FPS_WARN_CRC_DATA_BAD;
            res.warnings++;
        } else {
            res.sectors_with_data++;
        }
    }

    if (out) *out = res;
    return 0;
}
