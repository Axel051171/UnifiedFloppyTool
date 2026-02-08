/**
 * =============================================================================
 * Polyglotte Boot-Sektor Erkennung - Implementierung
 * =============================================================================
 *
 * Erkennt Multi-Platform Boot-Sektoren auf Dual- und Triple-Format Disketten.
 *
 * Erkennungs-Logik:
 *
 *   1. Byte 0-2 analysieren:
 *      - 0xEB xx 0x90 → PC Short JMP + NOP
 *      - 0xE9 xx xx   → PC Near JMP
 *      - 0x60 xx      → Atari ST 68000 BRA.S
 *      - "DOS"        → Amiga Bootblock
 *
 *   2. BPB (Offset 0x0B-0x23) parsen und validieren:
 *      - bytes_per_sector: 128, 256, 512, 1024
 *      - sectors_per_track: 1-26
 *      - num_heads: 1 oder 2
 *      - media_descriptor: 0xF0-0xFF
 *
 *   3. Plattform-spezifische Checks:
 *      - PC: 0x55AA Signatur bei Offset 0x1FE
 *      - ST: Checksum aller 256 BE-Words = 0x1234
 *      - Amiga: "DOS\x" Magic + Rootblock-Check
 *
 *   4. Polyglot-Erkennung:
 *      - Zähle gültige Plattformen → layout bestimmen
 *      - Spezialfall: 0xEB ist auch gültiger 68000-Opcode
 *        (BCLR auf absolute Adresse, daher kein Problem)
 *      - 0x60 ist kein gültiger x86-Opcode (PUSHA nur auf 186+),
 *        aber MS-DOS prüft nur BPB wenn kein gültiger JMP vorhanden
 *
 * =============================================================================
 */

#include "uft/formats/polyglot_boot.h"
#include <stdio.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  Hilfsfunktionen
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Little-Endian 16-Bit lesen (PC/FAT Format) */
static inline uint16_t le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/** Little-Endian 32-Bit lesen */
static inline uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/** Big-Endian 16-Bit lesen (Atari ST / Amiga) */
static inline uint16_t be16(const uint8_t *p)
{
    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

/** Big-Endian 32-Bit lesen */
static inline uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/** Prüfe ob Wert eine 2er-Potenz ist */
static inline bool is_power_of_2(uint32_t v)
{
    return v != 0 && (v & (v - 1)) == 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  BPB-Parsing (FAT12/16 BIOS Parameter Block)
 * ═══════════════════════════════════════════════════════════════════════════ */

bool poly_parse_bpb(const uint8_t *sector, poly_bpb_t *bpb)
{
    memset(bpb, 0, sizeof(*bpb));

    /* OEM-Name (Offset 0x03, 8 Bytes) */
    memcpy(bpb->oem_name, &sector[0x03], 8);
    bpb->oem_name[8] = '\0';

    /* BPB-Felder auslesen (Little-Endian, wie bei FAT üblich) */
    bpb->bytes_per_sector    = le16(&sector[0x0B]);
    bpb->sectors_per_cluster = sector[0x0D];
    bpb->reserved_sectors    = le16(&sector[0x0E]);
    bpb->num_fats            = sector[0x10];
    bpb->root_dir_entries    = le16(&sector[0x11]);
    bpb->total_sectors_16    = le16(&sector[0x13]);
    bpb->media_descriptor    = sector[0x15];
    bpb->sectors_per_fat     = le16(&sector[0x16]);
    bpb->sectors_per_track   = le16(&sector[0x18]);
    bpb->num_heads           = le16(&sector[0x1A]);
    bpb->hidden_sectors      = le32(&sector[0x1C]);
    bpb->total_sectors_32    = le32(&sector[0x20]);

    /* Plausibilitäts-Checks */
    bool valid = true;

    /* Bytes per Sector: 128, 256, 512, 1024 (Atari ST nutzt auch 512×2=1024) */
    if (bpb->bytes_per_sector == 0 || !is_power_of_2(bpb->bytes_per_sector))
        valid = false;
    if (bpb->bytes_per_sector > 4096)
        valid = false;

    /* Sectors per Cluster: muss 2er-Potenz sein, 1-128 */
    if (bpb->sectors_per_cluster == 0 ||
        !is_power_of_2(bpb->sectors_per_cluster))
        valid = false;

    /* Reserved Sectors: mindestens 1 (der Boot-Sektor selbst) */
    if (bpb->reserved_sectors == 0)
        valid = false;

    /* Number of FATs: typisch 1 oder 2 */
    if (bpb->num_fats == 0 || bpb->num_fats > 4)
        valid = false;

    /* Total Sectors: mindestens einer von beiden muss > 0 sein */
    if (bpb->total_sectors_16 == 0 && bpb->total_sectors_32 == 0)
        valid = false;

    /* Media Descriptor: 0xF0-0xFF (0xF0 = generisch, 0xF8 = Festplatte,
       0xF9 = 720K, 0xFD = 360K, etc.) */
    if (bpb->media_descriptor < 0xF0)
        valid = false;

    /* Sectors per FAT: muss > 0 sein für FAT12/16 */
    if (bpb->sectors_per_fat == 0)
        valid = false;

    /* Sectors per Track: 1-26 für Floppy (26 = 8" SD) */
    if (bpb->sectors_per_track == 0 || bpb->sectors_per_track > 36)
        valid = false;

    /* Heads: 1 oder 2 für Floppy, bis 255 für HD */
    if (bpb->num_heads == 0 || bpb->num_heads > 255)
        valid = false;

    /* Root Directory Entries: muss > 0 sein */
    if (bpb->root_dir_entries == 0)
        valid = false;

    bpb->valid = valid;
    return valid;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Atari ST Checksum
 * ═══════════════════════════════════════════════════════════════════════════ */

uint16_t poly_atari_checksum(const uint8_t *sector)
{
    uint32_t sum = 0;
    for (int i = 0; i < 512; i += 2) {
        sum += be16(&sector[i]);
    }
    return (uint16_t)(sum & 0xFFFF);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  PC Boot-Sektor Erkennung
 * ═══════════════════════════════════════════════════════════════════════════ */

static void analyze_pc(const uint8_t *sector, poly_result_t *r)
{
    r->pc.has_jmp = false;
    r->pc.has_55aa = false;
    r->pc.valid = false;
    r->pc.jmp_target = 0;

    /* JMP-Instruktion prüfen */
    if (sector[0] == 0xEB && sector[2] == 0x90) {
        /* Short JMP + NOP: 0xEB disp8 0x90 */
        r->pc.has_jmp = true;
        r->pc.jmp_target = sector[1];
        r->boot_type = POLY_BOOT_PC_JMP_SHORT;
    } else if (sector[0] == 0xE9) {
        /* Near JMP: 0xE9 disp16 */
        r->pc.has_jmp = true;
        r->pc.jmp_target = sector[1]; /* Low-Byte des Offsets */
        r->boot_type = POLY_BOOT_PC_JMP_NEAR;
    }

    /* 0x55AA Boot-Signatur bei Offset 0x1FE */
    if (sector[0x1FE] == 0x55 && sector[0x1FF] == 0xAA) {
        r->pc.has_55aa = true;
    }

    /* PC-gültig: JMP + gültiger BPB + optional 0x55AA
     * Hinweis: Einige ältere PC-Formate haben kein 0x55AA */
    if (r->pc.has_jmp && r->bpb.valid) {
        r->pc.valid = true;
        r->platforms |= POLY_PLATFORM_PC;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Atari ST Boot-Sektor Erkennung
 * ═══════════════════════════════════════════════════════════════════════════ */

static void analyze_atari(const uint8_t *sector, poly_result_t *r)
{
    memset(&r->atari, 0, sizeof(r->atari));

    r->atari.branch = be16(&sector[0]);

    /* 68000 BRA.S: Opcode 0x60xx, wobei xx der signed displacement ist.
     * Der Displacement muss >= 0x1C sein (nach dem BPB-Header).
     * Übliche Werte: 0x601C, 0x601E, 0x6038, 0x603C */
    bool has_bra = (sector[0] == 0x60);

    if (has_bra) {
        /* BRA.S Ziel berechnen: PC + 2 + sign_extend(displacement) */
        int8_t disp = (int8_t)sector[1];
        r->atari.exec_offset = (uint16_t)(2 + disp);

        /* Displacement muss nach den BPB-Daten zeigen (>= 0x1C)
         * und innerhalb des Sektors bleiben (< 0x1FE) */
        if (disp < 0x1C || r->atari.exec_offset >= 0x1FE) {
            has_bra = false;
        }
    }

    /* Seriennummer (Offset 0x08, 3 Bytes - vom ST zur Disk-Erkennung) */
    r->atari.serial[0] = sector[0x08];
    r->atari.serial[1] = sector[0x09];
    r->atari.serial[2] = sector[0x0A];

    /* Checksum berechnen */
    r->atari.checksum = poly_atari_checksum(sector);

    if (r->atari.checksum == 0x1234) {
        r->atari.cksum_status = POLY_ST_CKSUM_BOOT;
    } else {
        r->atari.cksum_status = POLY_ST_CKSUM_NONBOOT;
    }

    /* ST-gültig: BRA.S + gültiger BPB
     * Auch ohne bootbare Checksumme kann der ST den BPB lesen.
     * Die Checksumme bestimmt nur ob der Boot-Code ausgeführt wird. */
    if (has_bra && r->bpb.valid) {
        r->atari.valid = true;
        r->platforms |= POLY_PLATFORM_ATARI_ST;
        if (r->boot_type == POLY_BOOT_UNKNOWN) {
            r->boot_type = POLY_BOOT_ATARI_BRA;
        }
    }
    /* Auch ohne BRA.S kann der ST eine FAT12-Disk lesen,
     * wenn ein gültiger BPB vorhanden ist (TOS ignoriert Byte 0-1 dann).
     * Aber als "Atari ST Format" zählt das nur mit BRA.S. */

    /* Spezialfall: PC-JMP 0xEB als ST-BPB-Disk
     * Der ST kann auch PC-formatierte Disks lesen (TOS 1.04+),
     * markiere dies als Kombi-Format */
    if (!has_bra && r->bpb.valid && r->pc.has_jmp) {
        /* ST kann PC-Disks lesen, aber es ist kein natives ST-Format.
         * Trotzdem als ST-kompatibel markieren wenn BPB gültig ist
         * und die Geometrie typisch für ST ist (9×512 oder 10×512). */
        if (r->bpb.sectors_per_track >= 9 && r->bpb.sectors_per_track <= 11 &&
            r->bpb.bytes_per_sector == 512 && r->bpb.num_heads <= 2) {
            r->platforms |= POLY_PLATFORM_ATARI_ST;
            r->atari.valid = true;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Amiga Bootblock Erkennung
 * ═══════════════════════════════════════════════════════════════════════════ */

static void analyze_amiga_sector0(const uint8_t *sector, poly_result_t *r)
{
    memset(&r->amiga, 0, sizeof(r->amiga));

    /* Amiga Bootblock Magic: "DOS\x" wobei x = 0-7 */
    if (sector[0] == 'D' && sector[1] == 'O' && sector[2] == 'S') {
        uint8_t fs_type = sector[3];
        if (fs_type <= 7) {
            r->amiga.valid = true;
            r->amiga.type[0] = 'D';
            r->amiga.type[1] = 'O';
            r->amiga.type[2] = 'S';
            r->amiga.type[3] = '0' + fs_type;
            r->amiga.type[4] = '\0';

            r->amiga.is_ffs = (fs_type & 1) != 0;
            r->amiga.is_intl = (fs_type & 2) != 0;
            r->amiga.is_dircache = (fs_type & 4) != 0;

            r->amiga.checksum = be32(&sector[4]);
            r->amiga.root_block = be32(&sector[8]);

            r->platforms |= POLY_PLATFORM_AMIGA;

            switch (fs_type) {
                case 0: r->boot_type = POLY_BOOT_AMIGA_OFS; break;
                case 1: r->boot_type = POLY_BOOT_AMIGA_FFS; break;
                case 2: r->boot_type = POLY_BOOT_AMIGA_INTL_OFS; break;
                case 3: r->boot_type = POLY_BOOT_AMIGA_INTL_FFS; break;
                case 4: r->boot_type = POLY_BOOT_AMIGA_DC_OFS; break;
                case 5: r->boot_type = POLY_BOOT_AMIGA_DC_FFS; break;
                default: break;
            }
        }
    }

    /* Auf Dual/Triple-Format Disks ist Track 0 im Standard-MFM-Format.
     * Der Amiga-Teil beginnt dann NICHT bei Sektor 0, sondern auf
     * späteren Tracks. In diesem Fall hat Sektor 0 einen FAT12 BPB
     * und KEINEN Amiga-Magic.
     *
     * Die Amiga-Erkennung auf Dual-Disks geschieht daher über:
     * 1. Track-Level Analyse (Amiga-Sync-Words auf Tracks > 0)
     * 2. FAT12 Cluster-Map (einige Cluster als "bad" markiert = Amiga-Bereich)
     */
}

static void analyze_amiga_extended(const uint8_t *sector0,
                                   const uint8_t *sector1,
                                   poly_result_t *r)
{
    /* Zuerst Sektor 0 analysieren */
    analyze_amiga_sector0(sector0, r);

    if (!r->amiga.valid || !sector1) return;

    /* Amiga Bootblock Checksum validieren (über 1024 Bytes).
     * Die Checksum ist so berechnet, dass die Summe aller
     * 256 Big-Endian Longs (inkl. Checksum-Feld) = 0 ergibt.
     * Dabei wird Carry ins LSB rotiert. */
    uint32_t sum = 0;
    for (int i = 0; i < 512; i += 4) {
        uint32_t old = sum;
        sum += be32(&sector0[i]);
        if (sum < old) sum++; /* Carry */
    }
    for (int i = 0; i < 512; i += 4) {
        uint32_t old = sum;
        sum += be32(&sector1[i]);
        if (sum < old) sum++; /* Carry */
    }

    /* Summe sollte 0 sein bei gültigem Bootblock */
    if (sum != 0) {
        /* Checksum ungültig - könnte trotzdem Amiga sein
         * (nicht-bootbare Disks haben oft Checksum 0) */
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Polyglot-Erkennung: Kombination mehrerer Plattformen
 * ═══════════════════════════════════════════════════════════════════════════ */

static void analyze_polyglot(poly_result_t *r)
{
    /* Plattformen zählen */
    r->platform_count = 0;
    if (r->platforms & POLY_PLATFORM_PC)       r->platform_count++;
    if (r->platforms & POLY_PLATFORM_ATARI_ST) r->platform_count++;
    if (r->platforms & POLY_PLATFORM_AMIGA)    r->platform_count++;
    if (r->platforms & POLY_PLATFORM_MSX)      r->platform_count++;
    if (r->platforms & POLY_PLATFORM_CPM)      r->platform_count++;

    /* Layout bestimmen */
    if (r->platform_count >= 3) {
        r->layout = POLY_LAYOUT_TRIPLE;
    } else if (r->platform_count == 2) {
        r->layout = POLY_LAYOUT_DUAL;
    } else {
        r->layout = POLY_LAYOUT_SINGLE;
    }

    /* Boot-Typ bei Multi-Plattform auf POLYGLOT setzen */
    if (r->platform_count >= 2) {
        r->boot_type = POLY_BOOT_POLYGLOT;
    }

    /* Spezielle Polyglot-Muster erkennen */

    /* Muster 1: PC + ST
     * Beide nutzen FAT12, Unterschied nur im Boot-Opcode.
     * Sehr häufig - fast alle 720K DD-Disketten sind kompatibel.
     * ST-Boot: 0x60xx, PC-Boot: 0xEBxx90
     * Wenn BPB gültig ist, kann GEMDOS die Disk auch ohne BRA.S lesen. */
    if ((r->platforms & POLY_PLATFORM_PC) &&
        (r->platforms & POLY_PLATFORM_ATARI_ST) &&
        !(r->platforms & POLY_PLATFORM_AMIGA)) {
        /* Standard PC/ST Dual-Format */
        r->confidence = 90;
    }

    /* Muster 2: ST + Amiga (klassisches "Dual Format")
     * Track 0 hat ST BRA.S + FAT12 BPB.
     * Amiga-Daten auf separaten Tracks (11×512 Format).
     * Rob Northen Computing Technologie. */
    if ((r->platforms & POLY_PLATFORM_ATARI_ST) &&
        (r->platforms & POLY_PLATFORM_AMIGA)) {
        r->track_layout.fat_and_amiga = true;
        r->track_layout.shared_track0 = true;
        r->confidence = 85;
    }

    /* Muster 3: PC + ST + Amiga (Triple-Format)
     * Wie Muster 2, aber Boot-Sektor hat auch PC-kompatiblen JMP.
     * Beispiel: "3D Pool" von Maltese Falcon / Aardvark.
     * Track 0: Standard MFM mit polyglottem Boot-Sektor
     * PC/ST: FAT12 Filesystem auf Standard-MFM-Tracks
     * Amiga: OFS/FFS auf Amiga-Format Tracks */
    if (r->platform_count == 3) {
        r->track_layout.fat_and_amiga = true;
        r->track_layout.shared_track0 = true;
        r->confidence = 95;
    }

    /* MSX-DOS Erkennung:
     * MSX-DOS nutzt FAT12 mit eigenem Boot-Code.
     * Erkennung über OEM-String oder spezifische BPB-Werte. */
    if (r->bpb.valid) {
        if (memcmp(r->bpb.oem_name, "MSX_DOS ", 8) == 0 ||
            memcmp(r->bpb.oem_name, "MSX-DOS ", 8) == 0) {
            r->platforms |= POLY_PLATFORM_MSX;
            r->platform_count++;
        }
    }

    /* Track-Layout Schätzung für Dual/Triple-Format */
    if (r->track_layout.fat_and_amiga && r->bpb.valid) {
        uint32_t total = r->bpb.total_sectors_16;
        if (total == 0) total = r->bpb.total_sectors_32;

        uint16_t spt = r->bpb.sectors_per_track;
        uint16_t heads = r->bpb.num_heads;

        if (spt > 0 && heads > 0) {
            uint16_t total_tracks = (uint16_t)(total / (spt * heads));
            /* Auf Dual-Disks nutzt FAT12 typisch nur 40-60% der Tracks */
            r->track_layout.fat_tracks = total_tracks;
            /* Amiga-Tracks = Gesamt (80 Zylinder × 2 Seiten) minus FAT */
            r->track_layout.amiga_tracks = 160 - total_tracks;
        }
    }

    /* Konfidenz anpassen */
    if (r->platform_count == 1) {
        if (r->pc.valid && r->pc.has_55aa) r->confidence = 95;
        else if (r->pc.valid) r->confidence = 80;
        else if (r->atari.valid && r->atari.cksum_status == POLY_ST_CKSUM_BOOT)
            r->confidence = 95;
        else if (r->atari.valid) r->confidence = 80;
        else if (r->amiga.valid) r->confidence = 90;
        else r->confidence = 50;
    } else if (r->platform_count == 0) {
        r->confidence = 0;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  RNC (Rob Northen Computing) Erkennung
 * ═══════════════════════════════════════════════════════════════════════════ */

static void analyze_rnc(const uint8_t *sector, poly_result_t *r)
{
    memset(&r->rnc, 0, sizeof(r->rnc));

    /* RNC PDOS nutzt eigene Sync-Words (0x1448) und einen speziellen
     * Sektor-Marker (0x4891). Dies kann nicht aus einem einzelnen
     * Boot-Sektor erkannt werden - benötigt Track-Level Analyse.
     *
     * Was wir hier prüfen können:
     * 1. Bekannte RNC-Strings im Boot-Sektor
     * 2. Copylock-typische Muster */

    /* Suche nach "Rob Northen" oder "RNC" im Boot-Sektor */
    for (int i = 0; i < 500; i++) {
        if (i + 11 <= 512 &&
            memcmp(&sector[i], "Rob Northen", 11) == 0) {
            r->rnc.detected = true;
            break;
        }
        if (i + 3 <= 512 &&
            sector[i] == 'R' && sector[i+1] == 'N' && sector[i+2] == 'C') {
            /* Könnte zufällig sein, zusätzliche Checks */
            if (r->platform_count >= 2) {
                r->rnc.detected = true;
                break;
            }
        }
    }

    /* Auf Dual-Format Disks mit Atari ST BRA.S + gültigem BPB
     * und gleichzeitig Amiga-kompatiblem Layout → wahrscheinlich RNC */
    if (r->track_layout.fat_and_amiga && r->atari.valid) {
        /* Hohe Wahrscheinlichkeit für RNC-Technologie */
        r->rnc.detected = true;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Geometrie aus BPB ableiten
 * ═══════════════════════════════════════════════════════════════════════════ */

static void derive_geometry(poly_result_t *r)
{
    if (!r->bpb.valid) return;

    r->geometry.sector_size = r->bpb.bytes_per_sector;
    r->geometry.spt = (uint8_t)r->bpb.sectors_per_track;
    r->geometry.heads = (uint8_t)r->bpb.num_heads;

    uint32_t total = r->bpb.total_sectors_16;
    if (total == 0) total = r->bpb.total_sectors_32;

    r->geometry.total_bytes = total * r->bpb.bytes_per_sector;

    if (r->bpb.sectors_per_track > 0 && r->bpb.num_heads > 0) {
        r->geometry.cylinders = (uint16_t)(total /
            (r->bpb.sectors_per_track * r->bpb.num_heads));
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Öffentliche API
 * ═══════════════════════════════════════════════════════════════════════════ */

void poly_analyze_boot_sector(const uint8_t *sector, poly_result_t *result)
{
    poly_analyze_boot_extended(sector, NULL, result);
}

void poly_analyze_boot_extended(const uint8_t *sector0,
                                const uint8_t *sector1,
                                poly_result_t *result)
{
    memset(result, 0, sizeof(*result));
    memcpy(result->boot_sector, sector0, 512);

    /* 1. BPB parsen (vor Plattform-Analyse, da beide nutzen) */
    poly_parse_bpb(sector0, &result->bpb);

    /* 2. Plattform-spezifische Analysen */
    analyze_pc(sector0, result);
    analyze_atari(sector0, result);

    if (sector1) {
        analyze_amiga_extended(sector0, sector1, result);
    } else {
        analyze_amiga_sector0(sector0, result);
    }

    /* 3. Geometrie ableiten */
    derive_geometry(result);

    /* 4. Polyglot-Kombinationen erkennen */
    analyze_polyglot(result);

    /* 5. RNC-Erkennung */
    analyze_rnc(sector0, result);
}

bool poly_check_amiga_track(const uint8_t *track_data, size_t track_len)
{
    /* Amiga MFM Sync-Word: 0x4489
     * Das ist die spezielle MFM-Kodierung von 0xA1, die sich von
     * normalen Daten unterscheidet (missing clock transition).
     * Standard-MFM-Controller generieren dieses Pattern nicht. */
    if (!track_data || track_len < 4) return false;

    int sync_count = 0;
    for (size_t i = 0; i + 1 < track_len; i++) {
        if (track_data[i] == 0x44 && track_data[i+1] == 0x89) {
            sync_count++;
            i++; /* Skip past match */
        }
    }

    /* Amiga DD hat 11 Sektoren pro Track, jeder mit Sync.
     * Mindestens 8 Syncs deuten stark auf Amiga hin. */
    return sync_count >= 8;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  String-Konvertierung
 * ═══════════════════════════════════════════════════════════════════════════ */

const char *poly_platforms_str(uint8_t platforms, char *buf, size_t buf_len)
{
    buf[0] = '\0';
    size_t pos = 0;

    const struct { uint8_t flag; const char *name; } entries[] = {
        { POLY_PLATFORM_PC,       "PC/DOS" },
        { POLY_PLATFORM_ATARI_ST, "Atari ST" },
        { POLY_PLATFORM_AMIGA,    "Amiga" },
        { POLY_PLATFORM_MSX,      "MSX" },
        { POLY_PLATFORM_CPM,      "CP/M" },
    };

    bool first = true;
    for (int i = 0; i < 5; i++) {
        if (platforms & entries[i].flag) {
            int n = snprintf(buf + pos, buf_len - pos,
                             "%s%s", first ? "" : " + ", entries[i].name);
            if (n > 0) pos += (size_t)n;
            first = false;
        }
    }

    if (first) {
        snprintf(buf, buf_len, "(unbekannt)");
    }

    return buf;
}

const char *poly_boot_type_str(poly_boot_type_t type)
{
    switch (type) {
        case POLY_BOOT_UNKNOWN:       return "Unbekannt";
        case POLY_BOOT_PC_JMP_SHORT:  return "PC Short JMP (0xEB xx 0x90)";
        case POLY_BOOT_PC_JMP_NEAR:   return "PC Near JMP (0xE9 xx xx)";
        case POLY_BOOT_ATARI_BRA:     return "Atari ST BRA.S (0x60 xx)";
        case POLY_BOOT_AMIGA_OFS:     return "Amiga OFS (DOS\\0)";
        case POLY_BOOT_AMIGA_FFS:     return "Amiga FFS (DOS\\1)";
        case POLY_BOOT_AMIGA_INTL_OFS:return "Amiga Intl OFS (DOS\\2)";
        case POLY_BOOT_AMIGA_INTL_FFS:return "Amiga Intl FFS (DOS\\3)";
        case POLY_BOOT_AMIGA_DC_OFS:  return "Amiga DirCache OFS (DOS\\4)";
        case POLY_BOOT_AMIGA_DC_FFS:  return "Amiga DirCache FFS (DOS\\5)";
        case POLY_BOOT_POLYGLOT:      return "Polyglot (Multi-Plattform)";
    }
    return "?";
}

const char *poly_layout_str(poly_layout_t layout)
{
    switch (layout) {
        case POLY_LAYOUT_SINGLE: return "Single-Format";
        case POLY_LAYOUT_DUAL:   return "Dual-Format";
        case POLY_LAYOUT_TRIPLE: return "Triple-Format";
    }
    return "?";
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Report-Ausgabe
 * ═══════════════════════════════════════════════════════════════════════════ */

void poly_print_report(const poly_result_t *r, FILE *stream)
{
    char pbuf[128];

    fprintf(stream, "\n╔══════════════════════════════════════════════════════╗\n");
    fprintf(stream, "║  Polyglot Boot-Sektor Analyse                       ║\n");
    fprintf(stream, "╚══════════════════════════════════════════════════════╝\n\n");

    /* Boot-Sektor Hex-Dump (erste 32 Bytes) */
    fprintf(stream, "  Boot-Sektor (Byte 0-31):\n  ");
    for (int i = 0; i < 32; i++) {
        fprintf(stream, "%02X ", r->boot_sector[i]);
        if (i == 15) fprintf(stream, "\n  ");
    }
    fprintf(stream, "\n\n");

    /* Ergebnis */
    fprintf(stream, "  Boot-Typ:     %s\n", poly_boot_type_str(r->boot_type));
    fprintf(stream, "  Layout:       %s\n", poly_layout_str(r->layout));
    fprintf(stream, "  Plattformen:  %s\n",
            poly_platforms_str(r->platforms, pbuf, sizeof(pbuf)));
    fprintf(stream, "  Konfidenz:    %u%%\n", r->confidence);
    fprintf(stream, "\n");

    /* PC-Details */
    if (r->pc.valid || r->pc.has_jmp) {
        fprintf(stream, "  ── PC/DOS ────────────────────────────────\n");
        fprintf(stream, "  JMP:          %s",
                r->pc.has_jmp ? "Ja" : "Nein");
        if (r->pc.has_jmp) {
            fprintf(stream, " (→ Offset 0x%02X)", r->pc.jmp_target);
        }
        fprintf(stream, "\n");
        fprintf(stream, "  0x55AA:       %s\n",
                r->pc.has_55aa ? "Ja" : "Nein");
        fprintf(stream, "  Gültig:       %s\n",
                r->pc.valid ? "Ja" : "Nein");
        fprintf(stream, "\n");
    }

    /* Atari ST Details */
    if (r->atari.valid || (r->boot_sector[0] == 0x60)) {
        fprintf(stream, "  ── Atari ST ──────────────────────────────\n");
        fprintf(stream, "  BRA.S:        0x%04X",  r->atari.branch);
        if (r->boot_sector[0] == 0x60) {
            fprintf(stream, " (→ Offset 0x%04X)", r->atari.exec_offset);
        }
        fprintf(stream, "\n");
        fprintf(stream, "  Seriennr.:    %02X%02X%02X\n",
                r->atari.serial[0], r->atari.serial[1], r->atari.serial[2]);
        fprintf(stream, "  Checksum:     0x%04X (%s)\n",
                r->atari.checksum,
                r->atari.cksum_status == POLY_ST_CKSUM_BOOT ? "bootbar" :
                "nicht bootbar");
        fprintf(stream, "  Gültig:       %s\n",
                r->atari.valid ? "Ja" : "Nein");
        fprintf(stream, "\n");
    }

    /* Amiga Details */
    if (r->amiga.valid) {
        fprintf(stream, "  ── Amiga ─────────────────────────────────\n");
        fprintf(stream, "  Magic:        %s\n", r->amiga.type);
        fprintf(stream, "  FFS:          %s\n", r->amiga.is_ffs ? "Ja" : "Nein");
        fprintf(stream, "  International:%s\n", r->amiga.is_intl ? "Ja" : "Nein");
        fprintf(stream, "  DirCache:     %s\n", r->amiga.is_dircache ? "Ja" : "Nein");
        fprintf(stream, "  Rootblock:    %u\n", r->amiga.root_block);
        fprintf(stream, "\n");
    }

    /* BPB Details */
    if (r->bpb.valid) {
        fprintf(stream, "  ── FAT12 BPB ─────────────────────────────\n");
        fprintf(stream, "  OEM:          \"%s\"\n", r->bpb.oem_name);
        fprintf(stream, "  Sektorgröße:  %u Bytes\n", r->bpb.bytes_per_sector);
        fprintf(stream, "  Sek./Cluster: %u\n", r->bpb.sectors_per_cluster);
        fprintf(stream, "  FATs:         %u × %u Sektoren\n",
                r->bpb.num_fats, r->bpb.sectors_per_fat);
        fprintf(stream, "  Rootdir:      %u Einträge\n", r->bpb.root_dir_entries);
        fprintf(stream, "  Sektoren:     %u\n",
                r->bpb.total_sectors_16 ? r->bpb.total_sectors_16
                                         : r->bpb.total_sectors_32);
        fprintf(stream, "  Media:        0x%02X\n", r->bpb.media_descriptor);
        fprintf(stream, "  Sek./Spur:    %u\n", r->bpb.sectors_per_track);
        fprintf(stream, "  Köpfe:        %u\n", r->bpb.num_heads);
        fprintf(stream, "\n");
    }

    /* Geometrie */
    if (r->geometry.cylinders > 0) {
        fprintf(stream, "  ── Geometrie ─────────────────────────────\n");
        fprintf(stream, "  Zylinder:     %u\n", r->geometry.cylinders);
        fprintf(stream, "  Köpfe:        %u\n", r->geometry.heads);
        fprintf(stream, "  Sek./Spur:    %u × %u Bytes\n",
                r->geometry.spt, r->geometry.sector_size);
        fprintf(stream, "  Kapazität:    %u Bytes (%uK)\n",
                r->geometry.total_bytes, r->geometry.total_bytes / 1024);
        fprintf(stream, "\n");
    }

    /* RNC */
    if (r->rnc.detected) {
        fprintf(stream, "  ── Rob Northen Computing ─────────────────\n");
        fprintf(stream, "  RNC erkannt:  Ja\n");
        if (r->rnc.has_pdos)
            fprintf(stream, "  PDOS:         Ja\n");
        if (r->rnc.has_copylock)
            fprintf(stream, "  Copylock:     Ja\n");
        fprintf(stream, "\n");
    }

    /* Track-Layout */
    if (r->track_layout.fat_and_amiga) {
        fprintf(stream, "  ── Track-Layout (geschätzt) ──────────────\n");
        fprintf(stream, "  Gemischt:     FAT12 + Amiga\n");
        fprintf(stream, "  Track 0:      Geteilt (Multi-Plattform Boot)\n");
        if (r->track_layout.fat_tracks > 0) {
            fprintf(stream, "  FAT-Tracks:   ~%u\n", r->track_layout.fat_tracks);
            fprintf(stream, "  Amiga-Tracks: ~%u\n", r->track_layout.amiga_tracks);
        }
        fprintf(stream, "\n");
    }
}
