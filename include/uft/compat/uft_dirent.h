/**
 * @file uft_dirent.h
 * @brief Cross-platform directory enumeration (dirent.h replacement)
 * 
 * Provides POSIX-like dirent interface on Windows using Win32 API.
 */
#ifndef UFT_DIRENT_H
#define UFT_DIRENT_H

#ifdef _WIN32

#include <windows.h>
#include <stdlib.h>
#include <string.h>

/* Maximum path length */
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif

/* Directory entry types */
#define DT_UNKNOWN  0
#define DT_REG      8
#define DT_DIR      4

/* Directory entry structure */
struct dirent {
    char d_name[MAX_PATH];
    unsigned char d_type;
};

/* Directory stream structure */
typedef struct {
    HANDLE handle;
    WIN32_FIND_DATAA data;
    struct dirent entry;
    int first;
} DIR;

/* Open directory */
static inline DIR* opendir(const char* name) {
    DIR* dir = (DIR*)malloc(sizeof(DIR));
    if (!dir) return NULL;
    
    char path[MAX_PATH];
    snprintf(path, MAX_PATH, "%s\\*", name);
    
    dir->handle = FindFirstFileA(path, &dir->data);
    if (dir->handle == INVALID_HANDLE_VALUE) {
        free(dir);
        return NULL;
    }
    dir->first = 1;
    return dir;
}

/* Read directory entry */
static inline struct dirent* readdir(DIR* dir) {
    if (!dir) return NULL;
    
    if (dir->first) {
        dir->first = 0;
    } else {
        if (!FindNextFileA(dir->handle, &dir->data)) {
            return NULL;
        }
    }
    
    strncpy(dir->entry.d_name, dir->data.cFileName, MAX_PATH - 1);
    dir->entry.d_name[MAX_PATH - 1] = '\0';
    
    if (dir->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        dir->entry.d_type = DT_DIR;
    } else {
        dir->entry.d_type = DT_REG;
    }
    
    return &dir->entry;
}

/* Close directory */
static inline int closedir(DIR* dir) {
    if (!dir) return -1;
    FindClose(dir->handle);
    free(dir);
    return 0;
}

#else
/* POSIX - use native dirent.h */
#include <dirent.h>
#endif /* _WIN32 */

#endif /* UFT_DIRENT_H */
