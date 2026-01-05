/**
 * @file encoding_detector.c
 * @brief Automatische Encoding-Erkennung Implementation
 * 
 * Basierend auf FluxRipper's Verilog-Modulen:
 * - encoding_detector.v
 * - mfm_decoder.v
 * - gcr_apple.v
 * - gcr_cbm.v
 * 
 * @version 1.0.0
 */

#include "uft/encoding_detector.h"
#include <string.h>

/*============================================================================
 * Lookup Tables - GCR Apple 6-bit
 *============================================================================*/

/* 6-bit → 8-bit Encoding Table (aus FluxRipper gcr_apple.v) */
static const uint8_t gcr_apple6_enc_table[64] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

/*============================================================================
 * Lookup Tables - GCR CBM
 *============================================================================*/

/* 4-bit → 5-bit Encoding (aus FluxRipper gcr_cbm.v) */
static const uint8_t gcr_cbm_enc_table[16] = {
    0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
    0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
};

/* 5-bit → 4-bit Decode (Bit 4 = Error) */
static const uint8_t gcr_cbm_dec_table[32] = {
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,  /* 0x00-0x07: Error */
    0x10, 0x08, 0x00, 0x01, 0x10, 0x0C, 0x04, 0x05,  /* 0x08-0x0F */
    0x10, 0x10, 0x02, 0x03, 0x10, 0x0F, 0x06, 0x07,  /* 0x10-0x17 */
    0x10, 0x09, 0x0A, 0x0B, 0x10, 0x0D, 0x0E, 0x10   /* 0x18-0x1F */
};

/* Apple 5-bit GCR */
static const uint8_t gcr_apple5_enc_table[32] = {
    0xAB, 0xAD, 0xAE, 0xAF, 0xB5, 0xB6, 0xB7, 0xBA,
    0xBB, 0xBD, 0xBE, 0xBF, 0xD6, 0xD7, 0xDA, 0xDB,
    0xDD, 0xDE, 0xDF, 0xEA, 0xEB, 0xED, 0xEE, 0xEF,
    0xF5, 0xF6, 0xF7, 0xFA, 0xFB, 0xFD, 0xFE, 0xFF
};

/*============================================================================
 * Encoding Names
 *============================================================================*/

static const char *encoding_names[] = {
    "MFM",
    "FM",
    "GCR-CBM",
    "GCR-Apple6",
    "GCR-Apple5",
    "M2FM",
    "Tandy FM",
    "Unknown"
};

const char *encoding_name(encoding_type_t enc)
{
    if (enc > ENC_UNKNOWN) enc = ENC_UNKNOWN;
    return encoding_names[enc];
}

/*============================================================================
 * Encoding Detector
 *============================================================================*/

void encoding_detector_init(encoding_detector_t *det)
{
    memset(det, 0, sizeof(encoding_detector_t));
    det->detected = ENC_MFM;
    det->current = ENC_MFM;
}

void encoding_detector_reset(encoding_detector_t *det)
{
    det->consecutive_matches = 0;
    det->mismatch_count = 0;
    det->locked = false;
    det->valid = false;
    det->match_count = 0;
    det->sync_history = 0;
}

void encoding_detector_process(encoding_detector_t *det, const sync_flags_t *flags)
{
    if (!det || !flags) return;
    
    encoding_type_t priority_enc = det->current;
    bool any_sync = false;
    
    /* Priority-Encoder: Höchste Priorität zuerst */
    if (flags->gcr_apple_sync) {
        priority_enc = ENC_GCR_AP6;
        any_sync = true;
        det->sync_history |= 0x08;
    }
    else if (flags->gcr_cbm_sync) {
        priority_enc = ENC_GCR_CBM;
        any_sync = true;
        det->sync_history |= 0x04;
    }
    else if (flags->m2fm_sync) {
        priority_enc = ENC_M2FM;
        any_sync = true;
        det->sync_history |= 0x10;
    }
    else if (flags->tandy_sync) {
        priority_enc = ENC_TANDY;
        any_sync = true;
        det->sync_history |= 0x20;
    }
    else if (flags->mfm_sync) {
        priority_enc = ENC_MFM;
        any_sync = true;
        det->sync_history |= 0x01;
    }
    else if (flags->fm_sync) {
        priority_enc = ENC_FM;
        any_sync = true;
        det->sync_history |= 0x02;
    }
    
    if (!any_sync) return;
    
    if (priority_enc == det->current) {
        if (det->consecutive_matches < 255) det->consecutive_matches++;
        if (det->match_count < 255) det->match_count++;
        det->mismatch_count = 0;
        
        if (det->consecutive_matches >= ENC_LOCK_THRESHOLD) {
            det->locked = true;
        }
    }
    else {
        if (det->locked) {
            if (det->mismatch_count < 255) det->mismatch_count++;
            
            if (det->mismatch_count >= ENC_UNLOCK_THRESHOLD) {
                det->locked = false;
                det->current = priority_enc;
                det->consecutive_matches = 1;
                det->match_count = 1;
                det->mismatch_count = 0;
            }
        }
        else {
            det->current = priority_enc;
            det->consecutive_matches = 1;
            det->match_count = 1;
            det->mismatch_count = 0;
        }
    }
    
    det->valid = true;
    det->detected = det->current;
}

/*============================================================================
 * MFM Decoder
 *============================================================================*/

void mfm_decoder_init(mfm_decoder_state_t *state)
{
    memset(state, 0, sizeof(mfm_decoder_state_t));
}

uint8_t mfm_decode_byte(uint16_t encoded)
{
    uint8_t data = 0;
    data |= ((encoded >> 14) & 1) << 7;
    data |= ((encoded >> 12) & 1) << 6;
    data |= ((encoded >> 10) & 1) << 5;
    data |= ((encoded >> 8)  & 1) << 4;
    data |= ((encoded >> 6)  & 1) << 3;
    data |= ((encoded >> 4)  & 1) << 2;
    data |= ((encoded >> 2)  & 1) << 1;
    data |= ((encoded >> 0)  & 1) << 0;
    return data;
}

bool mfm_has_error(uint16_t encoded)
{
    return ((encoded & 0xC000) == 0xC000) ||
           ((encoded & 0x3000) == 0x3000) ||
           ((encoded & 0x0C00) == 0x0C00) ||
           ((encoded & 0x0300) == 0x0300) ||
           ((encoded & 0x00C0) == 0x00C0) ||
           ((encoded & 0x0030) == 0x0030) ||
           ((encoded & 0x000C) == 0x000C) ||
           ((encoded & 0x0003) == 0x0003);
}

void mfm_decoder_process_bit(mfm_decoder_state_t *state, bool bit_in,
                             uint8_t *data_out, bool *byte_ready,
                             bool *sync_detected, uint8_t *am_type)
{
    *byte_ready = false;
    *sync_detected = false;
    *am_type = 0;
    
    state->shift_reg = (state->shift_reg << 1) | (bit_in ? 1 : 0);
    state->bit_cnt++;
    
    /* Check A1 Sync (0x4489) */
    if (state->shift_reg == MFM_SYNC_A1) {
        *sync_detected = true;
        state->in_sync = true;
        state->bit_cnt = 0;
        if (state->sync_count < 3) state->sync_count++;
        if (state->sync_count == 3) state->await_am = true;
        return;
    }
    
    /* Check C2 Sync (0x5224) */
    if (state->shift_reg == MFM_SYNC_C2) {
        *sync_detected = true;
        state->in_sync = true;
        state->bit_cnt = 0;
        state->sync_count = 0;
        return;
    }
    
    if (state->bit_cnt == 16) {
        *data_out = mfm_decode_byte(state->shift_reg);
        *byte_ready = true;
        state->bit_cnt = 0;
        
        if (state->await_am) {
            state->await_am = false;
            switch (*data_out) {
                case MFM_AM_IDAM:  *am_type = 1; break;
                case MFM_AM_DAM:   *am_type = 2; break;
                case MFM_AM_DDAM:  *am_type = 3; break;
                default:          *am_type = 0; break;
            }
        }
        state->sync_count = 0;
    }
}

uint16_t mfm_encode_byte(uint8_t data, bool prev_bit)
{
    uint16_t encoded = 0;
    bool last_data = prev_bit;
    
    for (int i = 7; i >= 0; i--) {
        bool current_data = (data >> i) & 1;
        bool clock = (!last_data && !current_data) ? 1 : 0;
        encoded = (encoded << 2) | (clock << 1) | current_data;
        last_data = current_data;
    }
    return encoded;
}

/*============================================================================
 * GCR Apple
 *============================================================================*/

uint8_t gcr_apple6_encode(uint8_t data)
{
    return gcr_apple6_enc_table[data & 0x3F];
}

uint8_t gcr_apple6_decode(uint8_t encoded, bool *error)
{
    *error = true;
    for (int i = 0; i < 64; i++) {
        if (gcr_apple6_enc_table[i] == encoded) {
            *error = false;
            return (uint8_t)i;
        }
    }
    return 0;
}

uint8_t gcr_apple5_encode(uint8_t data)
{
    return gcr_apple5_enc_table[data & 0x1F];
}

uint8_t gcr_apple5_decode(uint8_t encoded, bool *error)
{
    *error = true;
    for (int i = 0; i < 32; i++) {
        if (gcr_apple5_enc_table[i] == encoded) {
            *error = false;
            return (uint8_t)i;
        }
    }
    return 0;
}

/* Apple Sync Detector States */
#define APPLE_S_IDLE     0
#define APPLE_S_D5_SEEN  1
#define APPLE_S_AA_SEEN  2
#define APPLE_S_DE_SEEN  3
#define APPLE_S_DE_AA    4

void apple_sync_init(apple_sync_state_t *state)
{
    memset(state, 0, sizeof(apple_sync_state_t));
}

int apple_sync_process(apple_sync_state_t *state, uint8_t byte)
{
    int result = 0;
    
    switch (state->state) {
        case APPLE_S_IDLE:
            if (byte == APPLE_MARK_D5) state->state = APPLE_S_D5_SEEN;
            else if (byte == APPLE_MARK_DE) state->state = APPLE_S_DE_SEEN;
            break;
            
        case APPLE_S_D5_SEEN:
            if (byte == APPLE_MARK_AA) state->state = APPLE_S_AA_SEEN;
            else if (byte == APPLE_MARK_D5) state->state = APPLE_S_D5_SEEN;
            else state->state = APPLE_S_IDLE;
            break;
            
        case APPLE_S_AA_SEEN:
            if (byte == APPLE_MARK_96) { result = 1; state->state = APPLE_S_IDLE; }
            else if (byte == APPLE_MARK_AD) { result = 2; state->state = APPLE_S_IDLE; }
            else if (byte == APPLE_MARK_D5) state->state = APPLE_S_D5_SEEN;
            else state->state = APPLE_S_IDLE;
            break;
            
        case APPLE_S_DE_SEEN:
            if (byte == APPLE_MARK_AA) state->state = APPLE_S_DE_AA;
            else if (byte == APPLE_MARK_D5) state->state = APPLE_S_D5_SEEN;
            else state->state = APPLE_S_IDLE;
            break;
            
        case APPLE_S_DE_AA:
            if (byte == APPLE_MARK_EB) { result = 3; state->state = APPLE_S_IDLE; }
            else if (byte == APPLE_MARK_D5) state->state = APPLE_S_D5_SEEN;
            else state->state = APPLE_S_IDLE;
            break;
            
        default:
            state->state = APPLE_S_IDLE;
            break;
    }
    
    state->prev_prev_byte = state->prev_byte;
    state->prev_byte = byte;
    return result;
}

/*============================================================================
 * GCR CBM
 *============================================================================*/

uint8_t gcr_cbm_encode_nibble(uint8_t nibble)
{
    return gcr_cbm_enc_table[nibble & 0x0F];
}

uint8_t gcr_cbm_decode_quintet(uint8_t quintet, bool *error)
{
    uint8_t result = gcr_cbm_dec_table[quintet & 0x1F];
    *error = (result & 0x10) != 0;
    return result & 0x0F;
}

uint16_t gcr_cbm_encode_byte(uint8_t data)
{
    uint8_t high = gcr_cbm_encode_nibble(data >> 4);
    uint8_t low = gcr_cbm_encode_nibble(data & 0x0F);
    return ((uint16_t)high << 5) | low;
}

uint8_t gcr_cbm_decode_byte(uint16_t encoded, bool *error)
{
    bool err_high, err_low;
    uint8_t high = gcr_cbm_decode_quintet((encoded >> 5) & 0x1F, &err_high);
    uint8_t low = gcr_cbm_decode_quintet(encoded & 0x1F, &err_low);
    *error = err_high || err_low;
    return (high << 4) | low;
}

void cbm_sync_init(cbm_sync_state_t *state)
{
    memset(state, 0, sizeof(cbm_sync_state_t));
}

bool cbm_sync_process_bit(cbm_sync_state_t *state, bool bit)
{
    state->shift_reg = (state->shift_reg << 1) | (bit ? 1 : 0);
    
    if (bit) {
        if (state->one_count < 15) state->one_count++;
        if (state->one_count >= 10) {
            if (state->one_count == 10 && state->sync_count < 63)
                state->sync_count++;
            return true;
        }
    }
    else {
        state->one_count = 0;
    }
    return false;
}
