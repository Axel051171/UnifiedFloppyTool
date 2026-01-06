/**
 * @file uft_session.c
 * @brief Session State Management Implementation
 * 
 * TICKET-005: Session State Persistence
 * Auto-save, crash recovery, and session management
 */

#include "uft/uft_session.h"
#include "uft/uft_param_bridge.h"
#include "uft/uft_memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#define PATH_SEP "\\"
#else
#include <pthread.h>
#include <signal.h>
#define PATH_SEP "/"
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define SESSION_MAGIC       0x55465453  /* "UFTS" */
#define SESSION_VERSION     1
#define MAX_TRACKS          400
#define LOCK_FILE_EXT       ".lock"
#define BACKUP_FILE_EXT     ".backup"
#define SESSION_FILE_EXT    ".json"

static char g_default_session_path[512] = {0};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal Structures
 * ═══════════════════════════════════════════════════════════════════════════════ */

struct uft_session {
    uft_session_info_t  info;
    uft_session_options_t options;
    
    uft_session_track_t tracks[MAX_TRACKS];
    int                 track_count;
    
    struct uft_params   *params;
    char                *preset_name;
    
    char                *lock_path;
    FILE                *lock_file;
    
    bool                autosave_enabled;
    time_t              last_autosave;
    
#ifndef _WIN32
    pthread_t           autosave_thread;
    bool                autosave_running;
#endif
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void ensure_directory(const char *path) {
#ifdef _WIN32
    CreateDirectoryA(path, NULL);
#else
    mkdir(path, 0755);
#endif
}

static bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static char *generate_session_id(void) {
    char *id = malloc(32);
    if (!id) return NULL;
    
    time_t now = time(NULL);
    unsigned int rand_part = (unsigned int)(now ^ (intptr_t)id);
    
    snprintf(id, 32, "ses_%08x_%04x", (unsigned int)now, rand_part & 0xFFFF);
    return id;
}

static const char *get_default_session_path(void) {
    if (g_default_session_path[0] == '\0') {
#ifdef _WIN32
        char appdata[MAX_PATH];
        if (SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appdata) == S_OK) {
            snprintf(g_default_session_path, sizeof(g_default_session_path),
                    "%s\\UFT\\sessions", appdata);
        } else {
            strcpy(g_default_session_path, ".\\uft_sessions");
        }
#else
        const char *home = getenv("HOME");
        if (home) {
            snprintf(g_default_session_path, sizeof(g_default_session_path),
                    "%s/.local/share/uft/sessions", home);
        } else {
            strcpy(g_default_session_path, "./uft_sessions");
        }
#endif
    }
    return g_default_session_path;
}

static char *build_session_path(const char *base, const char *id, const char *ext) {
    size_t len = strlen(base) + strlen(id) + strlen(ext) + 16;
    char *path = malloc(len);
    if (path) {
        snprintf(path, len, "%s%s%s%s", base, PATH_SEP, id, ext);
    }
    return path;
}

static bool create_lock_file(uft_session_t *session) {
    if (!session->lock_path) {
        session->lock_path = build_session_path(
            session->options.base_path ? session->options.base_path : get_default_session_path(),
            session->info.id, LOCK_FILE_EXT);
    }
    
    if (!session->lock_path) return false;
    
    session->lock_file = fopen(session->lock_path, "w");
    if (!session->lock_file) return false;
    
    fprintf(session->lock_file, "%d\n%ld\n", getpid(), (long)time(NULL));
    fflush(session->lock_file);
    
    return true;
}

static void remove_lock_file(uft_session_t *session) {
    if (session->lock_file) {
        fclose(session->lock_file);
        session->lock_file = NULL;
    }
    
    if (session->lock_path) {
        unlink(session->lock_path);
        free(session->lock_path);
        session->lock_path = NULL;
    }
}

static bool check_stale_lock(const char *lock_path) {
    FILE *f = fopen(lock_path, "r");
    if (!f) return true;  /* No lock = stale */
    
    int pid;
    long timestamp;
    if (fscanf(f, "%d\n%ld", &pid, &timestamp) != 2) {
        fclose(f);
        return true;
    }
    fclose(f);
    
    /* Check if process is still running */
#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProcess) {
        CloseHandle(hProcess);
        return false;  /* Process exists */
    }
    return true;
#else
    if (kill(pid, 0) == 0) {
        return false;  /* Process exists */
    }
    return true;
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Auto-Save Thread
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef _WIN32
static void *autosave_thread_func(void *arg) {
    uft_session_t *session = (uft_session_t *)arg;
    
    while (session->autosave_running) {
        sleep(session->options.autosave_interval_ms / 1000);
        
        if (!session->autosave_running) break;
        
        time_t now = time(NULL);
        if (now - session->last_autosave >= session->options.autosave_interval_ms / 1000) {
            uft_session_save(session);
            session->last_autosave = now;
            
            if (session->options.on_autosave) {
                session->options.on_autosave(session, session->options.callback_user);
            }
        }
    }
    
    return NULL;
}
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * JSON Serialization
 * ═══════════════════════════════════════════════════════════════════════════════ */

static char *session_to_json(const uft_session_t *session) {
    size_t size = 8192 + session->track_count * 128;
    char *json = malloc(size);
    if (!json) return NULL;
    
    int pos = 0;
    
    pos += snprintf(json + pos, size - pos,
        "{\n"
        "  \"magic\": %u,\n"
        "  \"version\": %d,\n"
        "  \"id\": \"%s\",\n"
        "  \"name\": \"%s\",\n"
        "  \"state\": %d,\n"
        "  \"operation\": %d,\n"
        "  \"created\": %ld,\n"
        "  \"last_modified\": %ld,\n"
        "  \"source_path\": \"%s\",\n"
        "  \"target_path\": \"%s\",\n"
        "  \"source_format\": %d,\n"
        "  \"target_format\": %d,\n"
        "  \"tracks_total\": %d,\n"
        "  \"tracks_completed\": %d,\n"
        "  \"tracks_failed\": %d,\n"
        "  \"current_cylinder\": %d,\n"
        "  \"current_head\": %d,\n"
        "  \"preset\": \"%s\",\n",
        SESSION_MAGIC,
        SESSION_VERSION,
        session->info.id ? session->info.id : "",
        session->info.name ? session->info.name : "",
        session->info.state,
        session->info.operation,
        (long)session->info.created,
        (long)session->info.last_modified,
        session->info.source_path ? session->info.source_path : "",
        session->info.target_path ? session->info.target_path : "",
        session->info.source_format,
        session->info.target_format,
        session->info.tracks_total,
        session->info.tracks_completed,
        session->info.tracks_failed,
        session->info.current_cylinder,
        session->info.current_head,
        session->preset_name ? session->preset_name : "");
    
    /* Tracks array */
    pos += snprintf(json + pos, size - pos, "  \"tracks\": [\n");
    for (int i = 0; i < session->track_count && pos < (int)size - 200; i++) {
        const uft_session_track_t *t = &session->tracks[i];
        pos += snprintf(json + pos, size - pos,
            "    {\"cyl\": %d, \"head\": %d, \"status\": %d, "
            "\"retries\": %d, \"good\": %d, \"bad\": %d}%s\n",
            t->cylinder, t->head, t->status,
            t->retry_count, t->sectors_good, t->sectors_bad,
            (i < session->track_count - 1) ? "," : "");
    }
    pos += snprintf(json + pos, size - pos, "  ],\n");
    
    /* Parameters */
    if (session->params) {
        char *params_json = uft_params_to_json(session->params, false);
        if (params_json) {
            pos += snprintf(json + pos, size - pos, 
                           "  \"params\": %s\n", params_json);
            free(params_json);
        } else {
            pos += snprintf(json + pos, size - pos, "  \"params\": {}\n");
        }
    } else {
        pos += snprintf(json + pos, size - pos, "  \"params\": {}\n");
    }
    
    pos += snprintf(json + pos, size - pos, "}\n");
    
    return json;
}

static uft_session_t *session_from_json(const char *json) {
    if (!json) return NULL;
    
    uft_session_t *session = calloc(1, sizeof(uft_session_t));
    if (!session) return NULL;
    
    /* Parse basic fields - simplified parser */
    const char *p;
    char buf[512];
    
    /* Parse id */
    p = strstr(json, "\"id\":");
    if (p) {
        p = strchr(p, ':') + 1;
        while (*p && (*p == ' ' || *p == '"')) p++;
        int i = 0;
        while (*p && *p != '"' && i < 63) buf[i++] = *p++;
        buf[i] = '\0';
        session->info.id = strdup(buf);
    }
    
    /* Parse name */
    p = strstr(json, "\"name\":");
    if (p) {
        p = strchr(p, ':') + 1;
        while (*p && (*p == ' ' || *p == '"')) p++;
        int i = 0;
        while (*p && *p != '"' && i < 127) buf[i++] = *p++;
        buf[i] = '\0';
        session->info.name = strdup(buf);
    }
    
    /* Parse state */
    p = strstr(json, "\"state\":");
    if (p) {
        session->info.state = atoi(strchr(p, ':') + 1);
    }
    
    /* Parse operation */
    p = strstr(json, "\"operation\":");
    if (p) {
        session->info.operation = atoi(strchr(p, ':') + 1);
    }
    
    /* Parse timestamps */
    p = strstr(json, "\"created\":");
    if (p) {
        session->info.created = atol(strchr(p, ':') + 1);
    }
    
    p = strstr(json, "\"last_modified\":");
    if (p) {
        session->info.last_modified = atol(strchr(p, ':') + 1);
    }
    
    /* Parse source_path */
    p = strstr(json, "\"source_path\":");
    if (p) {
        p = strchr(p, ':') + 1;
        while (*p && (*p == ' ' || *p == '"')) p++;
        int i = 0;
        while (*p && *p != '"' && i < 511) buf[i++] = *p++;
        buf[i] = '\0';
        if (buf[0]) session->info.source_path = strdup(buf);
    }
    
    /* Parse target_path */
    p = strstr(json, "\"target_path\":");
    if (p) {
        p = strchr(p, ':') + 1;
        while (*p && (*p == ' ' || *p == '"')) p++;
        int i = 0;
        while (*p && *p != '"' && i < 511) buf[i++] = *p++;
        buf[i] = '\0';
        if (buf[0]) session->info.target_path = strdup(buf);
    }
    
    /* Parse formats */
    p = strstr(json, "\"source_format\":");
    if (p) {
        session->info.source_format = atoi(strchr(p, ':') + 1);
    }
    
    p = strstr(json, "\"target_format\":");
    if (p) {
        session->info.target_format = atoi(strchr(p, ':') + 1);
    }
    
    /* Parse track counts */
    p = strstr(json, "\"tracks_total\":");
    if (p) {
        session->info.tracks_total = atoi(strchr(p, ':') + 1);
    }
    
    p = strstr(json, "\"tracks_completed\":");
    if (p) {
        session->info.tracks_completed = atoi(strchr(p, ':') + 1);
    }
    
    p = strstr(json, "\"current_cylinder\":");
    if (p) {
        session->info.current_cylinder = atoi(strchr(p, ':') + 1);
    }
    
    p = strstr(json, "\"current_head\":");
    if (p) {
        session->info.current_head = atoi(strchr(p, ':') + 1);
    }
    
    /* Parse preset */
    p = strstr(json, "\"preset\":");
    if (p) {
        p = strchr(p, ':') + 1;
        while (*p && (*p == ' ' || *p == '"')) p++;
        int i = 0;
        while (*p && *p != '"' && i < 63) buf[i++] = *p++;
        buf[i] = '\0';
        if (buf[0]) session->preset_name = strdup(buf);
    }
    
    /* Parse params */
    p = strstr(json, "\"params\":");
    if (p) {
        const char *start = strchr(p, '{');
        if (start) {
            int depth = 1;
            const char *end = start + 1;
            while (*end && depth > 0) {
                if (*end == '{') depth++;
                else if (*end == '}') depth--;
                end++;
            }
            size_t len = end - start;
            char *params_json = malloc(len + 1);
            if (params_json) {
                strncpy(params_json, start, len);
                params_json[len] = '\0';
                session->params = uft_params_from_json(params_json);
                free(params_json);
            }
        }
    }
    
    /* Calculate progress */
    if (session->info.tracks_total > 0) {
        session->info.progress_percent = 
            (float)session->info.tracks_completed / session->info.tracks_total * 100.0f;
    }
    
    return session;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Implementation - Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_session_t *uft_session_create(const char *name) {
    uft_session_options_t opts = UFT_SESSION_OPTIONS_DEFAULT;
    return uft_session_create_ex(name, &opts);
}

uft_session_t *uft_session_create_ex(const char *name, const uft_session_options_t *options) {
    uft_session_t *session = calloc(1, sizeof(uft_session_t));
    if (!session) return NULL;
    
    session->info.id = generate_session_id();
    session->info.name = name ? strdup(name) : strdup("Untitled");
    session->info.state = UFT_SESSION_STATE_NEW;
    session->info.created = time(NULL);
    session->info.last_modified = session->info.created;
    
    session->options = *options;
    if (!session->options.base_path) {
        session->options.base_path = get_default_session_path();
    }
    
    /* Ensure session directory exists */
    ensure_directory(session->options.base_path);
    
    /* Build session path */
    session->info.path = build_session_path(session->options.base_path,
                                             session->info.id, SESSION_FILE_EXT);
    
    /* Create lock file */
    create_lock_file(session);
    
    return session;
}

uft_session_t *uft_session_open(const char *session_id) {
    const char *base = get_default_session_path();
    char *path = build_session_path(base, session_id, SESSION_FILE_EXT);
    if (!path) return NULL;
    
    uft_session_t *session = uft_session_load(path);
    free(path);
    
    return session;
}

uft_session_t *uft_session_load(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    long size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    char *json = malloc(size + 1);
    if (!json) {
        fclose(f);
        return NULL;
    }
    
    if (fread(json, 1, size, f) != size) { /* I/O error */ }
    json[size] = '\0';
    fclose(f);
    
    uft_session_t *session = session_from_json(json);
    free(json);
    
    if (session) {
        session->info.path = strdup(path);
        session->options = (uft_session_options_t)UFT_SESSION_OPTIONS_DEFAULT;
        
        /* Create lock */
        create_lock_file(session);
    }
    
    return session;
}

uft_error_t uft_session_save(uft_session_t *session) {
    if (!session || !session->info.path) return UFT_ERR_INVALID_PARAM;
    
    session->info.last_modified = time(NULL);
    
    /* Create backup if requested */
    if (session->options.create_backup && file_exists(session->info.path)) {
        char *backup_path = malloc(strlen(session->info.path) + 16);
        if (backup_path) {
            snprintf(backup_path, strlen(session->info.path) + 16,
                    "%s%s", session->info.path, BACKUP_FILE_EXT);
            rename(session->info.path, backup_path);
            free(backup_path);
        }
    }
    
    char *json = session_to_json(session);
    if (!json) return UFT_ERR_MEMORY;
    
    FILE *f = fopen(session->info.path, "w");
    if (!f) {
        free(json);
        return UFT_ERR_IO;
    }
    
    fputs(json, f);
    fclose(f);
    free(json);
    
    session->last_autosave = time(NULL);
    
    return UFT_OK;
}

void uft_session_close(uft_session_t *session) {
    if (!session) return;
    
    /* Stop autosave */
    uft_session_disable_autosave(session);
    
    /* Save final state */
    if (session->info.state == UFT_SESSION_STATE_ACTIVE) {
        session->info.state = UFT_SESSION_STATE_PAUSED;
    }
    uft_session_save(session);
    
    /* Remove lock */
    remove_lock_file(session);
    
    /* Free resources */
    free(session->info.id);
    free(session->info.name);
    free(session->info.path);
    free(session->info.source_path);
    free(session->info.target_path);
    free(session->preset_name);
    
    if (session->params) {
        uft_params_free(session->params);
    }
    
    free(session);
}

uft_error_t uft_session_delete(uft_session_t *session) {
    if (!session) return UFT_ERR_INVALID_PARAM;
    
    /* Remove files */
    if (session->info.path) {
        unlink(session->info.path);
        
        /* Remove backup */
        char backup[512];
        snprintf(backup, sizeof(backup), "%s%s", session->info.path, BACKUP_FILE_EXT);
        unlink(backup);
    }
    
    remove_lock_file(session);
    
    /* Free session */
    uft_session_close(session);
    
    return UFT_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Implementation - Auto-Save
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_session_enable_autosave(uft_session_t *session, int interval_ms) {
    if (!session || interval_ms <= 0) return UFT_ERR_INVALID_PARAM;
    
    session->options.autosave_interval_ms = interval_ms;
    session->autosave_enabled = true;
    session->last_autosave = time(NULL);
    
#ifndef _WIN32
    session->autosave_running = true;
    if (pthread_create(&session->autosave_thread, NULL, 
                       autosave_thread_func, session) != 0) {
        session->autosave_running = false;
        return UFT_ERR_SYSTEM;
    }
#endif
    
    return UFT_OK;
}

void uft_session_disable_autosave(uft_session_t *session) {
    if (!session) return;
    
    session->autosave_enabled = false;
    
#ifndef _WIN32
    if (session->autosave_running) {
        session->autosave_running = false;
        pthread_join(session->autosave_thread, NULL);
    }
#endif
}

uft_error_t uft_session_autosave_now(uft_session_t *session) {
    if (!session) return UFT_ERR_INVALID_PARAM;
    return uft_session_save(session);
}

int uft_session_time_since_save(const uft_session_t *session) {
    if (!session) return -1;
    return (int)(time(NULL) - session->last_autosave);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Implementation - Crash Recovery
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool uft_session_has_recovery(void) {
    const char *base = get_default_session_path();
    DIR *dir = opendir(base);
    if (!dir) return false;
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, LOCK_FILE_EXT)) {
            char lock_path[512];
            snprintf(lock_path, sizeof(lock_path), "%s%s%s", 
                    base, PATH_SEP, entry->d_name);
            
            if (check_stale_lock(lock_path)) {
                closedir(dir);
                return true;
            }
        }
    }
    
    closedir(dir);
    return false;
}

uft_session_info_t *uft_session_get_recovery_info(void) {
    const char *base = get_default_session_path();
    DIR *dir = opendir(base);
    if (!dir) return NULL;
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, LOCK_FILE_EXT)) {
            char lock_path[512];
            snprintf(lock_path, sizeof(lock_path), "%s%s%s",
                    base, PATH_SEP, entry->d_name);
            
            if (check_stale_lock(lock_path)) {
                /* Find corresponding session file */
                char session_id[64];
                strncpy(session_id, entry->d_name, sizeof(session_id) - 1);
                char *ext = strstr(session_id, LOCK_FILE_EXT);
                if (ext) *ext = '\0';
                
                uft_session_t *session = uft_session_open(session_id);
                if (session) {
                    session->info.state = UFT_SESSION_STATE_CRASHED;
                    
                    uft_session_info_t *info = malloc(sizeof(uft_session_info_t));
                    if (info) {
                        *info = session->info;
                        /* Don't free strings - they're now owned by info */
                        session->info.id = NULL;
                        session->info.name = NULL;
                        session->info.path = NULL;
                        session->info.source_path = NULL;
                        session->info.target_path = NULL;
                    }
                    
                    free(session);
                    closedir(dir);
                    return info;
                }
            }
        }
    }
    
    closedir(dir);
    return NULL;
}

uft_session_t *uft_session_recover(void) {
    const char *base = get_default_session_path();
    DIR *dir = opendir(base);
    if (!dir) return NULL;
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, LOCK_FILE_EXT)) {
            char lock_path[512];
            snprintf(lock_path, sizeof(lock_path), "%s%s%s",
                    base, PATH_SEP, entry->d_name);
            
            if (check_stale_lock(lock_path)) {
                /* Remove stale lock */
                unlink(lock_path);
                
                /* Extract session ID */
                char session_id[64];
                strncpy(session_id, entry->d_name, sizeof(session_id) - 1);
                char *ext = strstr(session_id, LOCK_FILE_EXT);
                if (ext) *ext = '\0';
                
                uft_session_t *session = uft_session_open(session_id);
                if (session) {
                    session->info.state = UFT_SESSION_STATE_RECOVERED;
                    closedir(dir);
                    return session;
                }
            }
        }
    }
    
    closedir(dir);
    return NULL;
}

uft_error_t uft_session_discard_recovery(void) {
    const char *base = get_default_session_path();
    DIR *dir = opendir(base);
    if (!dir) return UFT_ERR_IO;
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, LOCK_FILE_EXT)) {
            char lock_path[512];
            snprintf(lock_path, sizeof(lock_path), "%s%s%s",
                    base, PATH_SEP, entry->d_name);
            
            if (check_stale_lock(lock_path)) {
                unlink(lock_path);
            }
        }
    }
    
    closedir(dir);
    return UFT_OK;
}

uft_session_info_t **uft_session_list_crashed(int *count) {
    /* Simplified implementation */
    *count = 0;
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Implementation - State Management
 * ═══════════════════════════════════════════════════════════════════════════════ */

const uft_session_info_t *uft_session_get_info(const uft_session_t *session) {
    return session ? &session->info : NULL;
}

void uft_session_set_state(uft_session_t *session, uft_session_state_t state) {
    if (!session) return;
    
    uft_session_state_t old = session->info.state;
    session->info.state = state;
    
    if (session->options.on_state_change) {
        session->options.on_state_change(session, old, state, 
                                         session->options.callback_user);
    }
}

void uft_session_set_operation(uft_session_t *session, uft_session_op_t op) {
    if (session) session->info.operation = op;
}

void uft_session_set_source(uft_session_t *session, const char *path, uft_format_t format) {
    if (!session) return;
    free(session->info.source_path);
    session->info.source_path = path ? strdup(path) : NULL;
    session->info.source_format = format;
}

void uft_session_set_target(uft_session_t *session, const char *path, uft_format_t format) {
    if (!session) return;
    free(session->info.target_path);
    session->info.target_path = path ? strdup(path) : NULL;
    session->info.target_format = format;
}

void uft_session_set_position(uft_session_t *session, int cylinder, int head) {
    if (!session) return;
    session->info.current_cylinder = cylinder;
    session->info.current_head = head;
}

void uft_session_set_track_status(uft_session_t *session, int cylinder, int head,
                                   uft_track_status_t status) {
    if (!session) return;
    
    /* Find or add track */
    for (int i = 0; i < session->track_count; i++) {
        if (session->tracks[i].cylinder == cylinder &&
            session->tracks[i].head == head) {
            session->tracks[i].status = status;
            return;
        }
    }
    
    /* Add new track */
    if (session->track_count < MAX_TRACKS) {
        uft_session_track_t *t = &session->tracks[session->track_count++];
        t->cylinder = cylinder;
        t->head = head;
        t->status = status;
        t->retry_count = 0;
        t->sectors_good = 0;
        t->sectors_bad = 0;
    }
    
    /* Update counters */
    if (status == UFT_TRACK_STATUS_COMPLETE) {
        session->info.tracks_completed++;
    } else if (status == UFT_TRACK_STATUS_FAILED) {
        session->info.tracks_failed++;
    }
    
    if (session->info.tracks_total > 0) {
        session->info.progress_percent = 
            (float)session->info.tracks_completed / session->info.tracks_total * 100.0f;
    }
}

uft_session_track_t *uft_session_get_tracks(const uft_session_t *session, int *count) {
    if (!session) {
        if (count) *count = 0;
        return NULL;
    }
    if (count) *count = session->track_count;
    return (uft_session_track_t *)session->tracks;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Implementation - Parameters
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_session_set_params(uft_session_t *session, const struct uft_params *params) {
    if (!session) return UFT_ERR_INVALID_PARAM;
    
    if (session->params) {
        uft_params_free(session->params);
    }
    
    session->params = params ? uft_params_clone(params) : NULL;
    return UFT_OK;
}

struct uft_params *uft_session_get_params(const uft_session_t *session) {
    return session ? session->params : NULL;
}

void uft_session_set_preset(uft_session_t *session, const char *preset_name) {
    if (!session) return;
    free(session->preset_name);
    session->preset_name = preset_name ? strdup(preset_name) : NULL;
}

const char *uft_session_get_preset(const uft_session_t *session) {
    return session ? session->preset_name : NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Implementation - Results Storage
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_session_save_track_result(uft_session_t *session, int cylinder, int head,
                                           const uint8_t *data, size_t size) {
    if (!session || !data) return UFT_ERR_INVALID_PARAM;
    
    char path[512];
    snprintf(path, sizeof(path), "%s%strack_%02d_%d.bin",
            session->options.base_path, PATH_SEP, cylinder, head);
    
    FILE *f = fopen(path, "wb");
    if (!f) return UFT_ERR_IO;
    
    if (fwrite(data, 1, size, f) != size) { /* I/O error */ }
    fclose(f);
    
    return UFT_OK;
}

uft_error_t uft_session_load_track_result(const uft_session_t *session, int cylinder, int head,
                                           uint8_t **data, size_t *size) {
    if (!session || !data || !size) return UFT_ERR_INVALID_PARAM;
    
    char path[512];
    snprintf(path, sizeof(path), "%s%strack_%02d_%d.bin",
            session->options.base_path, PATH_SEP, cylinder, head);
    
    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERR_NOT_FOUND;
    
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    *size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    *data = malloc(*size);
    if (!*data) {
        fclose(f);
        return UFT_ERR_MEMORY;
    }
    
    if (fread(*data, 1, *size, f) != *size) { /* I/O error */ }
    fclose(f);
    
    return UFT_OK;
}

uft_error_t uft_session_save_report(uft_session_t *session, const char *report_json) {
    if (!session || !report_json) return UFT_ERR_INVALID_PARAM;
    
    char path[512];
    snprintf(path, sizeof(path), "%s%sreport.json",
            session->options.base_path, PATH_SEP);
    
    FILE *f = fopen(path, "w");
    if (!f) return UFT_ERR_IO;
    
    fputs(report_json, f);
    fclose(f);
    
    return UFT_OK;
}

char *uft_session_load_report(const uft_session_t *session) {
    if (!session) return NULL;
    
    char path[512];
    snprintf(path, sizeof(path), "%s%sreport.json",
            session->options.base_path, PATH_SEP);
    
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    long size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    char *json = malloc(size + 1);
    if (!json) {
        fclose(f);
        return NULL;
    }
    
    if (fread(json, 1, size, f) != size) { /* I/O error */ }
    json[size] = '\0';
    fclose(f);
    
    return json;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Implementation - Session List
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_session_info_t **uft_session_list_all(int *count) {
    const char *base = get_default_session_path();
    DIR *dir = opendir(base);
    if (!dir) {
        *count = 0;
        return NULL;
    }
    
    uft_session_info_t **list = calloc(64, sizeof(uft_session_info_t *));
    *count = 0;
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && *count < 63) {
        if (strstr(entry->d_name, SESSION_FILE_EXT) && 
            !strstr(entry->d_name, BACKUP_FILE_EXT)) {
            
            char path[512];
            snprintf(path, sizeof(path), "%s%s%s", base, PATH_SEP, entry->d_name);
            
            uft_session_t *session = uft_session_load(path);
            if (session) {
                list[*count] = malloc(sizeof(uft_session_info_t));
                if (list[*count]) {
                    *list[*count] = session->info;
                    session->info.id = NULL;
                    session->info.name = NULL;
                    session->info.path = NULL;
                    session->info.source_path = NULL;
                    session->info.target_path = NULL;
                    (*count)++;
                }
                free(session);
            }
        }
    }
    
    closedir(dir);
    return list;
}

uft_session_info_t **uft_session_list_by_state(uft_session_state_t state, int *count) {
    int total;
    uft_session_info_t **all = uft_session_list_all(&total);
    if (!all) {
        *count = 0;
        return NULL;
    }
    
    uft_session_info_t **filtered = calloc(total + 1, sizeof(uft_session_info_t *));
    *count = 0;
    
    for (int i = 0; i < total; i++) {
        if (all[i]->state == state) {
            filtered[(*count)++] = all[i];
        } else {
            uft_session_info_free(all[i]);
        }
    }
    
    free(all);
    return filtered;
}

void uft_session_info_free(uft_session_info_t *info) {
    if (!info) return;
    free(info->id);
    free(info->name);
    free(info->path);
    free(info->source_path);
    free(info->target_path);
    free(info);
}

void uft_session_info_list_free(uft_session_info_t **list, int count) {
    if (!list) return;
    for (int i = 0; i < count; i++) {
        uft_session_info_free(list[i]);
    }
    free(list);
}

int uft_session_cleanup(int max_age_days, int max_count) {
    int total;
    uft_session_info_t **all = uft_session_list_all(&total);
    if (!all) return 0;
    
    time_t now = time(NULL);
    time_t max_age_sec = max_age_days * 24 * 60 * 60;
    int removed = 0;
    
    for (int i = 0; i < total; i++) {
        bool should_remove = false;
        
        /* Remove old sessions */
        if (now - all[i]->last_modified > max_age_sec) {
            should_remove = true;
        }
        
        /* Remove excess sessions */
        if (total - removed > max_count && 
            all[i]->state == UFT_SESSION_STATE_COMPLETED) {
            should_remove = true;
        }
        
        if (should_remove && all[i]->path) {
            unlink(all[i]->path);
            removed++;
        }
        
        uft_session_info_free(all[i]);
    }
    
    free(all);
    return removed;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Implementation - Export
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_session_export_cli(const uft_session_t *session, const char *script_path) {
    if (!session || !script_path) return UFT_ERR_INVALID_PARAM;
    
    if (session->params) {
        return uft_params_export_shell(session->params, script_path,
                                        session->info.source_path,
                                        session->info.target_path);
    }
    
    return UFT_ERR_NO_DATA;
}

char *uft_session_to_json(const uft_session_t *session) {
    return session ? session_to_json(session) : NULL;
}

void uft_session_print_summary(const uft_session_t *session) {
    if (!session) return;
    
    printf("Session: %s (%s)\n", session->info.name, session->info.id);
    printf("  State: %s\n", uft_session_state_string(session->info.state));
    printf("  Operation: %s\n", uft_session_op_string(session->info.operation));
    printf("  Progress: %.1f%% (%d/%d tracks)\n",
           session->info.progress_percent,
           session->info.tracks_completed,
           session->info.tracks_total);
    printf("  Position: Cyl %d, Head %d\n",
           session->info.current_cylinder,
           session->info.current_head);
    if (session->info.source_path) {
        printf("  Source: %s\n", session->info.source_path);
    }
    if (session->info.target_path) {
        printf("  Target: %s\n", session->info.target_path);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

const char *uft_session_state_string(uft_session_state_t state) {
    switch (state) {
        case UFT_SESSION_STATE_NEW:       return "NEW";
        case UFT_SESSION_STATE_ACTIVE:    return "ACTIVE";
        case UFT_SESSION_STATE_PAUSED:    return "PAUSED";
        case UFT_SESSION_STATE_COMPLETED: return "COMPLETED";
        case UFT_SESSION_STATE_FAILED:    return "FAILED";
        case UFT_SESSION_STATE_CRASHED:   return "CRASHED";
        case UFT_SESSION_STATE_RECOVERED: return "RECOVERED";
        default:                          return "UNKNOWN";
    }
}

const char *uft_session_op_string(uft_session_op_t op) {
    switch (op) {
        case UFT_SESSION_OP_READ:    return "READ";
        case UFT_SESSION_OP_WRITE:   return "WRITE";
        case UFT_SESSION_OP_ANALYZE: return "ANALYZE";
        case UFT_SESSION_OP_RECOVER: return "RECOVER";
        case UFT_SESSION_OP_CONVERT: return "CONVERT";
        case UFT_SESSION_OP_VERIFY:  return "VERIFY";
        default:                     return "UNKNOWN";
    }
}

const char *uft_track_status_string(uft_track_status_t status) {
    switch (status) {
        case UFT_TRACK_STATUS_PENDING:    return "PENDING";
        case UFT_TRACK_STATUS_PROCESSING: return "PROCESSING";
        case UFT_TRACK_STATUS_COMPLETE:   return "COMPLETE";
        case UFT_TRACK_STATUS_FAILED:     return "FAILED";
        case UFT_TRACK_STATUS_SKIPPED:    return "SKIPPED";
        default:                          return "UNKNOWN";
    }
}

const char *uft_session_get_default_path(void) {
    return get_default_session_path();
}

void uft_session_set_default_path(const char *path) {
    if (path) {
        strncpy(g_default_session_path, path, sizeof(g_default_session_path) - 1);
    }
}

char *uft_session_generate_id(void) {
    return generate_session_id();
}
