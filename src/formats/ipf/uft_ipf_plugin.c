/**
 * @file uft_ipf_plugin.c
 * @brief IPF (Interchangeable Preservation Format) Plugin-B
 *
 * IPF (CAPS/SPS) stores flux-level data with copy protection info.
 * This plugin bridges the AIR-enhanced parser in uft_ipf_air.c (which
 * parses block descriptors, gap elements, and SPS data elements) to the
 * uft_disk_t / uft_track_t plugin API.
 *
 * Coverage matrix:
 *   - SPS-encoded IPFs (encoder_type=2): full geometry, IMGE metadata
 *     and concatenated data-element payload bytes are surfaced via
 *     read_track().
 *   - CAPS-encoded IPFs (encoder_type=1): geometry + IMGE metadata
 *     only — data-element layout for CAPS encoder is not yet decoded
 *     in uft_ipf_air.c. read_track() returns metadata + raw_data=NULL.
 *
 * Honest forensic stance: no fabricated bitstream content. Where the
 * payload cannot yet be reconstructed (CAPS path, or full track
 * bitstream including gap-padding for SPS), the track structure
 * reflects descriptor truth and leaves the absent fields NULL.
 */
#include "uft/uft_format_common.h"
#include "uft/profiles/uft_ipf_format.h"
#include "uft/formats/ipf/uft_ipf_air.h"
#include "uft/formats/ipf/uft_ipf_helper.h"
#include "uft/uft_log.h"

extern bool uft_caps_is_ipf(const uint8_t *data, size_t size);

/* IPF IMGE.track_flags — fuzzy-bit indicator (matches IPF_TF_FUZZY in
 * uft_ipf_air.c). Local copy because the constant is TU-private there. */
#define IPF_PLUGIN_TF_FUZZY  0x01u

/* Platform IDs — only AMIGA is needed for encoding selection. Matches
 * caps_platform_t::CAPS_PLATFORM_AMIGA in include/uft/formats/ipf/uft_ipf_caps.h. */
#define IPF_PLUGIN_PLATFORM_AMIGA  1u

static bool ipf_plugin_probe(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    (void)file_size;
    if (size < 12) return false;
    /* IPF magic: "CAPS" at offset 0 */
    if (data[0] == 'C' && data[1] == 'A' && data[2] == 'P' && data[3] == 'S') {
        *confidence = 95;
        return true;
    }
    if (uft_caps_is_ipf(data, size)) {
        *confidence = 90;
        return true;
    }
    return false;
}

typedef struct {
    uint8_t        *data;     /* original file bytes (kept for future re-parse) */
    size_t          size;
    ipf_air_disk_t *air;      /* parsed handle from ipf_air_parse() */
    uint32_t        primary_platform;

    /* MF-917 — Weg 3 (QUARANTINE_PROCESS §5): wenn ein Helfer
     * eingerichtet ist, deutet ER die Datei, und `air` bleibt
     * unbenutzt. `via_helper` sagt, welcher der beiden Wege diese
     * Sitzung getragen hat — das ist keine Kosmetik: die beiden Wege
     * haben verschiedene Rechtslagen, und ein Benutzer, der ein
     * forensisches Ergebnis weitergibt, muss sagen koennen, woher es
     * kommt. */
    bool                   via_helper;
    uft_ipf_helper_reply_t reply;
    char                   idx_path[512];
    char                   blob_path[512];
} ipf_data_t;

/* Verzeichnis fuer die zwei Zwischendateien. Die IPF-Datei selbst wird
 * NIE angefasst — sie kann auf einem schreibgeschuetzten Traeger
 * liegen, und der Datenfluss-Schnitt aus §5 verlangt ohnehin, dass die
 * Quelle bei UFT bleibt. */
static const char *ipf_tmpdir(void)
{
    const char *v;
    if ((v = getenv("TMPDIR")) && *v) return v;
    if ((v = getenv("TEMP"))   && *v) return v;
    if ((v = getenv("TMP"))    && *v) return v;
#ifdef _WIN32
    return ".";
#else
    return "/tmp";
#endif
}

/* Versucht den Helfer. UFT_OK = er hat geantwortet und p->reply steht.
 * Jeder andere Wert heisst: dieser Weg traegt nicht — der Grund steht
 * in @p err und wird vom Aufrufer BENANNT weitergegeben, nie
 * verschluckt. */
static uft_error_t ipf_try_helper(ipf_data_t *p, const char *path,
                                  char *err, size_t errcap)
{
    static unsigned long lauf = 0;
    const char *td = ipf_tmpdir();
    snprintf(p->idx_path,  sizeof p->idx_path,  "%s/uft_ipf_%lu.idx",
             td, ++lauf);
    snprintf(p->blob_path, sizeof p->blob_path, "%s/uft_ipf_%lu.bin",
             td, lauf);
    return uft_ipf_helper_query(uft_ipf_helper_system_runner(), path,
                                p->idx_path, p->blob_path,
                                &p->reply, err, errcap);
}

static uft_error_t ipf_plugin_open(uft_disk_t *disk, const char *path, bool ro) {
    (void)ro;
    size_t sz = 0;
    uint8_t *data = uft_read_file(path, &sz);
    if (!data) return UFT_ERR_FILE_OPEN;

    ipf_data_t *p = calloc(1, sizeof(ipf_data_t));
    if (!p) { free(data); return UFT_ERR_MEMORY; }
    p->data = data; p->size = sz;

    /* ── MF-917: zuerst die Prozessgrenze ───────────────────────────
     *
     * Ist ein Helfer eingerichtet, deutet er — das ist der legale Weg
     * (QUARANTINE_PROCESS §5, Weg 3; Lizenzlage in
     * `uft_ipf_helper.h`). Ist keiner eingerichtet, uebernimmt der
     * vorhandene Leser, der unter LIZ-2 in Quarantaene vorgemerkt ist.
     *
     * Die Reihenfolge ist Absicht und steht in MF-699: "erst der
     * Ersatz, dann die Loeschung". Solange die Eigentuemer-
     * Entscheidung ueber `uft_ipf_air.c` offen ist, bleibt er der
     * Rueckfall — aber wer einen Helfer einrichtet, bekommt ihn nie
     * mehr zu sehen. */
    char herr[UFT_IPF_HELPER_ERRLEN] = {0};
    if (uft_ipf_helper_path() != NULL) {
        uft_error_t he = ipf_try_helper(p, path, herr, sizeof herr);
        if (he == UFT_OK) {
            p->via_helper       = true;
            p->primary_platform = p->reply.platform;
            disk->plugin_data   = p;
            disk->geometry.cylinders = p->reply.cylinders;
            disk->geometry.heads     = p->reply.heads;
            disk->geometry.sectors     = 11;
            disk->geometry.sector_size = 512;
            disk->geometry.total_sectors =
                (uint32_t)disk->geometry.cylinders *
                (uint32_t)disk->geometry.heads *
                (uint32_t)disk->geometry.sectors;
            UFT_INFO("IPF ueber Helfer gelesen: %d Spuren, %d/%d",
                     (int)p->reply.track_count,
                     disk->geometry.cylinders, disk->geometry.heads);
            return UFT_OK;
        }
        /* Ein eingerichteter Helfer, der nicht traegt, ist ein
         * Ereignis — nicht etwas, das man still uebergeht. */
        UFT_WARN("IPF-Helfer nicht verwendbar: %s", herr);
    }

    p->air = ipf_air_alloc();
    if (!p->air) {
        free(data); free(p);
        return UFT_ERR_MEMORY;
    }

    ipf_air_status_t st = ipf_air_parse(data, sz, p->air);
    if (st != IPF_AIR_OK) {
        /* MF-917: bis hierher stand hier ein stummes
         * UFT_ERR_FORMAT_INVALID. Der Benutzer sah "Format ungueltig"
         * und hatte keinen Anhaltspunkt, dass es einen zweiten,
         * besseren Weg gibt. Jetzt nennt die Meldung ihn — und genau
         * dieser Satz ist es, der nach einer Entscheidung zu LIZ-2 die
         * EINZIGE Antwort waere. */
        UFT_WARN("IPF '%s' nicht lesbar (Parser-Status %d). %s",
                 path, (int)st,
                 uft_ipf_helper_path()
                   ? "Der eingerichtete Helfer trug ebenfalls nicht."
                   : "IPF erkannt — Inhalt nicht lesbar. Helfer einrichten: "
                     UFT_IPF_HELPER_ENV "=<pfad>");
        ipf_air_free(p->air);
        free(p->air); free(data); free(p);
        return UFT_ERR_FORMAT_INVALID;
    }

    int cyls = 0, sides = 0;
    uint32_t platform = 0;
    if (ipf_air_get_geometry(p->air, &cyls, &sides, &platform) != 0) {
        ipf_air_free(p->air);
        free(p->air); free(data); free(p);
        return UFT_ERR_FORMAT_INVALID;
    }
    p->primary_platform = platform;

    disk->plugin_data = p;
    disk->geometry.cylinders = (cyls  > 0) ? cyls  : 84;
    disk->geometry.heads     = (sides > 0) ? sides : 2;
    disk->geometry.sectors = 11;            /* Amiga DD typical; sector layer not derivable from IMGE alone */
    disk->geometry.sector_size = 512;
    disk->geometry.total_sectors =
        (uint32_t)disk->geometry.cylinders *
        (uint32_t)disk->geometry.heads *
        (uint32_t)disk->geometry.sectors;
    return UFT_OK;
}

static void ipf_plugin_close(uft_disk_t *disk) {
    ipf_data_t *p = disk->plugin_data;
    if (!p) return;
    if (p->via_helper) {
        uft_ipf_helper_reply_free(&p->reply);
        /* Die zwei Zwischendateien gehoeren UFT, nicht dem Helfer —
         * also raeumt UFT sie weg. */
        if (p->idx_path[0])  remove(p->idx_path);
        if (p->blob_path[0]) remove(p->blob_path);
    }
    if (p->air) {
        ipf_air_free(p->air);
        free(p->air);
    }
    free(p->data);
    free(p);
    disk->plugin_data = NULL;
}

static uft_error_t ipf_plugin_read_track(uft_disk_t *disk, int cyl, int head,
                                          uft_track_t *track) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    ipf_data_t *p = (ipf_data_t *)disk->plugin_data;
    if (!p) return UFT_ERR_INVALID_STATE;
    uft_track_init(track, cyl, head);

    /* ── MF-917: der Helfer-Weg ─────────────────────────────────────
     * Dieselben Felder wie unten, dieselbe Ehrlichkeit: was der Helfer
     * nicht gemeldet hat, wird nicht erfunden. Eine Spur, die er nicht
     * nennt, gibt es nicht (UFT_ERR_MISSING_SECTOR) — sie wird nicht
     * als leere Spur ausgegeben. */
    if (p->via_helper) {
        const uft_ipf_helper_track_t *ht =
            uft_ipf_helper_find(&p->reply, cyl, head);
        if (!ht) return UFT_ERR_MISSING_SECTOR;

        track->encoding = (p->primary_platform == IPF_PLUGIN_PLATFORM_AMIGA)
            ? UFT_ENC_AMIGA_MFM : UFT_ENC_MFM;
        track->bitrate               = 500000;
        track->nominal_bit_rate_kbps = 250.0;
        track->nominal_rpm           = 300.0;
        track->avg_bit_cell_ns       = 2000.0;
        track->raw_bits              = ht->bits;

        if ((ht->flags & IPF_PLUGIN_TF_FUZZY) || ht->fuzzy) {
            track->status |= (uint32_t)UFT_TRACK_FUZZY |
                             (uint32_t)UFT_TRACK_PROTECTED;
            track->copy_protected = true;
        }
        if (ht->density >= 3) {
            track->status |= (uint32_t)UFT_TRACK_PROTECTED;
            track->copy_protected = true;
            UFT_INFO("IPF Spur %d/%d: Schutzart laut Datei: %s (Dichte %d)",
                     cyl, head, ipf_air_density_name(ht->density),
                     (int)ht->density);
        }

        uint8_t *buf = NULL; size_t n = 0;
        char herr[UFT_IPF_HELPER_ERRLEN] = {0};
        uft_error_t pe = uft_ipf_helper_payload(&p->reply, ht, &buf, &n,
                                                herr, sizeof herr);
        if (pe != UFT_OK) {
            /* Die Nutzdaten fehlen — das wird GESAGT, und die Spur
             * behaelt ihre Beschreibung. Kein stilles UFT_OK mit
             * leerem Inhalt. */
            UFT_WARN("IPF Spur %d/%d: Nutzdaten nicht lesbar: %s",
                     cyl, head, herr);
            return pe;
        }
        if (buf) {
            track->raw_data     = buf;
            track->raw_size     = n;
            track->raw_len      = n;
            track->raw_capacity = n;
            track->owns_data    = true;
            if (ht->bits == 0) track->raw_bits = (uint32_t)(n * 8u);
        }
        return UFT_OK;
    }

    if (!p->air) return UFT_ERR_INVALID_STATE;

    if (!ipf_air_track_present(p->air, cyl, head)) {
        return UFT_ERR_MISSING_SECTOR;
    }

    uint32_t track_bits = 0, density = 0, track_flags = 0;
    bool has_fuzzy = false;
    if (ipf_air_get_track_meta(p->air, cyl, head,
                                &track_bits, &density, &track_flags, &has_fuzzy) != 0) {
        return UFT_ERR_MISSING_SECTOR;
    }

    /* Encoding: Amiga images use AMIGA_MFM, others generic MFM. IPF uses
     * 2µs MFM cells universally — DD bitcell. */
    track->encoding = (p->primary_platform == IPF_PLUGIN_PLATFORM_AMIGA)
        ? UFT_ENC_AMIGA_MFM
        : UFT_ENC_MFM;
    track->bitrate              = 500000;   /* 500 kbps cell rate (250 kbps data) */
    track->nominal_bit_rate_kbps = 250.0;
    track->nominal_rpm           = 300.0;
    track->avg_bit_cell_ns       = 2000.0;
    track->raw_bits = track_bits;

    /* Status flags from IPF descriptor */
    if ((track_flags & IPF_PLUGIN_TF_FUZZY) || has_fuzzy) {
        track->status |= (uint32_t)UFT_TRACK_FUZZY |
                         (uint32_t)UFT_TRACK_PROTECTED;
        track->copy_protected = true;
    }
    /* MF-823: der Dichtewert BENENNT die Schutzart — er wird nicht mehr
     * stillschweigend zu einem Boolean eingedampft.
     *
     * Hier stand nur `if (density >= 3) copy_protected = true`, und der
     * Kommentar daneben („Copylock/Speedlock variants per IPF spec")
     * wusste es besser, als der Code tat. Neun benannte Schutzarten —
     * Copylock Amiga, Copylock Amiga New, Copylock ST, Speedlock Amiga,
     * Speedlock Amiga Old, Adam Brierley, Adam Brierley Key — wurden zu
     * einem Wahrheitswert.
     *
     * Und im selben Baum RIET `uft_atarist_protection.c` denselben Namen
     * aus drei ASCII-Zeichen in Sektordaten, in denen er physikalisch
     * nicht stehen kann (MF-820, zurueckgezogen). Das Werkzeug hatte die
     * belegte Antwort in der Datei und warf sie weg, waehrend es nebenan
     * riet.
     *
     * Der Name wird jetzt GEMELDET. Ihn strukturiert an den Aufrufer zu
     * geben, braucht ein Feld in `uft_track_t` — ein ABI-Eingriff am
     * oeffentlichen Kopf, getrennt zu entscheiden (P3-41). */
    if (density >= 3) {
        track->status |= (uint32_t)UFT_TRACK_PROTECTED;
        track->copy_protected = true;
        /* MF-844: war `fprintf(stderr, ...)`. Diese Stelle sitzt in
         * `read_track()` und feuert damit EINMAL JE SPUR — bei einer
         * Amiga-Diskette bis zu 160 Zeilen auf stderr, unabhaengig
         * davon, ob der Aufrufer sie sehen will. Eingefuehrt mit
         * MF-823, dort uebersehen.
         *
         * `UFT_INFO` ist zugleich die richtige Stufe: es ist eine
         * Feststellung AUS DER DATEI, keine Warnung. */
        UFT_INFO("IPF Spur %d/%d: Schutzart laut Datei: %s (Dichte %d)",
                 cyl, head, ipf_air_density_name(density), density);
    }

    /* Pull concatenated data-element payload bytes from the AIR parser.
     * For SPS-encoded files this gives the deterministic-decoded portion
     * of the track (SYNC/DATA/RAW/IGAP byte sequences as recorded in the
     * IPF DATA records). For CAPS-encoded files the accessor returns -2
     * — we honor that by leaving raw_data NULL without claiming success
     * was partial. */
    uint8_t *payload = NULL;
    uint32_t payload_bits = 0;
    int rc = ipf_air_get_track_raw(p->air, cyl, head, &payload, &payload_bits);
    if (rc == 0 && payload && payload_bits > 0) {
        size_t bytes = ((size_t)payload_bits + 7) / 8;
        track->raw_data    = payload;        /* ownership transfers; track_free will release */
        track->raw_size    = bytes;
        track->raw_len     = bytes;
        track->raw_bits    = payload_bits;
        track->raw_capacity = bytes;
        track->owns_data   = true;
    } else if (payload) {
        free(payload);   /* defensive — should be NULL if rc != 0 */
    }
    /* rc == -2 (CAPS-encoder fallback): leave raw_data NULL; metadata only. */

    return UFT_OK;
}

/* NOTE: write_track omitted by design — IPF is a proprietary preservation
 * format that requires libcapsimage (SPS, closed-source) to encode. Sector
 * write would also bypass the copy-protection signatures that IPF is
 * purpose-built to preserve. */

/* Prinzip 7 Feature-Matrix — see docs/DESIGN_PRINCIPLES.md §7 */
static const uft_plugin_feature_t ipf_features[] = {
    { "Standard MFM Tracks (SPS)",     UFT_FEATURE_PARTIAL,
      "SPS encoder: data-element payload concatenated; gap-padding not synthesized into bitstream" },
    { "Standard MFM Tracks (CAPS)",    UFT_FEATURE_PARTIAL,
      "CAPS encoder: metadata only; data-element decode for CAPS layout deferred" },
    { "Timing Tracks",                 UFT_FEATURE_PARTIAL,
      "track_flags + density exposed; per-cell timing not extracted" },
    { "Weak Bits / Fuzzy",             UFT_FEATURE_PARTIAL,
      "FUZZY-flagged tracks marked PROTECTED; exact bit positions not yet decoded" },
    { "Data Cells",                    UFT_FEATURE_PARTIAL,
      "SPS data-element bytes surfaced as raw_data; bitstream not gap-merged" },
    { "Multi-Revolution Samples",      UFT_FEATURE_UNSUPPORTED, NULL },
    { "SPS Protection Markers",        UFT_FEATURE_UNSUPPORTED, NULL },
    { "Write / encode",                UFT_FEATURE_UNSUPPORTED,
      "requires closed-source libcapsimage" },
    /* MF-917 */
    { "Lesen ueber Helfer-Prozess",    UFT_FEATURE_PARTIAL,
      "UFT-Seite gebaut und gemessen (Protokoll v1, docs/specs/capsimg-helper/); "
      "der Helfer selbst ist nicht Teil dieses Baums und muss vom Benutzer "
      "eingerichtet werden (UFT_IPF_HELPER). Ohne ihn liest der vorhandene "
      "Parser, der unter LIZ-2 in Quarantaene vorgemerkt ist" },
};

const uft_format_plugin_t uft_format_plugin_ipf = {
    .name = "IPF", .description = "CAPS/SPS Preservation Format",
    .extensions = "ipf;ct;ctr", .format = UFT_FORMAT_IPF,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_FLUX | UFT_FORMAT_CAP_VERIFY,
    .probe = ipf_plugin_probe, .open = ipf_plugin_open,
    .close = ipf_plugin_close, .read_track = ipf_plugin_read_track,
    .verify_track = uft_flux_verify_track,
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* SPS never published full IPF spec */
    .features = ipf_features,
    .feature_count = sizeof(ipf_features) / sizeof(ipf_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(ipf)
