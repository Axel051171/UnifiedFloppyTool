/**
 * @file uft_multi_decoder.c
 * @brief UFT Multi-Interpretations-Decoder Implementation
 * @version 3.2.0.003
 * @date 2026-01-04
 * 
 * @copyright Copyright (c) 2026 UFT Project
 * @license MIT
 */

#include "uft/uft_multi_decoder.h"
#include "uft/uft_threading.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * INTERNAL HELPERS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get current timestamp in microseconds
 */
static uint64_t get_timestamp_us(void)
{
    return uft_time_get_us();
}

/**
 * @brief Generate unique session ID
 */
static uint64_t generate_session_id(void)
{
    return uft_generate_session_id();
}

/**
 * @brief Find or create track in session
 */
static uft_mdec_track_t *find_or_create_track(uft_mdec_session_t *session,
                                               uint8_t track,
                                               uint8_t head)
{
    if (!session) return NULL;
    
    /* Search existing tracks */
    for (uint32_t i = 0; i < session->track_count; i++) {
        if (session->tracks[i].track == track && 
            session->tracks[i].head == head) {
            return &session->tracks[i];
        }
    }
    
    /* Need to create new track */
    if (session->track_count >= UFT_MDEC_MAX_TRACKS) {
        return NULL;  /* Overflow */
    }
    
    /* Reallocate if needed */
    if (!session->tracks) {
        session->tracks = calloc(UFT_MDEC_MAX_TRACKS, sizeof(uft_mdec_track_t));
        if (!session->tracks) return NULL;
        session->memory_used += UFT_MDEC_MAX_TRACKS * sizeof(uft_mdec_track_t);
    }
    
    uft_mdec_track_t *new_track = &session->tracks[session->track_count];
    memset(new_track, 0, sizeof(uft_mdec_track_t));
    new_track->track = track;
    new_track->head = head;
    
    session->track_count++;
    
    return new_track;
}

/**
 * @brief Find sector in track
 */
static uft_mdec_sector_t *find_sector(uft_mdec_track_t *trk, uint8_t sector)
{
    if (!trk) return NULL;
    
    for (uint32_t i = 0; i < trk->sector_count; i++) {
        if (trk->sectors[i].sector == sector) {
            return &trk->sectors[i];
        }
    }
    
    return NULL;
}

/**
 * @brief Find or create sector in track
 */
static uft_mdec_sector_t *find_or_create_sector(uft_mdec_track_t *trk,
                                                 uint8_t sector,
                                                 uint8_t track_num,
                                                 uint8_t head_num)
{
    if (!trk) return NULL;
    
    uft_mdec_sector_t *sec = find_sector(trk, sector);
    if (sec) return sec;
    
    /* Create new sector */
    if (trk->sector_count >= UFT_MDEC_MAX_SECTORS) {
        return NULL;
    }
    
    sec = &trk->sectors[trk->sector_count];
    memset(sec, 0, sizeof(uft_mdec_sector_t));
    sec->track = track_num;
    sec->head = head_num;
    sec->sector = sector;
    sec->selected_index = -1;
    sec->status = UFT_MDEC_STATUS_PENDING;
    sec->resolution_strategy = UFT_MDEC_STRATEGY_HIGHEST_CONF;
    
    trk->sector_count++;
    
    return sec;
}

/**
 * @brief Sort candidates by confidence (descending)
 */
static void sort_candidates(uft_mdec_sector_t *sector)
{
    if (!sector || sector->count < 2) return;
    
    /* Simple bubble sort (small N) */
    for (uint32_t i = 0; i < sector->count - 1; i++) {
        for (uint32_t j = 0; j < sector->count - i - 1; j++) {
            if (sector->candidates[j].confidence < sector->candidates[j + 1].confidence) {
                uft_mdec_candidate_t temp = sector->candidates[j];
                sector->candidates[j] = sector->candidates[j + 1];
                sector->candidates[j + 1] = temp;
            }
        }
    }
    
    /* Update statistics */
    if (sector->count > 0) {
        sector->max_confidence = sector->candidates[0].confidence;
        if (sector->count > 1) {
            sector->confidence_spread = sector->candidates[0].confidence - 
                                        sector->candidates[1].confidence;
        } else {
            sector->confidence_spread = 100.0f;
        }
    }
}

/**
 * @brief Calculate CRC32
 */
static uint32_t calculate_crc32(const uint8_t *data, size_t length)
{
    static const uint32_t crc_table[256] = {
        0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
        0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
        0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
        0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
        0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
        0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
        0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
        0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
        0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
        0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
        0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
        0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
        0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
        0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
        0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
        0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
        0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
        0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
        0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7A9D, 0x5005713C, 0x270241AA,
        0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
        0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
        0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
        0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
        0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
        0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
        0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
        0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
        0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
        0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
        0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
        0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
        0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
        0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
        0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
        0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
        0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
        0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
        0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
        0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
        0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
        0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD706B3,
        0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
        0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
    };
    
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc = (crc >> 8) ^ crc_table[(crc ^ data[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SESSION MANAGEMENT
 * ═══════════════════════════════════════════════════════════════════════════ */

void uft_mdec_config_default(uft_mdec_config_t *config)
{
    if (!config) return;
    
    memset(config, 0, sizeof(uft_mdec_config_t));
    
    config->strategy = UFT_MDEC_STRATEGY_HIGHEST_CONF;
    config->auto_threshold = UFT_MDEC_CONFIDENCE_AUTO;
    config->ambiguity_delta = UFT_MDEC_CONFIDENCE_DELTA;
    config->max_candidates = UFT_MDEC_MAX_CANDIDATES;
    config->generate_all = true;
    config->include_invalid = true;  /* Forensic default */
    config->memory_limit = 256 * 1024 * 1024;  /* 256 MB */
    config->stream_mode = false;
    config->forensic_mode = true;
    config->preserve_all = true;
    config->track_provenance = true;
    config->min_revolutions = 1;
    config->revolution_weight = 0.8f;
}

int uft_mdec_session_create(uft_mdec_session_t **session,
                            const uft_mdec_config_t *config,
                            const char *source_file)
{
    if (!session) return UFT_MDEC_ERR_NULL;
    
    *session = calloc(1, sizeof(uft_mdec_session_t));
    if (!*session) return UFT_MDEC_ERR_MEMORY;
    
    uft_mdec_session_t *s = *session;
    
    s->session_id = generate_session_id();
    s->created_us = get_timestamp_us();
    s->modified_us = s->created_us;
    
    if (source_file) {
        strncpy(s->source_file, source_file, sizeof(s->source_file) - 1);
    }
    
    /* Apply configuration */
    if (config) {
        s->default_strategy = config->strategy;
        s->auto_resolve_threshold = config->auto_threshold;
        s->lazy_evaluation = !config->generate_all;
        s->preserve_all = config->preserve_all;
        s->memory_limit = config->memory_limit;
    } else {
        s->default_strategy = UFT_MDEC_STRATEGY_HIGHEST_CONF;
        s->auto_resolve_threshold = UFT_MDEC_CONFIDENCE_AUTO;
        s->lazy_evaluation = true;
        s->preserve_all = true;
        s->memory_limit = 256 * 1024 * 1024;
    }
    
    s->memory_used = sizeof(uft_mdec_session_t);
    
    return UFT_MDEC_OK;
}

void uft_mdec_session_destroy(uft_mdec_session_t **session)
{
    if (!session || !*session) return;
    
    uft_mdec_session_t *s = *session;
    
    if (s->tracks) {
        free(s->tracks);
    }
    
    free(s);
    *session = NULL;
}

int uft_mdec_session_reset(uft_mdec_session_t *session)
{
    if (!session) return UFT_MDEC_ERR_NULL;
    
    /* Reset tracks */
    if (session->tracks) {
        memset(session->tracks, 0, UFT_MDEC_MAX_TRACKS * sizeof(uft_mdec_track_t));
    }
    session->track_count = 0;
    
    /* Reset statistics */
    session->total_sectors = 0;
    session->total_candidates = 0;
    session->resolved_sectors = 0;
    session->ambiguous_sectors = 0;
    session->overall_confidence = 0.0f;
    
    session->modified_us = get_timestamp_us();
    
    return UFT_MDEC_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CANDIDATE MANAGEMENT
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_mdec_add_candidate(uft_mdec_session_t *session,
                           uint8_t track,
                           uint8_t head,
                           uint8_t sector,
                           const uft_mdec_candidate_t *candidate)
{
    if (!session || !candidate) return UFT_MDEC_ERR_NULL;
    
    /* Check memory limit */
    if (session->memory_limit > 0 && 
        session->memory_used + sizeof(uft_mdec_candidate_t) > session->memory_limit) {
        return UFT_MDEC_ERR_MEMORY;
    }
    
    /* Find or create track */
    uft_mdec_track_t *trk = find_or_create_track(session, track, head);
    if (!trk) return UFT_MDEC_ERR_OVERFLOW;
    
    /* Find or create sector */
    uft_mdec_sector_t *sec = find_or_create_sector(trk, sector, track, head);
    if (!sec) return UFT_MDEC_ERR_OVERFLOW;
    
    /* Check candidate limit */
    if (sec->count >= UFT_MDEC_MAX_CANDIDATES) {
        /* In preserve_all mode, refuse. Otherwise, replace worst if new is better */
        if (session->preserve_all) {
            return UFT_MDEC_ERR_OVERFLOW;
        }
        
        /* Find worst candidate */
        uint32_t worst_idx = 0;
        float worst_conf = sec->candidates[0].confidence;
        for (uint32_t i = 1; i < sec->count; i++) {
            if (sec->candidates[i].confidence < worst_conf) {
                worst_conf = sec->candidates[i].confidence;
                worst_idx = i;
            }
        }
        
        /* Replace only if new is better */
        if (candidate->confidence > worst_conf) {
            memcpy(&sec->candidates[worst_idx], candidate, sizeof(uft_mdec_candidate_t));
            sec->candidates[worst_idx].id = sec->total_generated++;
            sec->candidates[worst_idx].modified_us = get_timestamp_us();
            sort_candidates(sec);
        }
        
        return UFT_MDEC_OK;
    }
    
    /* Add new candidate */
    uft_mdec_candidate_t *new_cand = &sec->candidates[sec->count];
    memcpy(new_cand, candidate, sizeof(uft_mdec_candidate_t));
    new_cand->id = sec->total_generated++;
    new_cand->created_us = get_timestamp_us();
    new_cand->modified_us = new_cand->created_us;
    new_cand->data_crc = calculate_crc32(new_cand->data, new_cand->data_size);
    
    sec->count++;
    session->total_candidates++;
    session->memory_used += sizeof(uft_mdec_candidate_t);
    
    /* Sort by confidence */
    sort_candidates(sec);
    
    /* Auto-resolve if confidence exceeds threshold */
    if (sec->max_confidence >= session->auto_resolve_threshold &&
        sec->confidence_spread >= UFT_MDEC_CONFIDENCE_DELTA &&
        !sec->resolved) {
        sec->resolved = true;
        sec->selected_index = 0;
        sec->status = UFT_MDEC_STATUS_AUTO_RESOLVED;
        session->resolved_sectors++;
    }
    
    session->modified_us = get_timestamp_us();
    
    return UFT_MDEC_OK;
}

int uft_mdec_generate_candidates(uft_mdec_session_t *session,
                                 uint8_t track,
                                 uint8_t head,
                                 const uint8_t *bitstream,
                                 size_t bit_count,
                                 uft_mdec_encoding_t encoding)
{
    if (!session || !bitstream) return UFT_MDEC_ERR_NULL;
    if (bit_count == 0) return UFT_MDEC_ERR_INVALID_PARAM;
    
    /* This is a placeholder for actual decoder integration */
    /* The real implementation would:
     * 1. Run PLL/clock recovery on bitstream
     * 2. Identify sync marks
     * 3. Decode sectors with multiple interpretations for weak bits
     * 4. Generate N candidates per sector
     */
    
    (void)track;
    (void)head;
    (void)bitstream;
    (void)bit_count;
    (void)encoding;
    
    /* Return 0 candidates generated (stub) */
    return 0;
}

int uft_mdec_get_sector(const uft_mdec_session_t *session,
                        uint8_t track,
                        uint8_t head,
                        uint8_t sector,
                        const uft_mdec_sector_t **sector_info)
{
    if (!session || !sector_info) return UFT_MDEC_ERR_NULL;
    
    /* Find track */
    const uft_mdec_track_t *trk = NULL;
    for (uint32_t i = 0; i < session->track_count; i++) {
        if (session->tracks[i].track == track && 
            session->tracks[i].head == head) {
            trk = &session->tracks[i];
            break;
        }
    }
    
    if (!trk) return UFT_MDEC_ERR_NOT_FOUND;
    
    /* Find sector */
    for (uint32_t i = 0; i < trk->sector_count; i++) {
        if (trk->sectors[i].sector == sector) {
            *sector_info = &trk->sectors[i];
            return UFT_MDEC_OK;
        }
    }
    
    return UFT_MDEC_ERR_NOT_FOUND;
}

int uft_mdec_get_best(uft_mdec_session_t *session,
                      uint8_t track,
                      uint8_t head,
                      uint8_t sector,
                      const uft_mdec_candidate_t **candidate)
{
    if (!session || !candidate) return UFT_MDEC_ERR_NULL;
    
    const uft_mdec_sector_t *sec;
    int ret = uft_mdec_get_sector(session, track, head, sector, &sec);
    if (ret != UFT_MDEC_OK) return ret;
    
    if (sec->count == 0) return UFT_MDEC_ERR_NO_CANDIDATES;
    
    /* Already resolved? */
    if (sec->resolved && sec->selected_index >= 0 && 
        (uint32_t)sec->selected_index < sec->count) {
        *candidate = &sec->candidates[sec->selected_index];
        return UFT_MDEC_OK;
    }
    
    /* Lazy resolution */
    if (sec->confidence_spread < UFT_MDEC_CONFIDENCE_DELTA && sec->count > 1) {
        /* Multiple close candidates - mark ambiguous */
        return UFT_MDEC_ERR_AMBIGUOUS;
    }
    
    /* Return best candidate */
    *candidate = &sec->candidates[0];
    
    /* Mark as resolved (modify the sector) */
    uft_mdec_sector_t *sec_mut = (uft_mdec_sector_t *)sec;
    sec_mut->resolved = true;
    sec_mut->selected_index = 0;
    sec_mut->status = UFT_MDEC_STATUS_AUTO_RESOLVED;
    session->resolved_sectors++;
    session->modified_us = get_timestamp_us();
    
    return UFT_MDEC_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * RESOLUTION
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_mdec_resolve_sector(uft_mdec_session_t *session,
                            uint8_t track,
                            uint8_t head,
                            uint8_t sector,
                            uft_mdec_strategy_t strategy)
{
    if (!session) return UFT_MDEC_ERR_NULL;
    
    const uft_mdec_sector_t *sec_const;
    int ret = uft_mdec_get_sector(session, track, head, sector, &sec_const);
    if (ret != UFT_MDEC_OK) return ret;
    
    uft_mdec_sector_t *sec = (uft_mdec_sector_t *)sec_const;
    
    if (sec->count == 0) return UFT_MDEC_ERR_NO_CANDIDATES;
    if (sec->resolved) return UFT_MDEC_ERR_ALREADY_RESOLVED;
    
    int selected = -1;
    
    switch (strategy) {
        case UFT_MDEC_STRATEGY_HIGHEST_CONF:
            selected = 0;  /* Already sorted */
            break;
            
        case UFT_MDEC_STRATEGY_CRC_PRIORITY:
            /* Find first CRC-valid candidate */
            for (uint32_t i = 0; i < sec->count; i++) {
                if (sec->candidates[i].crc_valid) {
                    selected = (int)i;
                    break;
                }
            }
            /* Fallback to highest confidence */
            if (selected < 0) selected = 0;
            break;
            
        case UFT_MDEC_STRATEGY_CONSERVATIVE:
            /* Only resolve if clear winner */
            if (sec->confidence_spread >= UFT_MDEC_CONFIDENCE_DELTA) {
                selected = 0;
            } else {
                sec->status = UFT_MDEC_STATUS_AMBIGUOUS;
                session->ambiguous_sectors++;
                return UFT_MDEC_ERR_AMBIGUOUS;
            }
            break;
            
        case UFT_MDEC_STRATEGY_MAJORITY:
            /* Would require multi-revolution data - use highest conf for now */
            selected = 0;
            break;
            
        case UFT_MDEC_STRATEGY_MANUAL:
            sec->resolution_deferred = true;
            return UFT_MDEC_OK;
            
        default:
            selected = 0;
            break;
    }
    
    sec->resolved = true;
    sec->selected_index = selected;
    sec->status = UFT_MDEC_STATUS_HEURISTIC;
    sec->resolution_strategy = strategy;
    session->resolved_sectors++;
    session->modified_us = get_timestamp_us();
    
    return UFT_MDEC_OK;
}

int uft_mdec_select_candidate(uft_mdec_session_t *session,
                              uint8_t track,
                              uint8_t head,
                              uint8_t sector,
                              uint32_t candidate_idx)
{
    if (!session) return UFT_MDEC_ERR_NULL;
    
    const uft_mdec_sector_t *sec_const;
    int ret = uft_mdec_get_sector(session, track, head, sector, &sec_const);
    if (ret != UFT_MDEC_OK) return ret;
    
    uft_mdec_sector_t *sec = (uft_mdec_sector_t *)sec_const;
    
    if (candidate_idx >= sec->count) return UFT_MDEC_ERR_INVALID_PARAM;
    
    if (!sec->resolved) {
        session->resolved_sectors++;
    }
    
    sec->resolved = true;
    sec->selected_index = (int32_t)candidate_idx;
    sec->status = UFT_MDEC_STATUS_USER_RESOLVED;
    session->modified_us = get_timestamp_us();
    
    return UFT_MDEC_OK;
}

int uft_mdec_resolve_all(uft_mdec_session_t *session,
                         uft_mdec_strategy_t strategy,
                         uft_mdec_statistics_t *stats)
{
    if (!session) return UFT_MDEC_ERR_NULL;
    
    int resolved_count = 0;
    
    for (uint32_t t = 0; t < session->track_count; t++) {
        uft_mdec_track_t *trk = &session->tracks[t];
        
        for (uint32_t s = 0; s < trk->sector_count; s++) {
            uft_mdec_sector_t *sec = &trk->sectors[s];
            
            if (!sec->resolved && sec->count > 0) {
                int ret = uft_mdec_resolve_sector(session, trk->track, trk->head,
                                                  sec->sector, strategy);
                if (ret == UFT_MDEC_OK) {
                    resolved_count++;
                }
            }
        }
    }
    
    if (stats) {
        uft_mdec_get_statistics(session, stats);
    }
    
    return resolved_count;
}

int uft_mdec_defer_resolution(uft_mdec_session_t *session,
                              uint8_t track,
                              uint8_t head,
                              uint8_t sector)
{
    if (!session) return UFT_MDEC_ERR_NULL;
    
    const uft_mdec_sector_t *sec_const;
    int ret = uft_mdec_get_sector(session, track, head, sector, &sec_const);
    if (ret != UFT_MDEC_OK) return ret;
    
    uft_mdec_sector_t *sec = (uft_mdec_sector_t *)sec_const;
    sec->resolution_deferred = true;
    session->modified_us = get_timestamp_us();
    
    return UFT_MDEC_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SCORING & CONFIDENCE
 * ═══════════════════════════════════════════════════════════════════════════ */

float uft_mdec_calculate_confidence(uft_mdec_candidate_t *candidate)
{
    if (!candidate) return 0.0f;
    
    float confidence = 50.0f;  /* Base confidence */
    
    /* CRC validity is major factor */
    if (candidate->crc_valid) {
        confidence += 30.0f;
    }
    
    /* Header validity */
    if (candidate->header_valid) {
        confidence += 10.0f;
    }
    
    /* Completeness */
    if (candidate->complete) {
        confidence += 5.0f;
    }
    
    /* Deduct for ambiguous regions */
    if (candidate->ambiguous_count > 0) {
        float amb_penalty = (float)candidate->ambiguous_count * 2.0f;
        if (amb_penalty > 20.0f) amb_penalty = 20.0f;
        confidence -= amb_penalty;
    }
    
    /* Deduct for corrections */
    if (candidate->errors_corrected > 0) {
        float corr_penalty = (float)candidate->errors_corrected * 1.0f;
        if (corr_penalty > 10.0f) corr_penalty = 10.0f;
        confidence -= corr_penalty;
    }
    
    /* Sub-confidence contributions */
    if (candidate->checksum_confidence > 0) {
        confidence = confidence * 0.4f + candidate->checksum_confidence * 0.6f;
    }
    
    /* Clamp to 0-100 */
    if (confidence < 0.0f) confidence = 0.0f;
    if (confidence > 100.0f) confidence = 100.0f;
    
    candidate->confidence = confidence;
    return confidence;
}

float uft_mdec_update_ambiguity(uft_mdec_candidate_t *candidate,
                                const uft_mdec_ambiguous_region_t *ambiguity)
{
    if (!candidate || !ambiguity) return 0.0f;
    
    if (candidate->ambiguous_count >= UFT_MDEC_MAX_AMBIGUOUS) {
        return candidate->confidence;
    }
    
    memcpy(&candidate->ambiguous[candidate->ambiguous_count], 
           ambiguity, sizeof(uft_mdec_ambiguous_region_t));
    candidate->ambiguous_count++;
    
    return uft_mdec_calculate_confidence(candidate);
}

int uft_mdec_compare_candidates(const uft_mdec_candidate_t *a,
                                const uft_mdec_candidate_t *b)
{
    if (!a && !b) return 0;
    if (!a) return 1;
    if (!b) return -1;
    
    /* Primary: confidence */
    if (a->confidence > b->confidence) return -1;
    if (a->confidence < b->confidence) return 1;
    
    /* Secondary: CRC validity */
    if (a->crc_valid && !b->crc_valid) return -1;
    if (!a->crc_valid && b->crc_valid) return 1;
    
    /* Tertiary: fewer ambiguities */
    if (a->ambiguous_count < b->ambiguous_count) return -1;
    if (a->ambiguous_count > b->ambiguous_count) return 1;
    
    return 0;
}

float uft_mdec_confidence_spread(const uft_mdec_sector_t *sector)
{
    if (!sector || sector->count < 2) return 100.0f;
    return sector->candidates[0].confidence - sector->candidates[1].confidence;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PROVENANCE TRACKING
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_mdec_add_provenance(uft_mdec_candidate_t *candidate,
                            uft_mdec_provenance_type_t type,
                            uint32_t offset,
                            uint32_t length,
                            float conf,
                            const char *note)
{
    if (!candidate) return UFT_MDEC_ERR_NULL;
    if (candidate->provenance_count >= UFT_MDEC_MAX_PROVENANCE) {
        return UFT_MDEC_ERR_OVERFLOW;
    }
    
    uft_mdec_provenance_t *prov = &candidate->provenance[candidate->provenance_count];
    prov->type = type;
    prov->bit_offset = offset;
    prov->bit_length = length;
    prov->confidence = conf;
    prov->timestamp_us = (uint32_t)(get_timestamp_us() & 0xFFFFFFFF);
    prov->revolution = candidate->revolution;
    
    if (note) {
        strncpy(prov->note, note, sizeof(prov->note) - 1);
    } else {
        prov->note[0] = '\0';
    }
    
    candidate->provenance_count++;
    candidate->modified_us = get_timestamp_us();
    
    return UFT_MDEC_OK;
}

int uft_mdec_export_provenance(const uft_mdec_candidate_t *candidate,
                               char *buffer,
                               size_t buffer_size)
{
    if (!candidate || !buffer || buffer_size == 0) return UFT_MDEC_ERR_NULL;
    
    int written = 0;
    int total = 0;
    
    for (uint32_t i = 0; i < candidate->provenance_count; i++) {
        const uft_mdec_provenance_t *prov = &candidate->provenance[i];
        
        written = snprintf(buffer + total, buffer_size - (size_t)total,
                          "[%u] %s @ bit %u-%u (%.1f%%) %s\n",
                          i,
                          uft_mdec_provenance_name(prov->type),
                          prov->bit_offset,
                          prov->bit_offset + prov->bit_length,
                          prov->confidence,
                          prov->note);
        
        if (written < 0) return UFT_MDEC_ERR_IO;
        total += written;
        if ((size_t)total >= buffer_size - 1) break;
    }
    
    return total;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * MULTI-REVOLUTION SUPPORT
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_mdec_merge_revolutions(uft_mdec_session_t *session,
                               uint8_t track,
                               uint8_t head)
{
    if (!session) return UFT_MDEC_ERR_NULL;
    
    /* Find track */
    uft_mdec_track_t *trk = find_or_create_track(session, track, head);
    if (!trk) return UFT_MDEC_ERR_NOT_FOUND;
    
    int merged = 0;
    
    for (uint32_t s = 0; s < trk->sector_count; s++) {
        uft_mdec_sector_t *sec = &trk->sectors[s];
        
        /* Group candidates by data CRC (same data = same interpretation) */
        /* Merge confidence from multiple revolutions of same data */
        
        for (uint32_t i = 0; i < sec->count; i++) {
            for (uint32_t j = i + 1; j < sec->count; j++) {
                if (sec->candidates[i].data_crc == sec->candidates[j].data_crc &&
                    sec->candidates[i].data_size == sec->candidates[j].data_size) {
                    /* Same data - merge confidence (weighted average) */
                    float weight_i = 0.5f + (sec->candidates[i].revolution * 0.1f);
                    float weight_j = 0.5f + (sec->candidates[j].revolution * 0.1f);
                    float total_weight = weight_i + weight_j;
                    
                    sec->candidates[i].confidence = 
                        (sec->candidates[i].confidence * weight_i +
                         sec->candidates[j].confidence * weight_j) / total_weight;
                    
                    /* Mark j for removal (shift array) */
                    for (uint32_t k = j; k < sec->count - 1; k++) {
                        sec->candidates[k] = sec->candidates[k + 1];
                    }
                    sec->count--;
                    j--;  /* Re-check this index */
                    merged++;
                }
            }
        }
        
        sort_candidates(sec);
    }
    
    session->modified_us = get_timestamp_us();
    return merged;
}

int uft_mdec_calculate_consensus(const uft_mdec_session_t *session,
                                 uint8_t track,
                                 uint8_t head,
                                 uint8_t sector,
                                 uint8_t *consensus,
                                 size_t size,
                                 float *confidence)
{
    if (!session || !consensus || !confidence) return UFT_MDEC_ERR_NULL;
    
    const uft_mdec_sector_t *sec;
    int ret = uft_mdec_get_sector(session, track, head, sector, &sec);
    if (ret != UFT_MDEC_OK) return ret;
    
    if (sec->count == 0) return UFT_MDEC_ERR_NO_CANDIDATES;
    
    /* Simple consensus: byte-by-byte majority vote */
    uint32_t data_size = sec->candidates[0].data_size;
    if (data_size > size) data_size = (uint32_t)size;
    
    float total_conf = 0.0f;
    
    for (uint32_t byte_idx = 0; byte_idx < data_size; byte_idx++) {
        /* Count votes for each byte value */
        uint32_t votes[256] = {0};
        float weights[256] = {0.0f};
        
        for (uint32_t c = 0; c < sec->count; c++) {
            if (byte_idx < sec->candidates[c].data_size) {
                uint8_t val = sec->candidates[c].data[byte_idx];
                votes[val]++;
                weights[val] += sec->candidates[c].confidence;
            }
        }
        
        /* Find winner */
        uint8_t winner = 0;
        float max_weight = 0.0f;
        for (int v = 0; v < 256; v++) {
            if (weights[v] > max_weight) {
                max_weight = weights[v];
                winner = (uint8_t)v;
            }
        }
        
        consensus[byte_idx] = winner;
        total_conf += max_weight / sec->count;
    }
    
    *confidence = total_conf / data_size;
    
    return UFT_MDEC_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * FORENSIC EXPORT
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_mdec_export_json(const uft_mdec_session_t *session, FILE *fp)
{
    if (!session || !fp) return UFT_MDEC_ERR_NULL;
    
    fprintf(fp, "{\n");
    fprintf(fp, "  \"version\": \"%d.%d.%d\",\n",
            UFT_MDEC_VERSION_MAJOR, UFT_MDEC_VERSION_MINOR, UFT_MDEC_VERSION_PATCH);
    fprintf(fp, "  \"session_id\": \"%llu\",\n", (unsigned long long)session->session_id);
    fprintf(fp, "  \"source_file\": \"%s\",\n", session->source_file);
    fprintf(fp, "  \"statistics\": {\n");
    fprintf(fp, "    \"total_tracks\": %u,\n", session->track_count);
    fprintf(fp, "    \"total_sectors\": %llu,\n", (unsigned long long)session->total_sectors);
    fprintf(fp, "    \"total_candidates\": %llu,\n", (unsigned long long)session->total_candidates);
    fprintf(fp, "    \"resolved_sectors\": %llu,\n", (unsigned long long)session->resolved_sectors);
    fprintf(fp, "    \"ambiguous_sectors\": %llu,\n", (unsigned long long)session->ambiguous_sectors);
    fprintf(fp, "    \"overall_confidence\": %.2f\n", session->overall_confidence);
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"tracks\": [\n");
    
    for (uint32_t t = 0; t < session->track_count; t++) {
        const uft_mdec_track_t *trk = &session->tracks[t];
        
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"track\": %u,\n", trk->track);
        fprintf(fp, "      \"head\": %u,\n", trk->head);
        fprintf(fp, "      \"sector_count\": %u,\n", trk->sector_count);
        fprintf(fp, "      \"sectors\": [\n");
        
        for (uint32_t s = 0; s < trk->sector_count; s++) {
            const uft_mdec_sector_t *sec = &trk->sectors[s];
            
            fprintf(fp, "        {\n");
            fprintf(fp, "          \"sector\": %u,\n", sec->sector);
            fprintf(fp, "          \"status\": \"%s\",\n", uft_mdec_status_name(sec->status));
            fprintf(fp, "          \"candidate_count\": %u,\n", sec->count);
            fprintf(fp, "          \"max_confidence\": %.2f,\n", sec->max_confidence);
            fprintf(fp, "          \"confidence_spread\": %.2f,\n", sec->confidence_spread);
            fprintf(fp, "          \"resolved\": %s,\n", sec->resolved ? "true" : "false");
            fprintf(fp, "          \"selected_index\": %d,\n", sec->selected_index);
            fprintf(fp, "          \"candidates\": [\n");
            
            for (uint32_t c = 0; c < sec->count; c++) {
                const uft_mdec_candidate_t *cand = &sec->candidates[c];
                
                fprintf(fp, "            {\n");
                fprintf(fp, "              \"id\": %u,\n", cand->id);
                fprintf(fp, "              \"confidence\": %.2f,\n", cand->confidence);
                fprintf(fp, "              \"crc_valid\": %s,\n", cand->crc_valid ? "true" : "false");
                fprintf(fp, "              \"data_size\": %u,\n", cand->data_size);
                fprintf(fp, "              \"data_crc\": \"0x%08X\",\n", cand->data_crc);
                fprintf(fp, "              \"encoding\": \"%s\",\n", uft_mdec_encoding_name(cand->encoding));
                fprintf(fp, "              \"ambiguous_regions\": %u,\n", cand->ambiguous_count);
                fprintf(fp, "              \"errors_corrected\": %u\n", cand->errors_corrected);
                fprintf(fp, "            }%s\n", c < sec->count - 1 ? "," : "");
            }
            
            fprintf(fp, "          ]\n");
            fprintf(fp, "        }%s\n", s < trk->sector_count - 1 ? "," : "");
        }
        
        fprintf(fp, "      ]\n");
        fprintf(fp, "    }%s\n", t < session->track_count - 1 ? "," : "");
    }
    
    fprintf(fp, "  ]\n");
    fprintf(fp, "}\n");
    
    return UFT_MDEC_OK;
}

int uft_mdec_export_markdown(const uft_mdec_session_t *session, FILE *fp)
{
    if (!session || !fp) return UFT_MDEC_ERR_NULL;
    
    fprintf(fp, "# UFT Multi-Decoder Report\n\n");
    fprintf(fp, "## Session Information\n\n");
    fprintf(fp, "- **Session ID:** %llu\n", (unsigned long long)session->session_id);
    fprintf(fp, "- **Source File:** %s\n", session->source_file);
    fprintf(fp, "- **Total Tracks:** %u\n", session->track_count);
    fprintf(fp, "- **Total Candidates:** %llu\n", (unsigned long long)session->total_candidates);
    fprintf(fp, "- **Resolved Sectors:** %llu\n", (unsigned long long)session->resolved_sectors);
    fprintf(fp, "- **Ambiguous Sectors:** %llu\n\n", (unsigned long long)session->ambiguous_sectors);
    
    fprintf(fp, "## Track Analysis\n\n");
    
    for (uint32_t t = 0; t < session->track_count; t++) {
        const uft_mdec_track_t *trk = &session->tracks[t];
        
        fprintf(fp, "### Track %u, Head %u\n\n", trk->track, trk->head);
        fprintf(fp, "| Sector | Status | Candidates | Best Conf | Spread | CRC Valid |\n");
        fprintf(fp, "|--------|--------|------------|-----------|--------|----------|\n");
        
        for (uint32_t s = 0; s < trk->sector_count; s++) {
            const uft_mdec_sector_t *sec = &trk->sectors[s];
            const char *crc_str = (sec->count > 0 && sec->candidates[0].crc_valid) ? "✓" : "✗";
            
            fprintf(fp, "| %u | %s | %u | %.1f%% | %.1f%% | %s |\n",
                    sec->sector,
                    uft_mdec_status_icon(sec->status),
                    sec->count,
                    sec->max_confidence,
                    sec->confidence_spread,
                    crc_str);
        }
        fprintf(fp, "\n");
    }
    
    /* Ambiguous sectors detail */
    fprintf(fp, "## Ambiguous Sectors\n\n");
    
    bool found_amb = false;
    for (uint32_t t = 0; t < session->track_count; t++) {
        const uft_mdec_track_t *trk = &session->tracks[t];
        
        for (uint32_t s = 0; s < trk->sector_count; s++) {
            const uft_mdec_sector_t *sec = &trk->sectors[s];
            
            if (sec->status == UFT_MDEC_STATUS_AMBIGUOUS || 
                (!sec->resolved && sec->count > 1)) {
                found_amb = true;
                
                fprintf(fp, "### Track %u, Sector %u\n\n", trk->track, sec->sector);
                fprintf(fp, "**Confidence spread:** %.1f%% (requires manual review)\n\n",
                        sec->confidence_spread);
                
                fprintf(fp, "| Candidate | Confidence | CRC | Ambig Regions |\n");
                fprintf(fp, "|-----------|------------|-----|---------------|\n");
                
                for (uint32_t c = 0; c < sec->count && c < 5; c++) {
                    const uft_mdec_candidate_t *cand = &sec->candidates[c];
                    fprintf(fp, "| #%u | %.1f%% | %s | %u |\n",
                            cand->id,
                            cand->confidence,
                            cand->crc_valid ? "Valid" : "Invalid",
                            cand->ambiguous_count);
                }
                fprintf(fp, "\n");
            }
        }
    }
    
    if (!found_amb) {
        fprintf(fp, "_No ambiguous sectors found._\n\n");
    }
    
    fprintf(fp, "---\n");
    fprintf(fp, "_Report generated by UFT Multi-Decoder v%d.%d.%d_\n",
            UFT_MDEC_VERSION_MAJOR, UFT_MDEC_VERSION_MINOR, UFT_MDEC_VERSION_PATCH);
    
    return UFT_MDEC_OK;
}

int uft_mdec_export_sector_json(const uft_mdec_sector_t *sector, FILE *fp)
{
    if (!sector || !fp) return UFT_MDEC_ERR_NULL;
    
    fprintf(fp, "{\n");
    fprintf(fp, "  \"track\": %u,\n", sector->track);
    fprintf(fp, "  \"head\": %u,\n", sector->head);
    fprintf(fp, "  \"sector\": %u,\n", sector->sector);
    fprintf(fp, "  \"status\": \"%s\",\n", uft_mdec_status_name(sector->status));
    fprintf(fp, "  \"candidate_count\": %u,\n", sector->count);
    fprintf(fp, "  \"max_confidence\": %.2f,\n", sector->max_confidence);
    fprintf(fp, "  \"candidates\": [\n");
    
    for (uint32_t c = 0; c < sector->count; c++) {
        const uft_mdec_candidate_t *cand = &sector->candidates[c];
        
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"id\": %u,\n", cand->id);
        fprintf(fp, "      \"confidence\": %.2f,\n", cand->confidence);
        fprintf(fp, "      \"crc_valid\": %s,\n", cand->crc_valid ? "true" : "false");
        fprintf(fp, "      \"header_valid\": %s,\n", cand->header_valid ? "true" : "false");
        fprintf(fp, "      \"data_size\": %u,\n", cand->data_size);
        fprintf(fp, "      \"data_crc\": \"0x%08X\",\n", cand->data_crc);
        fprintf(fp, "      \"encoding\": \"%s\",\n", uft_mdec_encoding_name(cand->encoding));
        fprintf(fp, "      \"revolution\": %u,\n", cand->revolution);
        fprintf(fp, "      \"ambiguous_count\": %u,\n", cand->ambiguous_count);
        fprintf(fp, "      \"errors_corrected\": %u\n", cand->errors_corrected);
        fprintf(fp, "    }%s\n", c < sector->count - 1 ? "," : "");
    }
    
    fprintf(fp, "  ]\n");
    fprintf(fp, "}\n");
    
    return UFT_MDEC_OK;
}

int uft_mdec_get_statistics(const uft_mdec_session_t *session,
                            uft_mdec_statistics_t *stats)
{
    if (!session || !stats) return UFT_MDEC_ERR_NULL;
    
    memset(stats, 0, sizeof(uft_mdec_statistics_t));
    
    for (uint32_t t = 0; t < session->track_count; t++) {
        const uft_mdec_track_t *trk = &session->tracks[t];
        
        for (uint32_t s = 0; s < trk->sector_count; s++) {
            const uft_mdec_sector_t *sec = &trk->sectors[s];
            
            for (uint32_t c = 0; c < sec->count; c++) {
                const uft_mdec_candidate_t *cand = &sec->candidates[c];
                
                /* Confidence bands */
                if (cand->confidence < 50.0f) stats->band_0_50++;
                else if (cand->confidence < 70.0f) stats->band_50_70++;
                else if (cand->confidence < 85.0f) stats->band_70_85++;
                else if (cand->confidence < 95.0f) stats->band_85_95++;
                else stats->band_95_100++;
                
                /* CRC */
                if (cand->crc_valid) stats->crc_valid_total++;
                if (cand->errors_corrected > 0) stats->crc_corrected++;
                
                /* Ambiguities */
                for (uint32_t a = 0; a < cand->ambiguous_count; a++) {
                    switch (cand->ambiguous[a].type) {
                        case UFT_MDEC_AMB_WEAK_BIT: stats->amb_weak_bits++; break;
                        case UFT_MDEC_AMB_TIMING: stats->amb_timing++; break;
                        case UFT_MDEC_AMB_SYNC_SLIP: stats->amb_sync++; break;
                        case UFT_MDEC_AMB_ENCODING: stats->amb_encoding++; break;
                        case UFT_MDEC_AMB_PROTECTION: stats->amb_protection++; break;
                        default: break;
                    }
                }
            }
            
            /* Resolution status */
            switch (sec->status) {
                case UFT_MDEC_STATUS_AUTO_RESOLVED: stats->auto_resolved++; break;
                case UFT_MDEC_STATUS_USER_RESOLVED: stats->user_resolved++; break;
                case UFT_MDEC_STATUS_HEURISTIC: stats->heuristic_resolved++; break;
                case UFT_MDEC_STATUS_FORCED: stats->forced_resolved++; break;
                case UFT_MDEC_STATUS_PENDING:
                case UFT_MDEC_STATUS_AMBIGUOUS:
                case UFT_MDEC_STATUS_FAILED:
                    stats->unresolved++; break;
            }
        }
    }
    
    return UFT_MDEC_OK;
}

void uft_mdec_print_summary(const uft_mdec_session_t *session)
{
    if (!session) return;
    
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║           UFT Multi-Decoder Session Summary                  ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ Session ID: %-48llu ║\n", (unsigned long long)session->session_id);
    printf("║ Source: %-52s ║\n", session->source_file[0] ? session->source_file : "(none)");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ Tracks:     %-10u  Sectors:     %-10llu              ║\n",
           session->track_count, (unsigned long long)session->total_sectors);
    printf("║ Candidates: %-10llu  Resolved:    %-10llu              ║\n",
           (unsigned long long)session->total_candidates,
           (unsigned long long)session->resolved_sectors);
    printf("║ Ambiguous:  %-10llu  Confidence:  %-10.1f%%             ║\n",
           (unsigned long long)session->ambiguous_sectors,
           session->overall_confidence);
    printf("║ Memory:     %-10zu bytes                                  ║\n",
           session->memory_used);
    printf("╚══════════════════════════════════════════════════════════════╝\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * GUI INTEGRATION
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_mdec_format_alternatives(const uft_mdec_sector_t *sector,
                                 char *buffer,
                                 size_t buffer_size)
{
    if (!sector || !buffer || buffer_size == 0) return UFT_MDEC_ERR_NULL;
    
    int written = 0;
    int total = 0;
    
    written = snprintf(buffer, buffer_size,
                      "Sector %u: %u candidates\n",
                      sector->sector, sector->count);
    if (written < 0) return UFT_MDEC_ERR_IO;
    total += written;
    
    for (uint32_t c = 0; c < sector->count && c < 5; c++) {
        const uft_mdec_candidate_t *cand = &sector->candidates[c];
        
        written = snprintf(buffer + total, buffer_size - (size_t)total,
                          "  [%u] %.1f%% %s %s\n",
                          c,
                          cand->confidence,
                          cand->crc_valid ? "CRC✓" : "CRC✗",
                          c == (uint32_t)sector->selected_index ? "← SELECTED" : "");
        
        if (written < 0) return UFT_MDEC_ERR_IO;
        total += written;
        if ((size_t)total >= buffer_size - 1) break;
    }
    
    return total;
}

uint32_t uft_mdec_confidence_color(float confidence)
{
    /* Returns RGBA color based on confidence */
    if (confidence >= 95.0f) return 0x00FF00FF;  /* Green */
    if (confidence >= 85.0f) return 0x80FF00FF;  /* Yellow-Green */
    if (confidence >= 70.0f) return 0xFFFF00FF;  /* Yellow */
    if (confidence >= 50.0f) return 0xFF8000FF;  /* Orange */
    return 0xFF0000FF;  /* Red */
}

const char *uft_mdec_status_icon(uft_mdec_status_t status)
{
    switch (status) {
        case UFT_MDEC_STATUS_PENDING:        return "⏳";
        case UFT_MDEC_STATUS_AUTO_RESOLVED:  return "✓";
        case UFT_MDEC_STATUS_USER_RESOLVED:  return "👤";
        case UFT_MDEC_STATUS_HEURISTIC:      return "🔮";
        case UFT_MDEC_STATUS_FORCED:         return "⚡";
        case UFT_MDEC_STATUS_FAILED:         return "✗";
        case UFT_MDEC_STATUS_AMBIGUOUS:      return "⚠";
        default:                             return "?";
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * UTILITY FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

const char *uft_mdec_error_string(uft_mdec_error_t error)
{
    switch (error) {
        case UFT_MDEC_OK:                    return "Success";
        case UFT_MDEC_ERR_NULL:              return "Null pointer";
        case UFT_MDEC_ERR_MEMORY:            return "Memory allocation failed";
        case UFT_MDEC_ERR_OVERFLOW:          return "Buffer overflow";
        case UFT_MDEC_ERR_INVALID_PARAM:     return "Invalid parameter";
        case UFT_MDEC_ERR_NO_CANDIDATES:     return "No candidates available";
        case UFT_MDEC_ERR_AMBIGUOUS:         return "Ambiguous - multiple valid candidates";
        case UFT_MDEC_ERR_RESOLUTION_FAILED: return "Resolution failed";
        case UFT_MDEC_ERR_IO:                return "I/O error";
        case UFT_MDEC_ERR_FORMAT:            return "Format error";
        case UFT_MDEC_ERR_CHECKSUM:          return "Checksum error";
        case UFT_MDEC_ERR_TIMEOUT:           return "Operation timeout";
        case UFT_MDEC_ERR_NOT_FOUND:         return "Not found";
        case UFT_MDEC_ERR_ALREADY_RESOLVED:  return "Already resolved";
        case UFT_MDEC_ERR_ENCODING:          return "Encoding error";
        case UFT_MDEC_ERR_SYNC:              return "Sync error";
        default:                             return "Unknown error";
    }
}

const char *uft_mdec_encoding_name(uft_mdec_encoding_t encoding)
{
    switch (encoding) {
        case UFT_MDEC_ENC_UNKNOWN:   return "Unknown";
        case UFT_MDEC_ENC_MFM:       return "MFM";
        case UFT_MDEC_ENC_GCR_CBM:   return "GCR (Commodore)";
        case UFT_MDEC_ENC_GCR_APPLE: return "GCR (Apple)";
        case UFT_MDEC_ENC_FM:        return "FM";
        case UFT_MDEC_ENC_M2FM:      return "M2FM";
        case UFT_MDEC_ENC_AMIGA:     return "Amiga MFM";
        case UFT_MDEC_ENC_RAW:       return "Raw";
        default:                     return "Unknown";
    }
}

const char *uft_mdec_ambiguity_name(uft_mdec_ambiguity_t type)
{
    switch (type) {
        case UFT_MDEC_AMB_NONE:          return "None";
        case UFT_MDEC_AMB_WEAK_BIT:      return "Weak Bit";
        case UFT_MDEC_AMB_TIMING:        return "Timing Uncertainty";
        case UFT_MDEC_AMB_SYNC_SLIP:     return "Sync Slip";
        case UFT_MDEC_AMB_ENCODING:      return "Encoding Ambiguity";
        case UFT_MDEC_AMB_CRC_COLLISION: return "CRC Collision";
        case UFT_MDEC_AMB_PROTECTION:    return "Copy Protection";
        case UFT_MDEC_AMB_DAMAGE:        return "Media Damage";
        case UFT_MDEC_AMB_PLL_DRIFT:     return "PLL Drift";
        default:                         return "Unknown";
    }
}

const char *uft_mdec_strategy_name(uft_mdec_strategy_t strategy)
{
    switch (strategy) {
        case UFT_MDEC_STRATEGY_HIGHEST_CONF: return "Highest Confidence";
        case UFT_MDEC_STRATEGY_MAJORITY:     return "Majority Vote";
        case UFT_MDEC_STRATEGY_CRC_PRIORITY: return "CRC Priority";
        case UFT_MDEC_STRATEGY_CONSERVATIVE: return "Conservative";
        case UFT_MDEC_STRATEGY_REFERENCE:    return "Reference Compare";
        case UFT_MDEC_STRATEGY_MANUAL:       return "Manual";
        default:                             return "Unknown";
    }
}

const char *uft_mdec_status_name(uft_mdec_status_t status)
{
    switch (status) {
        case UFT_MDEC_STATUS_PENDING:        return "Pending";
        case UFT_MDEC_STATUS_AUTO_RESOLVED:  return "Auto-Resolved";
        case UFT_MDEC_STATUS_USER_RESOLVED:  return "User-Resolved";
        case UFT_MDEC_STATUS_HEURISTIC:      return "Heuristic";
        case UFT_MDEC_STATUS_FORCED:         return "Forced";
        case UFT_MDEC_STATUS_FAILED:         return "Failed";
        case UFT_MDEC_STATUS_AMBIGUOUS:      return "Ambiguous";
        default:                             return "Unknown";
    }
}

const char *uft_mdec_provenance_name(uft_mdec_provenance_type_t type)
{
    switch (type) {
        case UFT_MDEC_PROV_DIRECT:        return "Direct Decode";
        case UFT_MDEC_PROV_MULTI_REV:     return "Multi-Revolution";
        case UFT_MDEC_PROV_CRC_CORRECTED: return "CRC Corrected";
        case UFT_MDEC_PROV_INTERPOLATED:  return "Interpolated";
        case UFT_MDEC_PROV_HEURISTIC:     return "Heuristic";
        case UFT_MDEC_PROV_USER_OVERRIDE: return "User Override";
        case UFT_MDEC_PROV_REFERENCE:     return "Reference Match";
        case UFT_MDEC_PROV_ECC:           return "ECC Reconstruction";
        default:                          return "Unknown";
    }
}
