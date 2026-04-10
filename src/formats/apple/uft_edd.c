/**
 * @file uft_edd.c
 * @brief EDD (Essential Data Duplicator) format parser implementation
 *
 * EDD captures raw nibble data from Apple II 5.25" disks.
 *
 * Layout:
 *   EDD 3: 35 tracks x 6656 nibbles = 232,960 bytes
 *   EDD 4: 35 tracks x 6656 nibbles x 2 = 465,920 bytes
 *          (first half = nibbles, second half = timing)
 *
 * Detection: file extension .edd + matching size
 *
 * @author UFT Project
 * @date 2026-04-10
 */

#include "uft/formats/apple/uft_edd.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Check if filename has .edd extension (case-insensitive)
 */
static bool has_edd_extension(const char *filename) {
    if (!filename) return false;

    const char *dot = strrchr(filename, '.');
    if (!dot || !dot[1]) return false;

    const char *ext = dot + 1;
    if (strlen(ext) != 3) return false;

    char lower[4];
    for (int i = 0; i < 3; i++) {
        char c = ext[i];
        lower[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
    }
    lower[3] = '\0';

    return (strcmp(lower, "edd") == 0);
}

/**
 * @brief Determine EDD version from file size
 */
static edd_version_t edd_version_from_size(size_t size) {
    if (size == EDD_SIZE_V3) return EDD_VERSION_3;
    if (size == EDD_SIZE_V4) return EDD_VERSION_4;

    /*
     * Some EDD files may have slightly different sizes due to
     * extra header or padding. Accept sizes that are exact multiples
     * of the track size.
     */
    if (size > 0 && (size % EDD_NIBBLES_PER_TRACK) == 0) {
        size_t track_pairs = size / EDD_NIBBLES_PER_TRACK;
        if (track_pairs == EDD_TRACKS) return EDD_VERSION_3;
        if (track_pairs == EDD_TRACKS * 2) return EDD_VERSION_4;
    }

    return EDD_VERSION_UNKNOWN;
}

/* ============================================================================
 * Detection
 * ============================================================================ */

bool uft_edd_detect(const uint8_t *data, size_t size, const char *filename) {
    (void)data; /* EDD has no magic bytes */

    if (!has_edd_extension(filename)) return false;

    edd_version_t ver = edd_version_from_size(size);
    return (ver != EDD_VERSION_UNKNOWN);
}

/* ============================================================================
 * Parsing
 * ============================================================================ */

uft_edd_error_t uft_edd_parse(const uint8_t *data, size_t size,
                                uft_edd_disk_t *disk) {
    if (!data || !disk) return UFT_EDD_ERR_NULL;

    memset(disk, 0, sizeof(*disk));

    edd_version_t ver = edd_version_from_size(size);
    if (ver == EDD_VERSION_UNKNOWN) {
        return UFT_EDD_ERR_SIZE;
    }

    disk->version = ver;
    disk->file_size = (uint32_t)size;
    disk->track_count = EDD_TRACKS;

    /* Parse track data */
    for (int t = 0; t < EDD_TRACKS; t++) {
        uft_edd_track_t *trk = &disk->tracks[t];
        size_t nibble_offset = (size_t)t * EDD_NIBBLES_PER_TRACK;

        if (nibble_offset + EDD_NIBBLES_PER_TRACK > size) {
            return UFT_EDD_ERR_TRUNCATED;
        }

        memcpy(trk->nibbles, data + nibble_offset, EDD_NIBBLES_PER_TRACK);
        trk->valid = true;

        /* EDD 4: timing data in the second half */
        if (ver == EDD_VERSION_4) {
            size_t timing_offset = (size_t)EDD_TRACKS * EDD_NIBBLES_PER_TRACK
                                   + (size_t)t * EDD_NIBBLES_PER_TRACK;
            if (timing_offset + EDD_NIBBLES_PER_TRACK > size) {
                return UFT_EDD_ERR_TRUNCATED;
            }
            memcpy(trk->timing, data + timing_offset, EDD_NIBBLES_PER_TRACK);
            trk->has_timing = true;
        } else {
            memset(trk->timing, 0, EDD_NIBBLES_PER_TRACK);
            trk->has_timing = false;
        }
    }

    disk->is_valid = true;

    snprintf(disk->description, sizeof(disk->description),
             "EDD %d — %d tracks, %d nibbles/track%s",
             ver, EDD_TRACKS, EDD_NIBBLES_PER_TRACK,
             ver >= EDD_VERSION_4 ? " + timing" : "");

    return UFT_EDD_OK;
}

uft_edd_error_t uft_edd_open(const char *path, uft_edd_disk_t *disk) {
    if (!path || !disk) return UFT_EDD_ERR_NULL;

    memset(disk, 0, sizeof(*disk));

    FILE *fp = fopen(path, "rb");
    if (!fp) return UFT_EDD_ERR_OPEN;

    /* Get file size */
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return UFT_EDD_ERR_READ;
    }
    long fsize = ftell(fp);
    if (fsize <= 0) {
        fclose(fp);
        return UFT_EDD_ERR_READ;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return UFT_EDD_ERR_READ;
    }

    size_t size = (size_t)fsize;
    uint8_t *data = (uint8_t *)malloc(size);
    if (!data) {
        fclose(fp);
        return UFT_EDD_ERR_MEMORY;
    }

    size_t read_bytes = fread(data, 1, size, fp);
    fclose(fp);

    if (read_bytes != size) {
        free(data);
        return UFT_EDD_ERR_READ;
    }

    uft_edd_error_t err = uft_edd_parse(data, size, disk);
    free(data);

    if (err == UFT_EDD_OK) {
        size_t path_len = strlen(path);
        if (path_len >= sizeof(disk->filepath)) {
            path_len = sizeof(disk->filepath) - 1;
        }
        memcpy(disk->filepath, path, path_len);
        disk->filepath[path_len] = '\0';
    }

    return err;
}

/* ============================================================================
 * Track Access
 * ============================================================================ */

uft_edd_error_t uft_edd_get_track(const uft_edd_disk_t *disk, int track,
                                    const uint8_t **nibbles, size_t *count) {
    if (!disk || !nibbles || !count) return UFT_EDD_ERR_NULL;
    if (track < 0 || track >= disk->track_count) return UFT_EDD_ERR_TRACK;
    if (!disk->tracks[track].valid) return UFT_EDD_ERR_TRACK;

    *nibbles = disk->tracks[track].nibbles;
    *count = EDD_NIBBLES_PER_TRACK;
    return UFT_EDD_OK;
}

uft_edd_error_t uft_edd_get_timing(const uft_edd_disk_t *disk, int track,
                                     const uint8_t **timing, size_t *count) {
    if (!disk || !timing || !count) return UFT_EDD_ERR_NULL;
    if (track < 0 || track >= disk->track_count) return UFT_EDD_ERR_TRACK;
    if (!disk->tracks[track].has_timing) return UFT_EDD_ERR_TRACK;

    *timing = disk->tracks[track].timing;
    *count = EDD_NIBBLES_PER_TRACK;
    return UFT_EDD_OK;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char *uft_edd_version_string(edd_version_t version) {
    switch (version) {
        case EDD_VERSION_3:     return "EDD 3 (nibbles only)";
        case EDD_VERSION_4:     return "EDD 4+ (nibbles + timing)";
        default:                return "Unknown";
    }
}

const char *uft_edd_strerror(uft_edd_error_t err) {
    switch (err) {
        case UFT_EDD_OK:            return "Success";
        case UFT_EDD_ERR_NULL:      return "Null pointer";
        case UFT_EDD_ERR_OPEN:      return "Cannot open file";
        case UFT_EDD_ERR_READ:      return "Read error";
        case UFT_EDD_ERR_SIZE:      return "Invalid EDD file size";
        case UFT_EDD_ERR_INVALID:   return "Invalid EDD format";
        case UFT_EDD_ERR_MEMORY:    return "Out of memory";
        case UFT_EDD_ERR_TRACK:     return "Invalid track or no data";
        default:                    return "Unknown error";
    }
}
