/**
 * @file uft_recovery_meta.c
 * @brief GOD MODE Meta/Decision Recovery Implementation
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include "uft_recovery_meta.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Source Tracking
 * ============================================================================ */

uft_source_tracking_t* uft_meta_source_create(void) {
    uft_source_tracking_t *tracking = calloc(1, sizeof(uft_source_tracking_t));
    if (!tracking) return NULL;
    
    tracking->sources = calloc(UFT_META_MAX_SOURCES, sizeof(uft_source_info_t));
    if (!tracking->sources) {
        free(tracking);
        return NULL;
    }
    
    return tracking;
}

bool uft_meta_source_add(uft_source_tracking_t *tracking,
                         const uft_source_info_t *source) {
    if (!tracking || !source) return false;
    if (tracking->source_count >= UFT_META_MAX_SOURCES) return false;
    
    memcpy(&tracking->sources[tracking->source_count], source, sizeof(uft_source_info_t));
    tracking->sources[tracking->source_count].timestamp = time(NULL);
    
    tracking->source_count++;
    
    /* Update overall confidence - weighted average */
    uint32_t total_conf = 0;
    for (size_t i = 0; i < tracking->source_count; i++) {
        total_conf += tracking->sources[i].confidence;
    }
    tracking->overall_confidence = (uint8_t)(total_conf / tracking->source_count);
    
    return true;
}

const uft_source_info_t* uft_meta_source_get_primary(
    const uft_source_tracking_t *tracking) {
    if (!tracking || tracking->source_count == 0) return NULL;
    return &tracking->sources[tracking->primary_source];
}

void uft_meta_source_set_primary(uft_source_tracking_t *tracking,
                                 size_t source_index) {
    if (!tracking || source_index >= tracking->source_count) return;
    tracking->primary_source = source_index;
}

void uft_meta_source_free(uft_source_tracking_t *tracking) {
    if (!tracking) return;
    free(tracking->sources);
    free(tracking);
}

/* ============================================================================
 * Confidence Functions
 * ============================================================================ */

uint8_t uft_meta_calc_confidence(const uft_confidence_breakdown_t *breakdown) {
    if (!breakdown) return 0;
    
    /* Weighted combination */
    uint32_t combined = 
        breakdown->raw_confidence * 25 +
        breakdown->crc_confidence * 40 +
        breakdown->pattern_confidence * 20 +
        breakdown->cross_confidence * 15;
    
    return (uint8_t)(combined / 100);
}

uft_confidence_level_t uft_meta_get_level(uint8_t confidence) {
    if (confidence >= 95) return UFT_CONFIDENCE_CERTAIN;
    if (confidence >= 75) return UFT_CONFIDENCE_HIGH;
    if (confidence >= 50) return UFT_CONFIDENCE_MEDIUM;
    if (confidence >= 25) return UFT_CONFIDENCE_LOW;
    return UFT_CONFIDENCE_NONE;
}

const char* uft_meta_describe_confidence(uft_confidence_level_t level) {
    switch (level) {
        case UFT_CONFIDENCE_CERTAIN: return "Certain (CRC verified)";
        case UFT_CONFIDENCE_HIGH: return "High confidence";
        case UFT_CONFIDENCE_MEDIUM: return "Medium confidence";
        case UFT_CONFIDENCE_LOW: return "Low confidence";
        default: return "No confidence (guess)";
    }
}

/* ============================================================================
 * Hypothesis Management
 * ============================================================================ */

static uint32_t next_hyp_id = 1;
static uint32_t next_decision_id = 1;

uft_hypothesis_set_t* uft_meta_hyp_create(const char *context) {
    uft_hypothesis_set_t *set = calloc(1, sizeof(uft_hypothesis_set_t));
    if (!set) return NULL;
    
    set->decision_id = next_decision_id++;
    if (context) {
        strncpy(set->context, context, sizeof(set->context) - 1);
    }
    
    set->hypotheses = calloc(UFT_META_MAX_HYPOTHESES, sizeof(uft_hypothesis_t));
    if (!set->hypotheses) {
        free(set);
        return NULL;
    }
    
    set->is_reversible = true;
    return set;
}

bool uft_meta_hyp_add(uft_hypothesis_set_t *set,
                      const uft_hypothesis_t *hyp) {
    if (!set || !hyp) return false;
    if (set->hypothesis_count >= UFT_META_MAX_HYPOTHESES) return false;
    
    memcpy(&set->hypotheses[set->hypothesis_count], hyp, sizeof(uft_hypothesis_t));
    set->hypotheses[set->hypothesis_count].hyp_id = next_hyp_id++;
    
    /* Copy data if present */
    if (hyp->data && hyp->data_len > 0) {
        set->hypotheses[set->hypothesis_count].data = malloc(hyp->data_len);
        if (set->hypotheses[set->hypothesis_count].data) {
            memcpy(set->hypotheses[set->hypothesis_count].data, hyp->data, hyp->data_len);
        }
    }
    
    set->hypothesis_count++;
    
    /* Auto-select if first and valid */
    if (set->hypothesis_count == 1 && !hyp->is_rejected) {
        set->selected = &set->hypotheses[0];
        set->hypotheses[0].is_selected = true;
    }
    
    return true;
}

bool uft_meta_hyp_select(uft_hypothesis_set_t *set, uint32_t hyp_id) {
    if (!set || set->is_finalized) return false;
    
    for (size_t i = 0; i < set->hypothesis_count; i++) {
        if (set->hypotheses[i].hyp_id == hyp_id && !set->hypotheses[i].is_rejected) {
            /* Deselect current */
            if (set->selected) {
                set->selected->is_selected = false;
            }
            
            /* Select new */
            set->hypotheses[i].is_selected = true;
            set->selected = &set->hypotheses[i];
            return true;
        }
    }
    return false;
}

void uft_meta_hyp_reject(uft_hypothesis_set_t *set, uint32_t hyp_id,
                         const char *reason) {
    if (!set) return;
    
    for (size_t i = 0; i < set->hypothesis_count; i++) {
        if (set->hypotheses[i].hyp_id == hyp_id) {
            set->hypotheses[i].is_rejected = true;
            if (reason) {
                set->hypotheses[i].rejection_reason = strdup(reason);
            }
            
            /* If this was selected, find another */
            if (set->selected == &set->hypotheses[i]) {
                set->selected = NULL;
                for (size_t j = 0; j < set->hypothesis_count; j++) {
                    if (!set->hypotheses[j].is_rejected) {
                        set->selected = &set->hypotheses[j];
                        set->hypotheses[j].is_selected = true;
                        break;
                    }
                }
            }
            break;
        }
    }
}

const uft_hypothesis_t* uft_meta_hyp_get_selected(
    const uft_hypothesis_set_t *set) {
    return set ? set->selected : NULL;
}

size_t uft_meta_hyp_get_valid(const uft_hypothesis_set_t *set,
                              const uft_hypothesis_t ***hypotheses) {
    if (!set || !hypotheses) return 0;
    
    /* Count valid */
    size_t count = 0;
    for (size_t i = 0; i < set->hypothesis_count; i++) {
        if (!set->hypotheses[i].is_rejected) count++;
    }
    
    if (count == 0) {
        *hypotheses = NULL;
        return 0;
    }
    
    *hypotheses = calloc(count, sizeof(uft_hypothesis_t*));
    if (!*hypotheses) return 0;
    
    size_t idx = 0;
    for (size_t i = 0; i < set->hypothesis_count && idx < count; i++) {
        if (!set->hypotheses[i].is_rejected) {
            (*hypotheses)[idx++] = &set->hypotheses[i];
        }
    }
    
    return count;
}

void uft_meta_hyp_free(uft_hypothesis_set_t *set) {
    if (!set) return;
    
    for (size_t i = 0; i < set->hypothesis_count; i++) {
        free(set->hypotheses[i].data);
        free(set->hypotheses[i].rejection_reason);
    }
    free(set->hypotheses);
    free(set);
}

/* ============================================================================
 * Undo Stack
 * ============================================================================ */

uft_undo_stack_t* uft_meta_undo_create(size_t max_records) {
    uft_undo_stack_t *stack = calloc(1, sizeof(uft_undo_stack_t));
    if (!stack) return NULL;
    
    stack->records = calloc(max_records, sizeof(uft_undo_record_t));
    if (!stack->records) {
        free(stack);
        return NULL;
    }
    
    stack->max_records = max_records;
    return stack;
}

static uint32_t next_action_id = 1;

bool uft_meta_undo_record(uft_undo_stack_t *stack,
                          const char *description,
                          uint8_t track, uint8_t head, uint8_t sector,
                          const uint8_t *original, size_t orig_len,
                          const uint8_t *modified, size_t mod_len) {
    if (!stack || stack->record_count >= stack->max_records) return false;
    
    /* Truncate undo stack if we're in the middle */
    if (stack->current_position < stack->record_count) {
        /* Free records after current position */
        for (size_t i = stack->current_position; i < stack->record_count; i++) {
            free(stack->records[i].original_data);
            free(stack->records[i].modified_data);
        }
        stack->record_count = stack->current_position;
    }
    
    uft_undo_record_t *rec = &stack->records[stack->record_count];
    memset(rec, 0, sizeof(uft_undo_record_t));
    
    rec->action_id = next_action_id++;
    if (description) {
        strncpy(rec->description, description, sizeof(rec->description) - 1);
    }
    rec->timestamp = time(NULL);
    
    rec->track = track;
    rec->head = head;
    rec->sector = sector;
    
    /* Store original */
    if (original && orig_len > 0) {
        rec->original_data = malloc(orig_len);
        if (rec->original_data) {
            memcpy(rec->original_data, original, orig_len);
            rec->original_len = orig_len;
        }
    }
    
    /* Store modified */
    if (modified && mod_len > 0) {
        rec->modified_data = malloc(mod_len);
        if (rec->modified_data) {
            memcpy(rec->modified_data, modified, mod_len);
            rec->modified_len = mod_len;
        }
    }
    
    rec->can_undo = true;
    
    stack->record_count++;
    stack->current_position = stack->record_count;
    
    return true;
}

bool uft_meta_undo(uft_undo_stack_t *stack,
                   uint8_t **restored, size_t *restored_len) {
    if (!stack || stack->current_position == 0) return false;
    
    stack->current_position--;
    uft_undo_record_t *rec = &stack->records[stack->current_position];
    
    if (restored && restored_len && rec->original_data) {
        *restored = malloc(rec->original_len);
        if (*restored) {
            memcpy(*restored, rec->original_data, rec->original_len);
            *restored_len = rec->original_len;
        }
    }
    
    rec->was_undone = true;
    return true;
}

bool uft_meta_redo(uft_undo_stack_t *stack,
                   uint8_t **result, size_t *result_len) {
    if (!stack || stack->current_position >= stack->record_count) return false;
    
    uft_undo_record_t *rec = &stack->records[stack->current_position];
    
    if (result && result_len && rec->modified_data) {
        *result = malloc(rec->modified_len);
        if (*result) {
            memcpy(*result, rec->modified_data, rec->modified_len);
            *result_len = rec->modified_len;
        }
    }
    
    rec->was_undone = false;
    stack->current_position++;
    
    return true;
}

bool uft_meta_can_undo(const uft_undo_stack_t *stack) {
    return stack && stack->current_position > 0;
}

bool uft_meta_can_redo(const uft_undo_stack_t *stack) {
    return stack && stack->current_position < stack->record_count;
}

void uft_meta_undo_free(uft_undo_stack_t *stack) {
    if (!stack) return;
    
    for (size_t i = 0; i < stack->record_count; i++) {
        free(stack->records[i].original_data);
        free(stack->records[i].modified_data);
    }
    free(stack->records);
    free(stack);
}

/* ============================================================================
 * Writer Warnings
 * ============================================================================ */

void uft_meta_warn_add(uft_meta_ctx_t *ctx,
                       uint32_t flag, uint8_t severity,
                       uint8_t track, uint8_t head, uint8_t sector,
                       const char *message) {
    if (!ctx) return;
    
    /* Grow array */
    uft_writer_warning_t *new_warnings = realloc(ctx->warnings,
        (ctx->warning_count + 1) * sizeof(uft_writer_warning_t));
    if (!new_warnings) return;
    
    ctx->warnings = new_warnings;
    uft_writer_warning_t *warn = &ctx->warnings[ctx->warning_count];
    
    warn->flag = flag;
    warn->severity = severity;
    warn->track = track;
    warn->head = head;
    warn->sector = sector;
    if (message) {
        strncpy(warn->message, message, sizeof(warn->message) - 1);
    }
    
    ctx->warning_count++;
}

size_t uft_meta_warn_get(const uft_meta_ctx_t *ctx,
                         uint8_t track, uint8_t head, uint8_t sector,
                         uft_writer_warning_t **warnings) {
    if (!ctx || !warnings) return 0;
    
    /* Count matching warnings */
    size_t count = 0;
    for (size_t i = 0; i < ctx->warning_count; i++) {
        if (ctx->warnings[i].track == track &&
            ctx->warnings[i].head == head &&
            (sector == 0xFF || ctx->warnings[i].sector == sector)) {
            count++;
        }
    }
    
    if (count == 0) {
        *warnings = NULL;
        return 0;
    }
    
    *warnings = calloc(count, sizeof(uft_writer_warning_t));
    if (!*warnings) return 0;
    
    size_t idx = 0;
    for (size_t i = 0; i < ctx->warning_count && idx < count; i++) {
        if (ctx->warnings[i].track == track &&
            ctx->warnings[i].head == head &&
            (sector == 0xFF || ctx->warnings[i].sector == sector)) {
            (*warnings)[idx++] = ctx->warnings[i];
        }
    }
    
    return count;
}

bool uft_meta_warn_has(const uft_meta_ctx_t *ctx,
                       uint8_t track, uint8_t head, uint8_t sector,
                       uint32_t flag) {
    if (!ctx) return false;
    
    for (size_t i = 0; i < ctx->warning_count; i++) {
        if (ctx->warnings[i].track == track &&
            ctx->warnings[i].head == head &&
            ctx->warnings[i].sector == sector &&
            (ctx->warnings[i].flag & flag)) {
            return true;
        }
    }
    return false;
}

char* uft_meta_warn_report(const uft_meta_ctx_t *ctx) {
    if (!ctx) return NULL;
    
    size_t buf_size = 4096;
    char *report = malloc(buf_size);
    if (!report) return NULL;
    
    int offset = snprintf(report, buf_size,
        "=== WRITER WARNINGS ===\n\n"
        "Total warnings: %zu\n\n", ctx->warning_count);
    
    for (size_t i = 0; i < ctx->warning_count && offset < (int)buf_size - 200; i++) {
        offset += snprintf(report + offset, buf_size - offset,
            "Track %d, Head %d, Sector %d: [%s] %s\n",
            ctx->warnings[i].track,
            ctx->warnings[i].head,
            ctx->warnings[i].sector,
            ctx->warnings[i].severity > 50 ? "CRITICAL" : "WARNING",
            ctx->warnings[i].message);
    }
    
    return report;
}

/* ============================================================================
 * Forensic Log
 * ============================================================================ */

uft_forensic_log_t* uft_meta_log_create(size_t max_entries) {
    uft_forensic_log_t *log = calloc(1, sizeof(uft_forensic_log_t));
    if (!log) return NULL;
    
    log->entries = calloc(max_entries, sizeof(uft_log_entry_t));
    if (!log->entries) {
        free(log);
        return NULL;
    }
    
    log->max_entries = max_entries;
    log->log_decisions = true;
    log->log_hypotheses = true;
    
    return log;
}

static uint32_t next_entry_id = 1;

void uft_meta_log_add(uft_forensic_log_t *log,
                      uft_log_type_t type,
                      int8_t track, int8_t head, int8_t sector,
                      const char *message) {
    uft_meta_log_add_data(log, type, track, head, sector, message, NULL, 0);
}

void uft_meta_log_add_data(uft_forensic_log_t *log,
                           uft_log_type_t type,
                           int8_t track, int8_t head, int8_t sector,
                           const char *message,
                           const uint8_t *data, size_t len) {
    if (!log || log->entry_count >= log->max_entries) return;
    if (type < log->min_level) return;
    if (type == UFT_LOG_DECISION && !log->log_decisions) return;
    if (type == UFT_LOG_HYPOTHESIS && !log->log_hypotheses) return;
    
    uft_log_entry_t *entry = &log->entries[log->entry_count];
    
    entry->entry_id = next_entry_id++;
    entry->type = type;
    entry->timestamp = time(NULL);
    entry->track = track;
    entry->head = head;
    entry->sector = sector;
    
    if (message) {
        strncpy(entry->message, message, sizeof(entry->message) - 1);
    }
    
    if (data && len > 0) {
        entry->data = malloc(len);
        if (entry->data) {
            memcpy(entry->data, data, len);
            entry->data_len = len;
        }
    }
    
    log->entry_count++;
    
    /* Write to file if set */
    if (log->log_file) {
        const char *type_str = "INFO";
        switch (type) {
            case UFT_LOG_WARNING: type_str = "WARN"; break;
            case UFT_LOG_ERROR: type_str = "ERROR"; break;
            case UFT_LOG_DECISION: type_str = "DECISION"; break;
            case UFT_LOG_HYPOTHESIS: type_str = "HYPOTHESIS"; break;
            case UFT_LOG_RECOVERY: type_str = "RECOVERY"; break;
            case UFT_LOG_PROTECTION: type_str = "PROTECTION"; break;
            default: break;
        }
        
        fprintf(log->log_file, "[%s] T%d/H%d/S%d: %s\n",
                type_str, track, head, sector, message ? message : "");
        fflush(log->log_file);
    }
}

void uft_meta_log_set_file(uft_forensic_log_t *log, FILE *file) {
    if (log) {
        log->log_file = file;
    }
}

char* uft_meta_log_export(const uft_forensic_log_t *log) {
    if (!log) return NULL;
    
    size_t buf_size = log->entry_count * 600 + 1024;
    char *export = malloc(buf_size);
    if (!export) return NULL;
    
    int offset = snprintf(export, buf_size,
        "=== FORENSIC LOG ===\n"
        "Entries: %zu\n\n", log->entry_count);
    
    for (size_t i = 0; i < log->entry_count && offset < (int)buf_size - 600; i++) {
        const uft_log_entry_t *e = &log->entries[i];
        
        const char *type_str = "INFO";
        switch (e->type) {
            case UFT_LOG_WARNING: type_str = "WARNING"; break;
            case UFT_LOG_ERROR: type_str = "ERROR"; break;
            case UFT_LOG_DECISION: type_str = "DECISION"; break;
            case UFT_LOG_HYPOTHESIS: type_str = "HYPOTHESIS"; break;
            case UFT_LOG_RECOVERY: type_str = "RECOVERY"; break;
            case UFT_LOG_PROTECTION: type_str = "PROTECTION"; break;
            default: break;
        }
        
        offset += snprintf(export + offset, buf_size - offset,
            "[%04u] %s | T%d/H%d/S%d | %s\n",
            e->entry_id, type_str, e->track, e->head, e->sector, e->message);
    }
    
    return export;
}

void uft_meta_log_free(uft_forensic_log_t *log) {
    if (!log) return;
    
    for (size_t i = 0; i < log->entry_count; i++) {
        free(log->entries[i].data);
    }
    free(log->entries);
    free(log);
}

/* ============================================================================
 * Meta Context
 * ============================================================================ */

uft_meta_ctx_t* uft_meta_create(void) {
    uft_meta_ctx_t *ctx = calloc(1, sizeof(uft_meta_ctx_t));
    if (!ctx) return NULL;
    
    ctx->track_sources = true;
    ctx->keep_hypotheses = true;
    ctx->enable_undo = true;
    ctx->max_undo_depth = 100;
    
    return ctx;
}

void uft_meta_configure(uft_meta_ctx_t *ctx,
                        bool track_sources,
                        bool keep_hypotheses,
                        bool enable_undo,
                        uint8_t max_undo) {
    if (!ctx) return;
    
    ctx->track_sources = track_sources;
    ctx->keep_hypotheses = keep_hypotheses;
    ctx->enable_undo = enable_undo;
    ctx->max_undo_depth = max_undo;
}

char* uft_meta_full_report(const uft_meta_ctx_t *ctx) {
    if (!ctx) return NULL;
    
    size_t buf_size = 8192;
    char *report = malloc(buf_size);
    if (!report) return NULL;
    
    int offset = snprintf(report, buf_size,
        "╔══════════════════════════════════════════════════════════════╗\n"
        "║             GOD MODE FORENSIC REPORT                         ║\n"
        "╠══════════════════════════════════════════════════════════════╣\n\n"
        "Hypothesis Sets: %zu\n"
        "Writer Warnings: %zu\n"
        "Undo Records: %zu\n"
        "Log Entries: %zu\n\n",
        ctx->hypothesis_set_count,
        ctx->warning_count,
        ctx->undo_stack.record_count,
        ctx->log.entry_count);
    
    /* Add warnings summary */
    if (ctx->warning_count > 0) {
        offset += snprintf(report + offset, buf_size - offset,
            "=== Warnings Summary ===\n");
        
        uint32_t crit_count = 0, warn_count = 0;
        for (size_t i = 0; i < ctx->warning_count; i++) {
            if (ctx->warnings[i].severity > 50) crit_count++;
            else warn_count++;
        }
        
        offset += snprintf(report + offset, buf_size - offset,
            "Critical: %u, Warnings: %u\n\n", crit_count, warn_count);
    }
    
    return report;
}

void uft_meta_free(uft_meta_ctx_t *ctx) {
    if (!ctx) return;
    
    /* Free source map */
    if (ctx->source_map) {
        for (size_t i = 0; i < ctx->source_map_size; i++) {
            uft_meta_source_free(ctx->source_map[i]);
        }
        free(ctx->source_map);
    }
    
    /* Free hypothesis sets */
    for (size_t i = 0; i < ctx->hypothesis_set_count; i++) {
        uft_meta_hyp_free(&ctx->hypothesis_sets[i]);
    }
    free(ctx->hypothesis_sets);
    
    /* Free warnings */
    free(ctx->warnings);
    
    /* Free undo stack contents (not the stack itself as it's embedded) */
    for (size_t i = 0; i < ctx->undo_stack.record_count; i++) {
        free(ctx->undo_stack.records[i].original_data);
        free(ctx->undo_stack.records[i].modified_data);
    }
    free(ctx->undo_stack.records);
    
    /* Free log entries */
    for (size_t i = 0; i < ctx->log.entry_count; i++) {
        free(ctx->log.entries[i].data);
    }
    free(ctx->log.entries);
    
    free(ctx);
}

/* ============================================================================
 * Tests
 * ============================================================================ */

#ifdef UFT_RECOVERY_META_TEST
#include <assert.h>

int main(void) {
    printf("=== GOD MODE Meta Recovery Tests ===\n");
    
    /* Test source tracking */
    printf("Testing source tracking... ");
    uft_source_tracking_t *tracking = uft_meta_source_create();
    assert(tracking != NULL);
    
    uft_source_info_t src = {
        .type = UFT_SOURCE_RAW_FLUX,
        .rev_index = 0,
        .confidence = 90
    };
    assert(uft_meta_source_add(tracking, &src));
    assert(tracking->source_count == 1);
    uft_meta_source_free(tracking);
    printf("✓\n");
    
    /* Test confidence */
    printf("Testing confidence... ");
    uft_confidence_breakdown_t bd = {80, 100, 70, 60};
    uint8_t conf = uft_meta_calc_confidence(&bd);
    assert(conf > 0 && conf <= 100);
    assert(uft_meta_get_level(95) == UFT_CONFIDENCE_CERTAIN);
    printf("✓\n");
    
    /* Test hypotheses */
    printf("Testing hypotheses... ");
    uft_hypothesis_set_t *set = uft_meta_hyp_create("Test");
    assert(set != NULL);
    
    uft_hypothesis_t hyp = {
        .description = "Test hypothesis",
        .score = 85.0,
        .confidence = 80
    };
    assert(uft_meta_hyp_add(set, &hyp));
    assert(set->hypothesis_count == 1);
    
    const uft_hypothesis_t *selected = uft_meta_hyp_get_selected(set);
    assert(selected != NULL);
    uft_meta_hyp_free(set);
    printf("✓\n");
    
    /* Test undo */
    printf("Testing undo... ");
    uft_undo_stack_t *stack = uft_meta_undo_create(10);
    assert(stack != NULL);
    
    uint8_t orig[] = {1, 2, 3};
    uint8_t mod[] = {4, 5, 6};
    assert(uft_meta_undo_record(stack, "Test", 0, 0, 1, orig, 3, mod, 3));
    assert(uft_meta_can_undo(stack));
    
    uint8_t *restored;
    size_t restored_len;
    assert(uft_meta_undo(stack, &restored, &restored_len));
    assert(restored_len == 3);
    assert(restored[0] == 1);
    free(restored);
    
    uft_meta_undo_free(stack);
    printf("✓\n");
    
    /* Test log */
    printf("Testing forensic log... ");
    uft_forensic_log_t *log = uft_meta_log_create(100);
    assert(log != NULL);
    
    uft_meta_log_add(log, UFT_LOG_INFO, 0, 0, 1, "Test message");
    assert(log->entry_count == 1);
    
    char *export = uft_meta_log_export(log);
    assert(export != NULL);
    free(export);
    uft_meta_log_free(log);
    printf("✓\n");
    
    /* Test meta context */
    printf("Testing meta context... ");
    uft_meta_ctx_t *ctx = uft_meta_create();
    assert(ctx != NULL);
    
    uft_meta_warn_add(ctx, UFT_WARN_CRC_INTENTIONAL, 80, 0, 0, 1, "Bad CRC");
    assert(ctx->warning_count == 1);
    assert(uft_meta_warn_has(ctx, 0, 0, 1, UFT_WARN_CRC_INTENTIONAL));
    
    char *report = uft_meta_full_report(ctx);
    assert(report != NULL);
    free(report);
    
    uft_meta_free(ctx);
    printf("✓\n");
    
    printf("=== All Meta Recovery Tests Passed! ===\n");
    return 0;
}
#endif
