/**
 * =============================================================================
 * Atari DOS Utilities & CLI Tool
 * =============================================================================
 *
 * Kommandozeilen-Tool für ATR Disk-Image Verwaltung:
 *
 *   atari_tool info <image.atr>          - Disk-Informationen anzeigen
 *   atari_tool dir <image.atr>           - Directory-Listing
 *   atari_tool vtoc <image.atr>          - VTOC-Bitmap anzeigen
 *   atari_tool extract <image.atr> <file> [output] - Datei extrahieren
 *   atari_tool add <image.atr> <local> [atari-name] - Datei hinzufügen
 *   atari_tool delete <image.atr> <file> - Datei löschen
 *   atari_tool rename <image.atr> <alt> <neu> - Datei umbenennen
 *   atari_tool check <image.atr>         - Dateisystem prüfen
 *   atari_tool fix <image.atr>           - Dateisystem reparieren
 *   atari_tool create <image.atr> <typ>  - Neues Image erstellen
 *   atari_tool hexdump <image.atr> <sector> - Sektor Hex-Dump
 * =============================================================================
 */

#include "uft/formats/atari_dos.h"
#include <stdarg.h>

/* ---- Utility-Funktionen ---- */

void ados_print_info(const atari_disk_t *disk, FILE *out)
{
    if (!disk || !out) return;

    fprintf(out, "╔══════════════════════════════════════════════════════╗\n");
    fprintf(out, "║            ATARI DISK IMAGE INFORMATION             ║\n");
    fprintf(out, "╠══════════════════════════════════════════════════════╣\n");

    if (disk->filepath[0])
        fprintf(out, "║ Datei:        %-38s ║\n", disk->filepath);

    fprintf(out, "║ Format:       %-38s ║\n", ados_density_str(disk->density));
    fprintf(out, "║ Dateisystem:  %-38s ║\n", ados_fs_type_str(disk->fs_type));

    char size_str[40];
    snprintf(size_str, sizeof(size_str), "%u Bytes (+ %d Header)",
             disk->data_size, ATR_HEADER_SIZE);
    fprintf(out, "║ Image-Größe:  %-38s ║\n", size_str);

    snprintf(size_str, sizeof(size_str), "%d Bytes", disk->sector_size);
    fprintf(out, "║ Sektorgröße:  %-38s ║\n", size_str);

    snprintf(size_str, sizeof(size_str), "%d", disk->total_sectors);
    fprintf(out, "║ Sektoren:     %-38s ║\n", size_str);

    snprintf(size_str, sizeof(size_str), "%d Bytes (%d + 3 Link-Bytes)",
             disk->data_bytes_per_sector, disk->data_bytes_per_sector);
    fprintf(out, "║ Daten/Sektor: %-38s ║\n", size_str);

    fprintf(out, "╠══════════════════════════════════════════════════════╣\n");
    fprintf(out, "║ ATR Header:                                         ║\n");

    snprintf(size_str, sizeof(size_str), "$%04X (OK=%s)",
             disk->header.magic, disk->header.magic == ATR_MAGIC ? "ja" : "NEIN");
    fprintf(out, "║   Magic:      %-38s ║\n", size_str);

    if (disk->header.flags) {
        snprintf(size_str, sizeof(size_str), "$%02X%s%s",
                 disk->header.flags,
                 (disk->header.flags & ATR_FLAG_COPY_PROTECTED) ? " [Kopierschutz]" : "",
                 (disk->header.flags & ATR_FLAG_WRITE_PROTECTED) ? " [Schreibschutz]" : "");
        fprintf(out, "║   Flags:      %-38s ║\n", size_str);
    }

    if (disk->fs_type != FS_SPARTADOS && disk->fs_type != FS_UNKNOWN) {
        fprintf(out, "╠══════════════════════════════════════════════════════╣\n");
        fprintf(out, "║ VTOC:                                               ║\n");

        snprintf(size_str, sizeof(size_str), "%d", disk->vtoc.dos_code);
        fprintf(out, "║   DOS Code:   %-38s ║\n", size_str);

        snprintf(size_str, sizeof(size_str), "%d / %d",
                 disk->vtoc.free_sectors, disk->vtoc.total_sectors);
        fprintf(out, "║   Frei/Total: %-38s ║\n", size_str);

        uint32_t free_bytes = dos2_free_space(disk);
        snprintf(size_str, sizeof(size_str), "%u Bytes (%.1f KB)",
                 free_bytes, free_bytes / 1024.0);
        fprintf(out, "║   Frei:       %-38s ║\n", size_str);

        if (disk->vtoc.has_vtoc2) {
            snprintf(size_str, sizeof(size_str), "Ja (Sektor 1024, %d frei >719)",
                     disk->vtoc.free_sectors_above_719);
            fprintf(out, "║   VTOC2:      %-38s ║\n", size_str);
        }

        fprintf(out, "╠══════════════════════════════════════════════════════╣\n");

        snprintf(size_str, sizeof(size_str), "BLDISP=$%02X (%s)",
                 disk->boot.bldisp,
                 disk->boot.bldisp == BLDISP_SD ? "Single Density" :
                 disk->boot.bldisp == BLDISP_DD ? "Double Density" : "?");
        fprintf(out, "║ Boot:         %-38s ║\n", size_str);

        snprintf(size_str, sizeof(size_str), "$%04X, Init=$%04X",
                 disk->boot.load_address, disk->boot.init_address);
        fprintf(out, "║   Load:       %-38s ║\n", size_str);
    }

    if (disk->fs_type == FS_SPARTADOS) {
        fprintf(out, "╠══════════════════════════════════════════════════════╣\n");
        fprintf(out, "║ SpartaDOS:                                          ║\n");

        if (disk->sparta.volume_name[0]) {
            snprintf(size_str, sizeof(size_str), "\"%s\"",
                     disk->sparta.volume_name);
            fprintf(out, "║   Volume:     %-38s ║\n", size_str);
        }

        snprintf(size_str, sizeof(size_str), "%d / %d",
                 disk->sparta.free_sectors, disk->sparta.total_sectors);
        fprintf(out, "║   Frei/Total: %-38s ║\n", size_str);

        snprintf(size_str, sizeof(size_str), "Sektor %d",
                 disk->sparta.root_dir_sector);
        fprintf(out, "║   Root-Dir:   %-38s ║\n", size_str);
    }

    fprintf(out, "╚══════════════════════════════════════════════════════╝\n");
}


void ados_print_directory(const atari_disk_t *disk, FILE *out)
{
    if (!disk || !out) return;

    if (disk->fs_type == FS_SPARTADOS) {
        /* SpartaDOS Directory */
        sparta_dir_entry_t entries[128];
        uint32_t count = 0;

        if (sparta_read_directory((atari_disk_t *)disk,
                                   disk->sparta.root_dir_sector,
                                   entries, &count, 128) != ATARI_OK) {
            fprintf(out, "Fehler beim Lesen des SpartaDOS Directories\n");
            return;
        }

        fprintf(out, " %-8s %-3s  %8s  %10s  %s\n",
                "Name", "Ext", "Größe", "Datum", "Flags");
        fprintf(out, " %-8s %-3s  %8s  %10s  %s\n",
                "--------", "---", "--------", "----------", "-----");

        for (uint32_t i = 0; i < count; i++) {
            sparta_dir_entry_t *e = &entries[i];
            if (e->is_deleted) continue;

            char flags[16] = "";
            if (e->is_subdir) strcat(flags, "D");
            if (e->is_locked) strcat(flags, "L");
            if (e->is_hidden) strcat(flags, "H");

            fprintf(out, " %-8s %-3s  %8u  %02d.%02d.%02d    %s\n",
                    e->filename, e->extension, e->file_size,
                    e->date_day, e->date_month, e->date_year,
                    flags);
        }

        fprintf(out, "\n %u Dateien, %u Bytes frei\n",
                count, sparta_free_space(disk));
        return;
    }

    /* DOS 2.0/2.5/MyDOS Directory */
    fprintf(out, "\n");

    int file_count = 0;
    uint16_t total_sectors_used = 0;

    for (int i = 0; i < MAX_FILES; i++) {
        const atari_dir_entry_t *e = &disk->directory[i];

        if (e->status == DIR_FLAG_NEVER_USED)
            break;

        if (e->is_deleted)
            continue;

        if (!e->is_valid)
            continue;

        char full_name[16];
        dos2_format_filename(e, full_name, sizeof(full_name));

        /* Atari DOS Style Listing */
        fprintf(out, " %c%c %3d %s\n",
                e->is_locked ? '*' : ' ',
                e->is_open ? 'O' : ' ',
                e->sector_count,
                full_name);

        file_count++;
        total_sectors_used += e->sector_count;
    }

    fprintf(out, "\n %d Dateien, %d Sektoren belegt, %d frei\n",
            file_count, total_sectors_used, disk->vtoc.free_sectors);
}


void ados_print_vtoc_map(const atari_disk_t *disk, FILE *out)
{
    if (!disk || !out) return;

    fprintf(out, "\nVTOC Sektor-Map (. = frei, # = belegt, S = System):\n\n");

    uint16_t max_s = disk->total_sectors;
    if (max_s > 1040) max_s = 1040;

    /* Header */
    fprintf(out, "     ");
    for (int i = 0; i < 40; i++)
        fprintf(out, "%d", i % 10);
    fprintf(out, "\n");

    for (uint16_t row = 0; row <= max_s / 40; row++) {
        fprintf(out, " %3d ", row * 40);

        for (int col = 0; col < 40; col++) {
            uint16_t sector = row * 40 + col;
            if (sector > max_s) {
                fprintf(out, " ");
                continue;
            }

            if (sector == 0) {
                fprintf(out, "x");  /* Existiert nicht */
            } else if (sector <= BOOT_SECTOR_COUNT) {
                fprintf(out, "B");  /* Boot */
            } else if (sector == VTOC_SECTOR) {
                fprintf(out, "V");  /* VTOC */
            } else if (sector >= DIR_SECTOR_START && sector <= DIR_SECTOR_END) {
                fprintf(out, "D");  /* Directory */
            } else if (sector == VTOC2_SECTOR && disk->vtoc.has_vtoc2) {
                fprintf(out, "V");  /* VTOC2 */
            } else if (dos2_is_sector_free(disk, sector)) {
                fprintf(out, ".");
            } else {
                fprintf(out, "#");
            }
        }
        fprintf(out, "\n");
    }

    fprintf(out, "\n Legende: B=Boot, V=VTOC, D=Directory, "
                 "#=Daten, .=Frei, x=Nicht vorhanden\n");
}


void ados_hex_dump_sector(const atari_disk_t *disk, uint16_t sector,
                           FILE *out)
{
    if (!disk || !out) return;

    uint8_t buf[SECTOR_SIZE_QD];
    uint16_t br;

    if (ados_atr_read_sector(disk, sector, buf, &br) != ATARI_OK) {
        fprintf(out, "Fehler: Sektor %d nicht lesbar\n", sector);
        return;
    }

    fprintf(out, "\nSektor %d ($%03X), %d Bytes:\n\n", sector, sector, br);

    for (uint16_t offset = 0; offset < br; offset += 16) {
        fprintf(out, "  %04X: ", offset);

        /* Hex */
        for (int i = 0; i < 16 && (offset + i) < br; i++) {
            fprintf(out, "%02X ", buf[offset + i]);
            if (i == 7) fprintf(out, " ");
        }

        /* Padding */
        uint16_t remaining = br - offset;
        if (remaining < 16) {
            for (int i = remaining; i < 16; i++) {
                fprintf(out, "   ");
                if (i == 7) fprintf(out, " ");
            }
        }

        fprintf(out, " |");

        /* ASCII */
        for (int i = 0; i < 16 && (offset + i) < br; i++) {
            uint8_t c = buf[offset + i];
            /* Atari ATASCII: 0x20-0x7E druckbar */
            if (c >= 0x20 && c <= 0x7E)
                fprintf(out, "%c", c);
            else
                fprintf(out, ".");
        }

        fprintf(out, "|\n");
    }

    /* Sektor-Link Interpretation (wenn DOS 2.0/2.5) */
    if (disk->fs_type != FS_SPARTADOS && disk->fs_type != FS_UNKNOWN) {
        sector_link_t link = dos2_parse_sector_link(buf, disk->sector_size);

        fprintf(out, "\n  Link-Bytes: File#=%d, Next=$%03X (%d), "
                     "Count=%d, Short=%s\n",
                link.file_number, link.next_sector, link.next_sector,
                link.byte_count, link.is_short_sector ? "ja" : "nein");
    }
}

