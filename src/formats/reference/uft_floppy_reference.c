/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "uft/formats/uft_floppy_reference.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uft_floppy_reference_t k_catalog[] = {
    /* Aus `data/floppy_reference_wikipedia.tsv` der Zulieferung
     * erzeugt; NUR zum Nachschlagen, nie als Schreibparameter.
     * Benannt initialisiert, damit die Tafel sagt, was sie setzt —
     * und damit `audit_dead_fields` es sehen kann (es liest keine
     * `.inc`-Dateien, MF-1195). */
    {.id = "wiki-acorn-01", .platform = "Acorn", .medium = "51⁄4 inch", .density = "Single", .capacity_label = "100 kB", .tracks_min = 40, .tracks_max = 40, .sides_min = 1, .sides_max = 1, .sectors_min = 10, .sectors_max = 10, .bytes_per_sector = 256, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(102400), .encoding = UFT_FLOPPY_ENCODING_FM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-acorn-02", .platform = "Acorn", .medium = "51⁄4 inch", .density = "Single", .capacity_label = "200 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 1, .sides_max = 1, .sectors_min = 10, .sectors_max = 10, .bytes_per_sector = 256, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(204800), .encoding = UFT_FLOPPY_ENCODING_FM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-acorn-03", .platform = "Acorn", .medium = "51⁄4 inch", .density = "Double", .capacity_label = "160 kB", .tracks_min = 40, .tracks_max = 40, .sides_min = 1, .sides_max = 1, .sectors_min = 16, .sectors_max = 16, .bytes_per_sector = 256, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(163840), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-acorn-04", .platform = "Acorn", .medium = "51⁄4 inch", .density = "Double", .capacity_label = "320 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 1, .sides_max = 1, .sectors_min = 16, .sectors_max = 16, .bytes_per_sector = 256, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(327680), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-acorn-05", .platform = "Acorn", .medium = "51⁄4 inch", .density = "Double", .capacity_label = "640 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 16, .sectors_max = 16, .bytes_per_sector = 256, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(655360), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-acorn-06", .platform = "Acorn", .medium = "31⁄2 inch", .density = "Double", .capacity_label = "640 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 16, .sectors_max = 16, .bytes_per_sector = 256, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(655360), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-acorn-07", .platform = "Acorn", .medium = "31⁄2 inch", .density = "Double", .capacity_label = "800 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 5, .sectors_max = 5, .bytes_per_sector = 1024, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(819200), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-acorn-08", .platform = "Acorn", .medium = "31⁄2 inch", .density = "High", .capacity_label = "1,600 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 10, .sectors_max = 10, .bytes_per_sector = 1024, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(1638400), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-agat-01", .platform = "Agat", .medium = "51⁄4 inch", .density = "Double", .capacity_label = "840 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 21, .sectors_max = 21, .bytes_per_sector = 256, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(860160), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-amiga-01", .platform = "Amiga", .medium = "51⁄4 inch", .density = "Double", .capacity_label = "440 kB", .tracks_min = 40, .tracks_max = 40, .sides_min = 2, .sides_max = 2, .sectors_min = 11, .sectors_max = 11, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(450560), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-amiga-02", .platform = "Amiga", .medium = "51⁄4 inch", .density = "Quad", .capacity_label = "880 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 11, .sectors_max = 11, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(901120), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-amiga-03", .platform = "Amiga", .medium = "31⁄2 inch", .density = "Double", .capacity_label = "880 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 11, .sectors_max = 11, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(901120), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-amiga-04", .platform = "Amiga", .medium = "31⁄2 inch", .density = "High", .capacity_label = "1,520 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 19, .sectors_max = 19, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(1556480), .encoding = UFT_FLOPPY_ENCODING_GCR, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-amiga-05", .platform = "Amiga", .medium = "31⁄2 inch", .density = "High", .capacity_label = "1,760 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 22, .sectors_max = 22, .bytes_per_sector = 512, .rpm_min = 150, .rpm_max = 150, .calculated_payload_bytes = UINT64_C(1802240), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-amstrad-cpc-pcw-01", .platform = "Amstrad CPC/PCW", .medium = "3 inch", .density = "Double", .capacity_label = "180 kB", .tracks_min = 40, .tracks_max = 40, .sides_min = 1, .sides_max = 1, .sectors_min = 9, .sectors_max = 9, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(184320), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-amstrad-pcw8512-9512-01", .platform = "Amstrad PCW8512/9512", .medium = "3 inch", .density = "Double", .capacity_label = "720 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 9, .sectors_max = 9, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(737280), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-apple-ii-01", .platform = "Apple II", .medium = "51⁄4 inch", .density = "Double", .capacity_label = "113.75 kB", .tracks_min = 35, .tracks_max = 35, .sides_min = 1, .sides_max = 1, .sectors_min = 13, .sectors_max = 13, .bytes_per_sector = 256, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(116480), .encoding = UFT_FLOPPY_ENCODING_GCR, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-apple-ii-02", .platform = "Apple II", .medium = "51⁄4 inch", .density = "Double", .capacity_label = "140 kB", .tracks_min = 35, .tracks_max = 35, .sides_min = 1, .sides_max = 1, .sectors_min = 16, .sectors_max = 16, .bytes_per_sector = 256, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(143360), .encoding = UFT_FLOPPY_ENCODING_GCR, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-apple-ii-03", .platform = "Apple II", .medium = "31⁄2 inch", .density = "Double", .capacity_label = "400 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 1, .sides_max = 1, .sectors_min = 8, .sectors_max = 12, .bytes_per_sector = 512, .rpm_min = 394, .rpm_max = 590, .calculated_payload_bytes = UINT64_C(0), .encoding = UFT_FLOPPY_ENCODING_GCR, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = UFT_FLOPPY_REF_VARIABLE_SPT | UFT_FLOPPY_REF_VARIABLE_RPM},
    {.id = "wiki-apple-ii-04", .platform = "Apple II", .medium = "31⁄2 inch", .density = "Double", .capacity_label = "800 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 8, .sectors_max = 12, .bytes_per_sector = 512, .rpm_min = 394, .rpm_max = 590, .calculated_payload_bytes = UINT64_C(0), .encoding = UFT_FLOPPY_ENCODING_GCR, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = UFT_FLOPPY_REF_VARIABLE_SPT | UFT_FLOPPY_REF_VARIABLE_RPM},
    {.id = "wiki-apple-ii-05", .platform = "Apple II", .medium = "31⁄2 inch", .density = "High", .capacity_label = "1,440 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 18, .sectors_max = 18, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(1474560), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-apple-lisa-01", .platform = "Apple Lisa", .medium = "51⁄4 inch FileWare", .density = "Double", .capacity_label = "851 kB", .tracks_min = 46, .tracks_max = 46, .sides_min = 2, .sides_max = 2, .sectors_min = 15, .sectors_max = 22, .bytes_per_sector = 512, .rpm_min = 218, .rpm_max = 320, .calculated_payload_bytes = UINT64_C(0), .encoding = UFT_FLOPPY_ENCODING_GCR, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = UFT_FLOPPY_REF_VARIABLE_SPT | UFT_FLOPPY_REF_VARIABLE_RPM},
    {.id = "wiki-apple-lisa-2-macintosh-xl-macintosh-01", .platform = "Apple Lisa 2/Macintosh XL, Macintosh", .medium = "31⁄2 inch", .density = "Double", .capacity_label = "400 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 1, .sides_max = 1, .sectors_min = 8, .sectors_max = 12, .bytes_per_sector = 512, .rpm_min = 394, .rpm_max = 590, .calculated_payload_bytes = UINT64_C(0), .encoding = UFT_FLOPPY_ENCODING_GCR, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = UFT_FLOPPY_REF_VARIABLE_SPT | UFT_FLOPPY_REF_VARIABLE_RPM},
    {.id = "wiki-macintosh-01", .platform = "Macintosh", .medium = "31⁄2 inch", .density = "Double", .capacity_label = "800 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 8, .sectors_max = 12, .bytes_per_sector = 512, .rpm_min = 394, .rpm_max = 590, .calculated_payload_bytes = UINT64_C(0), .encoding = UFT_FLOPPY_ENCODING_GCR, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = UFT_FLOPPY_REF_VARIABLE_SPT | UFT_FLOPPY_REF_VARIABLE_RPM},
    {.id = "wiki-macintosh-02", .platform = "Macintosh", .medium = "31⁄2 inch", .density = "High", .capacity_label = "1,440 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 18, .sectors_max = 18, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(1474560), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-atari-8-bit-01", .platform = "Atari 8-bit", .medium = "51⁄4 inch", .density = "Single", .capacity_label = "90 kB", .tracks_min = 40, .tracks_max = 40, .sides_min = 1, .sides_max = 1, .sectors_min = 18, .sectors_max = 18, .bytes_per_sector = 128, .rpm_min = 288, .rpm_max = 288, .calculated_payload_bytes = UINT64_C(92160), .encoding = UFT_FLOPPY_ENCODING_FM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-atari-8-bit-02", .platform = "Atari 8-bit", .medium = "51⁄4 inch", .density = "Dual", .capacity_label = "130 kB", .tracks_min = 40, .tracks_max = 40, .sides_min = 1, .sides_max = 1, .sectors_min = 26, .sectors_max = 26, .bytes_per_sector = 128, .rpm_min = 288, .rpm_max = 288, .calculated_payload_bytes = UINT64_C(133120), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-atari-8-bit-03", .platform = "Atari 8-bit", .medium = "51⁄4 inch", .density = "Double", .capacity_label = "180 kB", .tracks_min = 40, .tracks_max = 40, .sides_min = 1, .sides_max = 1, .sectors_min = 18, .sectors_max = 18, .bytes_per_sector = 256, .rpm_min = 288, .rpm_max = 288, .calculated_payload_bytes = UINT64_C(184320), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-atari-8-bit-04", .platform = "Atari 8-bit", .medium = "51⁄4 inch", .density = "Double", .capacity_label = "360 kB", .tracks_min = 40, .tracks_max = 40, .sides_min = 2, .sides_max = 2, .sectors_min = 18, .sectors_max = 18, .bytes_per_sector = 256, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(368640), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-atari-st-tt-falcon-01", .platform = "Atari ST/TT/Falcon", .medium = "31⁄2 inch", .density = "Double", .capacity_label = "360 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 1, .sides_max = 1, .sectors_min = 9, .sectors_max = 9, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(368640), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-atari-st-tt-falcon-02", .platform = "Atari ST/TT/Falcon", .medium = "31⁄2 inch", .density = "Double", .capacity_label = "720 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 9, .sectors_max = 9, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(737280), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-atari-st-tt-falcon-03", .platform = "Atari ST/TT/Falcon", .medium = "31⁄2 inch", .density = "High", .capacity_label = "1,440 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 18, .sectors_max = 18, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(1474560), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-burroughs-md122-01", .platform = "Burroughs MD122", .medium = "8 inch", .density = "Double", .capacity_label = "6.26 MB", .tracks_min = 139, .tracks_max = 139, .sides_min = 2, .sides_max = 2, .sectors_min = 44, .sectors_max = 44, .bytes_per_sector = 256, .rpm_min = 524, .rpm_max = 524, .calculated_payload_bytes = UINT64_C(3131392), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-coleco-adam-01", .platform = "Coleco ADAM", .medium = "51⁄4 inch", .density = "Double", .capacity_label = "160 kB", .tracks_min = 40, .tracks_max = 40, .sides_min = 1, .sides_max = 1, .sectors_min = 8, .sectors_max = 8, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(163840), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-commodore-64-8-bit-01", .platform = "Commodore 64 (8-bit)", .medium = "51⁄4 inch", .density = "Double", .capacity_label = "170 kB", .tracks_min = 35, .tracks_max = 35, .sides_min = 1, .sides_max = 1, .sectors_min = 17, .sectors_max = 21, .bytes_per_sector = 256, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(0), .encoding = UFT_FLOPPY_ENCODING_GCR, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = UFT_FLOPPY_REF_VARIABLE_SPT | UFT_FLOPPY_REF_ZONED_RECORDING},
    {.id = "wiki-commodore-64-8-bit-02", .platform = "Commodore 64 (8-bit)", .medium = "51⁄4 inch", .density = "Double", .capacity_label = "340 kB", .tracks_min = 35, .tracks_max = 35, .sides_min = 2, .sides_max = 2, .sectors_min = 17, .sectors_max = 21, .bytes_per_sector = 256, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(0), .encoding = UFT_FLOPPY_ENCODING_GCR, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = UFT_FLOPPY_REF_VARIABLE_SPT | UFT_FLOPPY_REF_ZONED_RECORDING},
    {.id = "wiki-commodore-64-8-bit-03", .platform = "Commodore 64 (8-bit)", .medium = "51⁄4 inch", .density = "Quad", .capacity_label = "521 kB", .tracks_min = 77, .tracks_max = 77, .sides_min = 1, .sides_max = 1, .sectors_min = 23, .sectors_max = 29, .bytes_per_sector = 256, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(0), .encoding = UFT_FLOPPY_ENCODING_GCR, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = UFT_FLOPPY_REF_VARIABLE_SPT | UFT_FLOPPY_REF_ZONED_RECORDING},
    {.id = "wiki-commodore-64-8-bit-04", .platform = "Commodore 64 (8-bit)", .medium = "51⁄4 inch", .density = "Quad", .capacity_label = "1,042 kB", .tracks_min = 77, .tracks_max = 77, .sides_min = 2, .sides_max = 2, .sectors_min = 23, .sectors_max = 29, .bytes_per_sector = 256, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(0), .encoding = UFT_FLOPPY_ENCODING_GCR, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = UFT_FLOPPY_REF_VARIABLE_SPT | UFT_FLOPPY_REF_ZONED_RECORDING},
    {.id = "wiki-commodore-64-8-bit-05", .platform = "Commodore 64 (8-bit)", .medium = "31⁄2 inch", .density = "Double", .capacity_label = "800 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 10, .sectors_max = 10, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(819200), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-commodore-128-cp-m-01", .platform = "Commodore 128 (CP/M)", .medium = "5 1/4", .density = "Double", .capacity_label = "260K", .tracks_min = 40, .tracks_max = 40, .sides_min = 2, .sides_max = 2, .sectors_min = 26, .sectors_max = 26, .bytes_per_sector = 128, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(266240), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-commodore-128-cp-m-02", .platform = "Commodore 128 (CP/M)", .medium = "5 1/4", .density = "Double", .capacity_label = "320K", .tracks_min = 40, .tracks_max = 40, .sides_min = 2, .sides_max = 2, .sectors_min = 16, .sectors_max = 16, .bytes_per_sector = 256, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(327680), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-commodore-128-cp-m-03", .platform = "Commodore 128 (CP/M)", .medium = "5 1/4", .density = "Double", .capacity_label = "360K", .tracks_min = 40, .tracks_max = 40, .sides_min = 2, .sides_max = 2, .sectors_min = 9, .sectors_max = 9, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(368640), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-commodore-128-cp-m-04", .platform = "Commodore 128 (CP/M)", .medium = "5 1/4", .density = "Double", .capacity_label = "400K", .tracks_min = 40, .tracks_max = 40, .sides_min = 2, .sides_max = 2, .sectors_min = 5, .sectors_max = 5, .bytes_per_sector = 1024, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(409600), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-commodore-900-01", .platform = "Commodore 900", .medium = "51⁄4 inch", .density = "High", .capacity_label = "1,200kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 13, .sectors_max = 16, .bytes_per_sector = 512, .rpm_min = 0, .rpm_max = 0, .calculated_payload_bytes = UINT64_C(0), .encoding = UFT_FLOPPY_ENCODING_GCR, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = UFT_FLOPPY_REF_VARIABLE_SPT},
    {.id = "wiki-dec-rx01-01", .platform = "DEC RX01", .medium = "8 inch", .density = "Single", .capacity_label = "250 kB", .tracks_min = 77, .tracks_max = 77, .sides_min = 1, .sides_max = 1, .sectors_min = 26, .sectors_max = 26, .bytes_per_sector = 128, .rpm_min = 360, .rpm_max = 360, .calculated_payload_bytes = UINT64_C(256256), .encoding = UFT_FLOPPY_ENCODING_FM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-dec-rx02-01", .platform = "DEC RX02", .medium = "8 inch", .density = "Double", .capacity_label = "500 kB", .tracks_min = 77, .tracks_max = 77, .sides_min = 1, .sides_max = 1, .sectors_min = 26, .sectors_max = 26, .bytes_per_sector = 256, .rpm_min = 360, .rpm_max = 360, .calculated_payload_bytes = UINT64_C(512512), .encoding = UFT_FLOPPY_ENCODING_FM_MFM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-dec-rx50-01", .platform = "DEC RX50", .medium = "51⁄4 inch", .density = "Quad", .capacity_label = "400 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 1, .sides_max = 1, .sectors_min = 10, .sectors_max = 10, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(409600), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-hp-110-01", .platform = "HP 110", .medium = "31⁄2 inch", .density = "Double", .capacity_label = "710kB", .tracks_min = 76, .tracks_max = 76, .sides_min = 2, .sides_max = 2, .sectors_min = 9, .sectors_max = 9, .bytes_per_sector = 512, .rpm_min = 600, .rpm_max = 600, .calculated_payload_bytes = UINT64_C(700416), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-hp-80-9800-1000-and-3000-01", .platform = "HP 80 9800 1000 and 3000", .medium = "8 inch", .density = "Double", .capacity_label = "1.18 MB", .tracks_min = 77, .tracks_max = 77, .sides_min = 2, .sides_max = 2, .sectors_min = 30, .sectors_max = 30, .bytes_per_sector = 256, .rpm_min = 360, .rpm_max = 360, .calculated_payload_bytes = UINT64_C(1182720), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-hp-86-01", .platform = "HP 86", .medium = "51⁄4 inch", .density = "Double", .capacity_label = "280 kB", .tracks_min = 35, .tracks_max = 35, .sides_min = 2, .sides_max = 2, .sectors_min = 16, .sectors_max = 16, .bytes_per_sector = 256, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(286720), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-ibm-33fd-01", .platform = "IBM 33FD", .medium = "8 inch", .density = "Single", .capacity_label = "240.5 kiB", .tracks_min = 77, .tracks_max = 77, .sides_min = 1, .sides_max = 1, .sectors_min = 26, .sectors_max = 26, .bytes_per_sector = 128, .rpm_min = 360, .rpm_max = 360, .calculated_payload_bytes = UINT64_C(256256), .encoding = UFT_FLOPPY_ENCODING_FM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-ibm-33fd-02", .platform = "IBM 33FD", .medium = "8 inch", .density = "Single", .capacity_label = "277.5 kiB", .tracks_min = 77, .tracks_max = 77, .sides_min = 1, .sides_max = 1, .sectors_min = 15, .sectors_max = 15, .bytes_per_sector = 256, .rpm_min = 360, .rpm_max = 360, .calculated_payload_bytes = UINT64_C(295680), .encoding = UFT_FLOPPY_ENCODING_FM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-ibm-33fd-03", .platform = "IBM 33FD", .medium = "8 inch", .density = "Single", .capacity_label = "296 kiB", .tracks_min = 77, .tracks_max = 77, .sides_min = 1, .sides_max = 1, .sectors_min = 8, .sectors_max = 8, .bytes_per_sector = 512, .rpm_min = 360, .rpm_max = 360, .calculated_payload_bytes = UINT64_C(315392), .encoding = UFT_FLOPPY_ENCODING_FM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-ibm-43fd-01", .platform = "IBM 43FD", .medium = "8 inch", .density = "Single", .capacity_label = "481 kiB", .tracks_min = 77, .tracks_max = 77, .sides_min = 2, .sides_max = 2, .sectors_min = 26, .sectors_max = 26, .bytes_per_sector = 128, .rpm_min = 360, .rpm_max = 360, .calculated_payload_bytes = UINT64_C(512512), .encoding = UFT_FLOPPY_ENCODING_FM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-ibm-43fd-02", .platform = "IBM 43FD", .medium = "8 inch", .density = "Single", .capacity_label = "555 kiB", .tracks_min = 77, .tracks_max = 77, .sides_min = 2, .sides_max = 2, .sectors_min = 15, .sectors_max = 15, .bytes_per_sector = 256, .rpm_min = 360, .rpm_max = 360, .calculated_payload_bytes = UINT64_C(591360), .encoding = UFT_FLOPPY_ENCODING_FM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-ibm-53fd-01", .platform = "IBM 53FD", .medium = "8 inch", .density = "Double", .capacity_label = "962 kiB", .tracks_min = 77, .tracks_max = 77, .sides_min = 2, .sides_max = 2, .sectors_min = 26, .sectors_max = 26, .bytes_per_sector = 256, .rpm_min = 360, .rpm_max = 360, .calculated_payload_bytes = UINT64_C(1025024), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-ibm-53fd-02", .platform = "IBM 53FD", .medium = "8 inch", .density = "Double", .capacity_label = "1.08 MiB", .tracks_min = 77, .tracks_max = 77, .sides_min = 2, .sides_max = 2, .sectors_min = 15, .sectors_max = 15, .bytes_per_sector = 512, .rpm_min = 360, .rpm_max = 360, .calculated_payload_bytes = UINT64_C(1182720), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-ibm-53fd-03", .platform = "IBM 53FD", .medium = "8 inch", .density = "Double", .capacity_label = "1.16 MiB", .tracks_min = 77, .tracks_max = 77, .sides_min = 2, .sides_max = 2, .sectors_min = 8, .sectors_max = 8, .bytes_per_sector = 1024, .rpm_min = 360, .rpm_max = 360, .calculated_payload_bytes = UINT64_C(1261568), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-ibm-pc-compatibles-01", .platform = "IBM PC compatibles", .medium = "8 inch", .density = "Single", .capacity_label = "250.25 kB", .tracks_min = 77, .tracks_max = 77, .sides_min = 1, .sides_max = 1, .sectors_min = 26, .sectors_max = 26, .bytes_per_sector = 128, .rpm_min = 360, .rpm_max = 360, .calculated_payload_bytes = UINT64_C(256256), .encoding = UFT_FLOPPY_ENCODING_FM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-ibm-pc-compatibles-02", .platform = "IBM PC compatibles", .medium = "8 inch", .density = "Single", .capacity_label = "500.5 kB", .tracks_min = 77, .tracks_max = 77, .sides_min = 2, .sides_max = 2, .sectors_min = 26, .sectors_max = 26, .bytes_per_sector = 128, .rpm_min = 360, .rpm_max = 360, .calculated_payload_bytes = UINT64_C(512512), .encoding = UFT_FLOPPY_ENCODING_FM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-ibm-pc-compatibles-03", .platform = "IBM PC compatibles", .medium = "8 inch", .density = "Double", .capacity_label = "616 kB", .tracks_min = 77, .tracks_max = 77, .sides_min = 1, .sides_max = 1, .sectors_min = 8, .sectors_max = 8, .bytes_per_sector = 1024, .rpm_min = 360, .rpm_max = 360, .calculated_payload_bytes = UINT64_C(630784), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-ibm-pc-compatibles-04", .platform = "IBM PC compatibles", .medium = "8 inch", .density = "Double", .capacity_label = "1,232 kB", .tracks_min = 77, .tracks_max = 77, .sides_min = 2, .sides_max = 2, .sectors_min = 8, .sectors_max = 8, .bytes_per_sector = 1024, .rpm_min = 360, .rpm_max = 360, .calculated_payload_bytes = UINT64_C(1261568), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-ibm-pc-compatibles-05", .platform = "IBM PC compatibles", .medium = "51⁄4 inch", .density = "Double", .capacity_label = "160 kB", .tracks_min = 40, .tracks_max = 40, .sides_min = 1, .sides_max = 1, .sectors_min = 8, .sectors_max = 8, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(163840), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-ibm-pc-compatibles-06", .platform = "IBM PC compatibles", .medium = "51⁄4 inch", .density = "Double", .capacity_label = "320 kB", .tracks_min = 40, .tracks_max = 40, .sides_min = 2, .sides_max = 2, .sectors_min = 8, .sectors_max = 8, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(327680), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-ibm-pc-compatibles-07", .platform = "IBM PC compatibles", .medium = "51⁄4 inch", .density = "Double", .capacity_label = "180 kB", .tracks_min = 40, .tracks_max = 40, .sides_min = 1, .sides_max = 1, .sectors_min = 9, .sectors_max = 9, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(184320), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-ibm-pc-compatibles-08", .platform = "IBM PC compatibles", .medium = "51⁄4 inch", .density = "Double", .capacity_label = "360 kB", .tracks_min = 40, .tracks_max = 40, .sides_min = 2, .sides_max = 2, .sectors_min = 9, .sectors_max = 9, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(368640), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-ibm-pc-compatibles-09", .platform = "IBM PC compatibles", .medium = "51⁄4 inch", .density = "Quad", .capacity_label = "320 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 1, .sides_max = 1, .sectors_min = 8, .sectors_max = 8, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(327680), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-ibm-pc-compatibles-10", .platform = "IBM PC compatibles", .medium = "51⁄4 inch", .density = "Quad", .capacity_label = "640 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 8, .sectors_max = 8, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(655360), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-ibm-pc-compatibles-11", .platform = "IBM PC compatibles", .medium = "51⁄4 inch", .density = "High", .capacity_label = "1,200 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 15, .sectors_max = 15, .bytes_per_sector = 512, .rpm_min = 360, .rpm_max = 360, .calculated_payload_bytes = UINT64_C(1228800), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-ibm-pc-compatibles-12", .platform = "IBM PC compatibles", .medium = "31⁄2 inch", .density = "Double", .capacity_label = "320 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 1, .sides_max = 1, .sectors_min = 8, .sectors_max = 8, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(327680), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-ibm-pc-compatibles-13", .platform = "IBM PC compatibles", .medium = "31⁄2 inch", .density = "Double", .capacity_label = "360 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 1, .sides_max = 1, .sectors_min = 9, .sectors_max = 9, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(368640), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-ibm-pc-compatibles-14", .platform = "IBM PC compatibles", .medium = "31⁄2 inch", .density = "Double", .capacity_label = "640 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 8, .sectors_max = 8, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(655360), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-ibm-pc-compatibles-15", .platform = "IBM PC compatibles", .medium = "31⁄2 inch", .density = "Double", .capacity_label = "720 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 9, .sectors_max = 9, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(737280), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-ibm-pc-compatibles-16", .platform = "IBM PC compatibles", .medium = "31⁄2 inch", .density = "High", .capacity_label = "1,440 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 18, .sectors_max = 18, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(1474560), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-ibm-pc-compatibles-17", .platform = "IBM PC compatibles", .medium = "31⁄2 inch", .density = "High", .capacity_label = "1,680 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 21, .sectors_max = 21, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(1720320), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-ibm-pc-compatibles-18", .platform = "IBM PC compatibles", .medium = "31⁄2 inch", .density = "High", .capacity_label = "1,720 kB", .tracks_min = 82, .tracks_max = 82, .sides_min = 2, .sides_max = 2, .sectors_min = 21, .sectors_max = 21, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(1763328), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-ibm-pc-compatibles-19", .platform = "IBM PC compatibles", .medium = "31⁄2 inch", .density = "Extended", .capacity_label = "2,880 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 36, .sectors_max = 36, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(2949120), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-memorex-650-01", .platform = "Memorex 650", .medium = "8 inch", .density = "Single", .capacity_label = "1.4 Mb", .tracks_min = 50, .tracks_max = 50, .sides_min = 1, .sides_max = 1, .sectors_min = 8, .sectors_max = 8, .bytes_per_sector = 0, .rpm_min = 375, .rpm_max = 375, .calculated_payload_bytes = UINT64_C(0), .encoding = UFT_FLOPPY_ENCODING_FM, .sectoring = UFT_FLOPPY_SECTORING_HARD, .flags = UFT_FLOPPY_REF_NON_BYTE_SECTOR | UFT_FLOPPY_REF_INCOMPLETE},
    {.id = "wiki-memorex-651-01", .platform = "Memorex 651", .medium = "8 inch", .density = "Single", .capacity_label = "2.2 Mb", .tracks_min = 64, .tracks_max = 64, .sides_min = 1, .sides_max = 1, .sectors_min = 32, .sectors_max = 32, .bytes_per_sector = 0, .rpm_min = 375, .rpm_max = 375, .calculated_payload_bytes = UINT64_C(0), .encoding = UFT_FLOPPY_ENCODING_FM, .sectoring = UFT_FLOPPY_SECTORING_HARD, .flags = UFT_FLOPPY_REF_NON_BYTE_SECTOR | UFT_FLOPPY_REF_INCOMPLETE},
    {.id = "wiki-mgt-sam-coupe-01", .platform = "MGT SAM Coupé", .medium = "31⁄2 inch", .density = "Double", .capacity_label = "800 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 10, .sectors_max = 10, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(819200), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-nec-pc-98-01", .platform = "NEC PC-98", .medium = "8 inch", .density = "Single", .capacity_label = "250.25 kB", .tracks_min = 77, .tracks_max = 77, .sides_min = 1, .sides_max = 1, .sectors_min = 26, .sectors_max = 26, .bytes_per_sector = 128, .rpm_min = 360, .rpm_max = 360, .calculated_payload_bytes = UINT64_C(256256), .encoding = UFT_FLOPPY_ENCODING_FM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-nec-pc-98-02", .platform = "NEC PC-98", .medium = "8 inch", .density = "Double", .capacity_label = "1,232 kB", .tracks_min = 77, .tracks_max = 77, .sides_min = 2, .sides_max = 2, .sectors_min = 8, .sectors_max = 8, .bytes_per_sector = 1024, .rpm_min = 360, .rpm_max = 360, .calculated_payload_bytes = UINT64_C(1261568), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-nec-pc-98-03", .platform = "NEC PC-98", .medium = "51⁄4 inch", .density = "Double", .capacity_label = "640 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 8, .sectors_max = 8, .bytes_per_sector = 512, .rpm_min = 360, .rpm_max = 360, .calculated_payload_bytes = UINT64_C(655360), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-nec-pc-98-04", .platform = "NEC PC-98", .medium = "51⁄4 inch", .density = "Double", .capacity_label = "720 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 9, .sectors_max = 9, .bytes_per_sector = 512, .rpm_min = 360, .rpm_max = 360, .calculated_payload_bytes = UINT64_C(737280), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-nec-pc-98-05", .platform = "NEC PC-98", .medium = "51⁄4 inch", .density = "High", .capacity_label = "1,200 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 15, .sectors_max = 15, .bytes_per_sector = 512, .rpm_min = 360, .rpm_max = 360, .calculated_payload_bytes = UINT64_C(1228800), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-nec-pc-98-06", .platform = "NEC PC-98", .medium = "51⁄4 inch", .density = "High", .capacity_label = "1,232 (1,280) kB", .tracks_min = 77, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 8, .sectors_max = 8, .bytes_per_sector = 1024, .rpm_min = 360, .rpm_max = 360, .calculated_payload_bytes = UINT64_C(0), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-nec-pc-98-07", .platform = "NEC PC-98", .medium = "31⁄2 inch", .density = "Double", .capacity_label = "640 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 8, .sectors_max = 8, .bytes_per_sector = 512, .rpm_min = 360, .rpm_max = 360, .calculated_payload_bytes = UINT64_C(655360), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-nec-pc-98-08", .platform = "NEC PC-98", .medium = "31⁄2 inch", .density = "Double", .capacity_label = "720 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 9, .sectors_max = 9, .bytes_per_sector = 512, .rpm_min = 360, .rpm_max = 360, .calculated_payload_bytes = UINT64_C(737280), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-nec-pc-98-09", .platform = "NEC PC-98", .medium = "31⁄2 inch", .density = "High", .capacity_label = "1,200 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 15, .sectors_max = 15, .bytes_per_sector = 512, .rpm_min = 360, .rpm_max = 360, .calculated_payload_bytes = UINT64_C(1228800), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-nec-pc-98-10", .platform = "NEC PC-98", .medium = "31⁄2 inch", .density = "High", .capacity_label = "1,232 (1,280) kB", .tracks_min = 77, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 8, .sectors_max = 8, .bytes_per_sector = 1024, .rpm_min = 360, .rpm_max = 360, .calculated_payload_bytes = UINT64_C(0), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-nec-pc-98-11", .platform = "NEC PC-98", .medium = "31⁄2 inch", .density = "High", .capacity_label = "1.44 MB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 18, .sectors_max = 18, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(1474560), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-nec-pc-98-12", .platform = "NEC PC-98", .medium = "31⁄2 inch", .density = "Triple", .capacity_label = "9,120 kB", .tracks_min = 240, .tracks_max = 240, .sides_min = 2, .sides_max = 2, .sectors_min = 38, .sectors_max = 38, .bytes_per_sector = 512, .rpm_min = 360, .rpm_max = 360, .calculated_payload_bytes = UINT64_C(9338880), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-osborne-1-01", .platform = "Osborne 1", .medium = "51⁄4 inch", .density = "Single", .capacity_label = "100 kB", .tracks_min = 40, .tracks_max = 40, .sides_min = 1, .sides_max = 1, .sectors_min = 10, .sectors_max = 10, .bytes_per_sector = 256, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(102400), .encoding = UFT_FLOPPY_ENCODING_FM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-osborne-1-02", .platform = "Osborne 1", .medium = "51⁄4 inch", .density = "Double", .capacity_label = "200 kB", .tracks_min = 40, .tracks_max = 40, .sides_min = 1, .sides_max = 1, .sectors_min = 5, .sectors_max = 5, .bytes_per_sector = 1024, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(204800), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-sega-sf-7000-01", .platform = "Sega SF-7000", .medium = "3 inch", .density = "Single", .capacity_label = "160 kB", .tracks_min = 40, .tracks_max = 40, .sides_min = 2, .sides_max = 2, .sectors_min = 16, .sectors_max = 16, .bytes_per_sector = 256, .rpm_min = 0, .rpm_max = 0, .calculated_payload_bytes = UINT64_C(327680), .encoding = UFT_FLOPPY_ENCODING_UNKNOWN, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-sharp-x68000-01", .platform = "SHARP X68000", .medium = "51⁄4 inch", .density = "High", .capacity_label = "1,232 kB", .tracks_min = 77, .tracks_max = 77, .sides_min = 2, .sides_max = 2, .sectors_min = 8, .sectors_max = 8, .bytes_per_sector = 1024, .rpm_min = 360, .rpm_max = 360, .calculated_payload_bytes = UINT64_C(1261568), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-sharp-x68000-02", .platform = "SHARP X68000", .medium = "31⁄2 inch", .density = "High", .capacity_label = "1,232 kB", .tracks_min = 77, .tracks_max = 77, .sides_min = 2, .sides_max = 2, .sectors_min = 8, .sectors_max = 8, .bytes_per_sector = 1024, .rpm_min = 360, .rpm_max = 360, .calculated_payload_bytes = UINT64_C(1261568), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-sharp-ce-1600f-ce-140f-01", .platform = "SHARP CE-1600F, CE-140F", .medium = "21⁄2 inch", .density = "Single", .capacity_label = "2× 64 kB", .tracks_min = 16, .tracks_max = 16, .sides_min = 1, .sides_max = 2, .sectors_min = 8, .sectors_max = 8, .bytes_per_sector = 512, .rpm_min = 270, .rpm_max = 270, .calculated_payload_bytes = UINT64_C(0), .encoding = UFT_FLOPPY_ENCODING_GCR, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = UFT_FLOPPY_REF_FLIPPABLE},
    {.id = "wiki-tandy-trs-80-01", .platform = "Tandy TRS-80", .medium = "51⁄4 inch", .density = "Single", .capacity_label = "88 kB", .tracks_min = 35, .tracks_max = 35, .sides_min = 1, .sides_max = 1, .sectors_min = 10, .sectors_max = 10, .bytes_per_sector = 256, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(89600), .encoding = UFT_FLOPPY_ENCODING_FM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-tandy-trs-80-02", .platform = "Tandy TRS-80", .medium = "51⁄4 inch", .density = "Double", .capacity_label = "180 kB", .tracks_min = 40, .tracks_max = 40, .sides_min = 1, .sides_max = 1, .sectors_min = 18, .sectors_max = 18, .bytes_per_sector = 256, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(184320), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-tandy-trs-80-03", .platform = "Tandy TRS-80", .medium = "51⁄4 inch", .density = "Double", .capacity_label = "360 kB", .tracks_min = 40, .tracks_max = 40, .sides_min = 2, .sides_max = 2, .sectors_min = 18, .sectors_max = 18, .bytes_per_sector = 256, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(368640), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-tandy-trs-80-04", .platform = "Tandy TRS-80", .medium = "8 inch", .density = "Double", .capacity_label = "500 kB", .tracks_min = 77, .tracks_max = 77, .sides_min = 1, .sides_max = 1, .sectors_min = 26, .sectors_max = 26, .bytes_per_sector = 256, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(512512), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-tandy-trs-80-05", .platform = "Tandy TRS-80", .medium = "31⁄2 inch", .density = "Single", .capacity_label = "100 kB", .tracks_min = 40, .tracks_max = 40, .sides_min = 1, .sides_max = 1, .sectors_min = 2, .sectors_max = 2, .bytes_per_sector = 1280, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(102400), .encoding = UFT_FLOPPY_ENCODING_FM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-tandy-trs-80-06", .platform = "Tandy TRS-80", .medium = "31⁄2 inch", .density = "Single", .capacity_label = "200 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 1, .sides_max = 1, .sectors_min = 2, .sectors_max = 2, .bytes_per_sector = 1280, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(204800), .encoding = UFT_FLOPPY_ENCODING_FM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-tandy-trs-80-07", .platform = "Tandy TRS-80", .medium = "51⁄4 inch", .density = "Double", .capacity_label = "157 kB", .tracks_min = 35, .tracks_max = 35, .sides_min = 1, .sides_max = 1, .sectors_min = 18, .sectors_max = 18, .bytes_per_sector = 256, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(161280), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
    {.id = "wiki-thomson-01", .platform = "Thomson", .medium = "51⁄4 inch", .density = "Single", .capacity_label = "80 kB", .tracks_min = 40, .tracks_max = 40, .sides_min = 1, .sides_max = 1, .sectors_min = 16, .sectors_max = 16, .bytes_per_sector = 128, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(81920), .encoding = UFT_FLOPPY_ENCODING_FM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-thomson-02", .platform = "Thomson", .medium = "51⁄4 inch", .density = "Double", .capacity_label = "320 kB", .tracks_min = 40, .tracks_max = 40, .sides_min = 2, .sides_max = 2, .sectors_min = 16, .sectors_max = 16, .bytes_per_sector = 256, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(327680), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-thomson-03", .platform = "Thomson", .medium = "31⁄2 inch", .density = "Double", .capacity_label = "320 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 1, .sides_max = 1, .sectors_min = 16, .sectors_max = 16, .bytes_per_sector = 256, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(327680), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-thomson-04", .platform = "Thomson", .medium = "31⁄2 inch", .density = "Double", .capacity_label = "640 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 16, .sectors_max = 16, .bytes_per_sector = 256, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(655360), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = 0u},
    {.id = "wiki-victor-9000-act-sirius-1-01", .platform = "Victor 9000 / ACT Sirius 1", .medium = "51⁄4 inch", .density = "Single", .capacity_label = "612 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 1, .sides_max = 1, .sectors_min = 11, .sectors_max = 19, .bytes_per_sector = 512, .rpm_min = 252, .rpm_max = 417, .calculated_payload_bytes = UINT64_C(0), .encoding = UFT_FLOPPY_ENCODING_GCR, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = UFT_FLOPPY_REF_VARIABLE_SPT | UFT_FLOPPY_REF_VARIABLE_RPM},
    {.id = "wiki-victor-9000-act-sirius-1-02", .platform = "Victor 9000 / ACT Sirius 1", .medium = "51⁄4 inch", .density = "Double", .capacity_label = "1,196 kB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 11, .sectors_max = 19, .bytes_per_sector = 512, .rpm_min = 252, .rpm_max = 417, .calculated_payload_bytes = UINT64_C(0), .encoding = UFT_FLOPPY_ENCODING_GCR, .sectoring = UFT_FLOPPY_SECTORING_UNKNOWN, .flags = UFT_FLOPPY_REF_VARIABLE_SPT | UFT_FLOPPY_REF_VARIABLE_RPM},
    {.id = "wiki-vtech-laser210-vz200-01", .platform = "Vtech Laser210/VZ200", .medium = "51⁄4 inch", .density = "Single", .capacity_label = "78 kB", .tracks_min = 40, .tracks_max = 40, .sides_min = 0, .sides_max = 0, .sectors_min = 16, .sectors_max = 16, .bytes_per_sector = 0, .rpm_min = 0, .rpm_max = 0, .calculated_payload_bytes = UINT64_C(0), .encoding = UFT_FLOPPY_ENCODING_FM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = UFT_FLOPPY_REF_INCOMPLETE},
    {.id = "wiki-ibm-pc-compatible-01", .platform = "IBM PC Compatible", .medium = "5 1/4", .density = "Quad", .capacity_label = "2.5MB", .tracks_min = 80, .tracks_max = 80, .sides_min = 2, .sides_max = 2, .sectors_min = 31, .sectors_max = 31, .bytes_per_sector = 512, .rpm_min = 300, .rpm_max = 300, .calculated_payload_bytes = UINT64_C(2539520), .encoding = UFT_FLOPPY_ENCODING_MFM, .sectoring = UFT_FLOPPY_SECTORING_SOFT, .flags = 0u},
};

/* MF-1195: die PHYSISCHE Tafel der Zulieferung (25 Zeilen, 13 Felder,
 * Spurdichte und Flussdichte je Medium) kommt hier NICHT mit.
 *
 * Gemessen: sie wurde ausserhalb ihrer eigenen Datei von niemandem
 * genannt — nicht vom Rangierer, nicht vom Aufrufer, nicht vom Test, und
 * von keiner anderen Datei des Baums. Ihre zwei Zugriffsfunktionen hatten
 * **0** Aufrufer. Sie mitzunehmen waere P3-204 zum fuenften Mal in dieser
 * Reihe, und D2 verbietet es.
 *
 * Sie liegt unveraendert in der Zulieferung `UFT_Floppy_Reference_AARD_
 * Paket.zip` und kann nachkommen, sobald ein Aufrufer sie braucht — etwa
 * fuer eine Aussage ueber Spurdichte, die aus einem Abbild nicht
 * ableitbar ist. Das ist kein Verlust, sondern eine nicht getroffene
 * Zusage. */

typedef struct json_buffer {
    char *data;
    size_t length;
    size_t capacity;
} json_buffer_t;

static int ascii_casecmp(const char *a, const char *b)
{
    unsigned char ca;
    unsigned char cb;
    if (!a || !b) return a == b ? 0 : (a ? 1 : -1);
    while (*a && *b) {
        ca = (unsigned char)tolower((unsigned char)*a++);
        cb = (unsigned char)tolower((unsigned char)*b++);
        if (ca != cb) return (int)ca - (int)cb;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static unsigned medium_code(const char *text)
{
    if (!text) return 0u;
    if (strstr(text, "2½") || strstr(text, "21⁄2") ||
        strstr(text, "2 1/2")) return 25u;
    if (strstr(text, "3½") || strstr(text, "31⁄2") ||
        strstr(text, "3 1/2") || strstr(text, "3.5")) return 35u;
    if (strstr(text, "5¼") || strstr(text, "51⁄4") ||
        strstr(text, "5 1/4") || strstr(text, "5.25")) return 525u;
    if (strstr(text, "3.25")) return 325u;
    if (strstr(text, "2 inch")) return 20u;
    if (strstr(text, "3 inch")) return 30u;
    if (strstr(text, "4 inch")) return 40u;
    if (strstr(text, "8 inch")) return 80u;
    return 0u;
}

static bool medium_equal(const char *a, const char *b)
{
    const unsigned ac = medium_code(a);
    const unsigned bc = medium_code(b);
    return ac && bc ? ac == bc : ascii_casecmp(a, b) == 0;
}

static bool in_range(unsigned value, unsigned minimum, unsigned maximum)
{
    return minimum != 0u && maximum != 0u &&
           value >= minimum && value <= maximum;
}

static void score_field(unsigned value, unsigned minimum, unsigned maximum,
                        unsigned weight, unsigned *points,
                        unsigned *possible, unsigned *compared,
                        unsigned *mismatches)
{
    if (value == 0u) return;
    *possible += weight;
    ++*compared;
    if (in_range(value, minimum, maximum)) {
        *points += weight;
    } else {
        ++*mismatches;
    }
}

static uft_floppy_match_t score_record(const uft_floppy_observation_t *o,
                                       size_t index)
{
    const uft_floppy_reference_t *r = &k_catalog[index];
    unsigned points = 0u;
    unsigned possible = 0u;
    unsigned compared = 0u;
    unsigned mismatches = 0u;
    uft_floppy_match_t match;

    score_field(o->tracks, r->tracks_min, r->tracks_max, 25u,
                &points, &possible, &compared, &mismatches);
    score_field(o->sides, r->sides_min, r->sides_max, 20u,
                &points, &possible, &compared, &mismatches);
    score_field(o->sectors_per_track, r->sectors_min, r->sectors_max, 25u,
                &points, &possible, &compared, &mismatches);
    score_field(o->bytes_per_sector, r->bytes_per_sector,
                r->bytes_per_sector, 20u,
                &points, &possible, &compared, &mismatches);
    score_field(o->rpm, r->rpm_min, r->rpm_max, 5u,
                &points, &possible, &compared, &mismatches);

    if (o->encoding != UFT_FLOPPY_ENCODING_UNKNOWN) {
        possible += 5u;
        ++compared;
        if (o->encoding == r->encoding) points += 5u;
        else ++mismatches;
    }
    if (o->medium && o->medium[0]) {
        possible += 5u;
        ++compared;
        if (medium_equal(o->medium, r->medium)) points += 5u;
        else ++mismatches;
    }

    match.reference_index = index;
    match.score = possible ? (points * 100u + possible / 2u) / possible : 0u;
    match.compared = compared;
    match.mismatches = mismatches;
    return match;
}

static int match_compare(const void *left, const void *right)
{
    const uft_floppy_match_t *a = (const uft_floppy_match_t *)left;
    const uft_floppy_match_t *b = (const uft_floppy_match_t *)right;
    if (a->score != b->score) return a->score < b->score ? 1 : -1;
    if (a->mismatches != b->mismatches)
        return a->mismatches > b->mismatches ? 1 : -1;
    if (a->compared != b->compared) return a->compared < b->compared ? 1 : -1;
    return a->reference_index > b->reference_index ? 1 :
           a->reference_index < b->reference_index ? -1 : 0;
}

/* ══════════════════════════════════════════════════════════════════════
 * P3-445 / MF-1195 — die Behebung.
 *
 * Die beiden Funktionen darueber bleiben unveraendert stehen
 * (Eigentuemerregel „nicht entfernen, weiter erweitern"); sie liefern die
 * sortierte Liste. Was hier dazukommt, ist das URTEIL — und es sagt ab,
 * wo die alte Fassung die Tabellenreihenfolge entscheiden liess.
 *
 * Der Unterschied in einem Satz: `score_field()` zaehlt `compared` hoch,
 * SOBALD die Beobachtung einen Wert hat, und `in_range()` gibt bei einer
 * fehlenden REFERENZangabe `false` zurueck — ein Satz ohne Drehzahl
 * bekommt damit einen Widerspruch angerechnet. Hier wird ein Feld nur
 * verglichen, wenn BEIDE Seiten etwas sagen.
 * ══════════════════════════════════════════════════════════════════════ */

typedef struct {
    unsigned points;
    unsigned possible;
    unsigned trifft;
    unsigned widerspricht;
    unsigned unbekannt;
} bewertung_t;

/* Ein Zahlenfeld in drei Zustaenden. Kein Gewicht ohne Vergleich: was
 * niemand kennt, geht auch nicht in `possible` ein — sonst senkt eine
 * fehlende Angabe die Punktzahl, statt sie unberuehrt zu lassen. */
static void feld_zahl(unsigned obs, unsigned rmin, unsigned rmax,
                      unsigned weight, bewertung_t *b)
{
    if (obs == 0u)                { b->unbekannt++; return; } /* Beobachtung */
    if (rmin == 0u || rmax == 0u) { b->unbekannt++; return; } /* Referenz    */
    b->possible += weight;
    if (obs >= rmin && obs <= rmax) { b->points += weight; b->trifft++; }
    else                            { b->widerspricht++; }
}

static bewertung_t bewerte_satz(const uft_floppy_observation_t *o,
                                size_t index)
{
    const uft_floppy_reference_t *r = &k_catalog[index];
    bewertung_t b;
    memset(&b, 0, sizeof(b));

    feld_zahl(o->tracks, r->tracks_min, r->tracks_max, 25u, &b);
    feld_zahl(o->sides, r->sides_min, r->sides_max, 20u, &b);
    feld_zahl(o->sectors_per_track, r->sectors_min, r->sectors_max, 25u, &b);
    /* Sektorgroesse ist ein Einzelwert, kein Bereich — 0 heisst „die
     * Quelle nennt keine", etwa bei nicht-byteweisen Sektoren. */
    feld_zahl(o->bytes_per_sector, r->bytes_per_sector, r->bytes_per_sector,
              20u, &b);
    feld_zahl(o->rpm, r->rpm_min, r->rpm_max, 5u, &b);

    if (o->encoding == UFT_FLOPPY_ENCODING_UNKNOWN ||
        r->encoding == UFT_FLOPPY_ENCODING_UNKNOWN) {
        b.unbekannt++;
    } else {
        b.possible += 5u;
        if (o->encoding == r->encoding) { b.points += 5u; b.trifft++; }
        else                            { b.widerspricht++; }
    }

    if (!o->medium || !o->medium[0] || !r->medium || !r->medium[0]) {
        b.unbekannt++;
    } else {
        b.possible += 5u;
        if (medium_equal(o->medium, r->medium)) { b.points += 5u; b.trifft++; }
        else                                    { b.widerspricht++; }
    }
    return b;
}

static unsigned punkte(const bewertung_t *b)
{
    return b->possible ? (b->points * 100u + b->possible / 2u) / b->possible
                       : 0u;
}

static void ranking_leeren(uft_floppy_ranking_t *out)
{
    memset(out, 0, sizeof(*out));
    out->best_index = UFT_FLOPPY_REFERENCE_KEIN_INDEX;
}

static void besten_uebernehmen(uft_floppy_ranking_t *out,
                               const bewertung_t *b)
{
    out->best_score        = punkte(b);
    out->best_trifft       = b->trifft;
    out->best_widerspricht = b->widerspricht;
    out->best_unbekannt    = b->unbekannt;
    out->best_compared     = b->trifft + b->widerspricht;
}

/* Ein Kandidat ist ein Satz, der etwas TRIFFT und NICHTS widerspricht.
 * „Nichts widerspricht" allein genuegt nicht — ein Satz, der zu allem
 * schweigt, waere sonst ein Kandidat fuer jede Diskette. */
static bool ist_kandidat(const bewertung_t *b)
{
    return b->trifft > 0u && b->widerspricht == 0u;
}

bool uft_floppy_reference_rank_one(const uft_floppy_observation_t *observation,
                                   size_t index,
                                   uft_floppy_ranking_t *out)
{
    bewertung_t b;
    if (!observation || !out) return false;
    if (index >= uft_floppy_reference_count()) return false;

    ranking_leeren(out);
    b = bewerte_satz(observation, index);
    besten_uebernehmen(out, &b);
    if (ist_kandidat(&b)) {
        out->kandidaten   = 1u;
        out->tied         = 1u;
        out->tied_with[0] = index;
        out->tied_listed  = 1u;
        out->best_index   = index;
    }
    return true;
}

bool uft_floppy_reference_rank(const uft_floppy_observation_t *observation,
                               uft_floppy_ranking_t *out)
{
    size_t count;
    bewertung_t beste;
    size_t i, bester = UFT_FLOPPY_REFERENCE_KEIN_INDEX;

    if (!observation || !out) return false;
    ranking_leeren(out);
    memset(&beste, 0, sizeof(beste));
    count = uft_floppy_reference_count();

    /* Erster Lauf: den besten Rang FINDEN, ohne ihn zu benennen. */
    for (i = 0u; i < count; i++) {
        const bewertung_t b = bewerte_satz(observation, i);
        if (!ist_kandidat(&b)) continue;
        out->kandidaten++;
        if (bester == UFT_FLOPPY_REFERENCE_KEIN_INDEX) {
            beste = b; bester = i; continue;
        }
        /* Regel 1: mehr Punkte. Regel 2: bei gleichen Punkten der
         * ENGERE Anspruch, also mehr verglichene Felder. Eine dritte
         * Stufe gibt es NICHT — hier brach die alte Fassung ueber
         * `reference_index` ab, also ueber die Tabellenreihenfolge. */
        {
            const unsigned pb = punkte(&b), pbest = punkte(&beste);
            const unsigned cb = b.trifft + b.widerspricht;
            const unsigned cbest = beste.trifft + beste.widerspricht;
            if (pb > pbest || (pb == pbest && cb > cbest)) {
                beste = b; bester = i;
            }
        }
    }

    if (bester == UFT_FLOPPY_REFERENCE_KEIN_INDEX) return true; /* nichts */
    besten_uebernehmen(out, &beste);

    /* Zweiter Lauf: WIE VIELE erreichen denselben Rang. `tied` zaehlt
     * weiter, auch wenn die Liste voll ist (wie `uft_probe_ranking`). */
    for (i = 0u; i < count; i++) {
        const bewertung_t b = bewerte_satz(observation, i);
        if (!ist_kandidat(&b)) continue;
        if (punkte(&b) != out->best_score) continue;
        if (b.trifft + b.widerspricht != out->best_compared) continue;
        out->tied++;
        if (out->tied_listed < UFT_FLOPPY_RANKING_MAX_TIED)
            out->tied_with[out->tied_listed++] = i;
    }

    /* Regel 3: bleibt es gleich, gewinnt keiner. `best_index` bleibt
     * `KEIN_INDEX` — ein Aufrufer, der die Absage ignoriert, bekommt
     * keinen Satz, sondern eine ungueltige Nummer. */
    if (out->tied > 1u) out->ambiguous = true;
    else                out->best_index = bester;

    return true;
}

size_t uft_floppy_reference_count(void)
{
    return sizeof(k_catalog) / sizeof(k_catalog[0]);
}

const uft_floppy_reference_t *uft_floppy_reference_get(size_t index)
{
    return index < uft_floppy_reference_count() ? &k_catalog[index] : NULL;
}

const uft_floppy_reference_t *uft_floppy_reference_find(const char *id)
{
    size_t i;
    if (!id) return NULL;
    for (i = 0u; i < uft_floppy_reference_count(); ++i) {
        if (strcmp(k_catalog[i].id, id) == 0) return &k_catalog[i];
    }
    return NULL;
}

size_t uft_floppy_reference_match(const uft_floppy_observation_t *observation,
                                  uft_floppy_match_t *matches,
                                  size_t matches_capacity)
{
    const size_t count = uft_floppy_reference_count();
    uft_floppy_match_t *all;
    size_t i;
    size_t written;
    if (!observation || (!matches && matches_capacity != 0u)) return 0u;
    if (matches_capacity == 0u) return count;
    all = (uft_floppy_match_t *)malloc(count * sizeof(*all));
    if (!all) return 0u;
    for (i = 0u; i < count; ++i) all[i] = score_record(observation, i);
    qsort(all, count, sizeof(*all), match_compare);
    written = matches_capacity < count ? matches_capacity : count;
    memcpy(matches, all, written * sizeof(*matches));
    free(all);
    return written;
}

bool uft_floppy_reference_is_write_safe(const uft_floppy_reference_t *record)
{
    return record && (record->flags & UFT_FLOPPY_REF_WRITE_SAFE) != 0u;
}

const char *uft_floppy_encoding_name(uft_floppy_encoding_t encoding)
{
    switch (encoding) {
    case UFT_FLOPPY_ENCODING_FM: return "FM";
    case UFT_FLOPPY_ENCODING_MFM: return "MFM";
    case UFT_FLOPPY_ENCODING_GCR: return "GCR";
    case UFT_FLOPPY_ENCODING_FM_MFM: return "FM/MFM";
    default: return "unknown";
    }
}

const char *uft_floppy_sectoring_name(uft_floppy_sectoring_t sectoring)
{
    switch (sectoring) {
    case UFT_FLOPPY_SECTORING_SOFT: return "soft";
    case UFT_FLOPPY_SECTORING_HARD: return "hard";
    case UFT_FLOPPY_SECTORING_SOFT_OR_HARD: return "soft-or-hard";
    default: return "unknown";
    }
}

static int json_reserve(json_buffer_t *buffer, size_t extra)
{
    size_t wanted;
    size_t capacity;
    char *grown;
    if (extra > (size_t)-1 - buffer->length - 1u) return -1;
    wanted = buffer->length + extra + 1u;
    if (wanted <= buffer->capacity) return 0;
    capacity = buffer->capacity ? buffer->capacity : 512u;
    while (capacity < wanted) {
        if (capacity > (size_t)-1 / 2u) return -1;
        capacity *= 2u;
    }
    grown = (char *)realloc(buffer->data, capacity);
    if (!grown) return -1;
    buffer->data = grown;
    buffer->capacity = capacity;
    return 0;
}

static int json_appendf(json_buffer_t *buffer, const char *format, ...)
{
    va_list args;
    va_list copy;
    int required;
    va_start(args, format);
    va_copy(copy, args);
    required = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (required < 0 || json_reserve(buffer, (size_t)required) != 0) {
        va_end(args);
        return -1;
    }
    (void)vsnprintf(buffer->data + buffer->length,
                    buffer->capacity - buffer->length, format, args);
    va_end(args);
    buffer->length += (size_t)required;
    return 0;
}

static int json_string(json_buffer_t *buffer, const char *text)
{
    const unsigned char *p = (const unsigned char *)(text ? text : "");
    if (json_appendf(buffer, "\"") != 0) return -1;
    while (*p) {
        if (*p == '\"' || *p == '\\') {
            if (json_appendf(buffer, "\\%c", *p) != 0) return -1;
        } else if (*p < 0x20u) {
            if (json_appendf(buffer, "\\u%04x", (unsigned)*p) != 0) return -1;
        } else {
            if (json_reserve(buffer, 1u) != 0) return -1;
            buffer->data[buffer->length++] = (char)*p;
            buffer->data[buffer->length] = '\0';
        }
        ++p;
    }
    return json_appendf(buffer, "\"");
}

int uft_floppy_matches_to_json_alloc(const uft_floppy_observation_t *observation,
                                     const uft_floppy_match_t *matches,
                                     size_t match_count,
                                     char **json_out,
                                     size_t *json_size_out)
{
    json_buffer_t b = {0};
    size_t i;
    if (!observation || (!matches && match_count) || !json_out) return -1;
    *json_out = NULL;
    if (json_size_out) *json_size_out = 0u;
    if (json_appendf(&b,
        "{\"schema_version\":%u,\"source\":\"%s\","
        "\"write_parameters_allowed\":false,\"observation\":{"
        "\"tracks\":%u,\"sides\":%u,\"sectors_per_track\":%u,"
        "\"bytes_per_sector\":%u,\"rpm\":%u,\"encoding\":\"%s\"},"
        "\"matches\":[",
        UFT_FLOPPY_REFERENCE_SCHEMA_VERSION, UFT_FLOPPY_REFERENCE_SOURCE_URL,
        observation->tracks, observation->sides,
        observation->sectors_per_track, observation->bytes_per_sector,
        observation->rpm, uft_floppy_encoding_name(observation->encoding)) != 0)
        goto fail;
    for (i = 0u; i < match_count; ++i) {
        const uft_floppy_reference_t *r =
            uft_floppy_reference_get(matches[i].reference_index);
        if (!r) continue;
        if (i && json_appendf(&b, ",") != 0) goto fail;
        if (json_appendf(&b, "{\"id\":") != 0 || json_string(&b, r->id) != 0 ||
            json_appendf(&b, ",\"platform\":") != 0 ||
            json_string(&b, r->platform) != 0 ||
            json_appendf(&b, ",\"medium\":") != 0 ||
            json_string(&b, r->medium) != 0 ||
            json_appendf(&b,
                ",\"score\":%u,\"compared\":%u,\"mismatches\":%u,"
                "\"write_safe\":false}", matches[i].score,
                matches[i].compared, matches[i].mismatches) != 0)
            goto fail;
    }
    if (json_appendf(&b, "]}") != 0) goto fail;
    *json_out = b.data;
    if (json_size_out) *json_size_out = b.length;
    return 0;
fail:
    free(b.data);
    return -1;
}
