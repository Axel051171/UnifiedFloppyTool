#include "cpcemu.h"

#include <assert.h>
#include <string.h>

#include "platformdef.h"

const char *CPCEMU_HEADER_STD   = "MV - CPCEMU Disk-File\r\nDisk-Info\r\n";
const char *CPCEMU_HEADER_EX    = "EXTENDED CPC DSK File\r\nDisk-Info\r\n";
const char *CPCEMU_CREATOR      = "AMS-DSK\r\n";
const char *CPCEMU_TRACK_HEADER = "Track-Info\r\n";

u8 g_sector_skew_table[NUM_SECTOR];

void read_disc_info(FILE *fp, struct cpcemu_disc_info_s *info)
{
    assert(fp);
    assert(info);

    fseek(fp, 0, SEEK_SET);
    fread(info, 1, sizeof(*info), fp);
}

void write_disc_info(FILE *fp, struct cpcemu_disc_info_s *info)
{
    assert(fp);
    assert(info);

    fseek(fp, 0, SEEK_SET);
    fwrite(info, 1, sizeof(*info), fp);
}

void read_track_info(FILE *fp, u8 track, struct cpcemu_track_info_s *track_info)
{
    int offset_track;

    assert(fp);
    assert(track_info);

    offset_track = CPCEMU_INFO_OFFSET + (track * SIZ_TRACK);

    fseek(fp, offset_track, SEEK_SET);
    fread(track_info, 1, sizeof(struct cpcemu_track_info_s), fp);
}

void write_track_info(FILE *fp, u8 track, struct cpcemu_track_info_s *track_info)
{
    int offset_track;

    assert(fp);
    assert(track_info);

    offset_track = CPCEMU_INFO_OFFSET + (track * SIZ_TRACK);

    fseek(fp, offset_track, SEEK_SET);
    fwrite(track_info, 1, sizeof(struct cpcemu_track_info_s), fp);
}

int check_disk_type(FILE *fp, u8 sector_id)
{
    struct cpcemu_track_info_s track_info;

    assert(fp);

    read_track_info(fp, 0, &track_info);

    return sector_id <= track_info.sector_info_table[0].sector_id
        && (sector_id + NUM_SECTOR) >= track_info.sector_info_table[0].sector_id; /* ?? */
}

int check_extended(FILE *fp)
{
    struct cpcemu_disc_info_s disc_info;

    assert(fp);

    read_disc_info(fp, &disc_info);

    return strncmp("EXTENDED", disc_info.header, 8) == 0;
}

#if 0
void read_physical_sector(FILE *fp, u8 track, u8 sector, u8 buffer[SIZ_SECTOR])
{
    int offset_track;
    int offset_sector;

    offset_track  = CPCEMU_INFO_OFFSET + (track * SIZ_TRACK);
    offset_sector = CPCEMU_TRACK_OFFSET + sector * SIZ_SECTOR;

    fseek(fp, offset_track + offset_sector, SEEK_SET);
    fread(buffer, 1, SIZ_SECTOR, fp);
}
#endif

void read_logical_sector(FILE *fp, u8 track, u8 sector, u8 buffer[SIZ_SECTOR])
{
    int offset_track;
    int logical_sector;
    int offset_sector;

    assert(track < NUM_TRACK);
    assert(sector < NUM_SECTOR);
    /* printf("DEBUG - read_logical_sector (track: %d, sector: %d)\n", track, sector); */

    offset_track   = CPCEMU_INFO_OFFSET + (track * SIZ_TRACK);
    logical_sector = g_sector_skew_table[sector];
    offset_sector  = CPCEMU_TRACK_OFFSET + logical_sector * SIZ_SECTOR;

    fseek(fp, offset_track + offset_sector, SEEK_SET);
    fread(buffer, 1, SIZ_SECTOR, fp);
}

void write_logical_sector(FILE *fp, u8 track, u8 sector, u8 buffer[SIZ_SECTOR])
{
    int offset_track;
    int logical_sector;
    int offset_sector;

    assert(track < NUM_TRACK);
    assert(sector < NUM_SECTOR);
    /* printf("DEBUG - write_logical_sector (track: %d, sector: %d)\n", track, sector); */

    offset_track   = CPCEMU_INFO_OFFSET + (track * SIZ_TRACK);
    logical_sector = g_sector_skew_table[sector];
    offset_sector  = CPCEMU_TRACK_OFFSET + logical_sector * SIZ_SECTOR;

    fseek(fp, offset_track + offset_sector, SEEK_SET);
    fwrite(buffer, 1, SIZ_SECTOR, fp);
}
