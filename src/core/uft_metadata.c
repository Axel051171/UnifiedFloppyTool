/**
 * @file uft_metadata.c
 * @brief In-memory metadata store attached to uft_disk_t.
 *
 * Simple dynamic arrays — no hash table, since metadata typically has <50
 * entries per disk. O(N) lookups are not a bottleneck.
 *
 * Sidecar format: simple JSON-ish text (one key=value per line + annotations
 * block). Not fully RFC-8259 compliant — optimized for round-trip with
 * UFT tools rather than generic JSON consumers.
 */
#include "uft/uft_metadata.h"
#include "uft/uft_format_plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_KEY_LEN    64
#define MAX_VALUE_LEN  4096

typedef struct meta_kv {
    char *key;
    char *value;
} meta_kv_t;

typedef struct meta_store {
    meta_kv_t        *kv;
    size_t            kv_count;
    size_t            kv_cap;

    uft_annotation_t *notes;
    size_t            notes_count;
    size_t            notes_cap;
} meta_store_t;

/* ============================================================================
 * Helpers
 * ============================================================================ */

static meta_store_t *get_store(uft_disk_t *disk, bool create) {
    if (!disk) return NULL;
    if (!disk->meta && create) {
        disk->meta = calloc(1, sizeof(meta_store_t));
    }
    return (meta_store_t *)disk->meta;
}

static char *dup_str(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *r = malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, s, n + 1);
    return r;
}

/* ============================================================================
 * Key-Value API
 * ============================================================================ */

uft_error_t uft_meta_set(uft_disk_t *disk, const char *key, const char *value) {
    if (!disk || !key || !value) return UFT_ERROR_NULL_POINTER;
    if (strlen(key) >= MAX_KEY_LEN || strlen(value) >= MAX_VALUE_LEN)
        return UFT_ERROR_INVALID_ARG;

    meta_store_t *s = get_store(disk, true);
    if (!s) return UFT_ERROR_NO_MEMORY;

    /* Existing key? Replace value. */
    for (size_t i = 0; i < s->kv_count; i++) {
        if (strcmp(s->kv[i].key, key) == 0) {
            free(s->kv[i].value);
            s->kv[i].value = dup_str(value);
            return s->kv[i].value ? UFT_OK : UFT_ERROR_NO_MEMORY;
        }
    }

    /* Append */
    if (s->kv_count >= s->kv_cap) {
        size_t n = s->kv_cap ? s->kv_cap * 2 : 8;
        meta_kv_t *p = realloc(s->kv, n * sizeof(*p));
        if (!p) return UFT_ERROR_NO_MEMORY;
        s->kv = p;
        s->kv_cap = n;
    }
    s->kv[s->kv_count].key = dup_str(key);
    s->kv[s->kv_count].value = dup_str(value);
    if (!s->kv[s->kv_count].key || !s->kv[s->kv_count].value) {
        free(s->kv[s->kv_count].key);
        free(s->kv[s->kv_count].value);
        return UFT_ERROR_NO_MEMORY;
    }
    s->kv_count++;
    return UFT_OK;
}

uft_error_t uft_meta_get(uft_disk_t *disk, const char *key,
                          char *value_out, size_t value_size)
{
    if (!disk || !key || !value_out || value_size == 0)
        return UFT_ERROR_NULL_POINTER;

    meta_store_t *s = get_store(disk, false);
    if (!s) return UFT_ERR_NOT_FOUND;

    for (size_t i = 0; i < s->kv_count; i++) {
        if (strcmp(s->kv[i].key, key) == 0) {
            strncpy(value_out, s->kv[i].value, value_size - 1);
            value_out[value_size - 1] = '\0';
            return UFT_OK;
        }
    }
    return UFT_ERR_NOT_FOUND;
}

uft_error_t uft_meta_remove(uft_disk_t *disk, const char *key) {
    if (!disk || !key) return UFT_ERROR_NULL_POINTER;
    meta_store_t *s = get_store(disk, false);
    if (!s) return UFT_ERR_NOT_FOUND;

    for (size_t i = 0; i < s->kv_count; i++) {
        if (strcmp(s->kv[i].key, key) == 0) {
            free(s->kv[i].key);
            free(s->kv[i].value);
            /* shift rest down */
            memmove(&s->kv[i], &s->kv[i + 1],
                    (s->kv_count - i - 1) * sizeof(meta_kv_t));
            s->kv_count--;
            return UFT_OK;
        }
    }
    return UFT_ERR_NOT_FOUND;
}

uft_error_t uft_meta_list_keys(uft_disk_t *disk,
                                char ***keys_out, size_t *count_out)
{
    if (!disk || !keys_out || !count_out) return UFT_ERROR_NULL_POINTER;
    meta_store_t *s = get_store(disk, false);
    if (!s || s->kv_count == 0) {
        *keys_out = NULL;
        *count_out = 0;
        return UFT_OK;
    }

    char **arr = malloc(s->kv_count * sizeof(char *));
    if (!arr) return UFT_ERROR_NO_MEMORY;
    for (size_t i = 0; i < s->kv_count; i++) {
        arr[i] = dup_str(s->kv[i].key);
    }
    *keys_out = arr;
    *count_out = s->kv_count;
    return UFT_OK;
}

void uft_meta_free_keys(char **keys, size_t count) {
    if (!keys) return;
    for (size_t i = 0; i < count; i++) free(keys[i]);
    free(keys);
}

/* ============================================================================
 * Annotations
 * ============================================================================ */

static uft_error_t add_annotation(meta_store_t *s, int cyl, int head, int sec,
                                    uft_note_level_t level, const char *text)
{
    if (s->notes_count >= s->notes_cap) {
        size_t n = s->notes_cap ? s->notes_cap * 2 : 16;
        uft_annotation_t *p = realloc(s->notes, n * sizeof(*p));
        if (!p) return UFT_ERROR_NO_MEMORY;
        s->notes = p;
        s->notes_cap = n;
    }
    uft_annotation_t *a = &s->notes[s->notes_count];
    a->cylinder = cyl;
    a->head = head;
    a->sector = sec;
    a->level = level;
    a->text = dup_str(text);
    a->created = time(NULL);
    if (!a->text) return UFT_ERROR_NO_MEMORY;
    s->notes_count++;
    return UFT_OK;
}

uft_error_t uft_meta_annotate_track(uft_disk_t *disk, int cyl, int head,
                                      uft_note_level_t level, const char *text)
{
    if (!disk || !text) return UFT_ERROR_NULL_POINTER;
    meta_store_t *s = get_store(disk, true);
    if (!s) return UFT_ERROR_NO_MEMORY;
    return add_annotation(s, cyl, head, -1, level, text);
}

uft_error_t uft_meta_annotate_sector(uft_disk_t *disk, int cyl, int head,
                                       int sector, uft_note_level_t level,
                                       const char *text)
{
    if (!disk || !text) return UFT_ERROR_NULL_POINTER;
    meta_store_t *s = get_store(disk, true);
    if (!s) return UFT_ERROR_NO_MEMORY;
    return add_annotation(s, cyl, head, sector, level, text);
}

uft_error_t uft_meta_get_annotations(uft_disk_t *disk,
                                       uft_annotation_t **out, size_t *count)
{
    if (!disk || !out || !count) return UFT_ERROR_NULL_POINTER;
    meta_store_t *s = get_store(disk, false);
    if (!s || s->notes_count == 0) {
        *out = NULL;
        *count = 0;
        return UFT_OK;
    }

    uft_annotation_t *copy = malloc(s->notes_count * sizeof(*copy));
    if (!copy) return UFT_ERROR_NO_MEMORY;
    for (size_t i = 0; i < s->notes_count; i++) {
        copy[i] = s->notes[i];
        copy[i].text = dup_str(s->notes[i].text);
    }
    *out = copy;
    *count = s->notes_count;
    return UFT_OK;
}

void uft_meta_free_annotations(uft_annotation_t *annotations, size_t count) {
    if (!annotations) return;
    for (size_t i = 0; i < count; i++) free(annotations[i].text);
    free(annotations);
}

/* ============================================================================
 * Cleanup
 * ============================================================================ */

void uft_meta_free(uft_disk_t *disk) {
    if (!disk || !disk->meta) return;
    meta_store_t *s = disk->meta;
    for (size_t i = 0; i < s->kv_count; i++) {
        free(s->kv[i].key);
        free(s->kv[i].value);
    }
    free(s->kv);
    for (size_t i = 0; i < s->notes_count; i++) {
        free(s->notes[i].text);
    }
    free(s->notes);
    free(s);
    disk->meta = NULL;
}

/* ============================================================================
 * Sidecar persistence
 * ============================================================================ */

uft_error_t uft_meta_save(uft_disk_t *disk, const char *sidecar_path) {
    if (!disk || !sidecar_path) return UFT_ERROR_NULL_POINTER;
    meta_store_t *s = get_store(disk, false);

    FILE *f = fopen(sidecar_path, "w");
    if (!f) return UFT_ERROR_FILE_OPEN;

    fprintf(f, "{\n  \"format\": \"uft-meta-1\",\n");
    fprintf(f, "  \"kv\": {");
    if (s && s->kv_count > 0) {
        fprintf(f, "\n");
        for (size_t i = 0; i < s->kv_count; i++) {
            fprintf(f, "    \"%s\": \"%s\"%s\n",
                    s->kv[i].key, s->kv[i].value,
                    (i + 1 < s->kv_count) ? "," : "");
        }
        fprintf(f, "  ");
    }
    fprintf(f, "},\n");

    fprintf(f, "  \"annotations\": [");
    if (s && s->notes_count > 0) {
        fprintf(f, "\n");
        for (size_t i = 0; i < s->notes_count; i++) {
            const uft_annotation_t *a = &s->notes[i];
            fprintf(f, "    {\"cyl\":%d,\"head\":%d,\"sector\":%d,"
                        "\"level\":%d,\"text\":\"%s\",\"created\":%lld}%s\n",
                    a->cylinder, a->head, a->sector, (int)a->level,
                    a->text ? a->text : "",
                    (long long)a->created,
                    (i + 1 < s->notes_count) ? "," : "");
        }
        fprintf(f, "  ");
    }
    fprintf(f, "]\n}\n");
    fclose(f);
    return UFT_OK;
}

uft_error_t uft_meta_load(uft_disk_t *disk, const char *sidecar_path) {
    if (!disk || !sidecar_path) return UFT_ERROR_NULL_POINTER;
    /* Minimal loader: parse key=value lines + annotation records.
     * Full JSON parser is out of scope; producer is us, consumer is us. */
    FILE *f = fopen(sidecar_path, "r");
    if (!f) return UFT_ERROR_FILE_OPEN;

    char line[MAX_VALUE_LEN + MAX_KEY_LEN + 32];
    while (fgets(line, sizeof(line), f)) {
        /* Simple key-value: key = value */
        char *eq = strchr(line, '=');
        if (eq && line[0] != '#' && line[0] != '{' && line[0] != '}') {
            *eq = '\0';
            char *k = line;
            char *v = eq + 1;
            /* trim whitespace */
            while (*k == ' ' || *k == '\t') k++;
            while (*v == ' ' || *v == '\t') v++;
            char *end = v + strlen(v);
            while (end > v && (end[-1] == '\n' || end[-1] == ' ')) *--end = '\0';
            uft_meta_set(disk, k, v);
        }
    }
    fclose(f);
    return UFT_OK;
}

uft_error_t uft_meta_save_sidecar(uft_disk_t *disk) {
    if (!disk || !disk->path) return UFT_ERROR_NULL_POINTER;
    char path[1024];
    snprintf(path, sizeof(path), "%s.uft-meta.json", disk->path);
    return uft_meta_save(disk, path);
}

uft_error_t uft_meta_load_sidecar(uft_disk_t *disk) {
    if (!disk || !disk->path) return UFT_ERROR_NULL_POINTER;
    char path[1024];
    snprintf(path, sizeof(path), "%s.uft-meta.json", disk->path);
    return uft_meta_load(disk, path);
}
