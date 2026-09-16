/**
 * @file uft_win98_fdb.c
 * @brief MSWIN4.1-Floppy-Bootrecord erkennen und benennen (MF-1181).
 *
 * Quelle, Lizenz und Geltungsbereich: siehe
 * `include/uft/formats/fat/uft_win98_fdb.h`. Kurz: Bytelagen nach
 * Daniel B. Sedorys Beschreibung gelesen (Kanal *Spec*), eigenstaendig
 * umgesetzt, kein fremder Code.
 *
 * Der BPB wird NICHT hier zerlegt. `fat_analyze_boot_sector()` tut es
 * seit Langem und ist von `tests/test_fat_bootsector.c` abgenommen
 * (64 Zusagen, Suite-Test #180). Dieses Modul fragt es und ergaenzt
 * genau das, was MSWIN4.1 von jedem anderen FAT12-Bootsektor
 * unterscheidet.
 *
 * SPDX-License-Identifier: MIT
 */

#include "uft/formats/fat/uft_win98_fdb.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "uft/uft_format_plugin.h"   /* uft_probe_konfidenz, UFT_BELEG_* */

/* ── FNV-1a-64 ───────────────────────────────────────────────────────
 * Die Parameter sind die veroeffentlichten: Offset-Basis
 * 0xcbf29ce484222325, Primzahl 0x100000001b3. Sie stehen hier, weil
 * ein Fingerabdruck ohne benannte Parameter keine nachrechenbare Zahl
 * ist. */
static uint64_t fdb_fnv1a64(const uint8_t *data, size_t size)
{
    uint64_t h = UINT64_C(0xcbf29ce484222325);
    for (size_t i = 0; i < size; ++i) {
        h ^= (uint64_t)data[i];
        h *= UINT64_C(0x100000001b3);
    }
    return h;
}

/* ── Auffaelligkeiten: kuerzen ist erlaubt, stilles Kuerzen nicht ──── */
static void fdb_warn(uft_win98_fdb_report_t *r,
                     uft_win98_fdb_warning_kind_t kind,
                     const char *fmt, ...)
{
    if (r->warning_count >= UFT_WIN98_FDB_MAX_WARNINGS) {
        r->warnings_truncated = true;
        return;
    }
    uft_win98_fdb_warning_t *w = &r->warnings[r->warning_count++];
    w->kind = kind;
    va_list ap;
    va_start(ap, fmt);
    (void)vsnprintf(w->message, sizeof(w->message), fmt, ap);
    va_end(ap);
}

/* ── Die drei MSWIN4.1-eigenen Bytepruefungen ───────────────────────
 * Jede an ihrer aus der Quelle belegten Stelle, jede mit ihrer eigenen
 * Laenge. Kein Ruecken, kein Suchen: eine Kennung an FESTER Position
 * ist etwas anderes als ein Muster irgendwo im Sektor (MF-1153). */
static bool fdb_oem_trifft(const uint8_t *d)
{
    return memcmp(d + UFT_WIN98_FDB_OEM_OFFSET, UFT_WIN98_FDB_OEM_NAME, 8u) == 0;
}

static bool fdb_io_sys_trifft(const uint8_t *d)
{
    return memcmp(d + UFT_WIN98_FDB_IO_SYS_OFFSET, "IO      SYS", 11u) == 0;
}

static bool fdb_msdos_sys_trifft(const uint8_t *d)
{
    return memcmp(d + UFT_WIN98_FDB_MSDOS_SYS_OFFSET, "MSDOS   SYS", 11u) == 0;
}

/* ── Belege nach der Doktrin sammeln ───────────────────────────────── */
static unsigned fdb_belege(const uint8_t *d, size_t size,
                           const fat_analysis_result_t *fat, bool fat_ok)
{
    unsigned belege = 0u;

    /* KENNUNG: nur die OEM-Zeichenfolge. Signatur 0x55AA und der
     * Sprung EB 3C 90 stehen auf JEDEM PC-Bootsektor und sind damit
     * keine formatspezifische Kennung. */
    if (fdb_oem_trifft(d)) belege |= UFT_BELEG_KENNUNG;

    /* SELBSTKONSISTENZ: der BPB rechnet die uebergebene Groesse auf.
     * `size` darf dafuer NICHT verworfen werden — genau dieser Fehler
     * war in diesem Baum fuenfmal der Defekt (MF-1029). */
    if (fat_ok && fat->total_bytes > 0u && (uint64_t)size == fat->total_bytes)
        belege |= UFT_BELEG_SELBSTKONSISTENZ;

    /* STRUKTUR: beide Systemdateinamen an ihrer belegten Stelle. */
    if (fdb_io_sys_trifft(d) && fdb_msdos_sys_trifft(d))
        belege |= UFT_BELEG_STRUKTUR;

    /* GEOMETRIE: der Analysator hat den BPB plausibel gefunden. */
    if (fat_ok && fat->has_valid_bpb) belege |= UFT_BELEG_GEOMETRIE;

    return belege;
}

bool uft_win98_fdb_probe(const uint8_t *data, size_t size, int *confidence_out)
{
    if (confidence_out) *confidence_out = 0;
    if (!data || size < UFT_WIN98_FDB_SECTOR_SIZE) return false;

    fat_analysis_result_t fat;
    const bool fat_ok = (fat_analyze_boot_sector(data, size, &fat) == FAT_OK);

    const unsigned belege = fdb_belege(data, size, &fat, fat_ok);
    const int k = uft_probe_konfidenz(belege);
    if (confidence_out) *confidence_out = k;

    /* Ohne Kennung nie zustimmen. Die Doktrin klemmt dort bei 45, also
     * waere ein Schwellenvergleich allein zu schwach: 45 liegt im Band
     * „nur die Groesse" und sagt ausdruecklich nicht „das ist es". */
    return (belege & UFT_BELEG_KENNUNG) != 0u;
}

uft_win98_fdb_status_t uft_win98_fdb_parse(const uint8_t *data, size_t size,
                                          uft_win98_fdb_report_t *report)
{
    if (!report) return UFT_WIN98_FDB_INVALID_ARG;
    memset(report, 0, sizeof(*report));
    report->schema_version = UFT_WIN98_FDB_SCHEMA_VERSION;

    if (!data) return UFT_WIN98_FDB_INVALID_ARG;
    if (size < UFT_WIN98_FDB_SECTOR_SIZE) return UFT_WIN98_FDB_TOO_SHORT;

    report->source_size = size;

    /* Der BPB kommt aus dem Baum, nicht von hier. */
    report->fat_ok = (fat_analyze_boot_sector(data, size, &report->fat) == FAT_OK);
    if (!report->fat_ok)
        fdb_warn(report, UFT_WIN98_FDB_WARN_BPB,
                 "fat_analyze_boot_sector hat abgesagt");
    else if (!report->fat.has_valid_bpb)
        fdb_warn(report, UFT_WIN98_FDB_WARN_BPB,
                 "BPB unplausibel: %u Byte/Sektor, %u Sektoren/Cluster",
                 (unsigned)report->fat.bytes_per_sector,
                 (unsigned)report->fat.sectors_per_cluster);

    report->has_oem_mswin41      = fdb_oem_trifft(data);
    report->has_io_sys_marker    = fdb_io_sys_trifft(data);
    report->has_msdos_sys_marker = fdb_msdos_sys_trifft(data);

    /* Zwei getrennte Abdruecke ueber die ZWEI Codebereiche, die die
     * Quelle benennt. Der Bereich der drei Fehlermeldungen liegt
     * ausdruecklich in KEINEM von beiden. */
    report->code_fingerprint_fnv1a64 =
        fdb_fnv1a64(data + UFT_WIN98_FDB_CODE_BEGIN,
                    UFT_WIN98_FDB_CODE_END - UFT_WIN98_FDB_CODE_BEGIN);
    report->tail_fingerprint_fnv1a64 =
        fdb_fnv1a64(data + UFT_WIN98_FDB_SUBROUTINE_BEGIN,
                    UFT_WIN98_FDB_SUBROUTINE_END - UFT_WIN98_FDB_SUBROUTINE_BEGIN);

    if (report->fat_ok && report->fat.total_bytes > 0u) {
        report->image_size_matches_bpb = ((uint64_t)size == report->fat.total_bytes);
        if (!report->image_size_matches_bpb && size > UFT_WIN98_FDB_SECTOR_SIZE)
            fdb_warn(report, UFT_WIN98_FDB_WARN_SIZE,
                     "Eingabe %llu Byte, BPB sagt %llu",
                     (unsigned long long)size,
                     (unsigned long long)report->fat.total_bytes);
    }

    if (report->fat_ok && report->fat.cluster_count == 0u)
        fdb_warn(report, UFT_WIN98_FDB_WARN_LAYOUT,
                 "Datenbereich traegt 0 Cluster");

    if (!report->has_io_sys_marker || !report->has_msdos_sys_marker)
        fdb_warn(report, UFT_WIN98_FDB_WARN_BOOTCODE,
                 "Systemdateinamen fehlen: IO.SYS %s, MSDOS.SYS %s",
                 report->has_io_sys_marker ? "ja" : "nein",
                 report->has_msdos_sys_marker ? "ja" : "nein");

    report->confidence =
        uft_probe_konfidenz(fdb_belege(data, size, &report->fat, report->fat_ok));

    /* Ohne die OEM-Kennung wird NICHT behauptet, es sei dieser
     * Bootrecord. Der Bericht bleibt vollstaendig gefuellt — er sagt
     * nur, dass er nichts erkannt hat. */
    if (!report->has_oem_mswin41) return UFT_WIN98_FDB_UNRECOGNIZED;
    return UFT_WIN98_FDB_OK;
}

uft_win98_fdb_status_t uft_win98_fdb_parse_file(const char *path,
                                               uft_win98_fdb_report_t *report)
{
    if (!report) return UFT_WIN98_FDB_INVALID_ARG;
    memset(report, 0, sizeof(*report));
    report->schema_version = UFT_WIN98_FDB_SCHEMA_VERSION;
    if (!path) return UFT_WIN98_FDB_INVALID_ARG;

    FILE *f = fopen(path, "rb");
    if (!f) return UFT_WIN98_FDB_IO_ERROR;

    uint8_t sector[UFT_WIN98_FDB_SECTOR_SIZE];
    const size_t gelesen = fread(sector, 1u, sizeof(sector), f);

    /* Dateigroesse fuer die Selbstkonsistenz. Scheitert das Ermitteln,
     * wird die gelesene Menge benutzt und der Beleg faellt damit weg —
     * lieber ein fehlender Beleg als eine erfundene Groesse. */
    size_t groesse = gelesen;
    if (fseek(f, 0L, SEEK_END) == 0) {
        const long ende = ftell(f);
        if (ende > 0L) groesse = (size_t)ende;
    }
    fclose(f);

    if (gelesen < UFT_WIN98_FDB_SECTOR_SIZE) return UFT_WIN98_FDB_TOO_SHORT;
    return uft_win98_fdb_parse(sector, groesse, report);
}

const char *uft_win98_fdb_status_name(uft_win98_fdb_status_t status)
{
    switch (status) {
    case UFT_WIN98_FDB_OK:           return "OK";
    case UFT_WIN98_FDB_INVALID_ARG:  return "INVALID_ARG";
    case UFT_WIN98_FDB_TOO_SHORT:    return "TOO_SHORT";
    case UFT_WIN98_FDB_UNRECOGNIZED: return "UNRECOGNIZED";
    case UFT_WIN98_FDB_IO_ERROR:     return "IO_ERROR";
    }
    return "UNKNOWN";
}

const char *uft_win98_fdb_warning_name(uft_win98_fdb_warning_kind_t kind)
{
    switch (kind) {
    case UFT_WIN98_FDB_WARN_NONE:     return "NONE";
    case UFT_WIN98_FDB_WARN_BPB:      return "BPB";
    case UFT_WIN98_FDB_WARN_LAYOUT:   return "LAYOUT";
    case UFT_WIN98_FDB_WARN_SIZE:     return "SIZE";
    case UFT_WIN98_FDB_WARN_BOOTCODE: return "BOOTCODE";
    }
    return "UNKNOWN";
}
