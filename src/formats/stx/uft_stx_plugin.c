/**
 * @file uft_stx_plugin.c
 * @brief STX (Pasti) Plugin-B — Atari ST protected format
 *
 * STX preserves sector timing and fuzzy bit masks for copy protection.
 * Magic: "RSY\0" (4 bytes). 16-byte file header + track descriptors.
 *
 * Reference: Pasti specification (Jean Louis-Guerin)
 */
#include "uft/uft_format_common.h"

#define STX_MAGIC_0 'R'
#define STX_MAGIC_1 'S'
#define STX_MAGIC_2 'Y'
#define STX_MAGIC_3 '\0'
#define STX_HEADER_SIZE 16

/* trackNumber (Spursatz-Byte 0x0E) traegt die Spur in Bit 0-6 und die
 * SEITE in Bit 7 — also hoechstens 128 Spuren je Seite. */
#define STX_MAX_TRACK   128
#define STX_MAX_SIDES   2

/* Spur-Flags, Pasti-Dokumentation (Jean Louis-Guerin). Bit 0 entscheidet,
 * ob dem Spursatz Sektordeskriptoren folgen oder unmittelbar Nutzdaten. */
#define STX_TF_SECT_DESC 0x01

/* Standardspuren fuehren ausschliesslich 512-Byte-Sektoren.
 * Vorlage: AIR pasti/PastiRead.cs:314-334 (GPL-3, J. Louis-Guerin),
 * im Baum als uft_stx_air.c:506-538 portiert. */
#define STX_SECTOR_STD  512

typedef struct {
    uint8_t  *file_data;
    size_t    file_size;
    uint16_t  version;
    uint8_t   track_count;
    uint8_t   revision;
    /* MF-847: Spursaetze werden ueber IHRE EIGENE Koordinate abgelegt,
     * nicht ueber ihre Reihenfolge in der Datei. 0 heisst "nicht
     * vorhanden" — Offset 0 ist der Dateikopf und nie ein Spursatz. */
    uint32_t  track_offsets[STX_MAX_TRACK][STX_MAX_SIDES];
} stx_pd_t;

static bool stx_plugin_probe(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    (void)file_size;
    if (size < STX_HEADER_SIZE) return false;
    if (data[0] == STX_MAGIC_0 && data[1] == STX_MAGIC_1 &&
        data[2] == STX_MAGIC_2 && data[3] == STX_MAGIC_3) {
        *confidence = 97;
        return true;
    }
    return false;
}

static uft_error_t stx_open(uft_disk_t *disk, const char *path, bool ro) {
    (void)ro;
    uft_error_t err = UFT_ERROR_NO_MEMORY;
    size_t raw_size = 0;
    uint8_t *raw = uft_read_file(path, &raw_size);
    if (!raw) return UFT_ERROR_FILE_OPEN;
    if (raw_size < STX_HEADER_SIZE) { err = UFT_ERROR_FORMAT_INVALID; goto fail; }
    if (raw[0] != STX_MAGIC_0 || raw[1] != STX_MAGIC_1) { err = UFT_ERROR_FORMAT_INVALID; goto fail; }

    stx_pd_t *p = calloc(1, sizeof(stx_pd_t));
    if (!p) goto fail;

    p->file_data = raw;
    p->file_size = raw_size;
    p->version = uft_read_le16(raw + 4);

    /* MF-847: `trackCount` ist EIN Byte an 0x0A, `revision` ein eigenes
     * an 0x0B (Pasti-Dokumentation; im Baum uft_stx_air.c:233-234, Port
     * von AIR pasti/PastiRead.cs). Ein LE16 an 0x0A faltet die Revision
     * in die Spurzahl: bei revision=2 wurde aus 1 Spur 0x0201 = 513, nach
     * der alten Klemme 200 — und die gemeldete Geometrie hatte 100
     * Zylinder statt 1. Gemessen in test_stx_registrierter_leser.c. */
    p->track_count = raw[10];
    p->revision    = raw[11];

    /* Spursaetze ueber ihre EIGENE Koordinate ablegen.
     *
     * MF-847: bisher zaehlte die Reihenfolge in der Datei
     * (`track_offsets[t] = pos`) und der Leser indizierte spaeter mit
     * `cyl*2 + head`. STX-Dateien enthalten aber regelmaessig nur EINEN
     * TEIL der Spuren — etwa nur die geschuetzten oder nur eine Seite.
     * Dann wanderte jede folgende Spur unter fremde Koordinaten, still.
     * Vorlage: AIR pasti/PastiRead.cs, im Baum uft_stx_air.c:265-266:
     *     track = td.track_number & 0x7F;
     *     side  = (td.track_number >> 7) & 1; */
    size_t pos = STX_HEADER_SIZE;
    int max_cyl = 0;
    for (int t = 0; t < (int)p->track_count && pos + 16 <= raw_size; t++) {
        uint32_t trk_size = uft_read_le32(raw + pos);
        if (trk_size < 16 || pos + trk_size > raw_size) break;

        uint8_t tnum = raw[pos + 14];
        int trk  = tnum & 0x7F;
        int side = (tnum >> 7) & 1;
        p->track_offsets[trk][side] = (uint32_t)pos;
        if (trk + 1 > max_cyl) max_cyl = trk + 1;

        pos += trk_size;
    }

    disk->plugin_data = p;
    disk->geometry.cylinders = max_cyl;
    disk->geometry.heads = 2;
    disk->geometry.sectors = 9;
    disk->geometry.sector_size = 512;
    disk->geometry.total_sectors = (uint32_t)disk->geometry.cylinders * 2 * 9;
    return UFT_OK;

fail:
    free(raw);
    return err;
}

static void stx_close(uft_disk_t *disk) {
    stx_pd_t *p = disk->plugin_data;
    if (p) { free(p->file_data); free(p); disk->plugin_data = NULL; }
}

static uft_error_t stx_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    stx_pd_t *p = disk->plugin_data;
    if (!p || !p->file_data) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);

    if (cyl >= STX_MAX_TRACK || head >= STX_MAX_SIDES) return UFT_OK;

    uint32_t trk_off = p->track_offsets[cyl][head];
    if (trk_off == 0) return UFT_OK;               /* Spur nicht im Abzug */
    if ((size_t)trk_off + 16 > p->file_size) return UFT_OK;

    /* Spursatz, 16 Byte LE:
     *   0x00 recordSize | 0x04 fuzzyCount | 0x08 sectorCount
     *   0x0A flags      | 0x0C trackLength| 0x0E trackNumber | 0x0F type */
    uint32_t fuzzy_count = uft_read_le32(p->file_data + trk_off + 4);
    uint16_t sec_count   = uft_read_le16(p->file_data + trk_off + 8);
    uint16_t trk_flags   = uft_read_le16(p->file_data + trk_off + 10);

    /* ---- Standardspur: keine Deskriptoren, nur 512-Byte-Sektoren ----
     *
     * MF-847: `trk_flags` wurde hier mit `(void)` kassiert und IMMER
     * angenommen, es folgten Sektordeskriptoren. Bei einer Standardspur
     * las der Leser damit Nutzdaten als Adressfelder und meldete
     * Sektoren, deren Nummern und Groessen aus dem Inhalt stammten.
     * Vorlage: AIR pasti/PastiRead.cs:314-334, im Baum
     * uft_stx_air.c:506-538. */
    if (!(trk_flags & STX_TF_SECT_DESC)) {
        size_t off = (size_t)trk_off + 16;
        for (int s = 0; s < (int)sec_count; s++) {
            if (off + STX_SECTOR_STD > p->file_size) break;
            uft_format_add_sector(track, (uint8_t)s, p->file_data + off,
                                  STX_SECTOR_STD, (uint8_t)cyl, (uint8_t)head);
            off += STX_SECTOR_STD;
        }
        return UFT_OK;
    }

    /* ---- Spur mit Sektordeskriptoren ----
     *
     * Aufbau ab trk_off+16:
     *     sectorCount * 16 Byte Deskriptoren
     *     fuzzyCount   Byte Fuzzy-Maske        <- LIEGT DAZWISCHEN
     *     Sektordaten; `dataOffset` zaehlt ab HIER
     *
     * MF-847: die Fuzzy-Maske wurde nicht uebersprungen. Jeder Sektor
     * einer Spur mit Fuzzy-Daten kam damit `fuzzyCount` Byte zu frueh —
     * der Leser lieferte die MASKE als Sektorinhalt, still, mit UFT_OK.
     * Das trifft nicht den Randfall, sondern den Zweck des Formats:
     * Spuren MIT Fuzzy-Maske sind die geschuetzten, und dafuer gibt es
     * STX ueberhaupt.
     *
     * Vorlage: AIR pasti/PastiRead.cs:199-210 — die Maske wird gelesen,
     * `bpos` wandert weiter, DANN erst `track_data_start = bpos`. Im
     * Baum portiert als uft_stx_air.c:333-346. Gemessen in
     * test_stx_registrierter_leser.c: erstes Sektorbyte 0x5C (Fuellbyte
     * der Maske) statt 0xA7 (Nutzdaten), 8 Byte zu frueh. */
    size_t sec_desc_off = (size_t)trk_off + 16;
    size_t data_off = sec_desc_off + (size_t)sec_count * 16 + fuzzy_count;

    /* MF-847: KEINE Klemme mehr. Hier stand `s < 26` — eine Zahl ohne
     * jede Begruendung, die 26 Sektoren stillschweigend durchliess und
     * alles darueber verwarf. Es gab auch keinen Kapazitaetsgrund:
     * `uft_track_add_sector()` waechst per realloc (Verdopplung ab 32).
     * Die Formatgrenze ist `sectorCount` als uint16; die Dateigrenze
     * faengt die Schleife unten ab. Belegte Extremfaelle liegen weit
     * ueber 26 — "Sherman M4" fuehrt 70 Sektoren je Spur (DrCoolZic,
     * Atari Copy Protection Rev 1.4, Klasse NOS). */
    for (int s = 0; s < (int)sec_count; s++) {
        size_t desc = sec_desc_off + (size_t)s * 16;
        if (desc + 16 > p->file_size) break;

        /* Pasti sector descriptor (Jean Louis-Guerin spec):
         *   0x00 data offset, 0x04 header pos, 0x06 read time,
         *   0x08 ID track (C), 0x09 ID head (H), 0x0A ID sector (R),
         *   0x0B ID size (N), 0x0C-0x0D ID CRC, 0x0E FDC status, 0x0F flags.
         * The sector NUMBER is R at 0x0A and the FDC status is at 0x0E — the
         * previous code read sec_id from 0x08 (track C) and the FDC status
         * from 0x0C (ID CRC low byte), so both the reported IDs and the
         * CRC-error detection were wrong. */
        uint32_t data_offset = uft_read_le32(p->file_data + desc);
        uint16_t bit_pos = uft_read_le16(p->file_data + desc + 4);
        uint16_t read_time = uft_read_le16(p->file_data + desc + 6);
        uint8_t  sec_id = p->file_data[desc + 0x0A];   /* R: sector number */
        uint8_t  sec_n = p->file_data[desc + 0x0B];    /* N: size code */
        uint8_t  fdcr = p->file_data[desc + 0x0E];     /* FDC status register */
        (void)bit_pos; (void)read_time;

        uint16_t sec_size = (sec_n < 4) ? (128 << sec_n) : 512;
        size_t abs_data = data_off + data_offset;
        if (abs_data + sec_size > p->file_size) continue;

        uft_format_add_sector(track, sec_id > 0 ? sec_id - 1 : 0,
                              p->file_data + abs_data, sec_size,
                              (uint8_t)cyl, (uint8_t)head);

        /* WD1772 FDC status marks STX carries per sector: bit3 (0x08) = data
         * CRC error, bit5 (0x20) = deleted data-address mark. Surface both
         * (read + represent) instead of silently dropping the deleted mark. */
        if (track->sector_count > 0) {
            if (fdcr & 0x08)
                uft_sector_set_crc(&track->sectors[track->sector_count - 1], false);
            if (fdcr & 0x20)
                track->sectors[track->sector_count - 1].deleted = true;
        }
    }
    return UFT_OK;
}

/* NOTE: write_track omitted by design — STX stores fuzzy-bit streams and
 * per-sector timing that cannot be regenerated from sector data alone.
 * The Pasti format is purpose-built to preserve ST copy-protection, so
 * round-tripping through a sector-level write would destroy protection. */
/* Prinzip 7 Feature-Matrix */
static const uft_plugin_feature_t stx_features[] = {
    { "Standard MFM sectors",     UFT_FEATURE_SUPPORTED,   NULL },
    { "Weak sectors (fuzzy)",     UFT_FEATURE_SUPPORTED,   NULL },
    { "Custom sector timing",     UFT_FEATURE_SUPPORTED,   NULL },
    { "Long tracks",              UFT_FEATURE_PARTIAL,
      "detected and preserved; not regenerated on re-encode" },
    { "Write / encode",           UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_stx = {
    .name = "STX", .description = "Atari ST Pasti (Protected)",
    .extensions = "stx", .format = UFT_FORMAT_STX,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_TIMING | UFT_FORMAT_CAP_VERIFY,
    .probe = stx_plugin_probe, .open = stx_open,
    .close = stx_close, .read_track = stx_read_track,
    .verify_track = uft_weak_bit_verify_track,
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* Pasti never had a public spec */
    .features = stx_features,
    .feature_count = sizeof(stx_features) / sizeof(stx_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(stx)
