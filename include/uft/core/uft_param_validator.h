/**
 * @file uft_param_validator.h
 * @brief Parameter Conflicts Validator
 * 
 * P2-008: Detect and resolve parameter conflicts
 * 
 * Features:
 * - Cross-parameter validation
 * - Conflict detection
 * - Auto-resolution suggestions
 * - Format-specific rules
 */

#ifndef UFT_PARAM_VALIDATOR_H
#define UFT_PARAM_VALIDATOR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum parameters and conflicts */
#define PARAM_MAX_PARAMS        64
#define PARAM_MAX_CONFLICTS     32
#define PARAM_MAX_NAME_LEN      32

/**
 * @brief Parameter types
 */
typedef enum {
    PARAM_TYPE_INT,
    PARAM_TYPE_FLOAT,
    PARAM_TYPE_BOOL,
    PARAM_TYPE_STRING,
    PARAM_TYPE_ENUM,
} param_type_t;

/**
 * @brief Parameter value
 */
typedef union {
    int64_t i;
    double f;
    bool b;
    const char *s;
} param_value_t;

/**
 * @brief Parameter definition
 */
typedef struct {
    char name[PARAM_MAX_NAME_LEN];
    param_type_t type;
    param_value_t value;
    param_value_t min;
    param_value_t max;
    param_value_t default_val;
    bool is_set;
    bool is_required;
} param_def_t;

/**
 * @brief Conflict severity
 */
typedef enum {
    CONFLICT_NONE,
    CONFLICT_WARNING,       /* Can proceed, may have issues */
    CONFLICT_ERROR,         /* Should not proceed */
    CONFLICT_CRITICAL,      /* Will cause failure */
} conflict_severity_t;

/**
 * @brief Conflict type
 */
typedef enum {
    CONFLICT_TYPE_NONE,
    CONFLICT_TYPE_MUTUAL_EXCLUSIVE,   /* A and B cannot both be set */
    CONFLICT_TYPE_DEPENDENCY,         /* A requires B */
    CONFLICT_TYPE_RANGE,              /* A must be < B */
    CONFLICT_TYPE_FORMAT_MISMATCH,    /* Value incompatible with format */
    CONFLICT_TYPE_HARDWARE_LIMIT,     /* Exceeds hardware capability */
    CONFLICT_TYPE_LOGICAL,            /* Logically inconsistent */
} conflict_type_t;

/**
 * @brief Detected conflict
 */
typedef struct {
    conflict_type_t type;
    conflict_severity_t severity;
    
    char param1[PARAM_MAX_NAME_LEN];
    char param2[PARAM_MAX_NAME_LEN];
    
    char message[128];
    char suggestion[128];
    
    /* Auto-resolution */
    bool can_auto_resolve;
    char resolution_param[PARAM_MAX_NAME_LEN];
    param_value_t resolution_value;
    
} param_conflict_t;

/**
 * @brief Validation result
 */
typedef struct {
    bool valid;
    int error_count;
    int warning_count;
    
    param_conflict_t conflicts[PARAM_MAX_CONFLICTS];
    int conflict_count;
    
} param_validation_result_t;

/**
 * @brief Parameter set
 */
typedef struct {
    param_def_t params[PARAM_MAX_PARAMS];
    int param_count;
    
    /* Format context */
    const char *format_name;
    uint32_t format_id;
    
} param_set_t;

/**
 * @brief Conflict rule
 */
typedef struct {
    const char *param1;
    const char *param2;
    conflict_type_t type;
    conflict_severity_t severity;
    const char *message;
    
    /* Condition function (optional) */
    bool (*condition)(const param_set_t *params);
    
} conflict_rule_t;

/**
 * @brief Validator context
 */
typedef struct {
    /* Built-in rules */
    conflict_rule_t *rules;
    int rule_count;
    int rule_capacity;
    
    /* Format-specific rules */
    const conflict_rule_t *format_rules;
    int format_rule_count;
    
} param_validator_t;

/* ============================================================================
 * Initialization
 * ============================================================================ */

/**
 * @brief Initialize parameter set
 */
void param_set_init(param_set_t *ps);

/**
 * @brief Add parameter to set
 */
int param_set_add(param_set_t *ps, const char *name, param_type_t type);

/**
 * @brief Set parameter value
 */
int param_set_value_int(param_set_t *ps, const char *name, int64_t value);
int param_set_value_float(param_set_t *ps, const char *name, double value);
int param_set_value_bool(param_set_t *ps, const char *name, bool value);
int param_set_value_string(param_set_t *ps, const char *name, const char *value);

/**
 * @brief Get parameter value
 */
bool param_set_get_int(const param_set_t *ps, const char *name, int64_t *out);
bool param_set_get_float(const param_set_t *ps, const char *name, double *out);
bool param_set_get_bool(const param_set_t *ps, const char *name, bool *out);

/* ============================================================================
 * Validator
 * ============================================================================ */

/**
 * @brief Initialize validator
 */
void param_validator_init(param_validator_t *v);

/**
 * @brief Free validator
 */
void param_validator_free(param_validator_t *v);

/**
 * @brief Add conflict rule
 */
int param_validator_add_rule(param_validator_t *v, const conflict_rule_t *rule);

/**
 * @brief Load format-specific rules
 */
void param_validator_load_format_rules(param_validator_t *v, 
                                       const char *format_name);

/**
 * @brief Validate parameter set
 */
param_validation_result_t param_validator_validate(const param_validator_t *v,
                                                   const param_set_t *ps);

/**
 * @brief Auto-resolve conflicts
 * @return Number of conflicts resolved
 */
int param_validator_auto_resolve(param_set_t *ps,
                                 const param_validation_result_t *result);

/* ============================================================================
 * Built-in Rules
 * ============================================================================ */

/**
 * @brief Get generic floppy rules
 */
const conflict_rule_t* param_get_floppy_rules(int *count);

/**
 * @brief Get MFM-specific rules
 */
const conflict_rule_t* param_get_mfm_rules(int *count);

/**
 * @brief Get GCR-specific rules
 */
const conflict_rule_t* param_get_gcr_rules(int *count);

/**
 * @brief Get flux-specific rules
 */
const conflict_rule_t* param_get_flux_rules(int *count);

/* ============================================================================
 * Common Validation Functions
 * ============================================================================ */

/**
 * @brief Validate track range
 */
bool param_validate_track_range(int start, int end, int max);

/**
 * @brief Validate sector count for format
 */
bool param_validate_sector_count(int sectors, const char *format);

/**
 * @brief Validate bitrate for encoding
 */
bool param_validate_bitrate(int bitrate, const char *encoding);

/**
 * @brief Validate RPM for drive type
 */
bool param_validate_rpm(int rpm, const char *drive_type);

/* ============================================================================
 * Format String for Conflicts
 * ============================================================================ */

/**
 * @brief Format conflict as string
 */
int param_conflict_to_string(const param_conflict_t *c, 
                             char *buffer, size_t size);

/**
 * @brief Format validation result as JSON
 */
int param_result_to_json(const param_validation_result_t *r,
                         char *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PARAM_VALIDATOR_H */
