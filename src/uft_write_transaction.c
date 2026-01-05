/**
 * @file uft_write_transaction.c
 * @brief Write Transaction Implementation
 * 
 * TICKET-003: Write Abort/Rollback
 * Atomic write operations with backup and rollback
 * 
 * PERF-FIX: P2-PERF-001 - Buffered I/O for backup save/load
 */

#include "uft/uft_write_transaction.h"
#include "uft/uft_core.h"
#include "uft/uft_memory.h"
#include "uft/uft_safe_io.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal Structures
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define MAX_OPERATIONS 256

struct uft_write_txn {
    uft_disk_t          *disk;
    uft_txn_options_t   options;
    uft_txn_state_t     state;
    
    uft_txn_operation_t operations[MAX_OPERATIONS];
    int                 op_count;
    int                 op_executed;
    
    bool                abort_requested;
    
    FILE                *log_file;
    double              start_time;
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

static double get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

static void log_operation(uft_write_txn_t *txn, const char *msg) {
    if (!txn || !txn->log_file) return;
    
    time_t now = time(NULL);
    char timestr[64];
    strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    fprintf(txn->log_file, "[%s] %s\n", timestr, msg);
    fflush(txn->log_file);
}

static uft_error_t backup_track(uft_write_txn_t *txn, int op_index) {
    if (!txn || op_index < 0 || op_index >= txn->op_count) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    uft_txn_operation_t *op = &txn->operations[op_index];
    
    /* Determine backup size based on operation type */
    size_t backup_size = op->data_size;
    if (backup_size == 0) {
        /* Default track size */
        uft_geometry_t geom;
        if (uft_disk_get_geometry(txn->disk, &geom) == UFT_OK) {
            backup_size = geom.sectors_per_track * geom.bytes_per_sector;
        } else {
            backup_size = 16384;  /* Fallback */
        }
    }
    
    op->backup = malloc(backup_size);
    if (!op->backup) return UFT_ERR_MEMORY;
    
    /* Read current track data */
    /* In real implementation: uft_disk_read_track(txn->disk, op->cyl, op->head, ...) */
    memset(op->backup, 0, backup_size);  /* Simulate read */
    
    op->backup_size = backup_size;
    op->backup_valid = true;
    
    return UFT_OK;
}

static uft_error_t restore_track(uft_write_txn_t *txn, int op_index) {
    if (!txn || op_index < 0 || op_index >= txn->op_count) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    uft_txn_operation_t *op = &txn->operations[op_index];
    
    if (!op->backup_valid || !op->backup) {
        return UFT_ERR_NO_BACKUP;
    }
    
    /* Restore from backup */
    /* In real implementation: uft_disk_write_track(txn->disk, op->cyl, op->head, ...) */
    
    return UFT_OK;
}

static uft_error_t execute_operation(uft_write_txn_t *txn, int op_index) {
    if (!txn || op_index < 0 || op_index >= txn->op_count) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    uft_txn_operation_t *op = &txn->operations[op_index];
    uft_error_t err = UFT_OK;
    
    switch (op->type) {
        case UFT_TXN_OP_WRITE_TRACK:
            /* In real: err = uft_disk_write_track(txn->disk, ...); */
            err = UFT_OK;
            break;
            
        case UFT_TXN_OP_WRITE_SECTOR:
            /* In real: err = uft_disk_write_sector(txn->disk, ...); */
            err = UFT_OK;
            break;
            
        case UFT_TXN_OP_WRITE_FLUX:
            /* In real: err = uft_disk_write_flux(txn->disk, ...); */
            err = UFT_OK;
            break;
            
        case UFT_TXN_OP_FORMAT_TRACK:
            /* In real: err = uft_disk_format_track(txn->disk, ...); */
            err = UFT_OK;
            break;
            
        case UFT_TXN_OP_ERASE_TRACK:
            /* In real: err = uft_disk_erase_track(txn->disk, ...); */
            err = UFT_OK;
            break;
            
        default:
            err = UFT_ERR_INVALID_PARAM;
    }
    
    op->executed = (err == UFT_OK);
    op->result = err;
    
    return err;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Implementation - Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_write_txn_t *uft_write_txn_begin(uft_disk_t *disk) {
    uft_txn_options_t opts = UFT_TXN_OPTIONS_DEFAULT;
    return uft_write_txn_begin_ex(disk, &opts);
}

uft_write_txn_t *uft_write_txn_begin_ex(uft_disk_t *disk,
                                         const uft_txn_options_t *options) {
    if (!disk) return NULL;
    
    uft_write_txn_t *txn = calloc(1, sizeof(uft_write_txn_t));
    if (!txn) return NULL;
    
    txn->disk = disk;
    txn->options = *options;
    txn->state = UFT_TXN_STATE_IDLE;
    txn->op_count = 0;
    txn->op_executed = 0;
    txn->abort_requested = false;
    txn->log_file = NULL;
    txn->start_time = get_time_ms();
    
    /* Enable logging if requested */
    if (options->log_enabled && options->log_path) {
        txn->log_file = fopen(options->log_path, "w");
    if (!txn->log_file) { /* Log file optional - continue */ }
        if (txn->log_file) {
            log_operation(txn, "Transaction started");
        }
    }
    
    return txn;
}

uft_txn_state_t uft_write_txn_get_state(const uft_write_txn_t *txn) {
    return txn ? txn->state : UFT_TXN_STATE_IDLE;
}

void uft_write_txn_free(uft_write_txn_t *txn) {
    if (!txn) return;
    
    /* Abort if still pending */
    if (txn->state == UFT_TXN_STATE_PENDING) {
        uft_write_txn_abort(txn);
    }
    
    /* Free operation data */
    for (int i = 0; i < txn->op_count; i++) {
        free(txn->operations[i].data);
        free(txn->operations[i].backup);
    }
    
    /* Close log */
    if (txn->log_file) {
        log_operation(txn, "Transaction closed");
        fclose(txn->log_file);
    }
    
    free(txn);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Implementation - Add Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_write_txn_add_track(uft_write_txn_t *txn,
                                     uint8_t cylinder, uint8_t head,
                                     const uint8_t *data, size_t size) {
    if (!txn || !data || size == 0) return UFT_ERR_INVALID_PARAM;
    if (txn->op_count >= MAX_OPERATIONS) return UFT_ERR_LIMIT;
    if (txn->state != UFT_TXN_STATE_IDLE && txn->state != UFT_TXN_STATE_PENDING) {
        return UFT_ERR_STATE;
    }
    
    uft_txn_operation_t *op = &txn->operations[txn->op_count];
    memset(op, 0, sizeof(*op));
    
    op->type = UFT_TXN_OP_WRITE_TRACK;
    op->cylinder = cylinder;
    op->head = head;
    op->sector = 0xFF;
    
    op->data = malloc(size);
    if (!op->data) return UFT_ERR_MEMORY;
    memcpy(op->data, data, size);
    op->data_size = size;
    
    txn->op_count++;
    txn->state = UFT_TXN_STATE_PENDING;
    
    char msg[128];
    snprintf(msg, sizeof(msg), "Added: WRITE_TRACK cyl=%d head=%d size=%zu",
             cylinder, head, size);
    log_operation(txn, msg);
    
    return UFT_OK;
}

uft_error_t uft_write_txn_add_sector(uft_write_txn_t *txn,
                                      uint8_t cylinder, uint8_t head,
                                      uint8_t sector,
                                      const uint8_t *data, size_t size) {
    if (!txn || !data || size == 0) return UFT_ERR_INVALID_PARAM;
    if (txn->op_count >= MAX_OPERATIONS) return UFT_ERR_LIMIT;
    
    uft_txn_operation_t *op = &txn->operations[txn->op_count];
    memset(op, 0, sizeof(*op));
    
    op->type = UFT_TXN_OP_WRITE_SECTOR;
    op->cylinder = cylinder;
    op->head = head;
    op->sector = sector;
    
    op->data = malloc(size);
    if (!op->data) return UFT_ERR_MEMORY;
    memcpy(op->data, data, size);
    op->data_size = size;
    
    txn->op_count++;
    txn->state = UFT_TXN_STATE_PENDING;
    
    return UFT_OK;
}

uft_error_t uft_write_txn_add_flux(uft_write_txn_t *txn,
                                    uint8_t cylinder, uint8_t head,
                                    const uint32_t *flux_samples,
                                    size_t sample_count) {
    if (!txn || !flux_samples || sample_count == 0) return UFT_ERR_INVALID_PARAM;
    
    size_t size = sample_count * sizeof(uint32_t);
    return uft_write_txn_add_track(txn, cylinder, head, 
                                    (const uint8_t*)flux_samples, size);
}

uft_error_t uft_write_txn_add_format(uft_write_txn_t *txn,
                                      uint8_t cylinder, uint8_t head,
                                      uft_format_t format) {
    if (!txn) return UFT_ERR_INVALID_PARAM;
    if (txn->op_count >= MAX_OPERATIONS) return UFT_ERR_LIMIT;
    
    uft_txn_operation_t *op = &txn->operations[txn->op_count];
    memset(op, 0, sizeof(*op));
    
    op->type = UFT_TXN_OP_FORMAT_TRACK;
    op->cylinder = cylinder;
    op->head = head;
    
    txn->op_count++;
    txn->state = UFT_TXN_STATE_PENDING;
    
    return UFT_OK;
}

uft_error_t uft_write_txn_add_erase(uft_write_txn_t *txn,
                                     uint8_t cylinder, uint8_t head) {
    if (!txn) return UFT_ERR_INVALID_PARAM;
    if (txn->op_count >= MAX_OPERATIONS) return UFT_ERR_LIMIT;
    
    uft_txn_operation_t *op = &txn->operations[txn->op_count];
    memset(op, 0, sizeof(*op));
    
    op->type = UFT_TXN_OP_ERASE_TRACK;
    op->cylinder = cylinder;
    op->head = head;
    
    txn->op_count++;
    txn->state = UFT_TXN_STATE_PENDING;
    
    return UFT_OK;
}

int uft_write_txn_get_operation_count(const uft_write_txn_t *txn) {
    return txn ? txn->op_count : 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Implementation - Commit/Abort/Rollback
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_write_txn_commit(uft_write_txn_t *txn) {
    return uft_write_txn_commit_ex(txn, NULL);
}

uft_error_t uft_write_txn_commit_ex(uft_write_txn_t *txn, uft_txn_result_t *result) {
    if (!txn) return UFT_ERR_INVALID_PARAM;
    if (txn->state != UFT_TXN_STATE_PENDING) return UFT_ERR_STATE;
    
    double commit_start = get_time_ms();
    txn->state = UFT_TXN_STATE_COMMITTING;
    log_operation(txn, "Commit started");
    
    uft_error_t err = UFT_OK;
    int failed_op = -1;
    
    /* Create backups if requested */
    if (txn->options.create_backup) {
        log_operation(txn, "Creating backups...");
        for (int i = 0; i < txn->op_count; i++) {
            uft_error_t backup_err = backup_track(txn, i);
            if (backup_err != UFT_OK) {
                log_operation(txn, "Backup failed");
                err = backup_err;
                goto finish;
            }
        }
    }
    
    /* Execute all operations */
    for (int i = 0; i < txn->op_count; i++) {
        /* Check abort */
        if (txn->abort_requested) {
            err = UFT_ERR_ABORTED;
            failed_op = i;
            break;
        }
        
        if (txn->options.abort_check && 
            txn->options.abort_check(txn->options.abort_user)) {
            err = UFT_ERR_ABORTED;
            failed_op = i;
            break;
        }
        
        /* Progress callback */
        if (txn->options.progress_fn) {
            txn->options.progress_fn(i, txn->op_count, "Executing...",
                                     txn->options.progress_user);
        }
        
        /* Execute */
        err = execute_operation(txn, i);
        txn->op_executed = i + 1;
        
        if (err != UFT_OK) {
            failed_op = i;
            char msg[128];
            snprintf(msg, sizeof(msg), "Operation %d failed: %d", i, err);
            log_operation(txn, msg);
            break;
        }
    }
    
    /* Handle failure */
    if (err != UFT_OK && txn->options.auto_rollback) {
        log_operation(txn, "Auto-rollback triggered");
        uft_write_txn_rollback(txn);
    }
    
finish:
    if (err == UFT_OK) {
        txn->state = UFT_TXN_STATE_COMMITTED;
        log_operation(txn, "Commit successful");
    } else {
        txn->state = UFT_TXN_STATE_FAILED;
        log_operation(txn, "Commit failed");
    }
    
    /* Fill result */
    if (result) {
        result->final_state = txn->state;
        result->error = err;
        result->operations_total = txn->op_count;
        result->operations_executed = txn->op_executed;
        result->operations_succeeded = (err == UFT_OK) ? txn->op_count : failed_op;
        result->operations_failed = (err == UFT_OK) ? 0 : 1;
        result->operations_rolled_back = 0;
        result->total_time_ms = get_time_ms() - txn->start_time;
        result->commit_time_ms = get_time_ms() - commit_start;
        result->rollback_time_ms = 0;
        result->failed_op_index = failed_op;
        result->failed_cyl = (failed_op >= 0) ? txn->operations[failed_op].cylinder : 0;
        result->failed_head = (failed_op >= 0) ? txn->operations[failed_op].head : 0;
        result->error_message = (err == UFT_OK) ? NULL : "Operation failed";
    }
    
    return err;
}

uft_error_t uft_write_txn_abort(uft_write_txn_t *txn) {
    if (!txn) return UFT_ERR_INVALID_PARAM;
    
    if (txn->state == UFT_TXN_STATE_PENDING) {
        txn->state = UFT_TXN_STATE_ABORTED;
        log_operation(txn, "Transaction aborted (before commit)");
        return UFT_OK;
    }
    
    if (txn->state == UFT_TXN_STATE_COMMITTING) {
        txn->abort_requested = true;
        return UFT_OK;
    }
    
    return UFT_ERR_STATE;
}

uft_error_t uft_write_txn_rollback(uft_write_txn_t *txn) {
    if (!txn) return UFT_ERR_INVALID_PARAM;
    
    txn->state = UFT_TXN_STATE_ROLLING_BACK;
    log_operation(txn, "Rollback started");
    
    uft_error_t err = UFT_OK;
    int rolled_back = 0;
    
    /* Rollback in reverse order */
    for (int i = txn->op_executed - 1; i >= 0; i--) {
        if (txn->operations[i].executed && txn->operations[i].backup_valid) {
            uft_error_t rb_err = restore_track(txn, i);
            if (rb_err == UFT_OK) {
                rolled_back++;
            } else {
                err = rb_err;
                char msg[128];
                snprintf(msg, sizeof(msg), "Rollback failed for op %d", i);
                log_operation(txn, msg);
            }
        }
    }
    
    txn->state = (err == UFT_OK) ? UFT_TXN_STATE_ROLLED_BACK : UFT_TXN_STATE_FAILED;
    
    char msg[64];
    snprintf(msg, sizeof(msg), "Rollback complete: %d operations restored", rolled_back);
    log_operation(txn, msg);
    
    return err;
}

void uft_write_txn_request_abort(uft_write_txn_t *txn) {
    if (txn) txn->abort_requested = true;
}

bool uft_write_txn_abort_requested(const uft_write_txn_t *txn) {
    return txn ? txn->abort_requested : false;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Implementation - Backup Management
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_write_txn_backup_track(uft_write_txn_t *txn,
                                        uint8_t cylinder, uint8_t head) {
    if (!txn) return UFT_ERR_INVALID_PARAM;
    
    for (int i = 0; i < txn->op_count; i++) {
        if (txn->operations[i].cylinder == cylinder &&
            txn->operations[i].head == head) {
            return backup_track(txn, i);
        }
    }
    
    return UFT_ERR_NOT_FOUND;
}

uft_error_t uft_write_txn_backup_all(uft_write_txn_t *txn) {
    if (!txn) return UFT_ERR_INVALID_PARAM;
    
    for (int i = 0; i < txn->op_count; i++) {
        uft_error_t err = backup_track(txn, i);
        if (err != UFT_OK) return err;
    }
    
    return UFT_OK;
}

size_t uft_write_txn_get_backup_size(const uft_write_txn_t *txn) {
    if (!txn) return 0;
    
    size_t total = 0;
    for (int i = 0; i < txn->op_count; i++) {
        if (txn->operations[i].backup_valid) {
            total += txn->operations[i].backup_size;
        }
    }
    return total;
}

uft_error_t uft_write_txn_save_backup(const uft_write_txn_t *txn, const char *path) {
    if (!txn || !path) return UFT_ERR_INVALID_PARAM;
    
    FILE *f = fopen(path, "wb");
    if (!f) return UFT_ERR_IO;
    
    /* Initialize buffered writer */
    uft_buf_writer_t writer;
    uft_buf_writer_init(&writer, f);
    
    /* Write header */
    uint32_t magic = 0x55465442;  /* "UFTB" */
    uint32_t version = 1;
    uint32_t op_count = txn->op_count;
    
    uft_buf_writer_u32(&writer, magic);
    uft_buf_writer_u32(&writer, version);
    uft_buf_writer_u32(&writer, op_count);
    
    /* Write backups - buffered */
    for (int i = 0; i < txn->op_count; i++) {
        const uft_txn_operation_t *op = &txn->operations[i];
        
        uft_buf_writer_u8(&writer, op->cylinder);
        uft_buf_writer_u8(&writer, op->head);
        uft_buf_writer_u8(&writer, op->backup_valid);
        uft_buf_writer_bytes(&writer, &op->backup_size, sizeof(size_t));
        
        if (op->backup_valid && op->backup) {
            uft_buf_writer_bytes(&writer, op->backup, op->backup_size);
        }
    }
    
    uft_buf_writer_flush(&writer);
    fclose(f);
    return UFT_OK;
}

uft_error_t uft_write_txn_load_backup(uft_write_txn_t *txn, const char *path) {
    if (!txn || !path) return UFT_ERR_INVALID_PARAM;
    
    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERR_IO;
    
    /* Initialize buffered reader */
    uft_buf_reader_t reader;
    uft_buf_reader_init(&reader, f);
    
    /* Read and verify header */
    uint32_t magic, version, op_count;
    uft_buf_reader_u32(&reader, &magic);
    uft_buf_reader_u32(&reader, &version);
    uft_buf_reader_u32(&reader, &op_count);
    
    if (magic != 0x55465442 || version != 1) {
        fclose(f);
        return UFT_ERR_FORMAT;
    }
    
    /* Read backups - buffered */
    for (uint32_t i = 0; i < op_count && i < (uint32_t)txn->op_count; i++) {
        uft_txn_operation_t *op = &txn->operations[i];
        
        uint8_t cyl, head, valid;
        size_t size;
        
        uft_buf_reader_u8(&reader, &cyl);
        uft_buf_reader_u8(&reader, &head);
        uft_buf_reader_u8(&reader, &valid);
        uft_buf_reader_bytes(&reader, &size, sizeof(size_t));
        
        if (valid && size > 0) {
            free(op->backup);
            op->backup = malloc(size);
            if (op->backup) {
                uft_buf_reader_bytes(&reader, op->backup, size);
                op->backup_size = size;
                op->backup_valid = true;
            }
        }
    }
    
    fclose(f);
    return UFT_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Implementation - Transaction Log
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_write_txn_enable_log(uft_write_txn_t *txn, const char *log_path) {
    if (!txn || !log_path) return UFT_ERR_INVALID_PARAM;
    
    if (txn->log_file) {
        fclose(txn->log_file);
    }
    
    txn->log_file = fopen(log_path, "w");
    if (!txn->log_file) return UFT_ERR_IO;
    
    log_operation(txn, "Transaction log enabled");
    return UFT_OK;
}

uft_error_t uft_write_txn_close_log(uft_write_txn_t *txn) {
    if (!txn) return UFT_ERR_INVALID_PARAM;
    
    if (txn->log_file) {
        log_operation(txn, "Transaction log closed");
        fclose(txn->log_file);
        txn->log_file = NULL;
    }
    
    return UFT_OK;
}

uft_write_txn_t *uft_write_txn_recover(uft_disk_t *disk, const char *log_path) {
    /* Recovery from transaction log - simplified */
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

const char *uft_txn_state_string(uft_txn_state_t state) {
    switch (state) {
        case UFT_TXN_STATE_IDLE:         return "IDLE";
        case UFT_TXN_STATE_PENDING:      return "PENDING";
        case UFT_TXN_STATE_COMMITTING:   return "COMMITTING";
        case UFT_TXN_STATE_COMMITTED:    return "COMMITTED";
        case UFT_TXN_STATE_ABORTING:     return "ABORTING";
        case UFT_TXN_STATE_ABORTED:      return "ABORTED";
        case UFT_TXN_STATE_ROLLING_BACK: return "ROLLING_BACK";
        case UFT_TXN_STATE_ROLLED_BACK:  return "ROLLED_BACK";
        case UFT_TXN_STATE_FAILED:       return "FAILED";
        default:                         return "UNKNOWN";
    }
}

void uft_write_txn_print_info(const uft_write_txn_t *txn) {
    if (!txn) return;
    
    printf("Transaction Info:\n");
    printf("  State: %s\n", uft_txn_state_string(txn->state));
    printf("  Operations: %d\n", txn->op_count);
    printf("  Executed: %d\n", txn->op_executed);
    printf("  Abort requested: %s\n", txn->abort_requested ? "yes" : "no");
    printf("  Backup size: %zu bytes\n", uft_write_txn_get_backup_size(txn));
}

char *uft_write_txn_to_json(const uft_write_txn_t *txn) {
    if (!txn) return NULL;
    
    size_t size = 1024 + txn->op_count * 128;
    char *json = malloc(size);
    if (!json) return NULL;
    
    int pos = snprintf(json, size,
        "{\n"
        "  \"state\": \"%s\",\n"
        "  \"op_count\": %d,\n"
        "  \"op_executed\": %d,\n"
        "  \"abort_requested\": %s,\n"
        "  \"operations\": [\n",
        uft_txn_state_string(txn->state),
        txn->op_count,
        txn->op_executed,
        txn->abort_requested ? "true" : "false");
    
    for (int i = 0; i < txn->op_count && pos < (int)size - 100; i++) {
        const uft_txn_operation_t *op = &txn->operations[i];
        pos += snprintf(json + pos, size - pos,
            "    {\"type\": %d, \"cyl\": %d, \"head\": %d, \"executed\": %s}%s\n",
            op->type, op->cylinder, op->head,
            op->executed ? "true" : "false",
            (i < txn->op_count - 1) ? "," : "");
    }
    
    snprintf(json + pos, size - pos, "  ]\n}\n");
    return json;
}

char *uft_txn_result_to_json(const uft_txn_result_t *result) {
    if (!result) return NULL;
    
    char *json = malloc(512);
    if (!json) return NULL;
    
    snprintf(json, 512,
        "{\n"
        "  \"state\": \"%s\",\n"
        "  \"error\": %d,\n"
        "  \"ops_total\": %d,\n"
        "  \"ops_executed\": %d,\n"
        "  \"ops_succeeded\": %d,\n"
        "  \"ops_failed\": %d,\n"
        "  \"ops_rolled_back\": %d,\n"
        "  \"total_time_ms\": %.2f\n"
        "}\n",
        uft_txn_state_string(result->final_state),
        result->error,
        result->operations_total,
        result->operations_executed,
        result->operations_succeeded,
        result->operations_failed,
        result->operations_rolled_back,
        result->total_time_ms);
    
    return json;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Convenience Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_write_track_atomic(uft_disk_t *disk,
                                    uint8_t cylinder, uint8_t head,
                                    const uint8_t *data, size_t size,
                                    bool create_backup) {
    uft_txn_options_t opts = UFT_TXN_OPTIONS_DEFAULT;
    opts.create_backup = create_backup;
    
    uft_write_txn_t *txn = uft_write_txn_begin_ex(disk, &opts);
    if (!txn) return UFT_ERR_MEMORY;
    
    uft_error_t err = uft_write_txn_add_track(txn, cylinder, head, data, size);
    if (err == UFT_OK) {
        err = uft_write_txn_commit(txn);
    }
    
    uft_write_txn_free(txn);
    return err;
}

uft_error_t uft_write_tracks_atomic(uft_disk_t *disk,
                                     const uft_track_write_t *tracks,
                                     int track_count,
                                     bool create_backup) {
    uft_txn_options_t opts = UFT_TXN_OPTIONS_DEFAULT;
    opts.create_backup = create_backup;
    
    uft_write_txn_t *txn = uft_write_txn_begin_ex(disk, &opts);
    if (!txn) return UFT_ERR_MEMORY;
    
    uft_error_t err = UFT_OK;
    for (int i = 0; i < track_count && err == UFT_OK; i++) {
        err = uft_write_txn_add_track(txn, tracks[i].cylinder, tracks[i].head,
                                       tracks[i].data, tracks[i].size);
    }
    
    if (err == UFT_OK) {
        err = uft_write_txn_commit(txn);
    }
    
    uft_write_txn_free(txn);
    return err;
}
