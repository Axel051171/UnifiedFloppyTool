/**
 * @file uft_formats.c
 * @brief Disk format specifications and validation helpers.
 * @version 1.0.0
 */

#include "uft/uft_formats.h"
#include "uft/uft_endian.h"
#include <string.h>
#include <stdio.h>
#include <limits.h>

#define UFT_G64_SIGNATURE "GCR-1541"
#define UFT_G64_SIGNATURE_LEN 8
#define UFT_G64_HEADER_SIZE 0x0C
#define UFT_G64_OFFSET_TABLE_SIZE (UFT_G64_MAX_TRACKS * 4u)
#define UFT_G64_SPEED_ZONE_TABLE_SIZE (UFT_G64_MAX_TRACKS)
#define UFT_G64_MIN_SIZE (UFT_G64_HEADER_SIZE + UFT_G64_OFFSET_TABLE_SIZE + UFT_G64_SPEED_ZONE_TABLE_SIZE)

static bool uft_mul_size(size_t a, size_t b, size_t *out)
{
    if (!out) {
        return false;
    }

    if (a == 0 || b == 0) {
        *out = 0;
        return true;
    }

    if (a > SIZE_MAX / b) {
        *out = 0;
        return false;
    }

    *out = a * b;
    return true;
}

static void uft_format_report_issue(
    uft_format_validation_report_t *report,
    uft_format_issue_code_t code,
    size_t offset,
    const char *message)
{
    if (!report || !report->issues || report->issue_count >= report->max_issues) {
        return;
    }

    uft_format_issue_t *issue = &report->issues[report->issue_count++];
    issue->code = code;
    issue->offset = offset;

    if (message) {
        snprintf(issue->message, sizeof(issue->message), "%s", message);
    } else {
        issue->message[0] = '\0';
    }
}

static const uft_format_spec_t k_known_formats[] = {
    {
        UFT_DISK_FORMAT_FAT12_160K,
        "FAT12 160K (5.25\" SS)",
        "IBM PC FAT12 single-sided 160K (40T/8S/512B)",
        40, 1, 8, 512,
        UFT_ENCODING_MFM, 250000, 300,
        1, 1,
        UFT_FORMAT_FLAG_BOOT_SIG_55AA,
        160 * 1024,
        UFT_OUTPUT_MASK(UFT_OUTPUT_RAW_IMG) | UFT_OUTPUT_MASK(UFT_OUTPUT_SCP)
    },
    {
        UFT_DISK_FORMAT_FAT12_180K,
        "FAT12 180K (5.25\" SS)",
        "IBM PC FAT12 single-sided 180K (40T/9S/512B)",
        40, 1, 9, 512,
        UFT_ENCODING_MFM, 250000, 300,
        1, 1,
        UFT_FORMAT_FLAG_BOOT_SIG_55AA,
        180 * 1024,
        UFT_OUTPUT_MASK(UFT_OUTPUT_RAW_IMG) | UFT_OUTPUT_MASK(UFT_OUTPUT_SCP)
    },
    {
        UFT_DISK_FORMAT_FAT12_320K,
        "FAT12 320K (5.25\" DS)",
        "IBM PC FAT12 double-sided 320K (40T/8S/512B)",
        40, 2, 8, 512,
        UFT_ENCODING_MFM, 250000, 300,
        1, 1,
        UFT_FORMAT_FLAG_BOOT_SIG_55AA,
        320 * 1024,
        UFT_OUTPUT_MASK(UFT_OUTPUT_RAW_IMG) | UFT_OUTPUT_MASK(UFT_OUTPUT_SCP)
    },
    {
        UFT_DISK_FORMAT_PC_360K,
        "PC 360K (5.25\" DS)",
        "IBM PC 5.25\" DD 360K (40T/9S/512B)",
        40, 2, 9, 512,
        UFT_ENCODING_MFM, 250000, 300,
        1, 1,
        UFT_FORMAT_FLAG_BOOT_SIG_55AA,
        360 * 1024,
        UFT_OUTPUT_MASK(UFT_OUTPUT_RAW_IMG) | UFT_OUTPUT_MASK(UFT_OUTPUT_SCP)
    },
    {
        UFT_DISK_FORMAT_PC_720K,
        "PC 720K (3.5\" DD)",
        "IBM PC 3.5\" DD 720K (80T/9S/512B)",
        80, 2, 9, 512,
        UFT_ENCODING_MFM, 250000, 300,
        1, 1,
        UFT_FORMAT_FLAG_BOOT_SIG_55AA,
        720 * 1024,
        UFT_OUTPUT_MASK(UFT_OUTPUT_RAW_IMG) | UFT_OUTPUT_MASK(UFT_OUTPUT_SCP)
    },
    {
        UFT_DISK_FORMAT_PC_1200K,
        "PC 1.2M (5.25\" HD)",
        "IBM PC 5.25\" HD 1.2M (80T/15S/512B)",
        80, 2, 15, 512,
        UFT_ENCODING_MFM, 500000, 360,
        1, 1,
        UFT_FORMAT_FLAG_BOOT_SIG_55AA,
        1200 * 1024,
        UFT_OUTPUT_MASK(UFT_OUTPUT_RAW_IMG) | UFT_OUTPUT_MASK(UFT_OUTPUT_SCP)
    },
    {
        UFT_DISK_FORMAT_PC_1440K,
        "PC 1.44M (3.5\" HD)",
        "IBM PC 3.5\" HD 1.44M (80T/18S/512B)",
        80, 2, 18, 512,
        UFT_ENCODING_MFM, 500000, 300,
        1, 1,
        UFT_FORMAT_FLAG_BOOT_SIG_55AA,
        1440 * 1024,
        UFT_OUTPUT_MASK(UFT_OUTPUT_RAW_IMG) | UFT_OUTPUT_MASK(UFT_OUTPUT_SCP)
    },
    {
        UFT_DISK_FORMAT_PC_2880K,
        "PC 2.88M (3.5\" ED)",
        "IBM PC 3.5\" ED 2.88M (80T/36S/512B)",
        80, 2, 36, 512,
        UFT_ENCODING_MFM, 1000000, 300,
        1, 1,
        UFT_FORMAT_FLAG_BOOT_SIG_55AA,
        2880 * 1024,
        UFT_OUTPUT_MASK(UFT_OUTPUT_RAW_IMG) | UFT_OUTPUT_MASK(UFT_OUTPUT_SCP)
    },

    {
        UFT_DISK_FORMAT_ATARI_ST_720K,
        "Atari ST 720K",
        "Atari ST DD 720K (80T/9S/512B) raw sector image (.st)",
        80, 2, 9, 512,
        UFT_ENCODING_MFM, 250000, 300,
        1, 1,
        UFT_FORMAT_FLAG_BOOT_SIG_55AA,
        720 * 1024,
        UFT_OUTPUT_MASK(UFT_OUTPUT_ATARI_ST) | UFT_OUTPUT_MASK(UFT_OUTPUT_RAW_IMG) | UFT_OUTPUT_MASK(UFT_OUTPUT_SCP)
    },
    {
        UFT_DISK_FORMAT_ATARI_ST_1440K,
        "Atari ST 1.44M",
        "Atari ST HD 1.44M (80T/18S/512B) raw sector image (.st)",
        80, 2, 18, 512,
        UFT_ENCODING_MFM, 500000, 300,
        1, 1,
        UFT_FORMAT_FLAG_BOOT_SIG_55AA,
        1440 * 1024,
        UFT_OUTPUT_MASK(UFT_OUTPUT_ATARI_ST) | UFT_OUTPUT_MASK(UFT_OUTPUT_RAW_IMG) | UFT_OUTPUT_MASK(UFT_OUTPUT_SCP)
    },
    {
        UFT_DISK_FORMAT_MAC_1440K,
        "Mac 1.44M (HFS)",
        "Apple Macintosh 1.44M (80T/18S/512B) raw sector image",
        80, 2, 18, 512,
        UFT_ENCODING_MFM, 500000, 300,
        1, 1,
        UFT_FORMAT_FLAG_BOOT_SIG_55AA,
        1440 * 1024,
        UFT_OUTPUT_MASK(UFT_OUTPUT_RAW_IMG) | UFT_OUTPUT_MASK(UFT_OUTPUT_SCP)
    },

    {
        UFT_DISK_FORMAT_AMIGA_ADF_880K,
        "Amiga ADF 880K",
        "Commodore Amiga DD ADF (80T/11S/512B)",
        80, 2, 11, 512,
        UFT_ENCODING_MFM, 250000, 300,
        0, 0,
        UFT_FORMAT_FLAG_NONE,
        880 * 1024,
        UFT_OUTPUT_MASK(UFT_OUTPUT_AMIGA_ADF) | UFT_OUTPUT_MASK(UFT_OUTPUT_RAW_IMG)
    },
    {
        UFT_DISK_FORMAT_AMIGA_ADF_1760K,
        "Amiga ADF 1.76M",
        "Commodore Amiga HD ADF (80T/22S/512B)",
        80, 2, 22, 512,
        UFT_ENCODING_MFM, 500000, 300,
        0, 0,
        UFT_FORMAT_FLAG_NONE,
        1760 * 1024,
        UFT_OUTPUT_MASK(UFT_OUTPUT_AMIGA_ADF) | UFT_OUTPUT_MASK(UFT_OUTPUT_RAW_IMG)
    },
    {
        UFT_DISK_FORMAT_C64_G64,
        "C64 G64",
        "Commodore 1541 GCR with timing (G64 container)",
        42, 1, 0, 0,
        UFT_ENCODING_GCR, 250000, 300,
        0, 0,
        UFT_FORMAT_FLAG_NONE,
        0,
        UFT_OUTPUT_MASK(UFT_OUTPUT_C64_G64) | UFT_OUTPUT_MASK(UFT_OUTPUT_SCP)
    },
    {
        UFT_DISK_FORMAT_APPLE2_DOS33,
        "Apple II DOS 3.3",
        "Apple II DOS 3.3 (35T/16S/256B)",
        35, 1, 16, 256,
        UFT_ENCODING_GCR, 250000, 300,
        0, 0,
        UFT_FORMAT_FLAG_NONE,
        35 * 16 * 256,
        UFT_OUTPUT_MASK(UFT_OUTPUT_APPLE_WOZ) | UFT_OUTPUT_MASK(UFT_OUTPUT_SCP) | UFT_OUTPUT_MASK(UFT_OUTPUT_A2R)
    }
};

const uft_format_spec_t *uft_format_get_known_specs(size_t *count)
{
    if (count) {
        *count = sizeof(k_known_formats) / sizeof(k_known_formats[0]);
    }
    return k_known_formats;
}

const uft_format_spec_t *uft_format_find_by_id(uft_disk_format_id_t id)
{
    size_t count = 0;
    const uft_format_spec_t *formats = uft_format_get_known_specs(&count);

    for (size_t i = 0; i < count; i++) {
        if (formats[i].id == id) {
            return &formats[i];
        }
    }

    return NULL;
}

size_t uft_format_expected_size(const uft_format_spec_t *spec)
{
    if (!spec) {
        return 0;
    }

    if (spec->expected_size_bytes != 0) {
        return spec->expected_size_bytes;
    }

    size_t total = 0;
    if (!uft_mul_size(spec->tracks, spec->heads, &total)) {
        return 0;
    }

    if (!uft_mul_size(total, spec->sectors_per_track, &total)) {
        return 0;
    }

    if (!uft_mul_size(total, spec->sector_size, &total)) {
        return 0;
    }

    return total;
}

bool uft_format_guess_from_size(size_t size_bytes, const uft_format_spec_t **out_spec)
{
    if (!out_spec) {
        return false;
    }

    size_t count = 0;
    const uft_format_spec_t *formats = uft_format_get_known_specs(&count);

    for (size_t i = 0; i < count; i++) {
        size_t expected = uft_format_expected_size(&formats[i]);
        if (expected != 0 && expected == size_bytes) {
            *out_spec = &formats[i];
            return true;
        }
    }

    *out_spec = NULL;
    return false;
}

bool uft_format_validate_raw_image(
    const uint8_t *data,
    size_t size_bytes,
    const uft_format_spec_t *spec,
    uft_format_validation_report_t *report)
{
    if (!spec || !data) {
        return false;
    }

    if (report) {
        report->expected_size = uft_format_expected_size(spec);
        report->actual_size = size_bytes;
        report->boot_signature_present = true;
        report->geometry_matches = true;
    }

    bool ok = true;
    size_t expected = uft_format_expected_size(spec);
    if (expected == 0) {
        ok = false;
        uft_format_report_issue(report, UFT_FORMAT_ISSUE_GEOMETRY_OVERFLOW, 0,
                                "Geometry overflow while computing expected size");
    } else if (expected != size_bytes) {
        ok = false;
        if (report) {
            report->geometry_matches = false;
        }
        uft_format_report_issue(report, UFT_FORMAT_ISSUE_SIZE_MISMATCH, 0,
                                "Image size does not match expected geometry");
    }

    if ((spec->flags & UFT_FORMAT_FLAG_BOOT_SIG_55AA) != 0) {
        if (size_bytes < 512) {
            ok = false;
            if (report) {
                report->boot_signature_present = false;
            }
            uft_format_report_issue(report, UFT_FORMAT_ISSUE_HEADER_TRUNCATED, 0,
                                    "Boot sector truncated (missing 0x55AA signature)");
        } else if (data[510] != 0x55 || data[511] != 0xAA) {
            if (report) {
                report->boot_signature_present = false;
            }
            uft_format_report_issue(report, UFT_FORMAT_ISSUE_BOOT_SIGNATURE_MISSING, 510,
                                    "Missing boot signature 0x55AA");
        }
    }

    return ok;
}

bool uft_format_raw_sector_offset(
    const uft_format_spec_t *spec,
    uint16_t track,
    uint8_t head,
    uint16_t sector_id,
    size_t *offset_out)
{
    if (!spec || !offset_out) {
        return false;
    }

    if (track >= spec->tracks || head >= spec->heads) {
        return false;
    }

    if (spec->sectors_per_track == 0 || spec->sector_size == 0) {
        return false;
    }

    if (sector_id < spec->first_sector_id) {
        return false;
    }

    uint16_t sector_index = (uint16_t)(sector_id - spec->first_sector_id);
    if (sector_index >= spec->sectors_per_track) {
        return false;
    }

    size_t offset = 0;
    size_t track_index = (size_t)track * spec->heads + head;
    size_t sectors_before = track_index * spec->sectors_per_track;
    size_t sector_offset = sectors_before + sector_index;

    if (!uft_mul_size(sector_offset, spec->sector_size, &offset)) {
        return false;
    }

    *offset_out = offset;
    return true;
}

bool uft_format_parse_g64(
    const uint8_t *data,
    size_t size_bytes,
    uft_g64_image_t *out,
    uft_format_validation_report_t *report)
{
    if (!data || !out) {
        return false;
    }

    if (report) {
        report->expected_size = 0;
        report->actual_size = size_bytes;
        report->boot_signature_present = false;
        report->geometry_matches = true;
    }

    if (size_bytes < UFT_G64_MIN_SIZE) {
        uft_format_report_issue(report, UFT_FORMAT_ISSUE_HEADER_TRUNCATED, 0,
                                "G64 header truncated");
        return false;
    }

    if (memcmp(data, UFT_G64_SIGNATURE, UFT_G64_SIGNATURE_LEN) != 0) {
        uft_format_report_issue(report, UFT_FORMAT_ISSUE_HEADER_INVALID, 0,
                                "Invalid G64 signature");
        return false;
    }

    out->version = data[8];
    out->track_count = data[9];

    if (out->track_count == 0 || out->track_count > UFT_G64_MAX_TRACKS) {
        uft_format_report_issue(report, UFT_FORMAT_ISSUE_HEADER_INVALID, 9,
                                "Invalid G64 track count");
        out->track_count = UFT_G64_MAX_TRACKS;
    }

    size_t offset_table_base = UFT_G64_HEADER_SIZE;
    size_t speed_table_base = offset_table_base + UFT_G64_OFFSET_TABLE_SIZE;

    if (size_bytes < speed_table_base + UFT_G64_SPEED_ZONE_TABLE_SIZE) {
        uft_format_report_issue(report, UFT_FORMAT_ISSUE_TRACK_TABLE_TRUNCATED, offset_table_base,
                                "G64 track table truncated");
        return false;
    }

    for (size_t i = 0; i < UFT_G64_MAX_TRACKS; i++) {
        out->track_offsets[i] = uft_read_le32(data + offset_table_base + i * 4);
        out->track_sizes[i] = 0;
        out->speed_zones[i] = data[speed_table_base + i];
    }

    bool ok = true;

    for (size_t i = 0; i < out->track_count; i++) {
        uint32_t offset = out->track_offsets[i];
        if (offset == 0) {
            continue;
        }

        if (offset + 2 > size_bytes) {
            ok = false;
            uft_format_report_issue(report, UFT_FORMAT_ISSUE_TRACK_OFFSET_OUT_OF_RANGE, offset,
                                    "G64 track offset outside image");
            continue;
        }

        uint16_t track_len = uft_read_le16(data + offset);
        out->track_sizes[i] = track_len;

        if (track_len == 0) {
            ok = false;
            uft_format_report_issue(report, UFT_FORMAT_ISSUE_TRACK_LENGTH_INVALID, offset,
                                    "G64 track length is zero");
            continue;
        }

        if ((size_t)offset + 2u + track_len > size_bytes) {
            ok = false;
            uft_format_report_issue(report, UFT_FORMAT_ISSUE_TRACK_DATA_TRUNCATED, offset,
                                    "G64 track data truncated");
        }
    }

    return ok;
}