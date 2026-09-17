/**
 * @file uft_fdi_pc98.c
 * @brief PC-98 FDI (Anex86) Plugin-B
 *
 * FDI is the standard disk image format for the Anex86 PC-98 emulator.
 * Variable-size header (typically 4096 bytes) followed by raw sector data.
 *
 * Header layout:
 *   Offset  Size  Description
 *   0x00    4     Reserved (must be 0)
 *   0x04    4     FDD type (0x10=2DD, 0x30=2DD 8sect, 0x90=2HD 1.2M, 0xB0=2HD 1024, etc.)
 *   0x08    4     Header size (typically 4096)
 *   0x0C    4     Data size in bytes
 *   0x10    4     Bytes per sector (128, 256, 512, 1024)
 *   0x14    4     Sectors per track
 *   0x18    4     Heads (1 or 2)
 *   0x1C    4     Cylinders (typically 77 or 80)
 *
 * Data follows header in sequential C/H/S order.
 *
 * Note: This plugin must have HIGHER confidence than the generic FDI
 * (signature-based) plugin when the file IS a PC-98 FDI, since PC-98 FDI
 * does NOT have the "FDI" ASCII signature.
 *
 * Reference: Anex86 emulator documentation, Common Source Code Project
 *
 * ── T2 -> T1b, und ein stiller Verlust dabei gefunden (MF-1224) ──
 *
 * MF-1026 hat diesen Leser gegen MAMEs `pc98fdi_dsk.cpp` abgenommen und
 * KEINEN Fehler gefunden: Kopffelder, beide Konsistenzbedingungen aus
 * `identify()`, die zylinder-dure Versatzformel und die 1-basierten
 * Sektornummern stimmten ueberein. Gefehlt hat ein Abbild von fremder
 * Hand; das liegt seit MF-1224 vor (`hdm_to_fdi.py` aus
 * `pc98-disk-tools`, Eigentuemerentscheidung zur fehlenden Lizenz vom
 * 2026-09-17).
 *
 * **Der Beleg ist schwaecher als die von MF-1222/1223, und das gehoert
 * hierher:** das Werkzeug modelliert nichts. Sein ganzer Rumpf ist
 * `pack('<8L4064x', ...)` plus `fdi_header + hdm_blob`, und die
 * Geometriewerte sind fest verdrahtet. Belegt ist die
 * BEHAELTER-ZERLEGUNG — dieselben acht Dwords an denselben acht
 * Versaetzen —, nicht die Geometrie-Herleitung. Deshalb steht sie auf
 * VIER Haenden: MAME als Spec, `pc98-disk-tools` als Schreiber, `hxcfe`
 * (`NEC_FDI`) als unabhaengiger Leser, der daraus eine IMD schreibt und
 * darin 1232 von 1232 Sektoren an ihrer Stelle hat, und UFT als vierter.
 *
 * ** UND DER ROTBEWEIS HAT EINEN ECHTEN DEFEKT GEFANGEN. ** `probe()`
 * prueft seit immer ZWEI Konsistenzen — `fddsize == cyl*heads*spt*ss`
 * und `file_size == hdr_size + fddsize`. `open()` prueft**te** keine von
 * beiden: es las `fddsize` (0x0C) gar nicht und die Dateigroesse
 * ueberhaupt nicht. Gemessen an dem fremden Abbild, dessen Zylinderfeld
 * von 77 auf 76 verfaelscht wurde:
 *
 *     probe()  ->  false                  (abgewiesen, richtig)
 *     open()   ->  UFT_OK, 76x2x8, total 1216
 *     lesbar   ->  1216 statt 1232 Sektoren
 *     VERLUST  ->  16 Sektoren = 16 384 Byte, STILL, bei Erfolgsmeldung
 *
 * Der letzte Zylinder wurde unerreichbar. Das ist die Gestalt von
 * MF-1038 (`fds`: „die Sonde war dabei ehrlich — das Oeffnen hat ihre
 * Zurueckhaltung aufgehoben"), MF-1039 (`cpm`) und MF-1019 (`dim`, wo
 * `open` die Dateigroesse seither wie die Sonde prueft). Seit MF-1224
 * prueft `open()` beides und SAGT AB statt zu kappen (D5).
 *
 * ── Eine Sonde ueber ihrem Band, benannt statt geaendert ──
 *
 * `fdi_pc98_probe()` vergibt **90**, und PC-98-FDI hat **keine
 * Kennung** — der Hinweis oben sagt das selbst. Die Sondendoktrin
 * (MF-1153, Eigentuemer-Entscheidung) deckelt ohne Kennung bei **45**.
 * 90 liegt im Band „Merkmal getroffen" (80-100, MF-729), und ein
 * Merkmal gibt es nicht. Die Zahl wird hier NICHT gesenkt — eine Zahl
 * zu aendern, damit eine Doktrin stimmt, ist derselbe Reflex, den
 * MF-1077 verbietet. Gefuehrt als `P3-476`, und die offene Frage ist,
 * ob der Leiter eine Sprosse fuer „fuenf Kopffelder sind untereinander
 * und mit der Dateigroesse konsistent" fehlt: die Sonde diskriminiert
 * nachweislich, sie weist Nullpuffer und Pseudozufall ab.
 */

#include "uft/uft_format_common.h"

#define FDI_PC98_MIN_HDR    32   /* Minimum bytes needed for header fields */

typedef struct {
    FILE    *file;
    uint32_t header_size;
    uint32_t cylinders;
    uint32_t heads;
    uint32_t spt;
    uint32_t sector_size;
} fdi_pc98_pd_t;

static bool fdi_pc98_validate_geometry(uint32_t cyls, uint32_t heads,
                                        uint32_t spt, uint32_t ss)
{
    /* Sanity checks for PC-98 geometry */
    if (cyls == 0 || cyls > 85) return false;
    if (heads == 0 || heads > 2) return false;
    if (spt == 0 || spt > 30) return false;
    if (ss != 128 && ss != 256 && ss != 512 && ss != 1024) return false;
    return true;
}

static bool fdi_pc98_probe(const uint8_t *data, size_t size, size_t file_size,
                            int *confidence)
{
    if (size < FDI_PC98_MIN_HDR) return false;

    /* Offset 0x00: reserved must be 0 */
    uint32_t reserved = uft_read_le32(data + 0x00);
    if (reserved != 0) return false;

    /* Offset 0x08: header size (reasonable range: 256-65536) */
    uint32_t hdr_size = uft_read_le32(data + 0x08);
    if (hdr_size < 256 || hdr_size > 65536) return false;

    /* Header must not exceed file size */
    if ((size_t)hdr_size > file_size) return false;

    /* Read geometry */
    uint32_t sector_size = uft_read_le32(data + 0x10);
    uint32_t spt         = uft_read_le32(data + 0x14);
    uint32_t heads       = uft_read_le32(data + 0x18);
    uint32_t cylinders   = uft_read_le32(data + 0x1C);

    if (!fdi_pc98_validate_geometry(cylinders, heads, spt, sector_size))
        return false;

    /* Offset 0x0C: data size */
    uint32_t data_size = uft_read_le32(data + 0x0C);
    uint32_t expected_data = cylinders * heads * spt * sector_size;

    /* Validate data size matches geometry */
    if (data_size != expected_data) return false;

    /* Validate total file size */
    if (file_size != (size_t)hdr_size + data_size) return false;

    /* Validate FDD type is reasonable */
    uint32_t fdd_type = uft_read_le32(data + 0x04);
    (void)fdd_type; /* Accept any type if geometry checks out */

    *confidence = 90;
    return true;
}

static uft_error_t fdi_pc98_open(uft_disk_t *disk, const char *path, bool ro)
{
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;

    uint8_t hdr[FDI_PC98_MIN_HDR];
    if (fread(hdr, 1, FDI_PC98_MIN_HDR, f) != FDI_PC98_MIN_HDR) {
        fclose(f);
        return UFT_ERROR_IO;
    }

    /* Validate reserved field */
    uint32_t reserved = uft_read_le32(hdr + 0x00);
    if (reserved != 0) { fclose(f); return UFT_ERROR_FORMAT_INVALID; }

    uint32_t hdr_size    = uft_read_le32(hdr + 0x08);
    uint32_t data_size   = uft_read_le32(hdr + 0x0C);   /* MF-1224 */
    uint32_t sector_size = uft_read_le32(hdr + 0x10);
    uint32_t spt         = uft_read_le32(hdr + 0x14);
    uint32_t heads       = uft_read_le32(hdr + 0x18);
    uint32_t cylinders   = uft_read_le32(hdr + 0x1C);

    if (!fdi_pc98_validate_geometry(cylinders, heads, spt, sector_size)) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }

    if (hdr_size < 256 || hdr_size > 65536) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }

    /* Overflow check */
    if (!uft_geometry_sane((uint16_t)cylinders, (uint8_t)heads,
                           (uint8_t)spt, (uint16_t)sector_size)) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }

    /* MF-1224: `open()` prueft jetzt DIESELBEN zwei Konsistenzen wie die
     * Sonde. Vorher las es `fddsize` (0x0C) gar nicht und die
     * Dateigroesse ueberhaupt nicht — ein Kopf, der ueber die Geometrie
     * log, wurde also mit UFT_OK angenommen, waehrend `probe()` ihn
     * abwies. Gemessen an einem Abbild von `pc98-disk-tools`, dessen
     * Zylinderfeld von 77 auf 76 verfaelscht wurde:
     *
     *     probe()  ->  false          (abgewiesen, richtig)
     *     open()   ->  UFT_OK, 76x2x8, total 1216
     *     lesbar   ->  1216 statt 1232 Sektoren
     *     VERLUST  ->  16 Sektoren = 16 384 Byte, STILL
     *
     * Der letzte Zylinder wurde unerreichbar, und gemeldet war Erfolg.
     * Das ist die Gestalt von MF-1038 (`fds`: „die Sonde war dabei
     * ehrlich — das Oeffnen hat ihre Zurueckhaltung aufgehoben"),
     * MF-1039 (`cpm`) und MF-1019 (`dim`, wo `open` die Dateigroesse
     * seither wie die Sonde prueft). Es wird ABGESAGT und nichts
     * gekappt: ein gekappter Wert saehe wie eine Messung aus (D5). */
    if (data_size != cylinders * heads * spt * sector_size) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long dateigroesse = ftell(f);
    if (dateigroesse < 0) { fclose(f); return UFT_ERROR_IO; }
    if ((uint64_t)dateigroesse != (uint64_t)hdr_size + (uint64_t)data_size) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }

    fdi_pc98_pd_t *p = calloc(1, sizeof(fdi_pc98_pd_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    p->header_size = hdr_size;
    p->cylinders = cylinders;
    p->heads = heads;
    p->spt = spt;
    p->sector_size = sector_size;

    disk->plugin_data = p;
    disk->geometry.cylinders = (uint16_t)cylinders;
    disk->geometry.heads = (uint16_t)heads;
    disk->geometry.sectors = (uint16_t)spt;
    disk->geometry.sector_size = (uint16_t)sector_size;
    disk->geometry.total_sectors = cylinders * heads * spt;
    return UFT_OK;
}

static void fdi_pc98_close(uft_disk_t *disk)
{
    fdi_pc98_pd_t *p = disk->plugin_data;
    if (p) {
        if (p->file) fclose(p->file);
        free(p);
        disk->plugin_data = NULL;
    }
}

static uft_error_t fdi_pc98_read_track(uft_disk_t *disk, int cyl, int head,
                                        uft_track_t *track)
{
    fdi_pc98_pd_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (cyl < 0 || (uint32_t)cyl >= p->cylinders ||
        head < 0 || (uint32_t)head >= p->heads)
        return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    /* Data offset: header + (cyl * heads + head) * spt * sector_size */
    long offset = (long)p->header_size +
                  (long)((uint32_t)cyl * p->heads + (uint32_t)head) *
                  p->spt * p->sector_size;

    if (fseek(p->file, offset, SEEK_SET) != 0) return UFT_ERROR_IO;

    uint8_t *buf = malloc(p->sector_size);
    if (!buf) return UFT_ERROR_NO_MEMORY;

    for (uint32_t s = 0; s < p->spt; s++) {
        /* MF-980: der kurze Lesevorgang wird GEMERKT, nicht nur
         * gefuellt. `uft_format_add_sector*()` legt jeden Sektor mit
         * `status = UFT_SECTOR_OK` und „CRC gueltig" an — die Fuellung
         * war damit von echten Daten nicht zu unterscheiden. Die Bytes
         * bleiben stehen, sie gelten nur nicht mehr als Messwert. */
        const bool kurz = (fread(buf, 1, p->sector_size, p->file) != p->sector_size);
        if (kurz) memset(buf, 0xE5, p->sector_size);
        uft_format_add_sector(track, (uint8_t)s, buf, (uint16_t)p->sector_size,
                              (uint8_t)cyl, (uint8_t)head);
        if (kurz) uft_format_mark_last_missing(track);
    }
    free(buf);
    return UFT_OK;
}

static uft_error_t fdi_pc98_write_track(uft_disk_t *disk, int cyl, int head,
                                         const uft_track_t *track)
{
    fdi_pc98_pd_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    if (cyl < 0 || (uint32_t)cyl >= p->cylinders ||
        head < 0 || (uint32_t)head >= p->heads)
        return UFT_ERROR_INVALID_STATE;

    long base_offset = (long)p->header_size +
                       (long)((uint32_t)cyl * p->heads + (uint32_t)head) *
                       p->spt * p->sector_size;

    uint8_t *pad = NULL;
    for (size_t s = 0; s < track->sector_count && s < p->spt; s++) {
        long sec_off = base_offset + (long)s * (long)p->sector_size;
        if (fseek(p->file, sec_off, SEEK_SET) != 0) {
            free(pad);
            return UFT_ERROR_IO;
        }
        const uint8_t *data = track->sectors[s].data;
        if (!data || track->sectors[s].data_len == 0) {
            if (!pad) {
                pad = malloc(p->sector_size);
                if (!pad) return UFT_ERROR_NO_MEMORY;
                memset(pad, 0xE5, p->sector_size);
            }
            data = pad;
        }
        if (fwrite(data, 1, p->sector_size, p->file) != p->sector_size) {
            free(pad);
            return UFT_ERROR_IO;
        }
    }
    free(pad);
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_fdi_pc98_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_fdi_pc98 = {
    .name = "FDI_PC98",
    .description = "PC-98 FDI (Anex86)",
    .extensions = "fdi",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = fdi_pc98_probe,
    .open = fdi_pc98_open,
    .close = fdi_pc98_close,
    .read_track = fdi_pc98_read_track,
    .write_track = fdi_pc98_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_OFFICIAL_PARTIAL,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_fdi_pc98_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_fdi_pc98_features) / sizeof(uft_format_plugin_fdi_pc98_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(fdi_pc98)
