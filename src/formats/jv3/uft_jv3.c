/**
 * @file uft_jv3.c
 * @brief JV3 (TRS-80) — Sektorabbild mit Verzeichnis
 *
 * ── Referenzen (EINFRIER-REGEL MF-363/498, Bedingung c) ───────────────
 *
 * **MAME `formats/trs80_dsk.cpp`** (BSD-3-Clause, Dirk Best), in
 * `neue-ideen/formats.zip`. Sein `jv3_format::identify()` gibt die
 * Satzarithmetik woertlich:
 *
 *     const uint32_t header_size = entries * 3 + 1;   // 2901*3+1 = 0x2200
 *     uint32_t data_ptr = header_size, last_data = header_size;
 *     for (sect = 0; sect < entries; sect++) {
 *         if (track < 0xff) { size = 128 << (flag_size ^ 1);
 *                             data_ptr += size; last_data = data_ptr; }
 *         else              { size = 128 << (flag_size ^ 2);
 *                             data_ptr += size; }
 *     }
 *
 * **Tim Mann, „Common File Formats for Emulated TRS-80 Floppy Disks"**
 * (https://www.tim-mann.org/trs80/dskspec.html, abgerufen 2026-09-10),
 * woertlich zur Groessenkodierung:
 *
 *   „if a sector is in use, xor'ing its JV3_SIZE field with 1 gives the
 *    IBM size code that appears in its sector ID size field. If a
 *    sector is free, xor'ing its JV3_SIZE field with 2 gives its IBM
 *    size code."
 *
 * Zwei unabhaengige Haende, dieselbe Regel.
 *
 * ── Aufbau ────────────────────────────────────────────────────────────
 *
 *   0x0000  2901 Satzkoepfe a 3 Byte:
 *             [0] Spur (0xFF = unbenutzt)
 *             [1] Sektornummer
 *             [2] Flags:
 *                 Bit 0-1  Groesse, benutzt ^1 / frei ^2 -> IBM-Code
 *                 Bit 2    non-IBM (nicht unterstuetzt, auch von MAME)
 *                 Bit 3    CRC-Fehler
 *                 Bit 4    Seite (0/1)
 *                 Bit 5-6  DAM-Kode
 *                 Bit 7    Dichte (1 = MFM/Doppeldichte)
 *   0x21FF  Schreibschutz-Byte (0x00 oder 0xFF)
 *   0x2200  Sektordaten, in Satzreihenfolge
 *
 * ── Befund 1 (MF-1017): der Datenanfang lag 256 Byte zu weit ──────────
 *
 * Hier stand `#define JV3_HEADER_SIZE 0x2300` mit dem Kommentar
 * „dir + writeprot + padding", und der Dateikopf sagte „Offset 0x2200:
 * Write-protect byte" / „Offset 0x2300: Sector data". Wirklich ist das
 * Schreibschutz-Byte das **letzte** Byte des Kopfbereichs (0x21FF), und
 * 2901*3+1 ergibt **0x2200** — es gibt keine Polsterung.
 *
 * Gemessen an einer nach MAMEs Arithmetik gebauten Pruefdatei mit drei
 * Sektoren (Fuellbytes A0, B1, D2) und einem freien Satz dazwischen
 * (CC):
 *
 *     Spur 0/0   : 2 Sektoren (Orakel: 3)
 *        Sektor 1: Fuellbyte B1        <- gehoert Sektor 1, nicht 0
 *        Sektor 1: Fuellbyte CC        <- der FREIE Raum
 *
 * **Jeder Sektor jeder JV3-Datei gab die Bytes des naechsten Blocks
 * zurueck.** Still, mit `UFT_OK`. Klasse MF-796 (`edsk` meldete
 * „9 Sektoren" und lieferte fuer jede Spur keinen einzigen) und MF-794
 * (`sad` las 158 von 160 Spuren an der falschen Stelle).
 *
 * ── Befund 2 (MF-1017): der Lauf brach beim ersten freien Satz ab ─────
 *
 *     if (trk == JV3_FREE_ENTRY && sid == JV3_FREE_ENTRY) break;
 *
 * MAMEs Beschreibung sagt ausdruecklich das Gegenteil: „Unused
 * descriptors are FF FF (FC | size). **These can be intermixed with
 * valid descriptors.**" Sein `identify()` laeuft alle 2901 Saetze ab
 * und **ueberspringt** die freien, wobei es den Datenzeiger um deren
 * Groesse weiterschiebt — freie Saetze belegen Datenraum.
 *
 * Der alte Lauf verlor damit **alles hinter der ersten Luecke** (oben
 * gemessen: 2 von 3 Sektoren), und dazu die Unterscheidung, dass ein
 * freier Satz seine Groesse mit **2** statt mit 1 verrechnet.
 *
 * ── Befund 3 (MF-1017): der Freimarker ist die SPUR allein ────────────
 *
 * Der alte Test verlangte `trk == 0xFF && sid == 0xFF`. MAME prueft
 * `if (track < 0xff)` — die Spur allein entscheidet. Ein Satz mit Spur
 * 0xFF und einer Sektornummer ungleich 0xFF galt hier als benutzt und
 * lieferte einen Sektor der Spur 255.
 *
 * ── Befund 4 (MF-1017): die Sektornummern kippten zusammen ────────────
 *
 *     uint8_t sec_num = e->sector_id;
 *     if (sec_num > 0) sec_num--;
 *     uft_format_add_sector(track, sec_num, ...);
 *
 * `uft_format_add_sector()` addiert 1 (so steht es in seinem Kopf), das
 * `--` war die Gegenrechnung dazu — und die Schranke `> 0` machte sie
 * fuer Sektor **0** unwirksam: 0 blieb 0, plus 1 ergab **1**. Sektor 0
 * und Sektor 1 landeten damit beide auf ID **1**. Genau so stand es in
 * der Messung oben („Sektor 1" zweimal). Jetzt
 * `uft_format_add_sector_with_id()` mit der Nummer aus dem Satz.
 *
 * ── Befund 5 (MF-1017): Dichte und DAM standen in falschen Bits ───────
 *
 * Der alte Dateikopf sagte „bit 5 = double density, bit 6 = deleted
 * address mark". MAME und Tim Mann sagen: **Bit 7** ist die Dichte,
 * **Bits 5-6** sind ein zweistelliger DAM-Kode:
 *
 *   | DAM  | Einzeldichte | Doppeldichte |
 *   |---|---|---|
 *   | 0xF8 „deleted"   | 0x60 | 0x20 |
 *   | 0xF9 (undefiniert)| 0x40 | ungueltig |
 *   | 0xFA (undefiniert)| 0x20 | ungueltig |
 *   | 0xFB „normal"    | 0x00 | 0x00 |
 *
 * Der Code las `flags & 0x40` als „deleted" — das trifft in
 * Einzeldichte die **0xF9**, nicht die 0xF8, und in Doppeldichte einen
 * ungueltigen Wert. Die Dichte (Bit 7) wurde **nie** gelesen.
 *
 * ── Was diese Datei bewusst NICHT tut ─────────────────────────────────
 *
 * **Nur ein Satzblock.** MAME: „the format allows another set of
 * descriptors and data. However no TRS-80 disks need this extra area,
 * so it's unsupported." Genauso hier — und ausdruecklich abgewiesen
 * statt still halbiert.
 *
 * **Kein non-IBM.** Bit 2 zeigt Sektorgroessen in 16-Byte-Schritten an,
 * die das Format selbst nicht bezeichnet; MAME sagt dazu „it's
 * impossible to support it". Ein solcher Satz wird benannt abgewiesen.
 */

#include "uft/uft_format_common.h"

#define JV3_DIR_ENTRIES     2901
#define JV3_DIR_SIZE        (JV3_DIR_ENTRIES * 3)               /* 8703 */
#define JV3_RO_FLAG_OFF     JV3_DIR_SIZE                        /* 0x21FF */
#define JV3_HEADER_SIZE     (JV3_DIR_SIZE + 1)                  /* 0x2200 */
#define JV3_FREE_ENTRY      0xFF
#define JV3_MAX_TRACKS      96      /* MAME MAX_TRACKS */
#define JV3_MAX_SECTORS     19      /* MAME MAX_SECTORS */

/* Flag-Bits */
#define JV3_F_SIZE          0x03
#define JV3_F_NONIBM        0x04
#define JV3_F_CRC_ERR       0x08
#define JV3_F_SIDE          0x10
#define JV3_F_DAM           0x60
#define JV3_F_DENSITY       0x80

/* IBM-Groessencode -> Bytes. MAME rechnet `128 << code`. */
static const uint16_t jv3_ibm_size[4] = { 128, 256, 512, 1024 };

/* Groesse eines Satzes. `benutzt` entscheidet, ob mit 1 oder 2
 * verrechnet wird — die Regel steht woertlich bei Tim Mann und in
 * MAMEs `identify()`. */
static uint16_t jv3_size_of(uint8_t flags, bool benutzt)
{
    uint8_t code = (uint8_t)((flags & JV3_F_SIZE) ^ (benutzt ? 1 : 2));
    return jv3_ibm_size[code & 3];
}

/* Der DAM-Wert, den die Bits 5-6 zusammen mit der Dichte bezeichnen.
 * 0 = ungueltige Kombination. */
static uint8_t jv3_dam_of(uint8_t flags)
{
    bool dd = (flags & JV3_F_DENSITY) != 0;
    switch (flags & JV3_F_DAM) {
    case 0x00: return 0xFB;                       /* normal, beide */
    case 0x20: return dd ? 0xF8 : 0xFA;
    case 0x40: return dd ? 0x00 : 0xF9;
    case 0x60: return dd ? 0x00 : 0xF8;
    default:   return 0x00;
    }
}

typedef struct {
    uint8_t     track;
    uint8_t     sector_id;
    uint8_t     flags;
    uint16_t    size;
    uint32_t    data_offset;
} jv3_dir_entry_t;

typedef struct {
    FILE*               file;
    jv3_dir_entry_t     entries[JV3_DIR_ENTRIES];
    uint16_t            entry_count;
    uint8_t             max_cyl;
    uint8_t             max_head;
} jv3_data_t;

/* MF-449: the previous body counted evidence that could not fail.
 *
 * It looked at 100 directory entries and counted one "valid" whenever
 *
 *     trk < 80 && (flags & 0x03) < 4
 *
 * The second half is vacuous — a two-bit field is always below 4 — so the test
 * reduced to "the byte at stride 3 is below 80", true for roughly a third of
 * arbitrary data. Five hits out of a hundred is then a near certainty for any
 * input, and the answer was confidence 70.
 *
 * It stayed invisible because uft_probe_file_format() reads 4096 bytes and this
 * probe needs JV3_HEADER_SIZE (8704) before it does anything: through that
 * entry point JV3 could never match at all. uft_smart_open() reads 65536, and
 * there the same Atari XFD image that XFD claims at 40 was taken by JV3 at 70
 * (ARCH-15). Unifying the buffer size without fixing this would have made the
 * false positive the normal case.
 *
 * MF-1017: der Lauf folgt jetzt MAMEs `identify()` statt der alten
 * Abbruchregel — alle 2901 Saetze, freie uebersprungen, und die
 * Pruefungen, die das Orakel wirklich anstellt (Schreibschutz-Byte,
 * DMK-Unterscheidung, Spur/Sektor-Grenzen, Doppelvergabe). */
bool jv3_probe(const uint8_t *data, size_t size, size_t file_size,
               int *confidence)
{
    if (!data || size < JV3_HEADER_SIZE || file_size < JV3_HEADER_SIZE)
        return false;

    /* MAME: „check the readonly flag" — 0x00 oder 0xFF, sonst still nein */
    if (data[JV3_RO_FLAG_OFF] != 0x00 && data[JV3_RO_FLAG_OFF] != 0xFF)
        return false;

    /* MAME: „Check for DMK" — sind die Bytes 5..15 alle null, ist es
     * eine DMK-Datei und nicht JV3. */
    uint8_t dmk_test = 0;
    for (int i = 5; i < 16; i++) dmk_test |= data[i];
    if (!dmk_test) return false;

    /* MAME: „Check for other disk formats" */
    if (data[1] >= JV3_MAX_SECTORS) return false;

    size_t data_ptr = JV3_HEADER_SIZE;
    size_t last_data = JV3_HEADER_SIZE;
    int used = 0;
    /* Doppelvergabe: Seite x Spur x Sektor, wie MAME */
    static uint8_t gesehen[2][JV3_MAX_TRACKS][JV3_MAX_SECTORS];
    memset(gesehen, 0, sizeof(gesehen));

    for (int i = 0; i < JV3_DIR_ENTRIES; i++) {
        uint8_t trk = data[i * 3];
        uint8_t sid = data[i * 3 + 1];
        uint8_t flg = data[i * 3 + 2];

        if (trk == JV3_FREE_ENTRY) {
            data_ptr += jv3_size_of(flg, false);
            continue;
        }
        if (trk >= JV3_MAX_TRACKS || sid >= JV3_MAX_SECTORS) return false;
        uint8_t seite = (flg & JV3_F_SIDE) ? 1 : 0;
        if (gesehen[seite][trk][sid]) return false;     /* doppelt */
        gesehen[seite][trk][sid] = 1;

        data_ptr += jv3_size_of(flg, true);
        last_data = data_ptr;
        used++;
    }

    /* MAME: „Is all data in the image? (unused tracks at the end are
     * optional)" — geprueft wird `last_data`, nicht `data_ptr`. */
    if (last_data > file_size) return false;
    if (used < 10) return false;        /* not even one track's worth */

    /* Gestaffelt: ein genauer Sitz ist der starke Fall. */
    *confidence = (last_data == file_size) ? 85 : 60;
    return true;
}

static uft_error_t jv3_open(uft_disk_t *disk, const char *path,
                              bool read_only)
{
    size_t file_size = 0;
    uint8_t *file_data = uft_read_file(path, &file_size);
    if (!file_data || file_size < JV3_HEADER_SIZE) {
        free(file_data);
        return UFT_ERROR_FORMAT_INVALID;
    }
    if (file_data[JV3_RO_FLAG_OFF] != 0x00
        && file_data[JV3_RO_FLAG_OFF] != 0xFF) {
        free(file_data);
        return UFT_ERROR_FORMAT_INVALID;
    }

    jv3_data_t *pdata = calloc(1, sizeof(jv3_data_t));
    if (!pdata) { free(file_data); return UFT_ERROR_NO_MEMORY; }

    /* Reopen as FILE — r+b for write, rb for read-only */
    pdata->file = fopen(path, read_only ? "rb" : "r+b");
    if (!pdata->file) {
        /* Fallback to read-only if r+b fails (e.g. permissions) */
        pdata->file = fopen(path, "rb");
        read_only = true;
    }
    if (!pdata->file) { free(pdata); free(file_data); return UFT_ERROR_FILE_OPEN; }
    disk->read_only = read_only;

    /* ── Verzeichnis, nach MAMEs identify()/load() ─────────────────── */
    uint32_t data_off = JV3_HEADER_SIZE;
    uint8_t max_cyl = 0, max_head = 0;
    bool zu_kurz = false;

    for (int i = 0; i < JV3_DIR_ENTRIES; i++) {
        uint8_t trk = file_data[i * 3];
        uint8_t sid = file_data[i * 3 + 1];
        uint8_t flg = file_data[i * 3 + 2];

        /* MF-1017, Befund 3: die SPUR allein entscheidet. */
        if (trk == JV3_FREE_ENTRY) {
            /* MF-1017, Befund 2: freie Saetze belegen Datenraum und
             * werden UEBERSPRUNGEN, nicht als Ende gelesen. Ihre
             * Groesse verrechnet sich mit 2, nicht mit 1. */
            data_off += jv3_size_of(flg, false);
            continue;
        }

        /* Ausserhalb der Grenzen, die das Orakel zieht: abweisen statt
         * eine unmoegliche Geometrie zu melden. */
        if (trk >= JV3_MAX_TRACKS || sid >= JV3_MAX_SECTORS) {
            fclose(pdata->file);
            free(pdata);
            free(file_data);
            return UFT_ERROR_FORMAT_INVALID;
        }
        /* MAME: „bit 2: non-ibm flag … it's impossible to support it." */
        if (flg & JV3_F_NONIBM) {
            fclose(pdata->file);
            free(pdata);
            free(file_data);
            return UFT_ERROR_NOT_SUPPORTED;
        }

        uint16_t sz = jv3_size_of(flg, true);
        if ((size_t)data_off + sz > file_size) { zu_kurz = true; break; }

        jv3_dir_entry_t *e = &pdata->entries[pdata->entry_count];
        e->track = trk;
        e->sector_id = sid;
        e->flags = flg;
        e->size = sz;
        e->data_offset = data_off;

        uint8_t head = (flg & JV3_F_SIDE) ? 1 : 0;
        if (trk > max_cyl) max_cyl = trk;
        if (head > max_head) max_head = head;

        data_off += sz;
        pdata->entry_count++;
    }

    free(file_data);

    /* Ein Sektor, dessen Daten nicht in der Datei stehen, waere ein
     * erfundener Sektor. MAME weist die Datei dafuer ab; hier ebenso,
     * statt sie still zu halbieren. */
    if (zu_kurz || pdata->entry_count == 0) {
        fclose(pdata->file);
        free(pdata);
        return UFT_ERROR_FORMAT_INVALID;
    }

    pdata->max_cyl = max_cyl;
    pdata->max_head = max_head;

    disk->plugin_data = pdata;
    disk->geometry.cylinders = max_cyl + 1;
    disk->geometry.heads = max_head + 1;
    disk->geometry.sectors = JV3_MAX_SECTORS;   /* varies per track */
    disk->geometry.sector_size = 256;
    disk->geometry.total_sectors = pdata->entry_count;
    return UFT_OK;
}

static void jv3_close(uft_disk_t *disk)
{
    jv3_data_t *p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t jv3_read_track(uft_disk_t *disk, int cyl, int head,
                                    uft_track_t *track)
{
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen gerechnet
     * oder indiziert wird. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    jv3_data_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    uint8_t buf[1024];
    for (int i = 0; i < p->entry_count; i++) {
        jv3_dir_entry_t *e = &p->entries[i];
        uint8_t e_head = (e->flags & JV3_F_SIDE) ? 1 : 0;
        if (e->track != cyl || e_head != head) continue;

        if (fseek(p->file, (long)e->data_offset, SEEK_SET) != 0)
            continue;
        uint16_t sz = e->size;
        if (sz > 1024) sz = 1024;
        if (fread(buf, 1, sz, p->file) != sz) continue;

        /* MF-1017, Befund 4: die Nummer aus dem Satz, unveraendert. */
        uft_format_add_sector_with_id(track, e->sector_id, buf, sz,
                                      (uint8_t)cyl, (uint8_t)head);
        if (track->sector_count > 0) {
            uft_sector_t *s = &track->sectors[track->sector_count - 1];
            if (e->flags & JV3_F_CRC_ERR)
                uft_sector_set_crc(s, false);
            /* MF-1017, Befund 5: „deleted" ist DAM 0xF8, und der Kode
             * steht in den Bits 5-6 ZUSAMMEN mit der Dichte in Bit 7 —
             * nicht in Bit 6 allein. */
            if (jv3_dam_of(e->flags) == 0xF8)
                s->deleted = true;
        }
    }
    return UFT_OK;
}

/* Write track: seeks to each sector's data_offset in the JV3 file and writes.
 * JV3 sectors have known offsets from the directory, so we can write in-place.
 * Matches incoming sectors by sector_id to directory entries.
 *
 * MF-1017: die Zuordnung ist jetzt geradeaus, weil die Leseseite die
 * Nummer unveraendert setzt. Vorher stand hier die Rechnung
 * „id.sector = (sector_id - 1) + 1 = sector_id", die fuer Sektor 0
 * nicht aufging (Befund 4). */
static uft_error_t jv3_write_track(uft_disk_t *disk, int cyl, int head,
                                    const uft_track_t *track)
{
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    jv3_data_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;

    for (size_t s = 0; s < track->sector_count; s++) {
        uint8_t sec_id = track->sectors[s].id.sector;
        /* Find matching directory entry */
        for (int i = 0; i < p->entry_count; i++) {
            jv3_dir_entry_t *e = &p->entries[i];
            uint8_t e_head = (e->flags & JV3_F_SIDE) ? 1 : 0;
            if (e->track != (uint8_t)cyl || e_head != (uint8_t)head)
                continue;
            if (e->sector_id != sec_id) continue;

            /* Found matching entry — write sector data */
            if (fseek(p->file, (long)e->data_offset, SEEK_SET) != 0)
                return UFT_ERROR_IO;
            const uint8_t *data = track->sectors[s].data;
            uint16_t sz = e->size;
            if (sz > 1024) sz = 1024;
            uint8_t pad[1024];
            if (!data || track->sectors[s].data_len == 0) {
                /* Der Satz sagt, wie gross der Sektor ist; es MUSS
                 * etwas dort stehen. `0xE5` ist eine benannte Wahl auf
                 * dem SCHREIBweg, keine Messung — auf dem Leseweg wird
                 * nichts erfunden (Tor 62). */
                memset(pad, 0xE5, sz); data = pad;
            }
            if (fwrite(data, 1, sz, p->file) != sz)
                return UFT_ERROR_IO;
            break;
        }
    }
    fflush(p->file);
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_jv3_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_jv3 = {
    .name = "JV3", .description = "TRS-80 JV3 (with sector directory)",
    .extensions = "jv3;dsk", .format = UFT_FORMAT_JV3,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = jv3_probe, .open = jv3_open, .close = jv3_close,
    .read_track = jv3_read_track, .write_track = jv3_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_OFFICIAL_FULL,  /* MF-1017: Tim Manns
                                             * Formatbeschreibung und MAMEs
                                             * Umsetzung liegen vor
                                             * (vorher DERIVED) */
    .features = uft_format_plugin_jv3_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_jv3_features) / sizeof(uft_format_plugin_jv3_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(jv3)
