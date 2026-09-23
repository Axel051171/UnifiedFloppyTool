/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_copy_plan.c
 * @brief Umsetzung zu uft_copy_plan.h (MF-1232)
 *
 * Die Tafeln hier sind die Eigentuemer-Vorgabe vom 2026-09-18, Zeile fuer
 * Zeile. Sie sind **Modell**, keine Messung — was gemessen ist, steht als
 * Zahl im Kopf des Headers und in `tests/test_copy_plan.c`.
 *
 * Eine Tafel, keine zweite daneben (MF-1177): `k_param[]` traegt je
 * Parameter seine Stufe UND seine Rolle je Ebene. `uft_copy_param_state()`
 * und der Lauf ueber alle Parameter befragen dieselbe Tafel.
 */

#include "uft/core/uft_copy_plan.h"
/* MF-1263: fuer `uft_convert_options_t`. Der KOPF bleibt bewusst
 * abhaengigkeitsfrei und deklariert den Struct nur vorwaerts; die
 * Umsetzung braucht die Felder und bindet ihn deshalb hier ein. */
#include "uft/uft_types.h"

#include <string.h>

/* ── Rolle eines Parameters auf einer Ebene ─────────────────────────── */
typedef enum {
    R_,        /* nicht vorgesehen          -> HIDDEN      */
    R_A,       /* aktiv                     -> ACTIVE      */
    R_B,       /* bedingt                   -> CONDITIONAL */
    R_L,       /* nur lesen                 -> READONLY    */
    R_X        /* ausdruecklich verboten    -> FORBIDDEN   */
} rolle_t;

typedef struct {
    const char       *id;
    uft_copy_stage_t  stage;
    rolle_t           je_ebene[UFT_COPY_LEVEL_ECHT];  /* FILE..FLUX */
    uint32_t          verlangt;   /* requiresCapabilities */
    uint32_t          verbietet;  /* forbidsCapabilities  */
} param_t;

/* Reihenfolge der Spalten: FILE, SECTOR, TRACK, BITSTREAM, NIBBLE, FLUX */
static const param_t k_param[] = {
 /* ── ueberall gueltig ─────────────────────────────────────────────── */
 { "source",                    UFT_STAGE_READ,   {R_A,R_A,R_A,R_A,R_A,R_A}, 0, 0 },
 { "target",                    UFT_STAGE_WRITE,  {R_A,R_A,R_A,R_A,R_A,R_A}, 0, 0 },
 { "source.format",             UFT_STAGE_READ,   {R_A,R_A,R_A,R_A,R_A,R_A}, 0, 0 },
 { "target.format",             UFT_STAGE_WRITE,  {R_A,R_A,R_A,R_A,R_A,R_A}, 0, 0 },
 { "force_format",              UFT_STAGE_READ,   {R_A,R_A,R_A,R_A,R_A,R_A}, 0, 0 },
 { "drive_type",                UFT_STAGE_READ,   {R_A,R_A,R_A,R_A,R_A,R_A}, 0, 0 },
 { "source.geometry.cylinders", UFT_STAGE_READ,   {R_,R_A,R_A,R_A,R_A,R_A}, 0, 0 },
 { "source.geometry.heads",     UFT_STAGE_READ,   {R_,R_A,R_A,R_A,R_A,R_A}, 0, 0 },
 { "target.geometry.cylinders", UFT_STAGE_WRITE,  {R_,R_A,R_A,R_A,R_A,R_A}, 0, 0 },
 { "target.geometry.heads",     UFT_STAGE_WRITE,  {R_,R_A,R_A,R_A,R_A,R_A}, 0, 0 },
 { "double_step",               UFT_STAGE_READ,   {R_,R_B,R_B,R_B,R_B,R_B}, 0, 0 },
 { "read.continue_on_error",    UFT_STAGE_READ,   {R_A,R_A,R_A,R_A,R_A,R_A}, 0, 0 },
 { "status_file",               UFT_STAGE_READ,   {R_,R_A,R_A,R_A,R_A,R_A}, 0, 0 },
 { "write.confirm",             UFT_STAGE_WRITE,  {R_A,R_A,R_A,R_A,R_A,R_A}, 0, 0 },
 { "write.enabled",             UFT_STAGE_WRITE,  {R_A,R_A,R_A,R_A,R_A,R_A}, 0, 0 },
 { "write.verify",              UFT_STAGE_VERIFY, {R_A,R_A,R_A,R_A,R_A,R_A}, 0, 0 },
 { "verify.retries",            UFT_STAGE_VERIFY, {R_,R_A,R_A,R_A,R_A,R_A}, 0, 0 },
 { "verify.level",              UFT_STAGE_VERIFY, {R_,R_A,R_A,R_A,R_A,R_A}, 0, 0 },
 { "allow_loss",                UFT_STAGE_WRITE,  {R_,R_,R_,R_A,R_A,R_A}, 0, 0 },

 /* ── Lesestufe ────────────────────────────────────────────────────── */
 { "read.retries",              UFT_STAGE_READ,   {R_,R_A,R_A,R_A,R_A,R_A}, 0, 0 },
 { "read.passes",               UFT_STAGE_READ,   {R_,R_A,R_A,R_A,R_A,R_A}, 0, 0 },
 { "read.revolutions",          UFT_STAGE_READ,   {R_X,R_,R_A,R_A,R_A,R_A}, 0, 0 },
 { "read.require_index",        UFT_STAGE_READ,   {R_X,R_,R_B,R_A,R_A,R_A}, 0, 0 },
 { "read.scan_past_index",      UFT_STAGE_READ,   {R_X,R_X,R_B,R_A,R_A,R_A}, 0, 0 },
 { "read.scan_overlapping",     UFT_STAGE_READ,   {R_,R_,R_B,R_A,R_A,R_A}, 0, 0 },
 { "rpm",                       UFT_STAGE_READ,   {R_X,R_B,R_A,R_L,R_,R_}, 0, 0 },
 { "data_rate",                 UFT_STAGE_READ,   {R_X,R_B,R_A,R_L,R_,R_}, 0, 0 },
 { "encoding",                  UFT_STAGE_READ,   {R_,R_B,R_A,R_L,R_A,R_}, 0, 0 },
 { "measured_rpm",              UFT_STAGE_READ,   {R_,R_,R_,R_,R_,R_L}, UFT_CAP_FLUX_IO, 0 },
 { "measured_data_rate",        UFT_STAGE_READ,   {R_,R_,R_,R_,R_,R_L}, UFT_CAP_FLUX_IO, 0 },
 { "measured_cell_time",        UFT_STAGE_READ,   {R_,R_,R_,R_,R_,R_L}, UFT_CAP_FLUX_IO, 0 },
 { "index_period",              UFT_STAGE_READ,   {R_,R_,R_,R_,R_,R_L}, UFT_CAP_FLUX_IO, 0 },
 { "track_duration",            UFT_STAGE_READ,   {R_,R_,R_,R_,R_,R_L}, UFT_CAP_FLUX_IO, 0 },

 /* ── Geometrie (Sektorebene) ──────────────────────────────────────── */
 { "geometry.cylinders",        UFT_STAGE_READ,   {R_,R_A,R_A,R_,R_,R_}, 0, 0 },
 { "geometry.heads",            UFT_STAGE_READ,   {R_,R_A,R_A,R_,R_,R_}, 0, 0 },
 { "geometry.sectors",          UFT_STAGE_READ,   {R_,R_A,R_,R_,R_,R_}, 0, 0 },
 { "geometry.sector_size",      UFT_STAGE_READ,   {R_,R_A,R_,R_,R_,R_}, 0, 0 },
 { "geometry.sector_base",      UFT_STAGE_READ,   {R_,R_A,R_,R_,R_,R_}, 0, 0 },
 { "geometry.variable_sectors", UFT_STAGE_READ,   {R_,R_A,R_,R_,R_,R_}, 0, 0 },

 /* ── Dekodierstufe ────────────────────────────────────────────────── */
 { "decode.confidence_floor",     UFT_STAGE_DECODE, {R_,R_,R_,R_A,R_A,R_A}, 0, 0 },
 { "decode.weak_phase_threshold", UFT_STAGE_DECODE, {R_X,R_,R_,R_,R_,R_A}, UFT_CAP_FLUX_IO, 0 },
 { "decode.weak_flux_variance",   UFT_STAGE_DECODE, {R_X,R_X,R_,R_,R_,R_A}, UFT_CAP_FLUX_IO, 0 },
 { "decode.splice_min_bad",       UFT_STAGE_DECODE, {R_,R_X,R_,R_,R_,R_A}, UFT_CAP_FLUX_IO, 0 },
 { "decode.max_gap_values",       UFT_STAGE_DECODE, {R_,R_,R_,R_,R_,R_A}, UFT_CAP_FLUX_IO, 0 },
 { "decode.ignore_bad_gcr",       UFT_STAGE_DECODE, {R_,R_,R_,R_,R_A,R_}, UFT_CAP_GCR, 0 },

 /* ── Layoutstufe ──────────────────────────────────────────────────── */
 { "layout.order",              UFT_STAGE_LAYOUT, {R_,R_A,R_,R_,R_,R_}, 0, 0 },
 { "layout.active_head",        UFT_STAGE_LAYOUT, {R_,R_A,R_,R_,R_,R_}, 0, 0 },
 { "layout.interleave",         UFT_STAGE_LAYOUT, {R_,R_,R_A,R_,R_,R_}, 0, 0 },
 { "layout.cyl_skew",           UFT_STAGE_LAYOUT, {R_,R_,R_A,R_,R_,R_}, 0, 0 },
 { "layout.head_skew",          UFT_STAGE_LAYOUT, {R_,R_,R_A,R_,R_,R_}, 0, 0 },
 { "layout.gap2",               UFT_STAGE_LAYOUT, {R_X,R_X,R_A,R_X,R_,R_}, 0, 0 },
 { "layout.gap3",               UFT_STAGE_LAYOUT, {R_X,R_X,R_A,R_X,R_,R_}, 0, 0 },
 { "layout.gap4a",              UFT_STAGE_LAYOUT, {R_X,R_X,R_A,R_X,R_,R_}, 0, 0 },
 { "layout.gap4b",              UFT_STAGE_LAYOUT, {R_,R_,R_A,R_X,R_,R_}, 0, 0 },
 { "layout.fill_byte",          UFT_STAGE_LAYOUT, {R_,R_,R_A,R_,R_,R_}, 0, 0 },
 { "layout.variable_sectors",   UFT_STAGE_LAYOUT, {R_,R_,R_A,R_,R_,R_}, 0, 0 },
 { "layout.preserve_sync",         UFT_STAGE_LAYOUT, {R_,R_,R_,R_A,R_A,R_}, UFT_CAP_BITSTREAM_IO, 0 },
 { "layout.preserve_gaps",         UFT_STAGE_LAYOUT, {R_,R_,R_,R_A,R_A,R_}, UFT_CAP_BITSTREAM_IO, 0 },
 { "layout.preserve_track_length", UFT_STAGE_LAYOUT, {R_,R_,R_,R_A,R_A,R_}, UFT_CAP_BITSTREAM_IO, 0 },
 { "layout.preserve_crc_errors",   UFT_STAGE_LAYOUT, {R_,R_,R_,R_A,R_,R_}, 0, 0 },
 { "layout.half_tracks",        UFT_STAGE_LAYOUT, {R_,R_,R_,R_,R_A,R_}, 0, 0 },
 { "layout.quarter_tracks",     UFT_STAGE_LAYOUT, {R_,R_,R_,R_,R_A,R_}, 0, 0 },

 /* ── Schreibstufe ─────────────────────────────────────────────────── */
 { "write.precomp",             UFT_STAGE_WRITE,  {R_X,R_,R_A,R_A,R_,R_A}, 0, 0 },

 /* ── GCR, nur auf der Nibble-Ebene ────────────────────────────────── */
 { "gcr.variant",               UFT_STAGE_DECODE, {R_,R_,R_,R_,R_A,R_}, UFT_CAP_GCR, 0 },
 { "gcr.sync_min_length",       UFT_STAGE_DECODE, {R_,R_,R_,R_,R_A,R_}, UFT_CAP_GCR, 0 },
 { "gcr.allow_illegal_codes",   UFT_STAGE_DECODE, {R_,R_,R_,R_,R_A,R_}, UFT_CAP_GCR, 0 },
 { "gcr.preserve_raw_nibbles",  UFT_STAGE_DECODE, {R_,R_,R_,R_,R_A,R_}, UFT_CAP_GCR, 0 },
 { "gcr.track_density_zone",    UFT_STAGE_DECODE, {R_,R_,R_,R_,R_A,R_}, UFT_CAP_GCR, 0 },
 { "gcr.error_policy",          UFT_STAGE_DECODE, {R_,R_,R_,R_,R_A,R_}, UFT_CAP_GCR, 0 },

 /* ── Fluss, nur auf der Flussebene ────────────────────────────────── */
 { "flux.sample_clock",         UFT_STAGE_READ,   {R_,R_,R_,R_,R_,R_A}, UFT_CAP_FLUX_IO, 0 },
 { "flux.revolution_policy",    UFT_STAGE_READ,   {R_,R_,R_,R_,R_,R_A}, UFT_CAP_FLUX_IO, 0 },
 { "flux.normalization",        UFT_STAGE_READ,   {R_,R_,R_,R_,R_,R_A}, UFT_CAP_FLUX_IO, 0 },
 { "flux.dewarp",               UFT_STAGE_READ,   {R_,R_,R_,R_,R_,R_A}, UFT_CAP_FLUX_IO, 0 },
 { "flux.jitter_policy",        UFT_STAGE_READ,   {R_,R_,R_,R_,R_,R_A}, UFT_CAP_FLUX_IO, 0 },
 { "flux.store_raw_capture",    UFT_STAGE_READ,   {R_,R_,R_,R_,R_,R_A}, UFT_CAP_FLUX_IO, 0 },
 { "flux.index_mode",           UFT_STAGE_READ,   {R_,R_,R_,R_,R_,R_A}, UFT_CAP_FLUX_IO, 0 },
 { "timing.preserve",           UFT_STAGE_LAYOUT, {R_,R_,R_,R_,R_,R_A}, UFT_CAP_TIMING, 0 },
 { "index.preserve",            UFT_STAGE_LAYOUT, {R_,R_,R_,R_,R_,R_A}, UFT_CAP_FLUX_IO, 0 },
 { "multi_revolution.preserve", UFT_STAGE_LAYOUT, {R_,R_,R_,R_,R_,R_A}, UFT_CAP_MULTI_REV, 0 },

 /* ── Dateisystem, nur auf der Dateiebene ──────────────────────────── */
 { "filesystem.show_deleted",        UFT_STAGE_FS, {R_A,R_,R_,R_,R_,R_}, UFT_CAP_FILESYSTEM, 0 },
 { "filesystem.show_slack",          UFT_STAGE_FS, {R_A,R_,R_,R_,R_,R_}, UFT_CAP_FILESYSTEM, 0 },
 { "filesystem.fat_contig_fallback", UFT_STAGE_FS, {R_A,R_,R_,R_,R_,R_}, UFT_CAP_FILESYSTEM, 0 },
 { "filesystem.fat_geometry_guess",  UFT_STAGE_FS, {R_A,R_,R_,R_,R_,R_}, UFT_CAP_FILESYSTEM, 0 },
 { "filesystem.preserve_timestamps", UFT_STAGE_FS, {R_A,R_,R_,R_,R_,R_}, UFT_CAP_FILESYSTEM, 0 },
 { "filesystem.preserve_attributes", UFT_STAGE_FS, {R_A,R_,R_,R_,R_,R_}, UFT_CAP_FILESYSTEM, 0 },
 { "filesystem.preserve_names",      UFT_STAGE_FS, {R_A,R_,R_,R_,R_,R_}, UFT_CAP_FILESYSTEM, 0 },
 { "filesystem.copy_deleted",        UFT_STAGE_FS, {R_A,R_,R_,R_,R_,R_}, UFT_CAP_FILESYSTEM, 0 },
 { "filesystem.copy_slack",          UFT_STAGE_FS, {R_A,R_,R_,R_,R_,R_}, UFT_CAP_FILESYSTEM, 0 },
 { "filesystem.preserve_directory_order", UFT_STAGE_FS, {R_A,R_,R_,R_,R_,R_}, UFT_CAP_FILESYSTEM, 0 },
 { "filesystem.preserve_allocation", UFT_STAGE_FS, {R_A,R_,R_,R_,R_,R_}, UFT_CAP_FILESYSTEM, 0 },
 { "filesystem.on_name_conflict",    UFT_STAGE_FS, {R_A,R_,R_,R_,R_,R_}, UFT_CAP_FILESYSTEM, 0 },
 { "filesystem.on_bad_file",         UFT_STAGE_FS, {R_A,R_,R_,R_,R_,R_}, UFT_CAP_FILESYSTEM, 0 },
 { "metadata.show",                  UFT_STAGE_FS, {R_A,R_,R_,R_,R_,R_}, UFT_CAP_FILESYSTEM, 0 },

 /* ── Commodore-BAM: Spezialisierung der Dateiebene ────────────────── */
 { "bam.validate",                 UFT_STAGE_FS, {R_B,R_,R_,R_,R_,R_}, UFT_CAP_CBM_BAM, 0 },
 { "bam.copy_allocated_only",      UFT_STAGE_FS, {R_B,R_,R_,R_,R_,R_}, UFT_CAP_CBM_BAM, 0 },
 { "bam.copy_unallocated",         UFT_STAGE_FS, {R_B,R_,R_,R_,R_,R_}, UFT_CAP_CBM_BAM, 0 },
 { "bam.preserve_error_map",       UFT_STAGE_FS, {R_B,R_,R_,R_,R_,R_}, UFT_CAP_CBM_BAM, 0 },
 { "bam.preserve_directory_slots", UFT_STAGE_FS, {R_B,R_,R_,R_,R_,R_}, UFT_CAP_CBM_BAM, 0 },
 { "bam.repair_policy",            UFT_STAGE_FS, {R_B,R_,R_,R_,R_,R_}, UFT_CAP_CBM_BAM, 0 },
 { "bam.geos_vlir",                UFT_STAGE_FS, {R_B,R_,R_,R_,R_,R_}, UFT_CAP_CBM_BAM, 0 },
 { "bam.rel_files",                UFT_STAGE_FS, {R_B,R_,R_,R_,R_,R_}, UFT_CAP_CBM_BAM, 0 },
 { "bam.preserve_scratched",       UFT_STAGE_FS, {R_B,R_,R_,R_,R_,R_}, UFT_CAP_CBM_BAM, 0 },
 { "bam.crosslink_policy",         UFT_STAGE_FS, {R_B,R_,R_,R_,R_,R_}, UFT_CAP_CBM_BAM, 0 },

 /* ── Uebereinstimmung und Wiederholung ────────────────────────────── */
 { "consensus.enabled",           UFT_STAGE_DECODE, {R_,R_B,R_B,R_B,R_B,R_B}, UFT_CAP_MULTI_REV, 0 },
 { "consensus.min_passes",        UFT_STAGE_DECODE, {R_,R_B,R_B,R_B,R_B,R_B}, UFT_CAP_MULTI_REV, 0 },
 { "consensus.majority_pct",      UFT_STAGE_DECODE, {R_,R_B,R_B,R_B,R_B,R_B}, UFT_CAP_MULTI_REV, 0 },
 { "consensus.min_confidence",    UFT_STAGE_DECODE, {R_,R_B,R_B,R_B,R_B,R_B}, UFT_CAP_MULTI_REV, 0 },
 { "consensus.voting_method",     UFT_STAGE_DECODE, {R_,R_B,R_B,R_B,R_B,R_B}, UFT_CAP_MULTI_REV, 0 },
 { "consensus.tie_policy",        UFT_STAGE_DECODE, {R_,R_B,R_B,R_B,R_B,R_B}, UFT_CAP_MULTI_REV, 0 },
 { "consensus.keep_alternatives", UFT_STAGE_DECODE, {R_,R_B,R_B,R_B,R_B,R_B}, UFT_CAP_MULTI_REV, 0 },
 { "consensus.save_all_passes",   UFT_STAGE_DECODE, {R_,R_B,R_B,R_B,R_B,R_B}, UFT_CAP_MULTI_REV, 0 },
 { "store_all_reads",             UFT_STAGE_READ,   {R_,R_B,R_B,R_B,R_B,R_B}, 0, 0 },
 { "retry_bad_only",              UFT_STAGE_READ,   {R_,R_B,R_B,R_B,R_B,R_B}, 0, 0 },
 { "alternate_direction",         UFT_STAGE_READ,   {R_,R_B,R_B,R_B,R_B,R_B}, 0, 0 },
 { "fill_missing",                UFT_STAGE_WRITE,  {R_,R_B,R_B,R_B,R_B,R_B}, 0, 0 },
 { "partial_output",              UFT_STAGE_WRITE,  {R_,R_B,R_B,R_B,R_B,R_B}, 0, 0 },
 { "bad_map",                     UFT_STAGE_WRITE,  {R_,R_B,R_B,R_B,R_B,R_B}, 0, 0 },
 { "resume",                      UFT_STAGE_READ,   {R_,R_B,R_B,R_B,R_B,R_B}, 0, 0 },

 /* ── Erhaltung ────────────────────────────────────────────────────── */
 { "preserve_interleave",         UFT_STAGE_LAYOUT, {R_,R_,R_B,R_B,R_B,R_B}, 0, 0 },
 { "preserve_skew",               UFT_STAGE_LAYOUT, {R_,R_,R_B,R_B,R_B,R_B}, 0, 0 },
 { "preserve_sector_ids",         UFT_STAGE_LAYOUT, {R_,R_B,R_B,R_B,R_B,R_B}, 0, 0 },
 { "preserve_crc_errors",         UFT_STAGE_LAYOUT, {R_,R_,R_,R_B,R_B,R_B}, 0, 0 },
 { "preserve_deleted_dam",        UFT_STAGE_LAYOUT, {R_,R_,R_B,R_B,R_B,R_B}, 0, 0 },
 { "preserve_duplicate_ids",      UFT_STAGE_LAYOUT, {R_,R_,R_,R_B,R_B,R_B}, 0, 0 },
 { "preserve_overlapping_sectors",UFT_STAGE_LAYOUT, {R_,R_,R_,R_B,R_B,R_B}, 0, 0 },
 { "preserve_missing_dam",        UFT_STAGE_LAYOUT, {R_,R_,R_,R_B,R_B,R_B}, 0, 0 },
 { "preserve_gaps",               UFT_STAGE_LAYOUT, {R_,R_,R_B,R_B,R_B,R_B}, 0, 0 },
 { "preserve_sync",               UFT_STAGE_LAYOUT, {R_,R_,R_,R_B,R_B,R_B}, 0, 0 },
 { "preserve_track_length",       UFT_STAGE_LAYOUT, {R_,R_,R_B,R_B,R_B,R_B}, 0, 0 },
 { "preserve_index_position",     UFT_STAGE_LAYOUT, {R_,R_,R_,R_B,R_B,R_B}, 0, 0 },
 { "preserve_weak_bits",          UFT_STAGE_LAYOUT, {R_,R_,R_,R_B,R_B,R_B}, UFT_CAP_WEAK_BITS, 0 },
 { "preserve_multi_revolution",   UFT_STAGE_LAYOUT, {R_,R_,R_,R_,R_,R_B}, UFT_CAP_MULTI_REV, 0 },
 { "preserve_half_tracks",        UFT_STAGE_LAYOUT, {R_,R_,R_,R_B,R_B,R_B}, 0, 0 },
 { "bitexact.kind",               UFT_STAGE_LAYOUT, {R_,R_B,R_B,R_B,R_B,R_B}, 0, 0 },

 /* ── Beweisfuehrung ───────────────────────────────────────────────── */
 { "source.write_enabled",      UFT_STAGE_WRITE,    {R_A,R_A,R_A,R_A,R_A,R_A}, 0, 0 },
 { "repair.enabled",            UFT_STAGE_WRITE,    {R_A,R_A,R_A,R_A,R_A,R_A}, 0, 0 },
 { "normalization.destructive", UFT_STAGE_WRITE,    {R_A,R_A,R_A,R_A,R_A,R_A}, 0, 0 },
 { "store_original_capture",    UFT_STAGE_EVIDENCE, {R_,R_B,R_B,R_B,R_B,R_B}, 0, 0 },
 { "verify_source_unchanged",   UFT_STAGE_EVIDENCE, {R_B,R_B,R_B,R_B,R_B,R_B}, 0, 0 },
 { "provenance.enabled",        UFT_STAGE_EVIDENCE, {R_B,R_B,R_B,R_B,R_B,R_B}, 0, 0 },
 { "hash.enabled",              UFT_STAGE_EVIDENCE, {R_B,R_B,R_B,R_B,R_B,R_B}, 0, 0 },
 { "attest.enabled",            UFT_STAGE_EVIDENCE, {R_B,R_B,R_B,R_B,R_B,R_B}, 0, 0 },
 { "evidence.case_id",          UFT_STAGE_EVIDENCE, {R_B,R_B,R_B,R_B,R_B,R_B}, 0, 0 },
 { "evidence.acquisition_id",   UFT_STAGE_EVIDENCE, {R_B,R_B,R_B,R_B,R_B,R_B}, 0, 0 },
 { "evidence.operator",         UFT_STAGE_EVIDENCE, {R_B,R_B,R_B,R_B,R_B,R_B}, 0, 0 },
 { "evidence.timestamp",        UFT_STAGE_EVIDENCE, {R_B,R_B,R_B,R_B,R_B,R_B}, 0, 0 },
 { "evidence.source_description", UFT_STAGE_EVIDENCE, {R_B,R_B,R_B,R_B,R_B,R_B}, 0, 0 },
 { "evidence.drive_model",      UFT_STAGE_EVIDENCE, {R_B,R_B,R_B,R_B,R_B,R_B}, 0, 0 },
 { "evidence.drive_serial",     UFT_STAGE_EVIDENCE, {R_B,R_B,R_B,R_B,R_B,R_B}, 0, 0 },
 { "evidence.controller",       UFT_STAGE_EVIDENCE, {R_B,R_B,R_B,R_B,R_B,R_B}, 0, 0 },
 { "evidence.controller_firmware", UFT_STAGE_EVIDENCE, {R_B,R_B,R_B,R_B,R_B,R_B}, 0, 0 },
 { "evidence.write_blocker",    UFT_STAGE_EVIDENCE, {R_B,R_B,R_B,R_B,R_B,R_B}, 0, 0 },
 { "evidence.hash_algorithms",  UFT_STAGE_EVIDENCE, {R_B,R_B,R_B,R_B,R_B,R_B}, 0, 0 },
 { "evidence.sign_report",      UFT_STAGE_EVIDENCE, {R_B,R_B,R_B,R_B,R_B,R_B}, 0, 0 },
 { "evidence.notes",            UFT_STAGE_EVIDENCE, {R_B,R_B,R_B,R_B,R_B,R_B}, 0, 0 },
};

#define K_PARAM_N (sizeof(k_param) / sizeof(k_param[0]))

/* ── Erzwungene Werte je Dimension ──────────────────────────────────── */
typedef struct { const char *param, *value; } setzt_t;

static const setzt_t k_strat_fast[] = {
    { "read.retries", "0" }, { "read.passes", "1" },
    { "read.revolutions", "1" }, { "consensus.enabled", "false" },
    { "store_all_reads", "false" }, { NULL, NULL } };
static const setzt_t k_strat_standard[] = {
    { "read.retries", "3" }, { "read.passes", "1" },
    { "read.revolutions", "2" }, { "retry_bad_only", "true" },
    { "consensus.enabled", "false" }, { NULL, NULL } };
static const setzt_t k_strat_deep[] = {
    { "read.retries", "10" }, { "read.passes", "5" },
    { "read.revolutions", "5" }, { "retry_bad_only", "false" },
    { "alternate_direction", "true" }, { "store_all_reads", "true" },
    { "status_file", "Pflicht" }, { "read.continue_on_error", "true" },
    { NULL, NULL } };
static const setzt_t k_strat_consensus[] = {
    { "consensus.enabled", "true" }, { "consensus.min_passes", "noetig" },
    { "consensus.majority_pct", "noetig" },
    { "consensus.min_confidence", "noetig" },
    { "consensus.voting_method", "noetig" },
    { "consensus.tie_policy", "noetig" },
    { "consensus.keep_alternatives", "true" },
    { "consensus.save_all_passes", "true" },
    /* Schwache Bits duerfen NICHT wegentschieden werden — die Vorgabe
     * sagt es ausdruecklich, und es ist die Missionszeile. */
    { "preserve_weak_bits", "true" }, { NULL, NULL } };
static const setzt_t k_strat_salvage[] = {
    { "read.retries", "hoch" }, { "read.passes", "hoch" },
    { "read.continue_on_error", "true" }, { "store_all_reads", "true" },
    { "status_file", "Pflicht" }, { "fill_missing", "false" },
    { "partial_output", "true" }, { "bad_map", "Pflicht" },
    { "resume", "true" }, { NULL, NULL } };

static const setzt_t *const k_strategie[UFT_READ_STRATEGY_N] = {
    k_strat_fast, k_strat_standard, k_strat_deep,
    k_strat_consensus, k_strat_salvage };

static const setzt_t k_erh_logical[] = { { NULL, NULL } };
static const setzt_t k_erh_layout[] = {
    { "preserve_interleave", "true" }, { "preserve_skew", "true" },
    { "preserve_sector_ids", "true" }, { NULL, NULL } };
static const setzt_t k_erh_protected[] = {
    { "allow_loss", "false" }, { "preserve_crc_errors", "true" },
    { "preserve_deleted_dam", "true" }, { "preserve_duplicate_ids", "true" },
    { "preserve_overlapping_sectors", "true" },
    { "preserve_missing_dam", "true" }, { "preserve_gaps", "true" },
    { "preserve_sync", "true" }, { "preserve_track_length", "true" },
    { "preserve_index_position", "true" }, { "preserve_weak_bits", "true" },
    { "preserve_multi_revolution", "true" },
    { "preserve_half_tracks", "true" }, { NULL, NULL } };
static const setzt_t k_erh_bitexact[] = {
    { "preserve_track_length", "true" }, { "bitexact.kind", "gewaehlt" },
    { NULL, NULL } };

static const setzt_t *const k_erhaltung[UFT_PRESERVE_N] = {
    k_erh_logical, k_erh_layout, k_erh_protected, k_erh_bitexact };

static const setzt_t k_pol_normal[] = { { NULL, NULL } };
static const setzt_t k_pol_verify[] = {
    { "write.verify", "true" }, { "verify.retries", "noetig" },
    { "verify.level", "noetig" }, { NULL, NULL } };
static const setzt_t k_pol_evidence[] = {
    { "source.write_enabled", "false" }, { "repair.enabled", "false" },
    { "normalization.destructive", "false" }, { "allow_loss", "false" },
    { "store_original_capture", "true" }, { "store_all_reads", "true" },
    { "verify_source_unchanged", "true" }, { "provenance.enabled", "true" },
    { "hash.enabled", "true" }, { "attest.enabled", "true" },
    { "write.verify", "true" }, { NULL, NULL } };

static const setzt_t *const k_politik[UFT_POLICY_N] = {
    k_pol_normal, k_pol_verify, k_pol_evidence };

/* Mindestebene je Erhaltungsrichtlinie. */
static const uft_copy_level_t k_mindestebene[UFT_PRESERVE_N] = {
    UFT_COPY_FILE, UFT_COPY_TRACK, UFT_COPY_BITSTREAM, UFT_COPY_BITSTREAM };

/* ── Namen ──────────────────────────────────────────────────────────── */
static const char *const k_level_name[UFT_COPY_LEVEL_N] = {
    "Dateien", "Sektoren", "Spuren", "Raw-Bitstrom", "Nibble/GCR", "Flux",
    "Automatisch" };
static const char *const k_strategy_name[UFT_READ_STRATEGY_N] = {
    "Schnell", "Standard", "Tiefenlesen", "Consensus", "Datenrettung" };
static const char *const k_preserve_name[UFT_PRESERVE_N] = {
    "Logischer Inhalt", "Spurlayout", "Kopierschutz", "Bitgenau" };
static const char *const k_policy_name[UFT_POLICY_N] = {
    "Normal", "Schreiben und pruefen", "Forensisches Abbild" };
static const char *const k_stage_name[UFT_STAGE_N] = {
    "Lesen", "Dekodieren", "Layout", "Schreiben", "Pruefen",
    "Dateisystem", "Beweis" };

const char *uft_copy_level_name(uft_copy_level_t v)
{ return (v >= 0 && v < UFT_COPY_LEVEL_N) ? k_level_name[v] : NULL; }
const char *uft_copy_strategy_name(uft_read_strategy_t v)
{ return (v >= 0 && v < UFT_READ_STRATEGY_N) ? k_strategy_name[v] : NULL; }
const char *uft_copy_preservation_name(uft_preservation_t v)
{ return (v >= 0 && v < UFT_PRESERVE_N) ? k_preserve_name[v] : NULL; }
const char *uft_copy_policy_name(uft_copy_policy_t v)
{ return (v >= 0 && v < UFT_POLICY_N) ? k_policy_name[v] : NULL; }
const char *uft_copy_stage_name(uft_copy_stage_t v)
{ return (v >= 0 && v < UFT_STAGE_N) ? k_stage_name[v] : NULL; }

/* ── Namen der Faehigkeitsflaggen (MF-1238) ─────────────────────────── */

/* Eine Zeile je Flagge. Die Tafel ist die EINZIGE Stelle, an der diese
 * acht Namen stehen — eine zweite Kopie in der Oberflaeche waere die
 * Lage aus MF-1177.
 *
 * MF-1261: die Zusage stimmte nicht. Zehn Zeilen tiefer, IN DIESER
 * DATEI, stand eine `if`-Kette, die sechs der acht Flaggen ein zweites
 * Mal aufzaehlte — und dabei etwas behauptete, das der Kern nicht weiss
 * (`P3-508`). Deshalb traegt die Tafel jetzt auch die Begruendung:
 * wer eine Flagge hinzufuegt, bekommt sie mit, statt sie zu vergessen.
 *
 * WARUM DIE BEGRUENDUNG SO KLINGT, WIE SIE KLINGT: `uft_copy_profile_
 * available()` sieht eine Merkmalsmaske und sonst nichts. Ob eine
 * fehlende Flagge GEMESSEN fehlt oder gar nicht messbar war, steht dort
 * nicht — also darf keine Begruendung „nicht erkannt" sagen. „Nicht
 * zugesagt" ist die Aussage, die der Kern wirklich treffen kann, und
 * sie war in einer der sechs alten Zeichenketten schon richtig
 * getroffen („keine Mehrfachlesung zugesagt"). */
static const struct {
    uft_copy_caps_t flagge;
    const char     *name;
    const char     *fehlt;   /* Begruendung, wenn ein Profil sie verlangt
                              * und die Maske sie nicht fuehrt. */
} k_cap_name[] = {
    { UFT_CAP_FLUX_IO,      "Fluss",
      "setzt Fluss voraus — nicht zugesagt" },
    { UFT_CAP_BITSTREAM_IO, "Bitstrom",
      "setzt Bitstrom voraus — nicht zugesagt" },
    { UFT_CAP_MULTI_REV,    "Mehrfachlesung",
      "setzt Mehrfachlesung voraus — nicht zugesagt" },
    { UFT_CAP_TIMING,       "Timing",
      "setzt Timing voraus — nicht zugesagt" },
    { UFT_CAP_WEAK_BITS,    "schwache Bits",
      "setzt schwache Bits voraus — nicht zugesagt" },
    { UFT_CAP_FILESYSTEM,   "Dateisystem",
      "setzt Dateisystem voraus — nicht zugesagt" },
    { UFT_CAP_CBM_BAM,      "Commodore-BAM",
      "setzt Commodore-BAM voraus — nicht zugesagt" },
    { UFT_CAP_GCR,          "GCR",
      "setzt GCR voraus — nicht zugesagt" },
};
#define K_CAP_N (sizeof(k_cap_name) / sizeof(k_cap_name[0]))

const char *uft_copy_cap_name(uft_copy_caps_t v)
{
    /* Genau eine Flagge, sonst nichts. `v & (v - 1)` ist ungleich null,
     * sobald mehr als ein Bit gesetzt ist. */
    const uint32_t b = (uint32_t)v;
    if (b == 0u || (b & (b - 1u)) != 0u) return NULL;
    for (size_t i = 0; i < K_CAP_N; i++)
        if ((uint32_t)k_cap_name[i].flagge == b) return k_cap_name[i].name;
    return NULL;
}

size_t uft_copy_cap_count(void) { return K_CAP_N; }

uft_copy_caps_t uft_copy_cap_at(size_t i)
{ return (i < K_CAP_N) ? k_cap_name[i].flagge : UFT_CAP_NONE; }

/* ── Namen der Feinheiten ───────────────────────────────────────────── */
static const char *const k_track_mode_name[UFT_TRACK_MODE_N] = {
    "decoded (Spur neu erzeugen)", "raw (Bitstrom erhalten)" };
static const char *const k_file_special_name[UFT_FILE_SPECIAL_N] = {
    "allgemein", "Commodore BAM", "DOS" };
static const char *const k_gcr_name[UFT_GCR_N] = {
    "Commodore GCR", "Apple GCR", "Macintosh GCR", "Victor 9000 GCR" };
static const char *const k_vote_name[UFT_VOTE_N] = {
    "strict-majority", "weighted-confidence", "crc-preferred",
    "timing-distance", "per-bit", "per-sector", "per-track" };
static const char *const k_exact_name[UFT_EXACT_N] = {
    "sector-exact", "track-bit-exact", "flux-timing-exact" };

const char *uft_copy_track_mode_name(uft_track_mode_t v)
{ return (v >= 0 && v < UFT_TRACK_MODE_N) ? k_track_mode_name[v] : NULL; }
const char *uft_copy_file_special_name(uft_file_special_t v)
{ return (v >= 0 && v < UFT_FILE_SPECIAL_N) ? k_file_special_name[v] : NULL; }
const char *uft_copy_gcr_name(uft_gcr_variant_t v)
{ return (v >= 0 && v < UFT_GCR_N) ? k_gcr_name[v] : NULL; }
const char *uft_copy_vote_name(uft_vote_method_t v)
{ return (v >= 0 && v < UFT_VOTE_N) ? k_vote_name[v] : NULL; }
const char *uft_copy_exact_name(uft_bitexact_kind_t v)
{ return (v >= 0 && v < UFT_EXACT_N) ? k_exact_name[v] : NULL; }

uft_copy_plan_t uft_copy_plan_default(void)
{
    uft_copy_plan_t p;
    p.level        = UFT_COPY_SECTOR;
    p.strategy     = UFT_READ_STANDARD;
    p.preservation = UFT_PRESERVE_LOGICAL;
    p.policy       = UFT_POLICY_NORMAL;
    p.exact_kind   = UFT_EXACT_SECTOR;
    p.track_mode   = UFT_TRACK_DECODED;
    p.file_special = UFT_FILE_GENERIC;
    p.gcr          = UFT_GCR_COMMODORE;
    p.vote         = UFT_VOTE_STRICT_MAJORITY;
    /* SHA-256 ist die Vorgabe, nicht CRC32: ein Pruefwert, den man in
     * Sekunden faelscht, ist kein Beweis. CRC32 kommt nur DAZU. */
    p.hashes       = (uint32_t)UFT_HASH_SHA256;
    return p;
}

/* ── Die geltende Quelle (MF-1265) ──────────────────────────────────
 *
 * `P3-509`, zweiter Halbsatz. Der Kern haelt KEINEN Plan, sondern die
 * Auskunft, wo einer zu holen ist — eine hinterlegte Kopie koennte
 * veralten, sobald der Bediener etwas umstellt.
 *
 * Die erste Fassung legte diese Auskunft in `FormatTab`. Der Binder
 * hat daran einen Entwurfsfehler gefunden: `toolstab.cpp` haette
 * damit an `formattab.cpp` gehangen, und zwei Qt-Tests binden das
 * eine ohne das andere. Eine GUI-Klasse von einer anderen abhaengig zu
 * machen ist die falsche Richtung — die Auskunft wohnt dort, wo der
 * Plan ohnehin wohnt.
 *
 * Kein Schutz gegen Faeden: die Quelle wird im GUI-Faden gesetzt und
 * im GUI-Faden gelesen. Wer woanders laeuft, nimmt eine Kopie mit
 * (`DecodeJob::setCopyPlan`) — das steht im Kopf und ist der Grund,
 * warum es hier kein Schloss braucht. */
static uft_copy_plan_quelle_fn s_quelle       = NULL;
static void                   *s_zusammenhang = NULL;

void uft_copy_plan_set_quelle(uft_copy_plan_quelle_fn fn, void *zusammenhang)
{
    s_quelle       = fn;
    s_zusammenhang = zusammenhang;
}

uft_copy_plan_t uft_copy_plan_current(void)
{
    /* Ohne Quelle die VORGABEN, nicht ein genullter Zufall: ein
     * genullter Plan waere ein GUELTIGER Plan (FILE/FAST/LOGICAL/
     * NORMAL) und damit eine Aussage, die niemand getroffen hat. */
    if (!s_quelle) return uft_copy_plan_default();
    return s_quelle(s_zusammenhang);
}

/* ── Die Profile (MF-1236) ──────────────────────────────────────────
 *
 * Herkunft: Entwurf des Eigentuemers, `test-gui/06_kopierplan-meine
 * -idee.html`. Uebernommen mit ZWEI Berichtigungen, beide gemessen:
 *
 *   BAMCopy stand dort auf `preservation: layout`. Die Erhaltung
 *   „Spurlayout“ verlangt mindestens die Spurebene (Rang 2), BAMCopy
 *   steht auf der Dateiebene (Rang 0) — `uft_copy_plan_check()` haette
 *   fuer JEDE Wahl `ebene_zu_hoch` gemeldet, hart. Der Denkfehler ist
 *   nachvollziehbar: BAM-Erhaltung fuehlt sich nach Layout an, laeuft
 *   aber ueber `bam.preserve_error_map` und
 *   `bam.preserve_directory_slots` — Dateisystemparameter. Also
 *   `logical`.
 *
 *   Cyclone und ProtectedCopy trugen dasselbe Viertupel
 *   (auto/deep/protected/verify) unter zwei Namen. Eigentuemer-
 *   Entscheidung vom 2026-09-18: Cyclone ist der C64-Nibbler, also
 *   `nibble`. Damit unterscheiden sie sich wirklich.
 *
 * Was hier NICHT steht: „Benutzerdefiniert“. Das ist die Abwesenheit
 * eines Profils und ein Zustand der Oberflaeche, kein Eintrag im Kern.
 */
/* `PLF` traegt die Dateispezialisierung mit — ohne sie waeren DOSCopy
 * und BAMCopy nach der Berichtigung oben DERSELBE Plan, und genau das
 * hat die Zusage P3 im ersten Lauf gemeldet. Die Spezialisierung ist
 * kein Beiwerk: sie IST der Unterschied zwischen den beiden. */
/* MF-1312 — die Bit-Genauigkeit ist ein PARAMETER geworden.
 *
 * Gemessen trugen ALLE 17 Profile `UFT_EXACT_SECTOR`: 15 ueber `PL`,
 * 2 ueber `PLF`, und beide reichten die Konstante fest durch. Fuer
 * `FluxCopy` ist das ein Widerspruch zur eigenen Beschreibung —
 * "Flussuebergaenge, Indexposition und Zeiten erhalten" gegen eine
 * Genauigkeit, die auf Sektoren endet. Dasselbe bei "Bittreu"
 * ("Hoechstmoegliche Bit- oder Flussgenauigkeit").
 *
 * Und der Widerspruch blieb nicht in der Anzeige: die Angabe erreicht
 * gemessen DREI Ausgaenge, einer davon bleibend — `textPlanJson`
 * (Anzeige), `onPlanJsonKopieren` (Zwischenablage) und
 * `onPlanJsonSichern`, das den Plan in eine DATEI schreibt. Eine falsche
 * Zusage, die auf Platte landet, ist keine Anzeigefrage mehr.
 *
 * `PLX` nimmt die Genauigkeit entgegen, `PLF` und `PL` bleiben als
 * Abkuerzung mit `UFT_EXACT_SECTOR` — damit aendert sich nur, was sich
 * aendern soll. Die beiden neuen Felder aus MF-1311 stehen hier
 * ausdruecklich: ein Profil ist eine Vorlage ohne gemessene
 * Faehigkeiten, also `caps = 0` und `caps_bekannt = false`. Ohne sie
 * meldete der Bau 17-mal "missing initializer for field 'caps'". */
#define PLX(l, s, e, p, ek, fs) { (l), (s), (e), (p), (ek),              \
                         UFT_TRACK_DECODED, (fs),                       \
                         UFT_GCR_COMMODORE, UFT_VOTE_STRICT_MAJORITY,   \
                         (uint32_t)UFT_HASH_SHA256, 0u, false }
#define PLF(l, s, e, p, fs) PLX(l, s, e, p, UFT_EXACT_SECTOR, fs)
#define PL(l, s, e, p) PLF(l, s, e, p, UFT_FILE_GENERIC)

static const uft_copy_profile_t k_profil[] = {
 { "standardcopy", "Standardkopie",
   "Universelle Kopie; UFT waehlt die Ebene aus dem Format.",
   PL(UFT_COPY_AUTO, UFT_READ_STANDARD, UFT_PRESERVE_LOGICAL, UFT_POLICY_VERIFY),
   0u, NULL },

 { "doscopy", "DOSCopy",
   "DOS/FAT-Dateien mit Verzeichnis und Zeitstempeln.",
   PLF(UFT_COPY_FILE, UFT_READ_STANDARD, UFT_PRESERVE_LOGICAL,
       UFT_POLICY_VERIFY, UFT_FILE_DOS),
   (uint32_t)UFT_CAP_FILESYSTEM,
   "IMG;IMA;VFD;IMD;TD0;CFI;QRST" },

 /* BERICHTIGT: war `layout` — siehe oben. */
 { "bamcopy", "BAMCopy",
   "Commodore-Dateien mit BAM, Verzeichnis und Fehlerkarte.",
   PLF(UFT_COPY_FILE, UFT_READ_STANDARD, UFT_PRESERVE_LOGICAL,
       UFT_POLICY_VERIFY, UFT_FILE_BAM),
   (uint32_t)UFT_CAP_CBM_BAM | (uint32_t)UFT_CAP_FILESYSTEM,
   "D64;D71;D81;G64;G71" },

 { "fastcopy", "FastCopy",
   "Schnelle Sektorkopie fuer nachweislich gute Medien.",
   PL(UFT_COPY_SECTOR, UFT_READ_FAST, UFT_PRESERVE_LOGICAL, UFT_POLICY_NORMAL),
   0u, NULL },

 { "sectorcopy", "SectorCopy",
   "Logische Sektorkopie mit Wiederholungen.",
   PL(UFT_COPY_SECTOR, UFT_READ_STANDARD, UFT_PRESERVE_LOGICAL, UFT_POLICY_VERIFY),
   0u, NULL },

 { "trackcopy", "TrackCopy",
   "Vollstaendige Spuren mit IDs, Interleave und Zwischenraeumen.",
   PL(UFT_COPY_TRACK, UFT_READ_STANDARD, UFT_PRESERVE_LAYOUT, UFT_POLICY_VERIFY),
   0u, NULL },

 { "spurlese", "Spurlese",
   "Spuren vollstaendig lesen und auswerten, ohne zu schreiben.",
   PL(UFT_COPY_TRACK, UFT_READ_STANDARD, UFT_PRESERVE_LAYOUT, UFT_POLICY_NORMAL),
   0u, NULL },

 { "rawcopy", "RawCopy",
   "Rohbitstrom mit Sync, Zwischenraeumen und CRC-Fehlern.",
   PL(UFT_COPY_BITSTREAM, UFT_READ_STANDARD, UFT_PRESERVE_LAYOUT, UFT_POLICY_VERIFY),
   (uint32_t)UFT_CAP_BITSTREAM_IO, NULL },

 { "nibblecopy", "NibbleCopy",
   "GCR-/Nibble-Kopie fuer Commodore- und Apple-Verfahren.",
   PL(UFT_COPY_NIBBLE, UFT_READ_STANDARD, UFT_PRESERVE_LAYOUT, UFT_POLICY_VERIFY),
   (uint32_t)UFT_CAP_GCR,
   /* MF-1262 (`P3-510`): `D81` ist hier RAUS. Das 1581 schreibt MFM,
    * und ein GCR-Behelf, der ein MFM-Format fuehrt, bietet ein
    * Verfahren auf einem Medium an, das es nicht traegt — ausgerechnet
    * dann, wenn die Flagge nicht zugesagt ist und niemand widerspricht.
    *
    * Vier Zeugen, alle im eigenen Baum: `uft_d81_parser_v2.c:9` („MFM
    * encoding (not GCR!)"), `mfm_detect.c:87/115` (die 1581 ist eine
    * Geometrie des MFM-Erkenners), `commodore/d81.c:100` („no GCR
    * timing ... preserved") — und `cyclone` weiter unten, das ebenfalls
    * GCR verlangt und D81 nie gefuehrt hat. Der Baum widersprach sich
    * selbst; die richtige Seite lag schon darin.
    *
    * `bamcopy` behaelt D81 zu Recht: es verlangt `CBM_BAM |
    * FILESYSTEM`, und eine BAM ist Dateisystemebene. */
   "D64;D71;G64;G71;NIB;WOZ;DO;PO" },

 /* MF-1312: die Genauigkeit sagt jetzt dasselbe wie die Beschreibung.
  *
  * Bis hierher stand `UFT_EXACT_SECTOR` — ein Profil, das Flusszeiten
  * verspricht und Sektorgleichheit zusagt. Der Widerspruch war nicht
  * folgenlos: er ging in die Anzeige, in die Zwischenablage UND ueber
  * `onPlanJsonSichern` in eine Datei. */
 { "fluxcopy", "FluxCopy",
   "Flussuebergaenge, Indexposition und Zeiten erhalten.",
   PLX(UFT_COPY_FLUX, UFT_READ_STANDARD, UFT_PRESERVE_BIT_EXACT,
       UFT_POLICY_VERIFY, UFT_EXACT_FLUX_TIMING, UFT_FILE_GENERIC),
   (uint32_t)UFT_CAP_FLUX_IO, NULL },

 { "deepcopy", "DeepCopy",
   "Mehrfach lesen, Rohlesungen behalten, Fortschritt festhalten.",
   PL(UFT_COPY_AUTO, UFT_READ_DEEP, UFT_PRESERVE_LOGICAL, UFT_POLICY_VERIFY),
   0u, NULL },

 { "consensuscopy", "ConsensusCopy",
   "Mehrere Lesungen vergleichen und das beste Ergebnis bilden.",
   PL(UFT_COPY_AUTO, UFT_READ_CONSENSUS, UFT_PRESERVE_LAYOUT, UFT_POLICY_VERIFY),
   (uint32_t)UFT_CAP_MULTI_REV, NULL },

 /* BERICHTIGT: war identisch mit ProtectedCopy — jetzt der C64-Nibbler,
  * Eigentuemer-Entscheidung 2026-09-18. */
 { "cyclone", "Cyclone",
   "Kopierschutz auf Nibble-Ebene intensiv lesen (C64).",
   PL(UFT_COPY_NIBBLE, UFT_READ_DEEP, UFT_PRESERVE_PROTECTED, UFT_POLICY_VERIFY),
   (uint32_t)UFT_CAP_GCR,
   "D64;D71;G64;G71;NIB" },

 { "protectedcopy", "ProtectedCopy",
   "CRC-Anomalien, schwache Bits und Timing erhalten.",
   PL(UFT_COPY_AUTO, UFT_READ_DEEP, UFT_PRESERVE_PROTECTED, UFT_POLICY_VERIFY),
   0u, NULL },

 /* MF-1312: "Hoechstmoegliche" heisst jetzt hoechstmoegliche.
  *
  * Die Aspiration steht hier, die Deckelung in `uft_copy_plan_resolve()`
  * — auf einem Sektorpaar wird daraus SECTOR, auf einem Flusspaar
  * FLUX_TIMING. Fest eingetragen waere sie auf jedem Sektorformat ein
  * harter Befund gewesen. */
 { "bitexact", "Bittreu",
   "Hoechstmoegliche Bit- oder Flussgenauigkeit.",
   PLX(UFT_COPY_AUTO, UFT_READ_CONSENSUS, UFT_PRESERVE_BIT_EXACT,
       UFT_POLICY_VERIFY, UFT_EXACT_FLUX_TIMING, UFT_FILE_GENERIC),
   (uint32_t)UFT_CAP_MULTI_REV, NULL },

 { "salvage", "Datenrettung",
   "Lesbares retten, Fehlerkarte fuehren, nichts erfinden.",
   PL(UFT_COPY_AUTO, UFT_READ_SALVAGE, UFT_PRESERVE_LOGICAL, UFT_POLICY_VERIFY),
   0u, NULL },

 { "evidence", "EvidenceCopy",
   "Beweissicherung mit Rohaufnahmen, SHA-256 und Herkunft.",
   PL(UFT_COPY_AUTO, UFT_READ_DEEP, UFT_PRESERVE_LOGICAL, UFT_POLICY_EVIDENCE),
   0u, NULL },
};
#undef PL

#define K_PROFIL_N (sizeof(k_profil) / sizeof(k_profil[0]))

size_t uft_copy_profile_count(void) { return K_PROFIL_N; }

const uft_copy_profile_t *uft_copy_profile(size_t i)
{
    return (i < K_PROFIL_N) ? &k_profil[i] : NULL;
}

const uft_copy_profile_t *uft_copy_profile_by_id(const char *id)
{
    if (!id) return NULL;
    for (size_t i = 0; i < K_PROFIL_N; i++)
        if (strcmp(k_profil[i].id, id) == 0) return &k_profil[i];
    return NULL;
}

/* Steht `format` in der Behelfsliste? Verglichen wird das ganze Glied,
 * nicht als Teilzeichenkette: „DO“ steckt sonst in „DOS“, und die Falle
 * ist in diesem Baum belegt (teilstring_statt_zeichen). */
static bool in_behelf(const char *liste, const char *format)
{
    if (!liste || !format || !*format) return false;
    const size_t n = strlen(format);
    for (const char *p = liste; *p; ) {
        const char *e = strchr(p, ';');
        const size_t len = e ? (size_t)(e - p) : strlen(p);
        if (len == n) {
            size_t i = 0;
            for (; i < n; i++) {
                char a = p[i], b = format[i];
                if (a >= 'a' && a <= 'z') a = (char)(a - 'a' + 'A');
                if (b >= 'a' && b <= 'z') b = (char)(b - 'a' + 'A');
                if (a != b) break;
            }
            if (i == n) return true;
        }
        if (!e) break;
        p = e + 1;
    }
    return false;
}

bool uft_copy_profile_available(const uft_copy_profile_t *p,
                                uint32_t caps, const char *format,
                                const char **grund)
{
    if (grund) *grund = NULL;
    if (!p) return false;
    if (!p->braucht) return true;

    if ((caps & p->braucht) == p->braucht) return true;

    /* Die Flagge fehlt. Bevor abgelehnt wird: traegt der Behelf?
     *
     * Das ist ausdruecklich KEINE Messung, sondern der Ersatz fuer eine
     * fehlende — und er gilt nur, solange die Flagge von NIEMANDEM
     * gesetzt wird. Sobald ein Plugin sie fuehrt, entscheidet sie. */
    if (p->behelf_formate && in_behelf(p->behelf_formate, format))
        return true;

    /* MF-1261 (`P3-508`): die Begruendung kommt aus der Tafel, und sie
     * behauptet KEINE Messung.
     *
     * Hier stand eine `if`-Kette mit sechs Zweigen, von denen drei einen
     * Befund ueber die DISKETTE aussprachen — „kein GCR-Format erkannt",
     * „keine Commodore-BAM erkannt", „kein Dateisystem erkannt". Diese
     * Funktion sieht aber nur `caps`; ob eine fehlende Flagge gemessen
     * fehlt oder gar nicht messbar war, weiss sie nicht. Gemessen hat
     * der Rotbeweis VIER solche Zeilen (`doscopy`, `bamcopy`,
     * `nibblecopy`, `cyclone`) — eine mehr, als der Befund nannte.
     *
     * Und der einzige Produktivaufrufer stellt die Falschaussage direkt
     * neben ihre Berichtigung: `src/formattab.cpp` zeigt fuer dieselben
     * vier Flaggen „nicht feststellbar (dieser Reiter oeffnet kein
     * Abbild)". Zwei Saetze ueber dieselbe Flagge, in einem Dialog.
     *
     * Die Schleife laeuft ueber die Tafel statt ueber eine Aufzaehlung:
     * die alte Kette kannte sechs der acht Flaggen, `Timing` und
     * `schwache Bits` fielen in einen Sammelzweig. Heute verlangt kein
     * Profil die zwei — die Luecke war also latent, nicht wirksam, und
     * sie ist trotzdem zu (MF-636/D3). Die Reihenfolge ist die der
     * Tafel und damit nachlesbar, nicht die einer gewachsenen Kette.
     *
     * GEMESSENE FOLGE DER REIHENFOLGE, damit sie niemanden ueberrascht:
     * verlangt ein Profil MEHRERE Flaggen und fehlen sie alle, nennt die
     * Schleife die erste der Tafel. `bamcopy` (Dateisystem + Commodore-
     * BAM) sagt bei leerer Maske deshalb „setzt Dateisystem voraus", wo
     * die alte Kette „Commodore-BAM" sagte. Beides ist wahr; und sobald
     * eine der beiden zugesagt ist, nennt die Schleife genau die, die
     * WIRKLICH fehlt — sie prueft `!(caps & f)`, nicht nur `braucht`. */
    if (grund) {
        *grund = "eine benoetigte Faehigkeit ist nicht zugesagt";
        for (size_t i = 0; i < K_CAP_N; i++) {
            const uint32_t f = (uint32_t)k_cap_name[i].flagge;
            if ((p->braucht & f) && !(caps & f)) {
                *grund = k_cap_name[i].fehlt;
                break;
            }
        }
    }
    return false;
}

/* ── Der Plan wirkt (MF-1263, `P3-509`) ─────────────────────────────── */

/* Eine Zeichenkette IST eine Zahl — oder sie ist es nicht.
 *
 * Die Wertetafel des Plans fuehrt beides nebeneinander: `"3"` ist eine
 * Zahl, `"hoch"` und `"noetig"` sind Forderungen. `strtoul` wuerde aus
 * `"hoch"` eine 0 machen und damit eine Forderung in eine Zahl
 * verwandeln — genau das Raten, das dieser Baum verbietet. Deshalb
 * wird streng geprueft: nur Ziffern, mindestens eine, sonst `false`. */
/* MF-1264: die Art eines Tafelwerts — an EINER Stelle entschieden.
 *
 * Vorher entschied jede Stelle fuer sich: `haenge_wert()` schnuefferte
 * beim JSON-Schreiben, `nur_zahl()` beim Abbilden, und der Kommentar
 * am Struct zaehlte die Faelle auf („5", „true", „false", „Pflicht").
 * Drei Lesarten derselben Frage.
 *
 * Die Reihenfolge ist Absicht: erst Wahrheitswert (exakt), dann Zahl
 * (nur Ziffern), sonst Forderung. Eine leere Zeichenkette ist KEINE
 * Zahl — `nur_zahl("")` waere sonst 0, und das ist die Falle, gegen
 * die diese Funktion steht. */
uft_wert_art_t uft_copy_wert_art(const char *v)
{
    if (!v || !*v) return UFT_WERT_FORDERUNG;
    if (strcmp(v, "true") == 0 || strcmp(v, "false") == 0)
        return UFT_WERT_WAHRHEIT;
    for (const char *p = v; *p; p++)
        if (*p < '0' || *p > '9') return UFT_WERT_FORDERUNG;
    return UFT_WERT_ZAHL;
}

static bool nur_zahl(const char *s, unsigned long *aus)
{
    if (!s || !*s) return false;
    unsigned long n = 0;
    for (const char *p = s; *p; p++) {
        if (*p < '0' || *p > '9') return false;
        n = n * 10u + (unsigned long)(*p - '0');
        if (n > 1000000u) return false;          /* nichts Sinnloses */
    }
    *aus = n;
    return true;
}

void uft_copy_plan_to_convert_options(const uft_copy_plan_t *plan,
                                      struct uft_convert_options *opts)
{
    if (!plan || !opts) return;
    uft_convert_options_t *o = (uft_convert_options_t *)opts;

    /* MF-1316 — den Plan mitreichen, damit der gemeinsame Verteiler ihn
     * pruefen kann.
     *
     * Hier und nicht in den Aufrufstellen: wer den Plan uebersetzt, hat
     * ihn per Definition in der Hand. Jede der drei Stellen einzeln zu
     * verdrahten hiesse, die vierte zu vergessen — genau so ist die
     * Ausgangslage entstanden.
     *
     * Der Zeiger wird nicht besessen; die Lebensdauer-Zusage steht am
     * Feld in `uft_types.h`. */
    o->copy_plan = plan;

    /* 1. Lesestrategie. Die Werte kommen aus der Tafel des Plans, nicht
     *    von hier — eine zweite Zahlenreihe waere MF-541. */
    if (plan->strategy < UFT_READ_STRATEGY_N) {
        const setzt_t *s = k_strategie[plan->strategy];
        for (; s && s->param; s++) {
            unsigned long n = 0;
            if (strcmp(s->param, "read.retries") == 0) {
                /* „hoch" ist keine Zahl. Dann bleibt die Vorgabe. */
                if (nur_zahl(s->value, &n)) o->decode_retries = (uint32_t)n;
            } else if (strcmp(s->param, "read.revolutions") == 0) {
                if (nur_zahl(s->value, &n)) o->use_multiple_revs = (n > 1u);
            } else if (strcmp(s->param, "consensus.enabled") == 0) {
                /* CONSENSUS nennt keine Umdrehungszahl, verlangt aber
                 * mehrere Durchgaenge — die Flagge sagt es direkt. */
                if (strcmp(s->value, "true") == 0) o->use_multiple_revs = true;
            }
        }
    }

    /* 2. Richtlinie. `NORMAL` heisst ausdruecklich „ohne Nachpruefung";
     *    VERIFY und EVIDENCE verlangen sie. Mehr ist aus dieser Achse
     *    heute nicht zu holen: der Hashsatz (`plan->hashes`) hat in
     *    `uft_convert_options_t` kein Feld. */
    o->verify_after = (plan->policy != UFT_POLICY_NORMAL);

    /* 3. Ebene und Erhaltung erreichen die Wandlung NICHT, und das ist
     *    gemessen statt vergessen: die Felder, auf die sie abbilden
     *    wuerden (`preserve_timing`, `normalize`), haben null Leser, die
     *    danach handeln. Eine Abbildung dorthin waere Schein. `P3-509`
     *    fuehrt die Frage weiter. */
}

uft_copy_plan_t uft_copy_plan_resolve(const uft_copy_plan_t *plan,
                                      uint32_t caps)
{
    uft_copy_plan_t p = plan ? *plan : uft_copy_plan_default();

    if (p.level == UFT_COPY_AUTO) {
        /* Von unten nach oben, und NUR aus den Faehigkeiten. Was nicht
         * zugesagt ist, wird nicht gewaehlt — die Automatik raet nicht. */
        if (caps & (uint32_t)UFT_CAP_FLUX_IO)          p.level = UFT_COPY_FLUX;
        else if (caps & (uint32_t)UFT_CAP_GCR)         p.level = UFT_COPY_NIBBLE;
        else if (caps & (uint32_t)UFT_CAP_BITSTREAM_IO) p.level = UFT_COPY_BITSTREAM;
        else if (caps & (uint32_t)UFT_CAP_FILESYSTEM)  p.level = UFT_COPY_FILE;
        else                                           p.level = UFT_COPY_SECTOR;
    }

    /* MF-1312 — die Bit-Genauigkeit wird auf die Ebene GEDECKELT.
     *
     * Anlass: das Profil "Bittreu" verspricht "Hoechstmoegliche Bit-
     * oder Flussgenauigkeit" und trug gemessen `UFT_EXACT_SECTOR`.
     * Traegt man dort stattdessen fest `UFT_EXACT_FLUX_TIMING` ein,
     * waere es auf jedem Sektorformat unbrauchbar — `check()` meldete
     * `genauigkeit_zu_hoch` und das Tor saegte es ab.
     *
     * Beides ist falsch. Richtig ist, was das Profil selbst sagt:
     * HOECHSTMOEGLICH. Die Aspiration steht in der Tafel, die Ebene
     * begrenzt sie hier — nach oben nie, nach unten nur so weit, wie
     * die aufgeloeste Ebene es hergibt.
     *
     * Die Deckelung folgt derselben Rangfolge, die `check()` fuer
     * `genauigkeit_zu_hoch` benutzt; sie wird nicht neu erfunden,
     * sondern nur vorgezogen. Deshalb kann `resolve()` danach keinen
     * Befund mehr ausloesen, den es selbst erzeugt hat.
     *
     * Nur bei BIT_EXACT: auf allen anderen Erhaltungsstufen ist
     * `exact_kind` laut Kopf "nur bei BIT_EXACT bedeutsam", und ein
     * bedeutungsloses Feld anzufassen waere eine stille Aenderung. */
    if (p.preservation == UFT_PRESERVE_BIT_EXACT) {
        uft_bitexact_kind_t hoechstens;
        switch (p.level) {
        case UFT_COPY_FLUX:      hoechstens = UFT_EXACT_FLUX_TIMING; break;
        case UFT_COPY_BITSTREAM:
        case UFT_COPY_NIBBLE:
        case UFT_COPY_TRACK:     hoechstens = UFT_EXACT_TRACK_BIT;   break;
        default:                 hoechstens = UFT_EXACT_SECTOR;      break;
        }
        if ((int)p.exact_kind > (int)hoechstens) p.exact_kind = hoechstens;
    }

    return p;
}

/* ── Stufe eines Parameters ─────────────────────────────────────────── */
static const param_t *finde(const char *param)
{
    if (!param) return NULL;
    for (size_t i = 0; i < K_PARAM_N; i++)
        if (strcmp(k_param[i].id, param) == 0) return &k_param[i];
    return NULL;
}

uft_copy_stage_t uft_copy_param_stage(const char *param)
{
    const param_t *p = finde(param);
    if (p) return p->stage;

    /* Nicht in der Tafel: ueber den Namensraum. Eine unbekannte
     * Vorsilbe ergibt UFT_STAGE_N — „nicht eingeordnet“. Sie
     * stillschweigend auf READ zu setzen waere ein Rueckfall, der wie
     * eine Aussage aussieht. */
    static const struct { const char *pre; uft_copy_stage_t st; } k[] = {
        { "read.",       UFT_STAGE_READ },
        { "flux.",       UFT_STAGE_READ },
        { "geometry.",   UFT_STAGE_READ },
        { "decode.",     UFT_STAGE_DECODE },
        { "gcr.",        UFT_STAGE_DECODE },
        { "consensus.",  UFT_STAGE_DECODE },
        { "layout.",     UFT_STAGE_LAYOUT },
        { "preserve",    UFT_STAGE_LAYOUT },
        { "write.",      UFT_STAGE_WRITE },
        { "verify.",     UFT_STAGE_VERIFY },
        { "filesystem.", UFT_STAGE_FS },
        { "bam.",        UFT_STAGE_FS },
        { "metadata.",   UFT_STAGE_FS },
        { "evidence.",   UFT_STAGE_EVIDENCE },
    };
    if (!param) return UFT_STAGE_N;
    for (size_t i = 0; i < sizeof(k) / sizeof(k[0]); i++) {
        size_t n = strlen(k[i].pre);
        if (strncmp(param, k[i].pre, n) == 0) return k[i].st;
    }
    return UFT_STAGE_N;
}

bool uft_copy_conflict_applies(const char *a, const char *b)
{
    uft_copy_stage_t sa = uft_copy_param_stage(a);
    uft_copy_stage_t sb = uft_copy_param_stage(b);
    if (sa == UFT_STAGE_N || sb == UFT_STAGE_N) return false;
    return sa == sb;
}

/* ── Erzwungene Werte ───────────────────────────────────────────────── */
static size_t schiebe(uft_copy_enforced_t *out, size_t max, size_t n,
                      const setzt_t *liste, const char *grund)
{
    for (const setzt_t *s = liste; s && s->param; s++) {
        if (out && n < max) {
            out[n].param  = s->param;
            out[n].value  = s->value;
            out[n].stage  = uft_copy_param_stage(s->param);
            out[n].art    = uft_copy_wert_art(s->value);   /* MF-1264 */
            out[n].reason = grund;
        }
        n++;
    }
    return n;
}

size_t uft_copy_plan_enforced(const uft_copy_plan_t *plan,
                              uft_copy_enforced_t *out, size_t max)
{
    if (!plan) return 0;
    size_t n = 0;
    /* Reihenfolge ist Absicht: Strategie, dann Erhaltung, dann
     * Sicherheit. Wer spaeter kommt, gewinnt — die Beweisrichtlinie
     * steht ueber der Bequemlichkeit. */
    if (plan->strategy >= 0 && plan->strategy < UFT_READ_STRATEGY_N)
        n = schiebe(out, max, n, k_strategie[plan->strategy], "Strategie");
    if (plan->preservation >= 0 && plan->preservation < UFT_PRESERVE_N)
        n = schiebe(out, max, n, k_erhaltung[plan->preservation], "Erhaltung");
    if (plan->policy >= 0 && plan->policy < UFT_POLICY_N)
        n = schiebe(out, max, n, k_politik[plan->policy], "Sicherheit");
    return n;
}

/* ── Zustand eines Parameters ───────────────────────────────────────── */
uft_copy_pstate_t uft_copy_param_state(const uft_copy_plan_t *plan,
                                       const char *param, const char **wert)
{
    if (wert) *wert = NULL;
    if (!plan || !param) return UFT_PSTATE_HIDDEN;

    const param_t *p = finde(param);
    rolle_t r = R_;
    /* `je_ebene` hat nur die ECHTEN Ebenen. Bei AUTO waere
     * `je_ebene[UFT_COPY_AUTO]` ein Zugriff HINTER das Feld - genau die
     * Klasse, die MF-519 an `track_data[-1]` gemessen hat, nur nach
     * oben. Wer den Zustand bei AUTO wissen will, loest den Plan
     * vorher auf. */
    if (p && plan->level >= 0 && plan->level < UFT_COPY_LEVEL_ECHT)
        r = p->je_ebene[plan->level];

    /* Ein Verbot der Ebene schlaegt alles. Ein Parameter, den die Ebene
     * ausdruecklich nicht zeigt, darf auch nicht durch die Hintertuer
     * „erzwungen“ erscheinen. */
    if (r == R_X) return UFT_PSTATE_FORBIDDEN;

    uft_copy_enforced_t e[96];
    size_t n = uft_copy_plan_enforced(plan, e, 96);
    if (n > 96) n = 96;
    const char *letzter = NULL;
    for (size_t i = 0; i < n; i++)
        if (e[i].param && strcmp(e[i].param, param) == 0)
            letzter = e[i].value;
    if (letzter) {
        if (wert) *wert = letzter;
        return UFT_PSTATE_FORCED;
    }

    switch (r) {
        case R_A: return UFT_PSTATE_ACTIVE;
        case R_B: return UFT_PSTATE_CONDITIONAL;
        case R_L: return UFT_PSTATE_READONLY;
        default:  return UFT_PSTATE_HIDDEN;
    }
}

uint32_t uft_copy_param_requires(const char *param)
{
    const param_t *p = finde(param);
    return p ? p->verlangt : 0u;
}

uint32_t uft_copy_param_forbids(const char *param)
{
    const param_t *p = finde(param);
    return p ? p->verbietet : 0u;
}

uft_copy_pstate_t uft_copy_param_state_caps(const uft_copy_plan_t *plan,
                                            uint32_t caps,
                                            const char *param,
                                            const char **wert)
{
    if (wert) *wert = NULL;
    const param_t *p = finde(param);
    if (p) {
        /* Was das Format nicht kann, ist keine Einstellung.
         *
         * Ausgegraut waere hier falsch: ein ausgegrauter Regler sagt
         * „spaeter vielleicht“. Ein Regler, der fuer DIESES Format
         * nichts bedeutet, gehoert weg. */
        if (p->verlangt && (caps & p->verlangt) != p->verlangt)
            return UFT_PSTATE_HIDDEN;
        if (p->verbietet && (caps & p->verbietet) != 0u)
            return UFT_PSTATE_HIDDEN;
    }
    return uft_copy_param_state(plan, param, wert);
}

size_t uft_copy_param_count(void) { return K_PARAM_N; }

const char *uft_copy_param_id(size_t i)
{
    return (i < K_PARAM_N) ? k_param[i].id : NULL;
}

/* ── Der Plan als JSON ──────────────────────────────────────────────── */

/* Kleiner Anhaenger, der wie snprintf zaehlt: er gibt immer die Laenge
 * zurueck, die noetig WAERE. Ein abgeschnittener Puffer faellt damit
 * am Rueckgabewert auf und nicht erst beim Lesen. */
static size_t haenge(char *buf, size_t n, size_t pos, const char *txt)
{
    size_t len = strlen(txt);
    if (buf && pos < n) {
        size_t platz = n - pos - 1;
        size_t kopie = (len < platz) ? len : platz;
        memcpy(buf + pos, txt, kopie);
        buf[pos + kopie] = '\0';
    }
    return pos + len;
}

static size_t haenge_zahl(char *buf, size_t n, size_t pos, unsigned v)
{
    char z[24];
    size_t i = 0;
    if (v == 0) { z[i++] = '0'; }
    else { char t[24]; size_t j = 0;
           while (v) { t[j++] = (char)('0' + (v % 10u)); v /= 10u; }
           while (j) z[i++] = t[--j]; }
    z[i] = '\0';
    return haenge(buf, n, pos, z);
}

/* Ein erzwungener Wert als JSON-Literal: Zahlen und true/false ohne
 * Anfuehrungszeichen, alles andere als Zeichenkette. „Pflicht“ und
 * „noetig“ sind KEINE Werte, sondern Forderungen - sie werden als
 * Zeichenkette geschrieben, damit niemand sie fuer eine Zahl haelt. */
static size_t haenge_wert(char *buf, size_t n, size_t pos, const char *v)
{
    if (!v) return haenge(buf, n, pos, "null");
    /* MF-1264: EINE Einteilung, nicht drei Lesarten. Zahl und
     * Wahrheitswert stehen nackt im JSON; eine FORDERUNG kommt in
     * Anfuehrungszeichen — und der Leser erfaehrt ueber
     * `uft_copy_enforced_t::art`, dass es eine ist, statt sie fuer
     * einen Zeichenkettenwert zu halten. */
    if (uft_copy_wert_art(v) != UFT_WERT_FORDERUNG)
        return haenge(buf, n, pos, v);
    pos = haenge(buf, n, pos, "\"");
    pos = haenge(buf, n, pos, v);
    return haenge(buf, n, pos, "\"");
}

/* Aus "read.passes" wird "passes", aus "preserve_crc_errors" wird
 * "crcErrors" — die Gestalt der Vorgabe. */
static void schluessel(const char *param, char *out, size_t n)
{
    const char *punkt = strchr(param, '.');
    const char *rest = punkt ? punkt + 1 : param;
    if (strncmp(rest, "preserve_", 9) == 0) rest += 9;
    size_t j = 0;
    int gross = 0;
    for (const char *c = rest; *c && j + 1 < n; c++) {
        if (*c == '_' || *c == '.') { gross = 1; continue; }
        char ch = *c;
        if (gross && ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
        out[j++] = ch;
        gross = 0;
    }
    out[j] = '\0';
}

/* Welcher JSON-Abschnitt nimmt diesen Parameter auf? */
static const char *abschnitt(const char *param)
{
    if (strncmp(param, "read.", 5) == 0)      return "read";
    if (strncmp(param, "consensus.", 10) == 0) return "consensus";
    if (strncmp(param, "preserve_", 9) == 0)  return "preserve";
    if (strncmp(param, "write.", 6) == 0)     return "write";
    if (strncmp(param, "evidence.", 9) == 0)  return "evidence";
    if (strcmp(param, "hash.enabled") == 0 ||
        strcmp(param, "attest.enabled") == 0 ||
        strcmp(param, "provenance.enabled") == 0 ||
        strcmp(param, "store_original_capture") == 0 ||
        strcmp(param, "verify_source_unchanged") == 0) return "evidence";
    if (strcmp(param, "store_all_reads") == 0 ||
        strcmp(param, "retry_bad_only") == 0 ||
        strcmp(param, "alternate_direction") == 0 ||
        strcmp(param, "status_file") == 0 ||
        strcmp(param, "resume") == 0)         return "read";
    return "plan";
}

size_t uft_copy_plan_to_json(const uft_copy_plan_t *plan, char *buf, size_t n)
{
    if (buf && n) buf[0] = '\0';
    if (!plan) return 0;

    const uft_copy_plan_t p = *plan;
    size_t pos = 0;

    pos = haenge(buf, n, pos, "{\n  \"copyPlan\": {\n    \"level\": \"");
    pos = haenge(buf, n, pos,
                 p.level == UFT_COPY_AUTO ? "auto" :
                 p.level == UFT_COPY_FILE ? "file" :
                 p.level == UFT_COPY_SECTOR ? "sector" :
                 p.level == UFT_COPY_TRACK ? "track" :
                 p.level == UFT_COPY_BITSTREAM ? "bitstream" :
                 p.level == UFT_COPY_NIBBLE ? "nibble" :
                 p.level == UFT_COPY_FLUX ? "flux" : "?");
    pos = haenge(buf, n, pos, "\",\n    \"strategy\": \"");
    pos = haenge(buf, n, pos,
                 p.strategy == UFT_READ_FAST ? "fast" :
                 p.strategy == UFT_READ_STANDARD ? "standard" :
                 p.strategy == UFT_READ_DEEP ? "deep" :
                 p.strategy == UFT_READ_CONSENSUS ? "consensus" :
                 p.strategy == UFT_READ_SALVAGE ? "salvage" : "?");
    pos = haenge(buf, n, pos, "\",\n    \"preservation\": \"");
    pos = haenge(buf, n, pos,
                 p.preservation == UFT_PRESERVE_LOGICAL ? "logical" :
                 p.preservation == UFT_PRESERVE_LAYOUT ? "layout" :
                 p.preservation == UFT_PRESERVE_PROTECTED ? "protected" :
                 p.preservation == UFT_PRESERVE_BIT_EXACT ? "bitExact" : "?");
    pos = haenge(buf, n, pos, "\",\n    \"policy\": \"");
    pos = haenge(buf, n, pos,
                 p.policy == UFT_POLICY_NORMAL ? "normal" :
                 p.policy == UFT_POLICY_VERIFY ? "verify" :
                 p.policy == UFT_POLICY_EVIDENCE ? "evidence" : "?");
    pos = haenge(buf, n, pos, "\"");

    /* Die Feinheiten, aber nur wo sie etwas bedeuten — ein
     * `gcrVariant` auf der Sektorebene waere eine Behauptung. */
    if (p.level == UFT_COPY_TRACK) {
        pos = haenge(buf, n, pos, ",\n    \"trackMode\": \"");
        pos = haenge(buf, n, pos,
                     p.track_mode == UFT_TRACK_RAW ? "raw" : "decoded");
        pos = haenge(buf, n, pos, "\"");
    }
    if (p.level == UFT_COPY_FILE && p.file_special != UFT_FILE_GENERIC) {
        pos = haenge(buf, n, pos, ",\n    \"fileSpecial\": \"");
        pos = haenge(buf, n, pos,
                     p.file_special == UFT_FILE_BAM ? "bam" : "dos");
        pos = haenge(buf, n, pos, "\"");
    }
    if (p.level == UFT_COPY_NIBBLE) {
        pos = haenge(buf, n, pos, ",\n    \"gcrVariant\": \"");
        pos = haenge(buf, n, pos,
                     p.gcr == UFT_GCR_APPLE ? "apple" :
                     p.gcr == UFT_GCR_MACINTOSH ? "macintosh" :
                     p.gcr == UFT_GCR_VICTOR9K ? "victor9k" : "commodore");
        pos = haenge(buf, n, pos, "\"");
    }
    if (p.preservation == UFT_PRESERVE_BIT_EXACT) {
        pos = haenge(buf, n, pos, ",\n    \"exactKind\": \"");
        pos = haenge(buf, n, pos, uft_copy_exact_name(p.exact_kind)
                                      ? uft_copy_exact_name(p.exact_kind) : "?");
        pos = haenge(buf, n, pos, "\"");
    }
    pos = haenge(buf, n, pos, "\n  }");

    /* Die erzwungenen Werte, nach Abschnitten gebuendelt. */
    uft_copy_enforced_t e[96];
    size_t m = uft_copy_plan_enforced(&p, e, 96);
    if (m > 96) m = 96;

    static const char *const gruppen[] =
        { "read", "consensus", "preserve", "write", "evidence", "plan" };
    for (size_t g = 0; g < sizeof(gruppen) / sizeof(gruppen[0]); g++) {
        int offen = 0;
        for (size_t i = 0; i < m; i++) {
            if (!e[i].param) continue;
            if (strcmp(abschnitt(e[i].param), gruppen[g]) != 0) continue;
            /* Der LETZTE Eintrag zu einem Parameter gewinnt — genau wie
             * in `uft_copy_param_state()`. Frueher Gesetztes wird
             * uebersprungen, sonst stuende es doppelt im JSON. */
            int spaeter = 0;
            for (size_t j = i + 1; j < m; j++)
                if (e[j].param && strcmp(e[j].param, e[i].param) == 0)
                    spaeter = 1;
            if (spaeter) continue;

            if (!offen) {
                pos = haenge(buf, n, pos, ",\n  \"");
                pos = haenge(buf, n, pos, gruppen[g]);
                pos = haenge(buf, n, pos, "\": {");
                offen = 1;
            } else {
                pos = haenge(buf, n, pos, ",");
            }
            char k[64];
            schluessel(e[i].param, k, sizeof(k));
            pos = haenge(buf, n, pos, "\n    \"");
            pos = haenge(buf, n, pos, k);
            pos = haenge(buf, n, pos, "\": ");
            pos = haenge_wert(buf, n, pos, e[i].value);
        }
        if (offen) pos = haenge(buf, n, pos, "\n  }");
    }

    /* Der Hashsatz gehoert dazu, wenn die Beweisrichtlinie gilt. */
    if (p.policy == UFT_POLICY_EVIDENCE) {
        pos = haenge(buf, n, pos, ",\n  \"hash\": [");
        int erst = 1;
        if (p.hashes & (uint32_t)UFT_HASH_SHA256) {
            pos = haenge(buf, n, pos, "\"sha256\""); erst = 0; }
        if (p.hashes & (uint32_t)UFT_HASH_SHA512) {
            pos = haenge(buf, n, pos, erst ? "\"sha512\"" : ", \"sha512\""); erst = 0; }
        if (p.hashes & (uint32_t)UFT_HASH_CRC32) {
            pos = haenge(buf, n, pos, erst ? "\"crc32\"" : ", \"crc32\""); }
        pos = haenge(buf, n, pos, "]");
    }

    pos = haenge(buf, n, pos, "\n}\n");
    (void)haenge_zahl;   /* fuer kuenftige Zahlenfelder vorgehalten */
    return pos;
}

/* ── Pruefung ───────────────────────────────────────────────────────── */
/* MF-1332: der volle Erzeuger. `befund()` darunter ist der Kurzweg fuer
 * alles, was keine Messung hinter sich hat.
 *
 * Das `memset` ist nicht Kosmetik: alle acht Aufrufer im Baum legen ihr
 * `uft_copy_finding_t f[16]` UNINITIALISIERT an (gemessen —
 * `uft_copy_plan.c` 3x, `formattab.cpp:2043`, `test_copy_plan.c` 4x).
 * Bis MF-1332 schrieb diese Funktion drei Felder; jedes weitere waere
 * beim Aufrufer Stapelmuell gewesen und haette eine Messung behauptet,
 * die nie stattfand. Genau das soll die Dreiteilung verhindern. */
static size_t befund_voll(uft_copy_finding_t *out, size_t max, size_t n,
                          bool hart, const char *id, const char *text,
                          uint32_t caps_noetig, bool caps_bekannt,
                          int8_t ebene, const char *verlust,
                          const char *quelle,
                          uint8_t konfidenz, bool konfidenz_gemessen)
{
    if (out && n < max) {
        memset(&out[n], 0, sizeof out[n]);
        out[n].hard                   = hart;
        out[n].id                     = id;
        out[n].text                   = text;
        out[n].caps_benoetigt         = caps_noetig;
        out[n].caps_benoetigt_bekannt = caps_bekannt;
        out[n].ebene_empfohlen        = ebene;
        out[n].verlust                = verlust;
        out[n].messquelle             = quelle;
        out[n].konfidenz              = konfidenz;
        out[n].konfidenz_gemessen     = konfidenz_gemessen;
    }
    return n + 1;
}

/* Kurzweg ohne Messung: die fuenf Felder bleiben ausdruecklich
 * UNBEKANNT. Kein `caps_benoetigt_bekannt`, keine Konfidenz, keine
 * Ebene — wer nichts gemessen hat, behauptet hier nichts. */
static size_t befund(uft_copy_finding_t *out, size_t max, size_t n,
                     bool hart, const char *id, const char *text)
{
    return befund_voll(out, max, n, hart, id, text,
                       0u, false, UFT_COPY_EBENE_KEINE,
                       NULL, NULL, 0u, false);
}

/* Ein Befund, der WEISS, welche Faehigkeit fehlt. Genau dafuer sind die
 * neuen Felder da: die Fahne steht im `if` daneben, und bis MF-1332
 * blieb sie dort stehen, statt den Aufrufer zu erreichen.
 *
 * `konfidenz = 100`, weil diese Befunde nicht geschaetzt sind: die
 * Bedingung im `if` IST die Messung. Die Schranke aus
 * `tools/uft-retrace/docs/COPYPLAN_MAPPING.md` — „Ein Raw-Byte-Treffer
 * darf nie `available=true` setzen" — gilt fuer Befunde AUS EINEM
 * TRACE, nicht fuer die hier gerechneten. */
static size_t befund_caps(uft_copy_finding_t *out, size_t max, size_t n,
                          bool hart, const char *id, const char *text,
                          uint32_t caps_noetig, const char *verlust)
{
    return befund_voll(out, max, n, hart, id, text,
                       caps_noetig, true, UFT_COPY_EBENE_KEINE,
                       verlust, "uft_copy_plan_check", 100u, true);
}

size_t uft_copy_plan_check(const uft_copy_plan_t *plan, uint32_t caps,
                           uft_copy_finding_t *out, size_t max)
{
    if (!plan) return 0;
    size_t n = 0;

    if (plan->level < 0 || plan->level >= UFT_COPY_LEVEL_N)
        return befund(out, max, n, true, "ebene_ungueltig",
                      "Die Kopierebene ist kein gueltiger Wert.");

    /* 0. „Automatisch“ ist keine Ebene, sondern eine offene Frage.
     *
     * Sie wird hier NICHT stillschweigend aufgeloest: wer sie stehen
     * laesst, bekommt einen Hinweis und den Namen der Funktion, die sie
     * beantwortet. Eine Automatik, die im Pruefer heimlich entscheidet,
     * waere genau die stille Annahme, gegen die dieser Plan gebaut ist. */
    if (plan->level == UFT_COPY_AUTO)
        return befund(out, max, n, false, "ebene_offen",
                      "Die Kopierebene steht auf „Automatisch“. Sie wird "
                      "aus den Faehigkeiten abgeleitet — rufe dafuer "
                      "uft_copy_plan_resolve(); bis dahin gilt keine "
                      "der ebenenabhaengigen Regeln.");

    /* 1. Die Erhaltung verlangt eine Mindestebene. */
    if (plan->preservation >= 0 && plan->preservation < UFT_PRESERVE_N) {
        uft_copy_level_t min = k_mindestebene[plan->preservation];
        if (plan->level < min)
            n = befund(out, max, n, true, "ebene_zu_hoch",
                       "Diese Erhaltungsrichtlinie braucht mindestens die "
                       "Spur- oder Bitstromebene. Auf einer hoeheren Ebene "
                       "wird das Verlangte gar nicht erst gelesen.");
    }

    /* 2. PROTECTED verlangt Timing UND schwache Bits — beides.
     *
     * Gemessen (MF-1231): von 88 Plugins sagen vier TIMING zu (G64, HFE,
     * SCP, STX) und zwei WEAK_BITS (ATX, PRO). Die Schnittmenge ist
     * LEER. Der Befund ist also heute fuer jedes Format wahr, und genau
     * deshalb steht er hier statt in einer Fussnote. */
    if (plan->preservation == UFT_PRESERVE_PROTECTED) {
        const uint32_t noetig = (uint32_t)UFT_CAP_TIMING |
                                (uint32_t)UFT_CAP_WEAK_BITS;
        if ((caps & noetig) != noetig)
            n = befund(out, max, n, true, "schutz_nicht_tragbar",
                       "Kopierschutz-Erhaltung verlangt, dass Quelle UND "
                       "Ziel Timing und schwache Bits tragen. Wenn das "
                       "Zielformat eine dieser Eigenschaften nicht "
                       "speichern kann, ist der Auftrag zu sperren oder "
                       "ausdruecklich als verlustbehaftet freizugeben.");
    }

    /* 3. Bitgenau ist nicht flussgleich. */
    if (plan->preservation == UFT_PRESERVE_BIT_EXACT) {
        if (plan->exact_kind == UFT_EXACT_FLUX_TIMING &&
            plan->level != UFT_COPY_FLUX)
            n = befund(out, max, n, true, "genauigkeit_zu_hoch",
                       "Fluss-Zeitgenauigkeit ist nur auf der Flussebene "
                       "erreichbar. Auf der Bitstromebene bleibt der "
                       "Bitstrom erhalten, die Flusszeiten nicht.");
        if (plan->exact_kind == UFT_EXACT_TRACK_BIT &&
            plan->level < UFT_COPY_BITSTREAM)
            n = befund(out, max, n, true, "genauigkeit_zu_hoch",
                       "Bitgenauigkeit je Spur verlangt mindestens die "
                       "Bitstromebene.");
    }

    /* 4. Die Flussebene braucht Fluss auf beiden Seiten. */
    if (plan->level == UFT_COPY_FLUX &&
        !(caps & (uint32_t)UFT_CAP_FLUX_IO))
        n = befund_caps(out, max, n, true, "kein_fluss",
                   "Die Flussebene verlangt eine Flussquelle UND ein "
                   "Flussziel.",
                   (uint32_t)UFT_CAP_FLUX_IO,
                   "Ohne Fluss gehen Zellzeiten, Weak Bits und "
                   "Mehrfachumdrehungen verloren.");

    /* 5. Uebereinstimmung braucht mehrere Umdrehungen.
     *
     * Gemessen (MF-1231): CAP_MULTI_REV sagt genau EIN Plugin zu — SCP. */
    if (plan->strategy == UFT_READ_CONSENSUS &&
        !(caps & (uint32_t)UFT_CAP_MULTI_REV))
        n = befund_caps(out, max, n, true, "keine_mehrfachlesung",
                   "Uebereinstimmung braucht mehrere Lesungen oder "
                   "Umdrehungen. Ohne sie gibt es nichts abzugleichen.",
                   (uint32_t)UFT_CAP_MULTI_REV,
                   "Ohne Mehrfachlesung faellt die Abstimmung weg; ein "
                   "einmal falsch gelesener Sektor bleibt unbemerkt.");

    /* 6. Beweis und Eile vertragen sich nicht. */
    if (plan->policy == UFT_POLICY_EVIDENCE &&
        plan->strategy == UFT_READ_FAST)
        n = befund(out, max, n, true, "beweis_und_eile",
                   "Ein forensisches Abbild darf nicht auf Wiederholungen "
                   "verzichten: Schnell setzt retries=0 und eine einzige "
                   "Umdrehung.");

    /* 7. Die Dateiebene braucht ein gelesenes Dateisystem. */
    if (plan->level == UFT_COPY_FILE &&
        !(caps & (uint32_t)UFT_CAP_FILESYSTEM))
        n = befund_caps(out, max, n, true, "kein_dateisystem",
                   "Auf der Dateiebene muss das Quell-Dateisystem erkannt "
                   "sein. Ohne Verzeichnis gibt es keine Dateien.",
                   (uint32_t)UFT_CAP_FILESYSTEM,
                   "Ohne Dateisystem gibt es keine Dateiauswahl; es "
                   "bliebe nur ein Abzug der ganzen Diskette.");

    /* 8. Nibble ohne GCR ist sinnlos — weich, weil die Flagge heute
     *    nicht je Format gefuehrt wird. */
    if (plan->level == UFT_COPY_NIBBLE && !(caps & (uint32_t)UFT_CAP_GCR))
        n = befund_caps(out, max, n, false, "gcr_nicht_zugesagt",
                   "Die Nibble-Ebene ist fuer GCR gedacht. Ein allgemeiner "
                   "Schalter GCR reicht nicht: Commodore, Apple, Macintosh "
                   "und Victor 9000 sind verschiedene Verfahren.",
                   (uint32_t)UFT_CAP_GCR,
                   "Weich: die Ebene laeuft, aber ohne Zusage, dass die "
                   "Kodierung wirklich GCR ist.");

    /* 9. BAMCopy nur, wenn es wirklich eine Commodore-BAM gibt.
     *
     * Die Vorgabe sagt es woertlich: „BAMCopy darf nur angeboten werden,
     * wenn Quellformat eine Commodore-BAM besitzt UND das
     * Quell-Dateisystem erkannt wurde. Bei ADF, FAT, Atari DOS oder
     * Apple DOS gehoert der Modus nicht in die GUI." Beide Bedingungen,
     * nicht eine. */
    if (plan->level == UFT_COPY_FILE &&
        plan->file_special == UFT_FILE_BAM) {
        const uint32_t noetig = (uint32_t)UFT_CAP_CBM_BAM |
                                (uint32_t)UFT_CAP_FILESYSTEM;
        if ((caps & noetig) != noetig)
            n = befund(out, max, n, true, "bam_ohne_bam",
                       "Die BAM-Spezialisierung verlangt eine "
                       "Commodore-BAM UND ein erkanntes Dateisystem. Bei "
                       "ADF, FAT, Atari DOS oder Apple DOS gehoert sie "
                       "gar nicht erst ins Auswahlfeld.");
    }

    /* 10. Die Spurart gilt nur auf der Spurebene. */
    if (plan->track_mode == UFT_TRACK_RAW && plan->level != UFT_COPY_TRACK)
        n = befund(out, max, n, false, "spurart_ohne_spurebene",
                   "„TrackCopy raw“ ist eine Spielart der Spurebene. Auf "
                   "einer anderen Ebene bedeutet sie nichts.");

    /* 11. Der Hashsatz unter der Beweisrichtlinie.
     *
     * SHA-256 ist Pflicht, SHA-512 optional — und CRC32 allein ist kein
     * Beweis, sondern ein technischer Vergleichswert. Ein Beweisabbild,
     * das nur eine Pruefsumme traegt, die sich in Sekunden faelschen
     * laesst, waere eine Zusage ohne Tat. */
    if (plan->policy == UFT_POLICY_EVIDENCE) {
        if (!(plan->hashes & (uint32_t)UFT_HASH_SHA256))
            n = befund(out, max, n, true, "hash_ohne_sha256",
                       "Ein forensisches Abbild verlangt SHA-256. SHA-512 "
                       "ist optional, CRC32 nur zusaetzlich fuer "
                       "technische Vergleiche — nie allein.");
        if (plan->hashes == (uint32_t)UFT_HASH_CRC32)
            n = befund(out, max, n, true, "hash_nur_crc32",
                       "CRC32 allein traegt keinen Beweis.");
    }

    /* 12. Die Abstimmung gilt nur, wenn abgestimmt wird. */
    if (plan->strategy != UFT_READ_CONSENSUS &&
        plan->vote != UFT_VOTE_STRICT_MAJORITY)
        n = befund(out, max, n, false, "abstimmung_ohne_consensus",
                   "Ein Abstimmungsverfahren ohne die Strategie "
                   "„Consensus“ hat nichts abzustimmen.");

    return n;
}

/* MF-1309 - das Tor. Siehe Kopf fuer die Begruendung der Zwei-Werte-Form
 * und dafuer, warum die Faehigkeitsfrage hier ausgeklammert bleibt.
 *
 * Anlass ist gemessen: `uft_copy_plan_is_executable()` war fertig gebaut
 * und hatte im ganzen Baum NULL Aufrufer, waehrend drei Ausfuehrungspfade
 * (`decodejob.cpp`, `toolstab.cpp`, `uft_save_image.cpp`) ungeprueft
 * `uft_copy_plan_to_convert_options()` riefen. Ein Tor, das niemand
 * oeffnet - dieselbe Klasse wie `plugin->flush` (MF-930).
 *
 * Was hier NICHT behauptet wird: dass der Wandlungspfad vorher ungesichert
 * war. `uft_preflight_check()` steht seit laengerem in
 * `uft_format_convert_dispatch.c:512` und sperrt UNTESTED und IMPOSSIBLE.
 * Ungeprueft blieben die Befunde des PLANS, nicht das Formatpaar. */
uft_copy_verdict_t uft_copy_plan_gate(const uft_copy_plan_t *plan,
                                      const char **grund)
{
    if (grund) *grund = NULL;
    if (!plan) {
        if (grund) *grund = "kein_plan";
        return UFT_COPY_DENY;
    }

    uft_copy_finding_t f[16];
    size_t n = uft_copy_plan_check(plan, 0xFFFFFFFFu, f, 16);
    if (n > 16) n = 16;
    for (size_t i = 0; i < n; i++)
        if (f[i].hard) {
            if (grund) *grund = f[i].id;
            return UFT_COPY_DENY;
        }
    return UFT_COPY_ALLOW;
}

/* MF-1311 — das Tor mit Faehigkeitsfrage. Siehe Kopf fuer die Bedeutung
 * der drei Ausgaenge.
 *
 * Dreistufig, weil die drei Fragen verschieden sicher zu beantworten
 * sind:
 *
 *   1. Widerspricht sich der Plan in sich? Das gilt immer und braucht
 *      keine Maske — dafuer ist `uft_copy_plan_gate()` da.
 *   2. Ist die Maske NICHT gemessen? Dann wird mit LEERER Maske
 *      gefragt. Meldet check() dabei einen harten Befund, haengt er an
 *      einer Faehigkeit — und genau dann ist die ehrliche Antwort
 *      "erst messen", nicht "nein".
 *   3. Ist sie gemessen, urteilt das Tor vollstaendig. */
uft_copy_verdict_t uft_copy_plan_gate_caps(const uft_copy_plan_t *plan,
                                           const char **grund)
{
    const uft_copy_verdict_t frei = uft_copy_plan_gate(plan, grund);
    if (frei != UFT_COPY_ALLOW) return frei;

    uft_copy_finding_t f[16];
    const uint32_t maske = plan->caps_bekannt ? plan->caps : 0u;

    size_t n = uft_copy_plan_check(plan, maske, f, 16);
    if (n > 16) n = 16;
    for (size_t i = 0; i < n; i++)
        if (f[i].hard) {
            if (grund) *grund = f[i].id;
            return plan->caps_bekannt ? UFT_COPY_DENY
                                      : UFT_COPY_NEEDS_MEASUREMENT;
        }
    return UFT_COPY_ALLOW;
}

bool uft_copy_plan_is_executable(const uft_copy_plan_t *plan, uint32_t caps)
{
    uft_copy_finding_t f[16];
    size_t n = uft_copy_plan_check(plan, caps, f, 16);
    if (n > 16) n = 16;
    for (size_t i = 0; i < n; i++)
        if (f[i].hard) return false;
    return true;
}
