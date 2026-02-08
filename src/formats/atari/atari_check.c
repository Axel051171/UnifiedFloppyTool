/**
 * =============================================================================
 * Atari DOS Filesystem Checker
 * =============================================================================
 *
 * Prüft und repariert Atari DOS 2.0/2.5/MyDOS Dateisysteme:
 *
 * 1. VTOC-Prüfung:
 *    - DOS-Code korrekt?
 *    - Freie Sektoren vs. tatsächlich freie
 *    - Bitmap-Konsistenz
 *
 * 2. Directory-Prüfung:
 *    - Gültige Status-Flags?
 *    - Sektorzählung plausibel?
 *    - Erster Sektor gültig?
 *    - Dateinamen gültig?
 *
 * 3. Sektor-Ketten:
 *    - Dateinummer konsistent?
 *    - Kette endet ordentlich (next_sector=0)?
 *    - Byte-Count plausibel?
 *    - Sektoranzahl stimmt mit Directory überein?
 *
 * 4. Cross-Links:
 *    - Kein Sektor von mehreren Dateien verwendet?
 *
 * 5. Verlorene Sektoren:
 *    - In VTOC belegt, aber keiner Datei zugeordnet
 * =============================================================================
 */

#include "uft/formats/atari_dos.h"
#include <stdarg.h>

/* ---- Checker Helpers ---- */

static void add_issue(check_result_t *result, check_severity_t sev,
                      uint16_t sector, uint8_t file_idx,
                      const char *fmt, ...)
{
    if (!result) return;

    /* Kapazität erweitern wenn nötig */
    if (result->issue_count >= result->issue_capacity) {
        uint32_t new_cap = result->issue_capacity ? result->issue_capacity * 2 : 64;
        check_issue_t *new_issues = (check_issue_t *)realloc(
            result->issues, new_cap * sizeof(check_issue_t));
        if (!new_issues) return;
        result->issues = new_issues;
        result->issue_capacity = new_cap;
    }

    check_issue_t *issue = &result->issues[result->issue_count];
    issue->severity = sev;
    issue->sector = sector;
    issue->file_index = file_idx;

    va_list args;
    va_start(args, fmt);
    vsnprintf(issue->message, sizeof(issue->message), fmt, args);
    va_end(args);

    result->issue_count++;

    switch (sev) {
    case CHECK_ERROR:   result->errors++; break;
    case CHECK_WARNING: result->warnings++; break;
    case CHECK_FIXED:   result->fixed++; break;
    default: break;
    }
}


/* ---- Checker API ---- */

check_result_t *check_create(void)
{
    check_result_t *result = (check_result_t *)calloc(1, sizeof(check_result_t));
    if (result) {
        result->is_valid = true;
    }
    return result;
}

void check_free(check_result_t *result)
{
    if (!result) return;
    if (result->issues) free(result->issues);
    free(result);
}


/* ---- VTOC-Prüfung ---- */

atari_error_t check_vtoc(atari_disk_t *disk, check_result_t *result, bool fix)
{
    if (!disk || !result)
        return ATARI_ERR_NULL_PTR;

    add_issue(result, CHECK_INFO, VTOC_SECTOR, 0xFF,
              "VTOC-Prüfung: DOS Code=%d, Total=%d, Frei=%d",
              disk->vtoc.dos_code, disk->vtoc.total_sectors,
              disk->vtoc.free_sectors);

    /* DOS Code prüfen */
    if (disk->vtoc.dos_code != 2) {
        add_issue(result, CHECK_WARNING, VTOC_SECTOR, 0xFF,
                  "Unerwarteter DOS-Code: %d (erwartet: 2)",
                  disk->vtoc.dos_code);
    }

    /* Freie Sektoren tatsächlich zählen */
    uint16_t actual_free = 0;
    uint16_t max_sector = (disk->total_sectors < 720) ?
                           disk->total_sectors : 719;

    for (uint16_t s = 1; s <= max_sector; s++) {
        /* System-Sektoren überspringen */
        if (s >= VTOC_SECTOR && s <= DIR_SECTOR_END) continue;
        if (s <= BOOT_SECTOR_COUNT) continue;
        if (dos2_is_sector_free(disk, s))
            actual_free++;
    }

    if (actual_free != disk->vtoc.free_sectors) {
        add_issue(result, CHECK_ERROR, VTOC_SECTOR, 0xFF,
                  "VTOC Freie-Sektoren-Zählung falsch: "
                  "VTOC sagt %d, tatsächlich %d",
                  disk->vtoc.free_sectors, actual_free);
        result->is_valid = false;

        if (fix) {
            disk->vtoc.free_sectors = actual_free;
            dos2_write_vtoc(disk);
            add_issue(result, CHECK_FIXED, VTOC_SECTOR, 0xFF,
                      "VTOC Freie-Sektoren auf %d korrigiert", actual_free);
        }
    }

    /* Sektor 0 muss als belegt markiert sein */
    if (dos2_is_sector_free(disk, 0)) {
        add_issue(result, CHECK_ERROR, 0, 0xFF,
                  "Sektor 0 als frei markiert (existiert nicht!)");
        result->is_valid = false;

        if (fix) {
            dos2_alloc_sector(disk, 0);
            disk->vtoc.free_sectors++;  /* Korrektur: alloc hat decrementiert */
            dos2_write_vtoc(disk);
            add_issue(result, CHECK_FIXED, 0, 0xFF,
                      "Sektor 0 als belegt markiert");
        }
    }

    /* Boot-Sektoren müssen belegt sein */
    for (int s = 1; s <= BOOT_SECTOR_COUNT; s++) {
        if (dos2_is_sector_free(disk, s)) {
            add_issue(result, CHECK_WARNING, s, 0xFF,
                      "Boot-Sektor %d als frei markiert", s);
        }
    }

    /* System-Sektoren (VTOC, Directory) müssen belegt sein */
    if (dos2_is_sector_free(disk, VTOC_SECTOR)) {
        add_issue(result, CHECK_ERROR, VTOC_SECTOR, 0xFF,
                  "VTOC-Sektor 360 als frei markiert");
        result->is_valid = false;
    }

    for (int s = DIR_SECTOR_START; s <= DIR_SECTOR_END; s++) {
        if (dos2_is_sector_free(disk, s)) {
            add_issue(result, CHECK_ERROR, s, 0xFF,
                      "Directory-Sektor %d als frei markiert", s);
            result->is_valid = false;
        }
    }

    /* DOS 2.5: VTOC2 prüfen */
    if (disk->vtoc.has_vtoc2) {
        uint16_t actual_free2 = 0;
        for (uint16_t s = 720; s < 1024; s++) {
            if (s == VTOC2_SECTOR) continue;
            if (dos2_is_sector_free(disk, s))
                actual_free2++;
        }

        if (actual_free2 != disk->vtoc.free_sectors_above_719) {
            add_issue(result, CHECK_ERROR, VTOC2_SECTOR, 0xFF,
                      "VTOC2 Freie-Sektoren falsch: VTOC2 sagt %d, "
                      "tatsächlich %d",
                      disk->vtoc.free_sectors_above_719, actual_free2);
            result->is_valid = false;

            if (fix) {
                disk->vtoc.free_sectors_above_719 = actual_free2;
                dos2_write_vtoc(disk);
                add_issue(result, CHECK_FIXED, VTOC2_SECTOR, 0xFF,
                          "VTOC2 Freie-Sektoren auf %d korrigiert",
                          actual_free2);
            }
        }
    }

    return ATARI_OK;
}


/* ---- Directory-Prüfung ---- */

atari_error_t check_directory(atari_disk_t *disk, check_result_t *result,
                              bool fix)
{
    if (!disk || !result)
        return ATARI_ERR_NULL_PTR;

    (void)fix;  /* Keine automatische Reparatur im Directory */

    bool found_end = false;
    int active_files = 0;
    int deleted_files = 0;

    for (int i = 0; i < MAX_FILES; i++) {
        atari_dir_entry_t *entry = &disk->directory[i];

        if (entry->status == DIR_FLAG_NEVER_USED) {
            if (!found_end) found_end = true;
            continue;
        }

        /* Einträge nach dem "nie benutzt" Marker */
        if (found_end && entry->status != DIR_FLAG_NEVER_USED) {
            add_issue(result, CHECK_WARNING, 0, i,
                      "Eintrag #%d nach Ende-Marker (Status=$%02X)",
                      i, entry->status);
        }

        if (entry->is_deleted) {
            deleted_files++;
            continue;
        }

        if (!entry->is_valid)
            continue;

        active_files++;

        /* Dateiname prüfen */
        if (entry->filename[0] == '\0' || entry->filename[0] == ' ') {
            add_issue(result, CHECK_ERROR, 0, i,
                      "Datei #%d: Leerer Dateiname", i);
            result->is_valid = false;
        }

        /* Erster Sektor prüfen */
        if (entry->first_sector == 0) {
            add_issue(result, CHECK_ERROR, 0, i,
                      "Datei #%d (%s): Erster Sektor ist 0",
                      i, entry->filename);
            result->is_valid = false;
        } else if (entry->first_sector > disk->total_sectors) {
            add_issue(result, CHECK_ERROR, entry->first_sector, i,
                      "Datei #%d (%s): Erster Sektor %d > max %d",
                      i, entry->filename, entry->first_sector,
                      disk->total_sectors);
            result->is_valid = false;
        }

        /* System-Sektor als Daten? */
        if (entry->first_sector >= VTOC_SECTOR &&
            entry->first_sector <= DIR_SECTOR_END) {
            add_issue(result, CHECK_ERROR, entry->first_sector, i,
                      "Datei #%d (%s): Erster Sektor %d ist System-Sektor!",
                      i, entry->filename, entry->first_sector);
            result->is_valid = false;
        }

        /* Sektorzahl prüfen */
        if (entry->sector_count == 0) {
            add_issue(result, CHECK_WARNING, 0, i,
                      "Datei #%d (%s): Sektorzählung ist 0",
                      i, entry->filename);
        }

        /* Open-for-output Flag */
        if (entry->is_open) {
            add_issue(result, CHECK_WARNING, 0, i,
                      "Datei #%d (%s): Noch als 'geöffnet' markiert",
                      i, entry->filename);
        }
    }

    add_issue(result, CHECK_INFO, 0, 0xFF,
              "Directory: %d aktive Dateien, %d gelöschte",
              active_files, deleted_files);

    return ATARI_OK;
}


/* ---- Sektor-Ketten prüfen ---- */

atari_error_t check_sector_chains(atari_disk_t *disk, check_result_t *result,
                                  bool fix)
{
    if (!disk || !result)
        return ATARI_ERR_NULL_PTR;

    for (int i = 0; i < MAX_FILES; i++) {
        atari_dir_entry_t *entry = &disk->directory[i];

        if (entry->status == DIR_FLAG_NEVER_USED) break;
        if (!entry->is_valid || entry->is_deleted) continue;
        if (entry->first_sector == 0) continue;

        /* Sektor-Kette verfolgen */
        uint16_t current = entry->first_sector;
        uint16_t sectors_counted = 0;
        uint16_t max_chain = entry->sector_count + 100;
        bool chain_ok = true;

        while (current != 0 && sectors_counted < max_chain) {
            /* Sektor-Nummer validieren */
            if (current > disk->total_sectors) {
                add_issue(result, CHECK_ERROR, current, i,
                          "Datei #%d (%s): Sektor %d außerhalb der Disk",
                          i, entry->filename, current);
                chain_ok = false;
                result->is_valid = false;
                break;
            }

            uint8_t sector_buf[SECTOR_SIZE_QD];
            uint16_t br;
            atari_error_t err = ados_atr_read_sector(disk, current,
                                                 sector_buf, &br);
            if (err != ATARI_OK) {
                add_issue(result, CHECK_ERROR, current, i,
                          "Datei #%d (%s): Sektor %d nicht lesbar",
                          i, entry->filename, current);
                chain_ok = false;
                result->is_valid = false;
                break;
            }

            sector_link_t link = dos2_parse_sector_link(sector_buf,
                                                         disk->sector_size);

            /* Dateinummer prüfen */
            if (link.file_number != i) {
                add_issue(result, CHECK_ERROR, current, i,
                          "Datei #%d (%s): Sektor %d hat File-Nr %d "
                          "(erwartet %d)",
                          i, entry->filename, current,
                          link.file_number, i);
                result->is_valid = false;

                if (fix) {
                    link.file_number = i;
                    dos2_write_sector_link(sector_buf, disk->sector_size,
                                           &link);
                    ados_atr_write_sector(disk, current, sector_buf,
                                     disk->sector_size <= SECTOR_SIZE_SD ?
                                     SECTOR_SIZE_SD : disk->sector_size);
                    add_issue(result, CHECK_FIXED, current, i,
                              "File-Nr in Sektor %d korrigiert", current);
                }
            }

            /* Byte-Count prüfen */
            uint16_t max_data = disk->data_bytes_per_sector;

            if (link.byte_count > max_data) {
                add_issue(result, CHECK_ERROR, current, i,
                          "Datei #%d (%s): Sektor %d Byte-Count %d > max %d",
                          i, entry->filename, current,
                          link.byte_count, max_data);
                result->is_valid = false;
            }

            /* Nicht-letzter Sektor sollte voll sein */
            if (link.next_sector != 0 && link.byte_count != max_data) {
                add_issue(result, CHECK_WARNING, current, i,
                          "Datei #%d (%s): Sektor %d nicht voll "
                          "(%d/%d Bytes) aber nicht letzter",
                          i, entry->filename, current,
                          link.byte_count, max_data);
            }

            sectors_counted++;
            current = link.next_sector;
        }

        /* Endlosschleife erkannt */
        if (sectors_counted >= max_chain && current != 0) {
            add_issue(result, CHECK_ERROR, current, i,
                      "Datei #%d (%s): Mögliche Endlosschleife in "
                      "Sektor-Kette (> %d Sektoren)",
                      i, entry->filename, max_chain);
            chain_ok = false;
            result->is_valid = false;
        }

        /* Sektorzählung vergleichen */
        if (chain_ok && sectors_counted != entry->sector_count) {
            add_issue(result, CHECK_ERROR, 0, i,
                      "Datei #%d (%s): Directory sagt %d Sektoren, "
                      "Kette hat %d",
                      i, entry->filename, entry->sector_count,
                      sectors_counted);
            result->is_valid = false;

            if (fix) {
                disk->directory[i].sector_count = sectors_counted;
                dos2_write_directory(disk);
                add_issue(result, CHECK_FIXED, 0, i,
                          "Sektorzählung für Datei #%d auf %d korrigiert",
                          i, sectors_counted);
            }
        }
    }

    return ATARI_OK;
}


/* ---- Cross-Link Erkennung ---- */

atari_error_t check_cross_links(atari_disk_t *disk, check_result_t *result)
{
    if (!disk || !result)
        return ATARI_ERR_NULL_PTR;

    /* Sektor-Zuordnungs-Map: Für jeden Sektor die Dateinummer (-1 = frei) */
    int16_t *sector_owner = (int16_t *)malloc(
        (disk->total_sectors + 1) * sizeof(int16_t));
    if (!sector_owner)
        return ATARI_ERR_ALLOC_FAILED;

    memset(sector_owner, 0xFF, (disk->total_sectors + 1) * sizeof(int16_t));

    /* System-Sektoren markieren */
    for (int s = 0; s <= BOOT_SECTOR_COUNT; s++)
        sector_owner[s] = -2;  /* System */
    sector_owner[VTOC_SECTOR] = -2;
    for (int s = DIR_SECTOR_START; s <= DIR_SECTOR_END; s++)
        sector_owner[s] = -2;
    if (disk->vtoc.has_vtoc2)
        sector_owner[VTOC2_SECTOR] = -2;

    /* Alle Datei-Ketten durchlaufen */
    for (int i = 0; i < MAX_FILES; i++) {
        atari_dir_entry_t *entry = &disk->directory[i];

        if (entry->status == DIR_FLAG_NEVER_USED) break;
        if (!entry->is_valid || entry->is_deleted) continue;

        uint16_t current = entry->first_sector;
        uint16_t count = 0;
        uint16_t max = entry->sector_count + 100;

        while (current != 0 && count < max) {
            if (current > disk->total_sectors) break;

            if (sector_owner[current] >= 0) {
                add_issue(result, CHECK_ERROR, current, i,
                          "CROSS-LINK: Sektor %d wird von Datei #%d (%s) "
                          "und Datei #%d (%s) verwendet!",
                          current, sector_owner[current],
                          disk->directory[sector_owner[current]].filename,
                          i, entry->filename);
                result->is_valid = false;
                break;
            }
            if (sector_owner[current] == -2) {
                add_issue(result, CHECK_ERROR, current, i,
                          "Datei #%d (%s) verwendet System-Sektor %d!",
                          i, entry->filename, current);
                result->is_valid = false;
                break;
            }

            sector_owner[current] = i;

            uint8_t sector_buf[SECTOR_SIZE_QD];
            uint16_t br;
            if (ados_atr_read_sector(disk, current, sector_buf, &br) != ATARI_OK)
                break;

            sector_link_t link = dos2_parse_sector_link(sector_buf,
                                                         disk->sector_size);
            current = link.next_sector;
            count++;
        }
    }

    free(sector_owner);
    return ATARI_OK;
}


/* ---- Verlorene Sektoren ---- */

atari_error_t check_lost_sectors(atari_disk_t *disk, check_result_t *result,
                                 bool fix)
{
    if (!disk || !result)
        return ATARI_ERR_NULL_PTR;

    /* Bitmap der tatsächlich belegten Sektoren erstellen */
    uint16_t max_s = (disk->total_sectors < 1024) ?
                      disk->total_sectors : 1023;

    uint8_t *used_by_files = (uint8_t *)calloc(1, max_s + 1);
    if (!used_by_files)
        return ATARI_ERR_ALLOC_FAILED;

    /* System-Sektoren */
    for (int s = 0; s <= BOOT_SECTOR_COUNT; s++)
        used_by_files[s] = 1;
    used_by_files[VTOC_SECTOR] = 1;
    for (int s = DIR_SECTOR_START; s <= DIR_SECTOR_END; s++)
        used_by_files[s] = 1;
    if (disk->vtoc.has_vtoc2 && VTOC2_SECTOR <= max_s)
        used_by_files[VTOC2_SECTOR] = 1;

    /* Alle Datei-Sektoren markieren */
    for (int i = 0; i < MAX_FILES; i++) {
        atari_dir_entry_t *entry = &disk->directory[i];
        if (entry->status == DIR_FLAG_NEVER_USED) break;
        if (!entry->is_valid || entry->is_deleted) continue;

        uint16_t current = entry->first_sector;
        uint16_t count = 0;

        while (current != 0 && current <= max_s && count < 1024) {
            used_by_files[current] = 1;

            uint8_t sector_buf[SECTOR_SIZE_QD];
            uint16_t br;
            if (ados_atr_read_sector(disk, current, sector_buf, &br) != ATARI_OK)
                break;

            sector_link_t link = dos2_parse_sector_link(sector_buf,
                                                         disk->sector_size);
            current = link.next_sector;
            count++;
        }
    }

    /* Vergleich: VTOC sagt belegt, aber keine Datei nutzt den Sektor */
    uint16_t lost_count = 0;

    for (uint16_t s = 1; s <= max_s; s++) {
        if (!dos2_is_sector_free(disk, s) && !used_by_files[s]) {
            lost_count++;
            if (lost_count <= 10) {
                add_issue(result, CHECK_WARNING, s, 0xFF,
                          "Verlorener Sektor: %d (belegt in VTOC, "
                          "aber keiner Datei zugeordnet)", s);
            }
        }
    }

    if (lost_count > 10) {
        add_issue(result, CHECK_WARNING, 0, 0xFF,
                  "... und %d weitere verlorene Sektoren",
                  lost_count - 10);
    }

    if (lost_count > 0) {
        add_issue(result, CHECK_WARNING, 0, 0xFF,
                  "Gesamt: %d verlorene Sektoren (%d Bytes)",
                  lost_count, lost_count * disk->data_bytes_per_sector);

        if (fix) {
            for (uint16_t s = 1; s <= max_s; s++) {
                if (!dos2_is_sector_free(disk, s) && !used_by_files[s]) {
                    dos2_free_sector(disk, s);
                }
            }
            dos2_write_vtoc(disk);
            add_issue(result, CHECK_FIXED, 0, 0xFF,
                      "%d verlorene Sektoren freigegeben", lost_count);
        }
    }

    free(used_by_files);
    return ATARI_OK;
}


/* ---- Vollständige Prüfung ---- */

atari_error_t check_filesystem(atari_disk_t *disk, check_result_t *result,
                               bool fix)
{
    if (!disk || !result)
        return ATARI_ERR_NULL_PTR;

    if (disk->fs_type == FS_SPARTADOS) {
        add_issue(result, CHECK_INFO, 0, 0xFF,
                  "SpartaDOS Dateisystem erkannt - "
                  "Checker noch nicht vollständig implementiert");
        return ATARI_OK;
    }

    add_issue(result, CHECK_INFO, 0, 0xFF,
              "=== Dateisystem-Prüfung: %s, %s ===",
              ados_fs_type_str(disk->fs_type),
              ados_density_str(disk->density));

    /* VTOC und Directory müssen geladen sein */
    dos2_read_boot(disk);
    dos2_read_vtoc(disk);
    dos2_read_directory(disk);

    /* Phase 1: VTOC */
    add_issue(result, CHECK_INFO, 0, 0xFF, "--- Phase 1: VTOC ---");
    check_vtoc(disk, result, fix);

    /* Phase 2: Directory */
    add_issue(result, CHECK_INFO, 0, 0xFF, "--- Phase 2: Directory ---");
    check_directory(disk, result, fix);

    /* Phase 3: Sektor-Ketten */
    add_issue(result, CHECK_INFO, 0, 0xFF, "--- Phase 3: Sektor-Ketten ---");
    check_sector_chains(disk, result, fix);

    /* Phase 4: Cross-Links */
    add_issue(result, CHECK_INFO, 0, 0xFF, "--- Phase 4: Cross-Links ---");
    check_cross_links(disk, result);

    /* Phase 5: Verlorene Sektoren */
    add_issue(result, CHECK_INFO, 0, 0xFF, "--- Phase 5: Verlorene Sektoren ---");
    check_lost_sectors(disk, result, fix);

    /* Zusammenfassung */
    add_issue(result, CHECK_INFO, 0, 0xFF,
              "=== Ergebnis: %d Fehler, %d Warnungen, %d repariert ===",
              result->errors, result->warnings, result->fixed);

    return ATARI_OK;
}


/* ---- Bericht ausgeben ---- */

void check_print_report(const check_result_t *result, FILE *out)
{
    if (!result || !out) return;

    static const char *severity_str[] = {
        "INFO", "WARNUNG", "FEHLER", "REPARIERT"
    };

    static const char *severity_prefix[] = {
        "  ", "⚠ ", "✗ ", "✓ "
    };

    for (uint32_t i = 0; i < result->issue_count; i++) {
        const check_issue_t *issue = &result->issues[i];

        fprintf(out, "%s[%s] %s",
                severity_prefix[issue->severity],
                severity_str[issue->severity],
                issue->message);

        if (issue->sector != 0 && issue->severity != CHECK_INFO)
            fprintf(out, " [Sektor %d]", issue->sector);

        fprintf(out, "\n");
    }

    fprintf(out, "\n");
    if (result->is_valid)
        fprintf(out, "Dateisystem: OK\n");
    else
        fprintf(out, "Dateisystem: BESCHÄDIGT\n");
}
