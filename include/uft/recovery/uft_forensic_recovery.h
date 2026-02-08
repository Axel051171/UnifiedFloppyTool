/**
 * @file uft_forensic_recovery.h
 * @brief Forensic Recovery Functions
 */
#ifndef UFT_FORENSIC_RECOVERY_H
#define UFT_FORENSIC_RECOVERY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uft_recovery_ctx uft_recovery_ctx_t;
typedef struct uft_recovery_result uft_recovery_result_t;

int uft_recovery_init(uft_recovery_ctx_t **ctx);
void uft_recovery_free(uft_recovery_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORENSIC_RECOVERY_H */
