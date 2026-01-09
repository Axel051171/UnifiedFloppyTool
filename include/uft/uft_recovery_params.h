/**
 * @file uft_recovery_params.h
 * @brief Recovery Parameters
 */
#ifndef UFT_RECOVERY_PARAMS_H
#define UFT_RECOVERY_PARAMS_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int max_retries;
    int retry_delay_ms;
    bool use_multirev;
    int multirev_count;
    bool try_alternate_density;
    bool aggressive_mode;
} uft_recovery_params_t;

#define UFT_RECOVERY_PARAMS_DEFAULT { \
    .max_retries = 5, \
    .retry_delay_ms = 100, \
    .use_multirev = true, \
    .multirev_count = 3, \
    .try_alternate_density = false, \
    .aggressive_mode = false \
}

#endif /* UFT_RECOVERY_PARAMS_H */
