/*
 * uft_scp_tool.c — CLI helper for SCP images (C99)
 *
 * This is designed to be embedded into a bigger project (your UFT backend)
 * but also usable standalone for debugging + GUI-sidecar generation.
 */
#include "uft/formats/uft_scp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
  #include <io.h>
  #define access _access
  #define F_OK 0
#else
  #include <unistd.h>
#endif

static void usage(const char *argv0) {
    fprintf(stderr,
        "Usage:\n"
        "  %s --in <file.scp> [--summary]\n"
        "  %s --in <file.scp> --track <idx> --rev <r> --dump <out.csv>\n"
        "  %s --in <file.scp> --catalog <out.json>\n"
        "\n"
        "Flags:\n"
        "  --in <path>            Input SCP file\n"
        "  --summary              Print quick header + track presence summary\n"
        "  --catalog <json>       Write GUI-friendly JSON catalog (tracks + revs)\n"
        "  --track <0..167>       Track entry index in SCP offsets table\n"
        "  --rev <0..N-1>         Revolution index\n"
        "  --dump <csv>           Dump transitions as CSV: idx,time\n"
        "  --max-transitions <n>  Cap transitions (default: 200000)\n"
        "  --strict-marks         Placeholder (for compatibility with larger tool)\n",
        argv0, argv0, argv0);
}

static int streq(const char *a, const char *b) { return a && b && strcmp(a,b) == 0; }

static int write_csv(const char *path, const uint32_t *t, size_t n) {
    FILE *fo = fopen(path, "wb");
    if (!fo) return 0;
    fprintf(fo, "index,time\n");
    for (size_t i = 0; i < n; i++) fprintf(fo, "%zu,%u\n", i, (unsigned)t[i]);
    fclose(fo);
    return 1;
}

static void print_summary(uft_scp_image_t *img) {
    printf("SCP v%u diskType=%u numRevs=%u startTrack=%u endTrack=%u sides=%u flags=0x%02X bitcellEnc=%u extended=%u\n",
           img->hdr.version, img->hdr.disk_type, img->hdr.num_revs, img->hdr.start_track, img->hdr.end_track,
           img->hdr.sides, img->hdr.flags, img->hdr.bitcell_encoding, img->extended_mode);

    unsigned present = 0;
    for (unsigned i = 0; i < UFT_SCP_MAX_TRACK_ENTRIES; i++) if (img->track_offsets[i]) present++;
    printf("Track entries present: %u / %u\n", present, (unsigned)UFT_SCP_MAX_TRACK_ENTRIES);
}

static int write_catalog_json(const char *path, uft_scp_image_t *img) {
    FILE *fo = fopen(path, "wb");
    if (!fo) return 0;

    fprintf(fo, "{\n");
    fprintf(fo, "  \"header\": {\n");
    fprintf(fo, "    \"version\": %u,\n", img->hdr.version);
    fprintf(fo, "    \"disk_type\": %u,\n", img->hdr.disk_type);
    fprintf(fo, "    \"num_revs\": %u,\n", img->hdr.num_revs);
    fprintf(fo, "    \"start_track\": %u,\n", img->hdr.start_track);
    fprintf(fo, "    \"end_track\": %u,\n", img->hdr.end_track);
    fprintf(fo, "    \"sides\": %u,\n", img->hdr.sides);
    fprintf(fo, "    \"flags\": %u,\n", img->hdr.flags);
    fprintf(fo, "    \"bitcell_encoding\": %u,\n", img->hdr.bitcell_encoding);
    fprintf(fo, "    \"extended_mode\": %u\n", img->extended_mode);
    fprintf(fo, "  },\n");
    fprintf(fo, "  \"tracks\": [\n");

    int first = 1;
    for (unsigned i = 0; i < UFT_SCP_MAX_TRACK_ENTRIES; i++) {
        uft_scp_track_info_t ti;
        if (uft_scp_get_track_info(img, (uint8_t)i, &ti) != UFT_SCP_OK) continue;
        if (!ti.present) continue;

        uft_scp_track_rev_t revs[32];
        uft_scp_track_header_t th;
        if (img->hdr.num_revs > 32) continue;
        int rc = uft_scp_read_track_revs(img, (uint8_t)i, revs, 32, &th);
        if (rc != UFT_SCP_OK) continue;

        if (!first) fprintf(fo, ",\n");
        first = 0;

        fprintf(fo, "    {\n");
        fprintf(fo, "      \"track_index\": %u,\n", i);
        fprintf(fo, "      \"file_offset\": %u,\n", ti.file_offset);
        fprintf(fo, "      \"track_number\": %u,\n", ti.track_number);
        fprintf(fo, "      \"revs\": [\n");

        for (unsigned r = 0; r < img->hdr.num_revs; r++) {
            fprintf(fo, "        {\"rev\": %u, \"time_duration\": %u, \"data_length\": %u, \"data_offset\": %u}%s\n",
                    r, revs[r].time_duration, revs[r].data_length, revs[r].data_offset,
                    (r + 1 == img->hdr.num_revs) ? "" : ",");
        }
        fprintf(fo, "      ]\n");
        fprintf(fo, "    }");
    }

    fprintf(fo, "\n  ]\n}\n");
    fclose(fo);
    return 1;
}

int main(int argc, char **argv) {
    const char *in_path = NULL;
    const char *dump_path = NULL;
    const char *catalog_path = NULL;
    int do_summary = 0;
    int have_track = 0, have_rev = 0;
    unsigned track_idx = 0, rev_idx = 0;
    size_t max_transitions = 200000;

    for (int i = 1; i < argc; i++) {
        if (streq(argv[i], "--in") && i + 1 < argc) in_path = argv[++i];
        else if (streq(argv[i], "--dump") && i + 1 < argc) dump_path = argv[++i];
        else if (streq(argv[i], "--catalog") && i + 1 < argc) catalog_path = argv[++i];
        else if (streq(argv[i], "--summary")) do_summary = 1;
        else if (streq(argv[i], "--track") && i + 1 < argc) { have_track = 1; track_idx = (unsigned)strtoul(argv[++i], NULL, 10); }
        else if (streq(argv[i], "--rev") && i + 1 < argc) { have_rev = 1; rev_idx = (unsigned)strtoul(argv[++i], NULL, 10); }
        else if (streq(argv[i], "--max-transitions") && i + 1 < argc) max_transitions = (size_t)strtoull(argv[++i], NULL, 10);
        else if (streq(argv[i], "--strict-marks")) { /* placeholder */ }
        else { usage(argv[0]); return 2; }
    }

    if (!in_path) { usage(argv[0]); return 2; }
    if (access(in_path, F_OK) != 0) { fprintf(stderr, "Input not found: %s\n", in_path); return 2; }

    uft_scp_image_t img;
    int rc = uft_scp_open(&img, in_path);
    if (rc != UFT_SCP_OK) {
        fprintf(stderr, "SCP open failed (%d). Not an SCP or read error.\n", rc);
        return 1;
    }

    if (do_summary) print_summary(&img);

    if (catalog_path) {
        if (!write_catalog_json(catalog_path, &img)) {
            fprintf(stderr, "Failed to write catalog: %s\n", catalog_path);
            uft_scp_close(&img);
            return 1;
        }
        printf("Wrote catalog: %s\n", catalog_path);
    }

    if (dump_path) {
        if (!have_track || !have_rev) {
            fprintf(stderr, "--dump requires --track and --rev\n");
            uft_scp_close(&img);
            return 2;
        }
        if (track_idx >= UFT_SCP_MAX_TRACK_ENTRIES) {
            fprintf(stderr, "track out of range (0..167)\n");
            uft_scp_close(&img);
            return 2;
        }
        if (rev_idx >= img.hdr.num_revs) {
            fprintf(stderr, "rev out of range (0..%u)\n", img.hdr.num_revs ? img.hdr.num_revs-1 : 0);
            uft_scp_close(&img);
            return 2;
        }

        uint32_t *trans = (uint32_t*)calloc(max_transitions, sizeof(uint32_t));
        if (!trans) { fprintf(stderr, "OOM\n"); uft_scp_close(&img); return 1; }

        size_t n = 0;
        uint32_t total = 0;
        rc = uft_scp_read_rev_transitions(&img, (uint8_t)track_idx, (uint8_t)rev_idx,
                                         trans, max_transitions, &n, &total);
        if (rc != UFT_SCP_OK && rc != UFT_SCP_EBOUNDS) {
            fprintf(stderr, "Read transitions failed (%d)\n", rc);
            free(trans);
            uft_scp_close(&img);
            return 1;
        }
        if (!write_csv(dump_path, trans, n)) {
            fprintf(stderr, "Failed to write CSV: %s\n", dump_path);
            free(trans);
            uft_scp_close(&img);
            return 1;
        }
        printf("Wrote %zu transitions (total_time=%u ticks) to %s%s\n",
               n, (unsigned)total, dump_path, (rc == UFT_SCP_EBOUNDS) ? " (TRUNCATED)" : "");
        free(trans);
    }

    uft_scp_close(&img);
    return 0;
}
