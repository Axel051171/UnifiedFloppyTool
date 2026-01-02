#include "uft/uft_error.h"
#include "cpc_meta.h"

#include <string.h>
#include <errno.h>

/* Minimal JSON escaping for strings we control. */
static void json_put_escaped(FILE *out, const char *s)
{
    fputc('"', out);
    for (; *s; ++s) {
        const unsigned char c = (unsigned char)*s;
        switch (c) {
            case '\\': fputs("\\\\", out); break;
            case '"':  fputs("\\\"", out); break;
            case '\n': fputs("\\n", out); break;
            case '\r': fputs("\\r", out); break;
            case '\t': fputs("\\t", out); break;
            default:
                if (c < 0x20) fprintf(out, "\\u%04x", (unsigned)c);
                else fputc((int)c, out);
                break;
        }
    }
    fputc('"', out);
}

static void json_bool(FILE *out, int v) { fputs(v ? "true" : "false", out); }

int cpc_write_sector_map_json(
    FILE *out,
    uint16_t cyl, uint16_t head,
    const ufm_logical_image_t *li,
    const cpc_flux_decode_result_t *decode_stats)
{
    if (!out || !li) return -EINVAL;

    fputs("{\n", out);

    /* Track identity */
    fprintf(out, "  \"cyl\": %u,\n", (unsigned)cyl);
    fprintf(out, "  \"head\": %u,\n", (unsigned)head);

    /* Optional decode stats */
    if (decode_stats) {
        fputs("  \"decode\": {\n", out);
        fprintf(out, "    \"slices_tried\": %u,\n", (unsigned)decode_stats->slices_tried);
        fprintf(out, "    \"slices_used\": %u,\n", (unsigned)decode_stats->slices_used);
        fprintf(out, "    \"pll\": { \"total_ns\": %llu, \"min_dt_ns\": %u, \"max_dt_ns\": %u, \"resync_events\": %u, \"cells_emitted\": %u },\n",
            (unsigned long long)decode_stats->pll_stats.total_ns,
            (unsigned)decode_stats->pll_stats.min_dt_ns,
            (unsigned)decode_stats->pll_stats.max_dt_ns,
            (unsigned)decode_stats->pll_stats.resync_events,
            (unsigned)decode_stats->pll_stats.cells_emitted);
        fprintf(out, "    \"consensus\": { \"revs_in\": %u, \"revs_used\": %u, \"anchor_hits\": %u, \"weak_bits\": %u, \"unanimous_bits\": %u, \"total_bits\": %u },\n",
            (unsigned)decode_stats->consensus_stats.revs_in,
            (unsigned)decode_stats->consensus_stats.revs_used,
            (unsigned)decode_stats->consensus_stats.anchor_hits,
            (unsigned)decode_stats->consensus_stats.weak_bits,
            (unsigned)decode_stats->consensus_stats.unanimous_bits,
            (unsigned)decode_stats->consensus_stats.total_bits);
        fprintf(out, "    \"mfm\": { \"idams\": %u, \"dams\": %u, \"sectors\": %u, \"bad_crc_idam\": %u, \"bad_crc_dam\": %u }\n",
            (unsigned)decode_stats->mfm_stats.idams,
            (unsigned)decode_stats->mfm_stats.dams,
            (unsigned)decode_stats->mfm_stats.sectors_emitted,
            (unsigned)decode_stats->mfm_stats.bad_crc_idam,
            (unsigned)decode_stats->mfm_stats.bad_crc_dam);
        fputs("  },\n", out);
    }

    /* Sector list */
    fputs("  \"sectors\": [\n", out);

    int first = 1;
    for (uint32_t i = 0; i < li->count; i++) {
        const ufm_sector_t *s = &li->sectors[i];

        if (s->cyl != cyl || s->head != head) continue;

        if (!first) fputs(",\n", out);
        first = 0;

        fputs("    { ", out);
        fprintf(out, "\"sec\": %u, \"size\": %u, \"confidence\": %u, ", (unsigned)s->sec, (unsigned)s->size, (unsigned)s->confidence);

        fputs("\"flags\": { ", out);
        fputs("\"bad_crc\": ", out); json_bool(out, (s->flags & UFM_SEC_BAD_CRC) != 0); fputs(", ", out);
        fputs("\"deleted\": ", out); json_bool(out, (s->flags & UFM_SEC_DELETED_DAM) != 0); fputs(", ", out);
        fputs("\"weak\": ", out);    json_bool(out, (s->flags & UFM_SEC_WEAK) != 0);
        fputs(" }", out);

        if (s->meta_type == UFM_SECTOR_META_MFM_IBM && s->meta) {
            const ufm_sector_meta_mfm_ibm_t *m = (const ufm_sector_meta_mfm_ibm_t*)s->meta;
            fputs(", \"mfm\": { ", out);
            fprintf(out, "\"id\": { \"C\": %u, \"H\": %u, \"R\": %u, \"N\": %u }, ",
                (unsigned)m->id_c, (unsigned)m->id_h, (unsigned)m->id_r, (unsigned)m->id_n);
            fprintf(out, "\"dam_mark\": %u, ", (unsigned)m->dam_mark);
            fprintf(out, "\"idam_crc\": { \"read\": %u, \"calc\": %u, \"ok\": %s }, ",
                (unsigned)m->idam_crc_read, (unsigned)m->idam_crc_calc,
                (m->idam_crc_read == m->idam_crc_calc) ? "true" : "false");
            fprintf(out, "\"dam_crc\": { \"read\": %u, \"calc\": %u, \"ok\": %s }, ",
                (unsigned)m->dam_crc_read, (unsigned)m->dam_crc_calc,
                (m->dam_crc_read == m->dam_crc_calc) ? "true" : "false");
            fprintf(out, "\"bitpos\": { \"idam\": %u, \"dam\": %u }",
                (unsigned)m->idam_bitpos, (unsigned)m->dam_bitpos);
            fputs(" }", out);
        }

        fputs(" }", out);
    }

    fputs("\n  ]\n", out);
    fputs("}\n", out);
    return 0;
}
