/**
 * @file tests/emulators/applesauce/firmware_state_machine.c
 * @brief Implementation of the Applesauce ASCII-line firmware model.
 *
 * See firmware_state_machine.h for the protocol-truth hierarchy and the
 * forensic invariants. Every response this file produces is traceable to
 * a documented command in src/hardware_providers/applesauce_serial_runners.cpp
 * (truth source #1) or the status-char table in applesauce_provider_v2.h
 * (#2). Where neither pins a behaviour down, the choice is recorded in
 * DIVERGENCES.md rather than guessed silently.
 */

#include "firmware_state_machine.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ─── small helpers ─────────────────────────────────────────────────── */

static as_fw_resp_class_t emit(char *resp, size_t cap,
                               as_fw_resp_class_t cls, const char *text) {
    if (resp && cap > 0) {
        if (text) {
            snprintf(resp, cap, "%s", text);
        } else {
            resp[0] = '\0';
        }
    }
    return cls;
}

/** Match `line` against `cmd` exactly (whole line, no argument). */
static bool is_cmd(const char *line, const char *cmd) {
    return strcmp(line, cmd) == 0;
}

/** Match `line` against `prefix` and return the argument tail (may be
 *  empty). Returns NULL if the prefix does not match. */
static const char *arg_after(const char *line, const char *prefix) {
    size_t n = strlen(prefix);
    if (strncmp(line, prefix, n) != 0) return NULL;
    return line + n;
}

static const char *kind_str(as_fw_drive_kind_t k) {
    switch (k) {
        case AS_FW_KIND_525:  return "5.25";
        case AS_FW_KIND_35:   return "3.5";
        case AS_FW_KIND_PC:   return "PC";
        case AS_FW_KIND_NONE: /* fallthrough */
        default:              return "NONE";
    }
}

/* ─── lifecycle ─────────────────────────────────────────────────────── */

void as_fw_reset(as_fw_t *fw) {
    if (!fw) return;
    memset(fw, 0, sizeof(*fw));
    fw->state         = AS_FW_STATE_POWER_ON;
    fw->drive_kind    = AS_FW_KIND_NONE;
    fw->buffer_max    = AS_FW_BUFFER_STANDARD;
    fw->current_track = -1;
    fw->current_side  = 0;
    fw->rpm           = 0.0;
    snprintf(fw->version_str, sizeof(fw->version_str), "%s", "Applesauce 0.0");
    snprintf(fw->pcb_str,     sizeof(fw->pcb_str),     "%s", "0.0");
}

void as_fw_power_on_defaults(as_fw_t *fw) {
    if (!fw) return;
    as_fw_reset(fw);
    fw->drive_kind      = AS_FW_KIND_525;
    fw->buffer_max      = AS_FW_BUFFER_PLUS;
    fw->disk_present    = true;
    fw->write_protected = false;
    fw->index_present   = true;
    fw->rpm             = 300.0;
    snprintf(fw->version_str, sizeof(fw->version_str), "%s", "Applesauce 2.05");
    snprintf(fw->pcb_str,     sizeof(fw->pcb_str),     "%s", "2.1");
}

void as_fw_set_drive_kind(as_fw_t *fw, as_fw_drive_kind_t kind) {
    if (fw) fw->drive_kind = kind;
}
void as_fw_set_disk_present(as_fw_t *fw, bool present) {
    if (fw) fw->disk_present = present;
}
void as_fw_set_write_protected(as_fw_t *fw, bool wp) {
    if (fw) fw->write_protected = wp;
}
void as_fw_set_index_present(as_fw_t *fw, bool present) {
    if (fw) fw->index_present = present;
}
void as_fw_set_rpm(as_fw_t *fw, double rpm) {
    if (fw) fw->rpm = rpm;
}
void as_fw_set_buffer_max(as_fw_t *fw, uint32_t bytes) {
    if (fw) fw->buffer_max = bytes;
}
void as_fw_set_version(as_fw_t *fw, const char *vers, const char *pcb) {
    if (!fw) return;
    if (vers) snprintf(fw->version_str, sizeof(fw->version_str), "%s", vers);
    if (pcb)  snprintf(fw->pcb_str,     sizeof(fw->pcb_str),     "%s", pcb);
}
void as_fw_set_silent_next(as_fw_t *fw, int n) {
    if (fw) fw->silent_next = n;
}

void as_fw_load_capture(as_fw_t *fw, const uint8_t *bytes, size_t len) {
    if (!fw) return;
    fw->capture     = bytes;
    fw->capture_len = len;
}

/* ─── command handlers ──────────────────────────────────────────────── */

/* All handlers return the response class and fill `resp`. They may only
 * change state on success — no silent partial mutation. */

static as_fw_resp_class_t handle_psu(as_fw_t *fw, const char *line,
                                     char *resp, size_t cap) {
    if (is_cmd(line, "psu:?")) {
        return emit(resp, cap, AS_FW_RESP_OK_PAYLOAD,
                    fw->psu_on ? "+on" : "+off");
    }
    if (is_cmd(line, "psu:on")) {
        fw->psu_on = true;
        if (fw->state == AS_FW_STATE_POWER_ON) fw->state = AS_FW_STATE_PSU_ON;
        return emit(resp, cap, AS_FW_RESP_ACK, ".");
    }
    if (is_cmd(line, "psu:off")) {
        fw->psu_on = fw->motor_on = fw->sync_on = false;
        fw->state = AS_FW_STATE_POWER_ON;
        return emit(resp, cap, AS_FW_RESP_ACK, ".");
    }
    return AS_FW_RESP_UNKNOWN;
}

static as_fw_resp_class_t handle_motor(as_fw_t *fw, const char *line,
                                       char *resp, size_t cap) {
    if (is_cmd(line, "motor:on")) {
        if (!fw->psu_on) return emit(resp, cap, AS_FW_RESP_NO_POWER, "v");
        fw->motor_on = true;
        if (fw->state < AS_FW_STATE_MOTOR_ON) fw->state = AS_FW_STATE_MOTOR_ON;
        return emit(resp, cap, AS_FW_RESP_ACK, ".");
    }
    if (is_cmd(line, "motor:off")) {
        fw->motor_on = fw->sync_on = false;
        if (fw->state > AS_FW_STATE_PSU_ON) fw->state = AS_FW_STATE_PSU_ON;
        return emit(resp, cap, AS_FW_RESP_ACK, ".");
    }
    return AS_FW_RESP_UNKNOWN;
}

static as_fw_resp_class_t handle_sync(as_fw_t *fw, const char *line,
                                      char *resp, size_t cap) {
    if (is_cmd(line, "sync:on")) {
        if (!fw->motor_on)
            return emit(resp, cap, AS_FW_RESP_ERROR, "-motor off");
        fw->sync_on = true;
        if (fw->state < AS_FW_STATE_SYNC_ON) fw->state = AS_FW_STATE_SYNC_ON;
        return emit(resp, cap, AS_FW_RESP_ACK, ".");
    }
    if (is_cmd(line, "sync:off")) {
        fw->sync_on = false;
        if (fw->state > AS_FW_STATE_MOTOR_ON) fw->state = AS_FW_STATE_MOTOR_ON;
        return emit(resp, cap, AS_FW_RESP_ACK, ".");
    }
    if (is_cmd(line, "sync:?speed")) {
        if (!fw->index_present)
            return emit(resp, cap, AS_FW_RESP_ERROR, "-no index");
        char buf[AS_FW_LINE_MAX];
        snprintf(buf, sizeof(buf), "+%.2f", fw->rpm);
        return emit(resp, cap, AS_FW_RESP_OK_PAYLOAD, buf);
    }
    return AS_FW_RESP_UNKNOWN;
}

static as_fw_resp_class_t handle_head(as_fw_t *fw, const char *line,
                                      char *resp, size_t cap) {
    const char *a;
    if ((a = arg_after(line, "head:track ")) != NULL) {
        if (!fw->psu_on) return emit(resp, cap, AS_FW_RESP_NO_POWER, "v");
        char *end = NULL;
        long t = strtol(a, &end, 10);
        if (end == a || *end != '\0' || t < 0 || t > AS_FW_MAX_TRACK)
            return emit(resp, cap, AS_FW_RESP_ERROR, "-bad track");
        fw->current_track = (int)t;
        return emit(resp, cap, AS_FW_RESP_ACK, ".");
    }
    if ((a = arg_after(line, "head:side ")) != NULL) {
        char *end = NULL;
        long s = strtol(a, &end, 10);
        if (end == a || *end != '\0' || (s != 0 && s != 1))
            return emit(resp, cap, AS_FW_RESP_ERROR, "-bad side");
        fw->current_side = (int)s;
        return emit(resp, cap, AS_FW_RESP_ACK, ".");
    }
    if (is_cmd(line, "head:zero")) {
        if (!fw->psu_on) return emit(resp, cap, AS_FW_RESP_NO_POWER, "v");
        fw->current_track = 0;
        return emit(resp, cap, AS_FW_RESP_ACK, ".");
    }
    return AS_FW_RESP_UNKNOWN;
}

static as_fw_resp_class_t handle_disk(as_fw_t *fw, const char *line,
                                      char *resp, size_t cap) {
    const char *a;
    if ((a = arg_after(line, "disk:readx ")) != NULL) {
        if (fw->state < AS_FW_STATE_SYNC_ON)
            return emit(resp, cap, AS_FW_RESP_ERROR, "-not synced");
        if (!fw->disk_present)
            return emit(resp, cap, AS_FW_RESP_ERROR, "-no disk");
        if (!fw->index_present)
            return emit(resp, cap, AS_FW_RESP_ERROR, "-no index");
        char *end = NULL;
        long r = strtol(a, &end, 10);
        if (end == a || *end != '\0' || r < AS_FW_MIN_REVS || r > AS_FW_MAX_REVS)
            return emit(resp, cap, AS_FW_RESP_ERROR, "-bad revs");
        if (!fw->capture || fw->capture_len == 0)
            return emit(resp, cap, AS_FW_RESP_ERROR, "-empty media");
        /* "Capture": the buffer holds exactly what the generator built. */
        fw->captured_len  = fw->capture_len;
        fw->captured_revs = (int)r;
        fw->state         = AS_FW_STATE_CAPTURED;
        return emit(resp, cap, AS_FW_RESP_ACK, ".");
    }
    if (is_cmd(line, "disk:?write")) {
        return emit(resp, cap, AS_FW_RESP_OK_PAYLOAD,
                    fw->write_protected ? "+protected" : "+writable");
    }
    if (is_cmd(line, "disk:write")) {
        /* Forensic-safety: writes are ALWAYS refused, regardless of
         * media state. Matches the UFT HAL's write gate. */
        return emit(resp, cap, AS_FW_RESP_PROTOCOL, "!");
    }
    return AS_FW_RESP_UNKNOWN;
}

static as_fw_resp_class_t handle_data(as_fw_t *fw, const char *line,
                                      char *resp, size_t cap) {
    const char *a;
    if (is_cmd(line, "data:?size")) {
        char buf[AS_FW_LINE_MAX];
        snprintf(buf, sizeof(buf), "+%zu", fw->captured_len);
        return emit(resp, cap, AS_FW_RESP_OK_PAYLOAD, buf);
    }
    if (is_cmd(line, "data:?max")) {
        char buf[AS_FW_LINE_MAX];
        snprintf(buf, sizeof(buf), "+%u", fw->buffer_max);
        return emit(resp, cap, AS_FW_RESP_OK_PAYLOAD, buf);
    }
    if (is_cmd(line, "data:clear")) {
        fw->captured_len = 0;
        fw->captured_revs = 0;
        if (fw->state == AS_FW_STATE_CAPTURED)
            fw->state = fw->sync_on ? AS_FW_STATE_SYNC_ON
                                    : AS_FW_STATE_MOTOR_ON;
        return emit(resp, cap, AS_FW_RESP_ACK, ".");
    }
    if ((a = arg_after(line, "data:< ")) != NULL) {
        if (fw->state != AS_FW_STATE_CAPTURED || fw->captured_len == 0)
            return emit(resp, cap, AS_FW_RESP_ERROR, "-no capture");
        char *end = NULL;
        long n = strtol(a, &end, 10);
        if (end == a || *end != '\0' || n <= 0 ||
            (size_t)n > fw->captured_len)
            return emit(resp, cap, AS_FW_RESP_ERROR, "-bad length");
        fw->binary_offset    = 0;
        fw->binary_remaining = (size_t)n;
        fw->state            = AS_FW_STATE_BINARY_TX;
        /* No text line — the caller drains via as_fw_pop_binary. */
        return emit(resp, cap, AS_FW_RESP_BINARY, NULL);
    }
    if (arg_after(line, "data:> ") != NULL) {
        /* Write upload — refused for forensic safety, same as disk:write. */
        return emit(resp, cap, AS_FW_RESP_PROTOCOL, "!");
    }
    return AS_FW_RESP_UNKNOWN;
}

static as_fw_resp_class_t handle_query(as_fw_t *fw, const char *line,
                                       char *resp, size_t cap) {
    if (is_cmd(line, "?kind")) {
        char buf[AS_FW_LINE_MAX];
        snprintf(buf, sizeof(buf), "+%s", kind_str(fw->drive_kind));
        return emit(resp, cap, AS_FW_RESP_OK_PAYLOAD, buf);
    }
    if (is_cmd(line, "?vers")) {
        char buf[AS_FW_LINE_MAX];
        snprintf(buf, sizeof(buf), "+%s", fw->version_str);
        return emit(resp, cap, AS_FW_RESP_OK_PAYLOAD, buf);
    }
    if (is_cmd(line, "?pcb")) {
        char buf[AS_FW_LINE_MAX];
        snprintf(buf, sizeof(buf), "+%s", fw->pcb_str);
        return emit(resp, cap, AS_FW_RESP_OK_PAYLOAD, buf);
    }
    return AS_FW_RESP_UNKNOWN;
}

/* ─── protocol entry point ──────────────────────────────────────────── */

as_fw_resp_class_t as_fw_process_line(as_fw_t *fw, const char *line,
                                      char *response, size_t response_cap) {
    if (!fw || !line || !response || response_cap == 0)
        return AS_FW_RESP_SILENT;

    fw->cmd_count++;

    /* Fault injection: simulated cable pull / wedged firmware. */
    if (fw->silent_next > 0) {
        fw->silent_next--;
        return emit(response, response_cap, AS_FW_RESP_SILENT, NULL);
    }

    /* Sticky desync: once wedged, only as_fw_reset() recovers (D-5). */
    if (fw->state == AS_FW_STATE_ERROR)
        return emit(response, response_cap, AS_FW_RESP_SILENT, NULL);

    /* A host command line arriving while a binary download is pending is
     * a protocol desync — the host was supposed to drain the bytes. This
     * wedges the device (unrecoverable without reset). */
    if (fw->state == AS_FW_STATE_BINARY_TX) {
        fw->state = AS_FW_STATE_ERROR;
        return emit(response, response_cap, AS_FW_RESP_SILENT, NULL);
    }

    /* Over-length line: malformed framing. */
    if (strlen(line) >= AS_FW_LINE_MAX)
        return emit(response, response_cap, AS_FW_RESP_PROTOCOL, "!");

    as_fw_resp_class_t r;

    if (strncmp(line, "psu:", 4) == 0) {
        r = handle_psu(fw, line, response, response_cap);
    } else if (strncmp(line, "motor:", 6) == 0) {
        r = handle_motor(fw, line, response, response_cap);
    } else if (strncmp(line, "sync:", 5) == 0) {
        r = handle_sync(fw, line, response, response_cap);
    } else if (strncmp(line, "head:", 5) == 0) {
        r = handle_head(fw, line, response, response_cap);
    } else if (strncmp(line, "disk:", 5) == 0) {
        r = handle_disk(fw, line, response, response_cap);
    } else if (strncmp(line, "data:", 5) == 0) {
        r = handle_data(fw, line, response, response_cap);
    } else if (line[0] == '?') {
        r = handle_query(fw, line, response, response_cap);
    } else {
        r = AS_FW_RESP_UNKNOWN;
    }

    if (r == AS_FW_RESP_UNKNOWN)
        return emit(response, response_cap, AS_FW_RESP_UNKNOWN, "?");
    return r;
}

size_t as_fw_pop_binary(as_fw_t *fw, uint8_t *out, size_t want) {
    if (!fw || !out || want == 0) return 0;
    if (fw->state != AS_FW_STATE_BINARY_TX) return 0;
    if (fw->binary_remaining == 0) return 0;
    if (!fw->capture) return 0;

    size_t n = want < fw->binary_remaining ? want : fw->binary_remaining;
    /* Stream from the loaded capture at the running offset. */
    if (fw->binary_offset + n > fw->capture_len)
        n = fw->capture_len - fw->binary_offset;
    memcpy(out, fw->capture + fw->binary_offset, n);
    fw->binary_offset    += n;
    fw->binary_remaining -= n;
    fw->bytes_tx_to_host += n;

    if (fw->binary_remaining == 0)
        fw->state = AS_FW_STATE_CAPTURED;   /* transfer complete */
    return n;
}
