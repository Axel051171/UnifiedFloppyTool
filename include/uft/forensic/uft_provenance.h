#ifndef UFT_PROVENANCE_H
#define UFT_PROVENANCE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UFT_PROV_MAX_ENTRIES   256
#define UFT_PROV_HASH_SIZE     32   /* SHA-256 = 32 bytes */

typedef enum {
    UFT_PROV_CAPTURE,       /* Raw flux captured from hardware */
    UFT_PROV_DECODE,        /* Flux decoded to sectors */
    UFT_PROV_ANALYZE,       /* OTDR/DeepRead analysis performed */
    UFT_PROV_CONVERT,       /* Format conversion */
    UFT_PROV_EXPORT,        /* Exported to file */
    UFT_PROV_MODIFY,        /* Data modified (sector edit) */
    UFT_PROV_VERIFY,        /* Hash verification performed */
    UFT_PROV_IMPORT         /* Imported from external source */
} uft_prov_action_t;

typedef struct {
    uft_prov_action_t action;
    time_t            timestamp;
    uint8_t           input_hash[UFT_PROV_HASH_SIZE];   /* SHA-256 of input */
    uint8_t           output_hash[UFT_PROV_HASH_SIZE];  /* SHA-256 of output */
    uint8_t           chain_hash[UFT_PROV_HASH_SIZE];   /* SHA-256(prev_chain + this_entry) */
    char              tool_version[32];                   /* "UFT 4.1.0" */
    char              description[128];                   /* Human-readable */
    char              operator_id[64];                    /* Who performed this */
    char              controller[32];                     /* "Greaseweazle v4.2" */
    uint32_t          data_size;                          /* Size of data processed */
} uft_prov_entry_t;

typedef struct {
    uft_prov_entry_t entries[UFT_PROV_MAX_ENTRIES];
    uint32_t         count;
    uint8_t          root_hash[UFT_PROV_HASH_SIZE];     /* Hash of first entry */
    bool             valid;                              /* Chain integrity verified */
    char             media_fingerprint[65];              /* Hex string from DeepRead */
} uft_provenance_chain_t;

/* Create a new provenance chain */
uft_provenance_chain_t *uft_prov_create(void);

/* Add an entry to the chain */
int uft_prov_add(uft_provenance_chain_t *chain,
                  uft_prov_action_t action,
                  const uint8_t *data, size_t data_size,
                  const char *description,
                  const char *operator_id);

/* Verify chain integrity (all hashes link correctly) */
bool uft_prov_verify(const uft_provenance_chain_t *chain);

/* Export chain as JSON */
int uft_prov_export_json(const uft_provenance_chain_t *chain,
                          const char *output_path);

/* Import chain from JSON */
uft_provenance_chain_t *uft_prov_import_json(const char *path);

/* Free chain */
void uft_prov_free(uft_provenance_chain_t *chain);

/* Get human-readable action name */
const char *uft_prov_action_name(uft_prov_action_t action);

#ifdef __cplusplus
}
#endif
#endif
