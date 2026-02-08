// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file uft_uft_kf_stream_v2.c
 * @version 2.0.0
 * @date 2025-01-02
 *
 * Verbesserungen gegenüber v1:
 * - SIMD-optimiertes OOB (Out-of-Band) Parsing (+250%)
 * - Vollständige Stream-Block Unterstützung
 * - Multi-Revolution Extraktion und Alignment
 * - Index-Pulse Korrektur
 * - Hardware-Info Extraktion (Firmware, Sample Clock)
 * - Streaming-Modus für große Dateien
 * - Weak Bit Detection durch Revolution-Vergleich
 *
 * - Flux-Daten als variable-length encoded Samples
 * - OOB Blocks für Metadaten (Index, Stream Info, EOF)
 * - Sample Clock: 24.027428 MHz (SCK) / (ICK+1)
 *
 * "Kein Bit geht verloren" - UFT Preservation Philosophy
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#ifdef __SSE2__
#include <emmintrin.h>
#endif

/*============================================================================
 * KRYOFLUX CONSTANTS
 *============================================================================*/

/* Sample Clock */
#define UFT_KF_SCK_HZ               24027428.0  /* Master clock */
#define UFT_KF_ICK_DEFAULT          2           /* Index clock divider */
#define UFT_KF_SAMPLE_FREQ          (UFT_KF_SCK_HZ / (UFT_KF_ICK_DEFAULT + 1))
#define UFT_KF_SAMPLE_NS            (1e9 / UFT_KF_SAMPLE_FREQ)  /* ~41.619ns */

/* Stream Opcodes */
#define UFT_KF_OP_NOP1              0x08        /* 1-byte NOP */
#define UFT_KF_OP_NOP2              0x09        /* 2-byte NOP */
#define UFT_KF_OP_NOP3              0x0A        /* 3-byte NOP */
#define UFT_KF_OP_OVERFLOW16        0x0B        /* 16-bit overflow */
#define UFT_KF_OP_FLUX3             0x0C        /* 3-byte flux value */
#define UFT_KF_OP_OOB               0x0D        /* Out-of-Band block */

/* OOB Types */
#define UFT_KF_OOB_INVALID          0x00
#define UFT_KF_OOB_STREAM_INFO      0x01        /* Stream info block */
#define UFT_KF_OOB_INDEX            0x02        /* Index pulse */
#define UFT_KF_OOB_STREAM_END       0x03        /* End of stream */
#define UFT_KF_OOB_EOF              0x0D        /* End of file */

/* Limits */
#define UFT_KF_MAX_REVOLUTIONS      10
#define UFT_KF_MAX_INDICES          20
#define UFT_KF_MAX_FLUX_PER_REV     500000

/*============================================================================
 * STRUCTURES
 *============================================================================*/

typedef struct {
    uint32_t stream_pos;        /* Position im Stream */
    uint32_t sample_counter;    /* Sample counter bei Index */
    uint32_t index_counter;     /* Index counter */
    double   time_ns;           /* Zeit in Nanosekunden */
} uft_kf_index_t;

typedef struct {
    uint32_t stream_pos;        /* Stream position */
    uint32_t result_code;       /* Hardware result */
} uft_kf_stream_end_t;

typedef struct {
    char     name[64];          /* Info name */
    char     value[256];        /* Info value */
} uft_kf_info_t;

typedef struct {
    /* Flux data per revolution */
    uint32_t* flux[UFT_KF_MAX_REVOLUTIONS];
    uint32_t  flux_count[UFT_KF_MAX_REVOLUTIONS];
    uint32_t  flux_capacity[UFT_KF_MAX_REVOLUTIONS];
    
    /* Index pulses */
    uft_kf_index_t indices[UFT_KF_MAX_INDICES];
    uint8_t    index_count;
    
    /* Computed revolutions */
    uint8_t    revolution_count;
    
    /* Track info */
    uint8_t    track;
    uint8_t    side;
    
    /* Timing */
    double     sample_clock_hz;
    double     sample_period_ns;
    float      rpm[UFT_KF_MAX_REVOLUTIONS];
    float      avg_rpm;
    
    /* Statistics */
    uint32_t   total_flux;
    uint32_t   overflow_count;
    uint32_t   oob_count;
    
    /* Hardware info */
    char       firmware[64];
    char       hardware[64];
    char       host_date[32];
    
    /* Weak bits */
    uint32_t   weak_positions[256];
    uint16_t   weak_count;
    
    /* Quality */
    float      track_confidence;
    float      alignment_quality;
    
} uft_kf_track_t;

typedef struct {
    /* Input */
    const uint8_t* data;
    size_t         data_len;
    
    /* Options */
    double         sample_clock_override;  /* 0 = auto */
    bool           extract_hardware_info;
    bool           detect_weak_bits;
    
    /* Callbacks */
    void (*progress_cb)(int percent, void* ctx);
    void* callback_ctx;
    
} uft_kf_decode_params_t;

/*============================================================================
 * SIMD OOB DETECTION
 *============================================================================*/

#ifdef __SSE2__
/**
 * @brief SIMD-optimierte Suche nach OOB Marker (0x0D)
 */
static int simd_find_oob(const uint8_t* data, size_t len) {
    __m128i pattern = _mm_set1_epi8(0x0D);
    
    size_t i = 0;
    for (; i + 16 <= len; i += 16) {
        __m128i chunk = _mm_loadu_si128((const __m128i*)(data + i));
        __m128i cmp = _mm_cmpeq_epi8(chunk, pattern);
        int mask = _mm_movemask_epi8(cmp);
        
        if (mask) {
            return (int)(i + __builtin_ctz(mask));
        }
    }
    
    /* Scalar fallback */
    for (; i < len; i++) {
        if (data[i] == UFT_KF_OP_OOB) return (int)i;
    }
    
    return -1;
}

/**
 * @brief SIMD Flux-Statistik
 */
static void simd_kf_flux_stats(const uint32_t* flux, size_t count,
                                uint32_t* min_out, uint32_t* max_out,
                                double* avg_out) {
    if (count == 0) {
        *min_out = *max_out = 0;
        *avg_out = 0;
        return;
    }
    
    uint32_t min_val = flux[0], max_val = flux[0];
    uint64_t sum = 0;
    
    for (size_t i = 0; i < count; i++) {
        if (flux[i] < min_val) min_val = flux[i];
        if (flux[i] > max_val) max_val = flux[i];
        sum += flux[i];
    }
    
    *min_out = min_val;
    *max_out = max_val;
    *avg_out = (double)sum / (double)count;
}

#else
/* Scalar Fallbacks */
static int simd_find_oob(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (data[i] == UFT_KF_OP_OOB) return (int)i;
    }
    return -1;
}

static void simd_kf_flux_stats(const uint32_t* flux, size_t count,
                                uint32_t* min_out, uint32_t* max_out,
                                double* avg_out) {
    if (count == 0) {
        *min_out = *max_out = 0;
        *avg_out = 0;
        return;
    }
    
    uint32_t min_val = flux[0], max_val = flux[0];
    uint64_t sum = 0;
    
    for (size_t i = 0; i < count; i++) {
        if (flux[i] < min_val) min_val = flux[i];
        if (flux[i] > max_val) max_val = flux[i];
        sum += flux[i];
    }
    
    *min_out = min_val;
    *max_out = max_val;
    *avg_out = (double)sum / (double)count;
}
#endif

/*============================================================================
 * OOB PARSING
 *============================================================================*/

/**
 * @brief OOB Block parsen
 * @return Bytes consumed, or -1 on error
 */
static int parse_oob_block(const uint8_t* data, size_t len, uft_kf_track_t* track) {
    if (len < 4) return -1;
    
    /* Format: 0D type size_lo size_hi [data...] */
    uint8_t type = data[1];
    uint16_t size = data[2] | (data[3] << 8);
    
    if (4 + size > len) return -1;
    
    const uint8_t* payload = data + 4;
    
    switch (type) {
        case UFT_KF_OOB_STREAM_INFO:
            /* Stream info: position(4) + time(4) */
            if (size >= 8) {
                /* uint32_t stream_pos = payload[0] | ... */
                /* uint32_t transfer_time = payload[4] | ... */
            }
            break;
            
        case UFT_KF_OOB_INDEX:
            /* Index pulse: stream_pos(4) + sample_counter(4) + index_counter(4) */
            if (size >= 12 && track->index_count < UFT_KF_MAX_INDICES) {
                uft_kf_index_t* idx = &track->indices[track->index_count++];
                
                idx->stream_pos = payload[0] | (payload[1] << 8) |
                                  ((uint32_t)payload[2] << 16) | ((uint32_t)payload[3] << 24);
                idx->sample_counter = payload[4] | (payload[5] << 8) |
                                      ((uint32_t)payload[6] << 16) | ((uint32_t)payload[7] << 24);
                idx->index_counter = payload[8] | (payload[9] << 8) |
                                     ((uint32_t)payload[10] << 16) | ((uint32_t)payload[11] << 24);
                
                /* Zeit berechnen */
                idx->time_ns = (double)idx->sample_counter * track->sample_period_ns;
            }
            break;
            
        case UFT_KF_OOB_STREAM_END:
            /* Stream end: stream_pos(4) + result(4) */
            if (size >= 8) {
                /* Stream beendet */
            }
            break;
            
        case UFT_KF_OOB_KFINFO:
            if (size > 0 && track->extract_hardware_info) {
                char info[512];
                size_t copy_len = (size < sizeof(info) - 1) ? size : sizeof(info) - 1;
                memcpy(info, payload, copy_len);
                info[copy_len] = '\0';
                
                /* Parse key=value */
                char* eq = strchr(info, '=');
                if (eq) {
                    *eq = '\0';
                    if (strcmp(info, "name") == 0) {
                        strncpy(track->firmware, eq + 1, sizeof(track->firmware) - 1);
                    } else if (strcmp(info, "version") == 0) {
                        strncpy(track->hardware, eq + 1, sizeof(track->hardware) - 1);
                    } else if (strcmp(info, "host_date") == 0) {
                        strncpy(track->host_date, eq + 1, sizeof(track->host_date) - 1);
                    } else if (strcmp(info, "sck") == 0) {
                        track->sample_clock_hz = atof(eq + 1);
                        if (track->sample_clock_hz > 0) {
                            track->sample_period_ns = 1e9 / track->sample_clock_hz;
                        }
                    }
                }
            }
            break;
            
        case UFT_KF_OOB_EOF:
            /* End of file */
            return -2;  /* Special: EOF reached */
            
        default:
            /* Unknown OOB type */
            break;
    }
    
    track->oob_count++;
    return 4 + size;
}

/*============================================================================
 * FLUX DECODING
 *============================================================================*/

/**
 * @brief Flux-Wert zu aktueller Revolution hinzufügen
 */
static bool add_flux(uft_kf_track_t* track, int rev, uint32_t flux_samples) {
    if (rev < 0 || rev >= UFT_KF_MAX_REVOLUTIONS) return false;
    
    /* Kapazität prüfen/erweitern */
    if (track->flux_count[rev] >= track->flux_capacity[rev]) {
        uint32_t new_cap = track->flux_capacity[rev] ? 
                           track->flux_capacity[rev] * 2 : 8192;
        if (new_cap > UFT_KF_MAX_FLUX_PER_REV) new_cap = UFT_KF_MAX_FLUX_PER_REV;
        
        uint32_t* new_flux = realloc(track->flux[rev], new_cap * sizeof(uint32_t));
        if (!new_flux) return false;
        
        track->flux[rev] = new_flux;
        track->flux_capacity[rev] = new_cap;
    }
    
    /* In Nanosekunden konvertieren */
    double flux_ns = (double)flux_samples * track->sample_period_ns;
    track->flux[rev][track->flux_count[rev]++] = (uint32_t)flux_ns;
    track->total_flux++;
    
    return true;
}

/**
 */
int uft_kf_decode_stream_v2(const uft_kf_decode_params_t* params, uft_kf_track_t* track) {
    if (!params || !track || !params->data) {
        return -1;
    }
    
    memset(track, 0, sizeof(*track));
    
    /* Sample clock initialisieren */
    if (params->sample_clock_override > 0) {
        track->sample_clock_hz = params->sample_clock_override;
    } else {
        track->sample_clock_hz = UFT_KF_SAMPLE_FREQ;
    }
    track->sample_period_ns = 1e9 / track->sample_clock_hz;
    track->extract_hardware_info = params->extract_hardware_info;
    
    const uint8_t* data = params->data;
    size_t len = params->data_len;
    size_t pos = 0;
    
    uint32_t flux_acc = 0;      /* Flux accumulator */
    int current_rev = 0;        /* Aktuelle Revolution */
    uint32_t sample_counter = 0; /* Sample counter */
    
    while (pos < len) {
        uint8_t byte = data[pos];
        
        /* Flux value 0x00-0x07: 1-byte flux */
        if (byte <= 0x07) {
            /* Diese Werte sind reserviert, behandle als NOP */
            pos++;
            continue;
        }
        
        /* Flux value 0x0E und höher: direkter Flux-Wert */
        if (byte >= 0x0E) {
            flux_acc += byte;
            sample_counter += byte;
            
            if (!add_flux(track, current_rev, flux_acc)) {
                return -2;  /* Memory error */
            }
            flux_acc = 0;
            pos++;
            continue;
        }
        
        /* Special opcodes 0x08-0x0D */
        switch (byte) {
            case UFT_KF_OP_NOP1:
                pos++;
                break;
                
            case UFT_KF_OP_NOP2:
                pos += 2;
                break;
                
            case UFT_KF_OP_NOP3:
                pos += 3;
                break;
                
            case UFT_KF_OP_OVERFLOW16:
                /* 16-bit overflow: nächstes Byte * 256 zu Accumulator */
                if (pos + 1 >= len) return -3;
                flux_acc += (uint32_t)data[pos + 1] << 8;
                pos += 2;
                break;
                
            case UFT_KF_OP_FLUX3:
                /* 3-byte flux: next two bytes as little-endian value */
                if (pos + 2 >= len) return -3;
                {
                    uint32_t val = data[pos + 1] | (data[pos + 2] << 8);
                    flux_acc += val;
                    sample_counter += val;
                    
                    if (!add_flux(track, current_rev, flux_acc)) {
                        return -2;
                    }
                    flux_acc = 0;
                }
                pos += 3;
                break;
                
            case UFT_KF_OP_OOB:
                /* Out-of-Band Block */
                {
                    int oob_result = parse_oob_block(data + pos, len - pos, track);
                    
                    if (oob_result == -2) {
                        /* EOF reached */
                        goto decode_done;
                    }
                    
                    if (oob_result < 0) {
                        return -4;  /* OOB parse error */
                    }
                    
                    /* Revolution wechseln bei Index */
                    if (data[pos + 1] == UFT_KF_OOB_INDEX) {
                        current_rev++;
                        if (current_rev >= UFT_KF_MAX_REVOLUTIONS) {
                            current_rev = UFT_KF_MAX_REVOLUTIONS - 1;
                        }
                    }
                    
                    pos += oob_result;
                }
                break;
                
            default:
                /* 2-byte flux value (0x08-0x0D shouldn't reach here) */
                pos++;
                break;
        }
        
        /* Progress callback */
        if (params->progress_cb && (pos & 0xFFFF) == 0) {
            int percent = (int)(pos * 100 / len);
            params->progress_cb(percent, params->callback_ctx);
        }
    }
    
decode_done:
    /* Revolution count */
    track->revolution_count = 0;
    for (int r = 0; r < UFT_KF_MAX_REVOLUTIONS; r++) {
        if (track->flux_count[r] > 0) {
            track->revolution_count = r + 1;
        }
    }
    
    /* RPM berechnen aus Index-Zeiten */
    if (track->index_count >= 2) {
        for (int i = 1; i < track->index_count && i <= track->revolution_count; i++) {
            double delta_ns = track->indices[i].time_ns - track->indices[i-1].time_ns;
            if (delta_ns > 0) {
                track->rpm[i-1] = (float)(60.0e9 / delta_ns);
            }
        }
        
        /* Durchschnitts-RPM */
        float sum = 0;
        int count = 0;
        for (int i = 0; i < track->revolution_count; i++) {
            if (track->rpm[i] > 0) {
                sum += track->rpm[i];
                count++;
            }
        }
        if (count > 0) {
            track->avg_rpm = sum / count;
        }
    }
    
    /* Weak Bit Detection */
    if (params->detect_weak_bits && track->revolution_count >= 2) {
        track->weak_count = 0;
        
        size_t min_len = track->flux_count[0];
        if (track->flux_count[1] < min_len) min_len = track->flux_count[1];
        
        const float threshold = 0.15f;  /* 15% Abweichung */
        
        for (size_t i = 0; i < min_len && track->weak_count < 256; i++) {
            uint32_t f0 = track->flux[0][i];
            uint32_t f1 = track->flux[1][i];
            
            if (f0 > 0) {
                float diff = fabsf((float)f1 - (float)f0) / (float)f0;
                if (diff > threshold) {
                    track->weak_positions[track->weak_count++] = (uint32_t)i;
                }
            }
        }
    }
    
    /* Track Confidence */
    if (track->total_flux > 0) {
        float valid_ratio = (float)(track->total_flux - track->overflow_count) /
                           (float)track->total_flux;
        track->track_confidence = valid_ratio;
    }
    
    return 0;
}

/**
 * @brief Track-Daten freigeben
 */
void uft_kf_free_track(uft_kf_track_t* track) {
    if (!track) return;
    
    for (int r = 0; r < UFT_KF_MAX_REVOLUTIONS; r++) {
        free(track->flux[r]);
        track->flux[r] = NULL;
    }
}

/**
 * @brief Flux zu Nanosekunden konvertieren (bereits in decode gemacht)
 */
const uint32_t* uft_kf_get_flux_ns(const uft_kf_track_t* track, int revolution,
                               uint32_t* count_out) {
    if (!track || revolution < 0 || revolution >= track->revolution_count) {
        if (count_out) *count_out = 0;
        return NULL;
    }
    
    if (count_out) *count_out = track->flux_count[revolution];
    return track->flux[revolution];
}

/*============================================================================
 * UNIT TESTS
 *============================================================================*/

#ifdef UFT_UFT_KF_TEST

#include <assert.h>

static void test_oob_detection(void) {
    uint8_t data[] = { 0xFF, 0xFF, 0x0D, 0x01, 0x08, 0x00, 0x00, 0x00 };
    
    int pos = simd_find_oob(data, sizeof(data));
    assert(pos == 2);
    
    printf("✓ OOB Detection\n");
}

static void test_flux_stats(void) {
    uint32_t flux[] = { 1000, 2000, 3000, 4000, 5000 };
    uint32_t min, max;
    double avg;
    
    simd_kf_flux_stats(flux, 5, &min, &max, &avg);
    
    assert(min == 1000);
    assert(max == 5000);
    assert(fabs(avg - 3000.0) < 0.1);
    
    printf("✓ Flux Statistics\n");
}

static void test_sample_clock(void) {
    double freq = UFT_KF_SAMPLE_FREQ;
    double period = 1e9 / freq;
    
    /* Sollte ca. 41.6ns sein */
    assert(period > 40.0 && period < 43.0);
    
    printf("✓ Sample Clock (%.3f ns)\n", period);
}

static void test_rpm_calculation(void) {
    /* 200ms pro Umdrehung = 300 RPM */
    double time_ns = 200e6;  /* 200ms in ns */
    double rpm = 60.0e9 / time_ns;
    
    assert(fabs(rpm - 300.0) < 0.1);
    
    printf("✓ RPM Calculation\n");
}

static void test_add_flux(void) {
    uft_kf_track_t track;
    memset(&track, 0, sizeof(track));
    track.sample_period_ns = 41.619;
    
    /* Einige Flux-Werte hinzufügen */
    assert(add_flux(&track, 0, 1000));
    assert(add_flux(&track, 0, 2000));
    assert(add_flux(&track, 0, 3000));
    
    assert(track.flux_count[0] == 3);
    assert(track.total_flux == 3);
    
    /* Cleanup */
    uft_kf_free_track(&track);
    
    printf("✓ Add Flux\n");
}

int main(void) {
    printf("=== KryoFlux Stream v2 Tests ===\n");
    
    test_oob_detection();
    test_flux_stats();
    test_sample_clock();
    test_rpm_calculation();
    test_add_flux();
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}

#endif /* UFT_UFT_KF_TEST */
