/**
 * @file uft_linux_fdc.c
 * @brief Native Linux FDC Implementation
 * 
 * Uses Linux floppy driver FDRAWCMD ioctl for direct FDC access.
 * 
 * @copyright MIT License
 */

#ifdef __linux__

#include "uft/hal/uft_linux_fdc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/fd.h>
#include <linux/fdreg.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * FDC Commands
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define FDC_READ_DATA       0x46    /* MFM, skip deleted */
#define FDC_READ_DELETED    0x4C    /* MFM, read deleted */
#define FDC_WRITE_DATA      0x45    /* MFM */
#define FDC_WRITE_DELETED   0x49    /* MFM, write deleted */
#define FDC_READ_TRACK      0x42    /* MFM */
#define FDC_READ_ID         0x4A    /* MFM */
#define FDC_FORMAT_TRACK    0x4D    /* MFM */
#define FDC_RECALIBRATE     0x07
#define FDC_SEEK            0x0F
#define FDC_SENSE_INT       0x08
#define FDC_SPECIFY         0x03
#define FDC_SENSE_DRIVE     0x04

/* Status register bits */
#define ST0_IC_MASK         0xC0    /* Interrupt code */
#define ST0_IC_NORMAL       0x00    /* Normal termination */
#define ST0_IC_ABNORMAL     0x40    /* Abnormal termination */
#define ST0_IC_INVALID      0x80    /* Invalid command */
#define ST0_IC_READY_CHG    0xC0    /* Ready changed */
#define ST0_SE              0x20    /* Seek end */
#define ST0_EC              0x10    /* Equipment check */

#define ST1_EN              0x80    /* End of cylinder */
#define ST1_DE              0x20    /* Data error (CRC) */
#define ST1_OR              0x10    /* Overrun */
#define ST1_ND              0x04    /* No data */
#define ST1_NW              0x02    /* Not writable */
#define ST1_MA              0x01    /* Missing address mark */

#define ST2_CM              0x40    /* Control mark (deleted) */
#define ST2_DD              0x20    /* Data error in data field */
#define ST2_WC              0x10    /* Wrong cylinder */
#define ST2_BC              0x02    /* Bad cylinder */
#define ST2_MD              0x01    /* Missing data address mark */

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal Helpers
 * ═══════════════════════════════════════════════════════════════════════════════ */

static int fdc_raw_cmd(uft_lfdc_device_t *dev, struct floppy_raw_cmd *raw) {
    raw->flags |= FD_RAW_NEED_SEEK;
    
    int ret = ioctl(dev->fd, FDRAWCMD, raw);
    if (ret < 0) {
        return UFT_LFDC_ERR_IOCTL;
    }
    
    /* Check status */
    if (raw->reply_count >= 1) {
        uint8_t st0 = raw->reply[0];
        if ((st0 & ST0_IC_MASK) == ST0_IC_ABNORMAL) {
            if (raw->reply_count >= 2) {
                uint8_t st1 = raw->reply[1];
                if (st1 & ST1_DE) return UFT_LFDC_ERR_CRC;
                if (st1 & ST1_ND) return UFT_LFDC_ERR_NO_DATA;
                if (st1 & ST1_NW) return UFT_LFDC_ERR_PROTECTED;
                if (st1 & ST1_MA) return UFT_LFDC_ERR_NO_DATA;
            }
            return UFT_LFDC_ERR_READ;
        }
        if ((st0 & ST0_IC_MASK) == ST0_IC_INVALID) {
            return UFT_LFDC_ERR_IOCTL;
        }
    }
    
    return UFT_LFDC_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Initialization
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_lfdc_params_init(uft_lfdc_params_t *params) {
    if (!params) return;
    
    params->data_rate = UFT_LFDC_RATE_500;
    params->retries = UFT_LFDC_MAX_RETRIES;
    params->ignore_errors = false;
    params->read_deleted = false;
    params->gap3 = 0;  /* Auto */
    params->seek_multiplier = 1;
}

int uft_lfdc_open(uft_lfdc_device_t *dev, const char *device) {
    if (!dev || !device) return UFT_LFDC_ERR_PARAM;
    
    memset(dev, 0, sizeof(*dev));
    strncpy(dev->device, device, sizeof(dev->device) - 1);
    uft_lfdc_params_init(&dev->params);
    
    /* Open device */
    dev->fd = open(device, O_RDWR | O_NONBLOCK);
    if (dev->fd < 0) {
        return UFT_LFDC_ERR_OPEN;
    }
    
    /* Get drive parameters */
    struct floppy_drive_params dp;
    if (ioctl(dev->fd, FDGETDRVPRM, &dp) == 0) {
        dev->drive_type = dp.cmos;
    }
    
    /* Default to 2HD */
    dev->media_type = UFT_LFDC_MEDIA_2HD;
    dev->max_cylinder = 79;
    dev->max_head = 1;
    
    /* Reset FDC */
    uft_lfdc_reset(dev);
    
    return UFT_LFDC_OK;
}

void uft_lfdc_close(uft_lfdc_device_t *dev) {
    if (!dev) return;
    
    if (dev->fd >= 0) {
        /* Turn off motor */
        ioctl(dev->fd, FDFLUSH);
        close(dev->fd);
    }
    
    memset(dev, 0, sizeof(*dev));
    dev->fd = -1;
}

bool uft_lfdc_disk_present(uft_lfdc_device_t *dev) {
    if (!dev || dev->fd < 0) return false;
    
    struct floppy_drive_struct ds;
    if (ioctl(dev->fd, FDPOLLDRVSTAT, &ds) < 0) {
        return false;
    }
    
    return (ds.flags & FD_DISK_NEWCHANGE) == 0;
}

bool uft_lfdc_write_protected(uft_lfdc_device_t *dev) {
    if (!dev || dev->fd < 0) return true;
    
    struct floppy_drive_struct ds;
    if (ioctl(dev->fd, FDGETDRVSTAT, &ds) < 0) {
        return true;
    }
    
    return (ds.flags & FD_DISK_WRITABLE) == 0;
}

int uft_lfdc_reset(uft_lfdc_device_t *dev) {
    if (!dev || dev->fd < 0) return UFT_LFDC_ERR_PARAM;
    
    int ret = ioctl(dev->fd, FDRESET, FD_RESET_ALWAYS);
    if (ret < 0) return UFT_LFDC_ERR_IOCTL;
    
    dev->current_cyl = 0;
    dev->current_head = 0;
    
    return UFT_LFDC_OK;
}

int uft_lfdc_set_media(uft_lfdc_device_t *dev, uft_lfdc_media_type_t type) {
    if (!dev) return UFT_LFDC_ERR_PARAM;
    
    dev->media_type = type;
    
    switch (type) {
    case UFT_LFDC_MEDIA_2HD:
        dev->max_cylinder = 79;
        dev->max_head = 1;
        dev->params.data_rate = UFT_LFDC_RATE_500;
        break;
    case UFT_LFDC_MEDIA_2DD:
        dev->max_cylinder = 79;
        dev->max_head = 1;
        dev->params.data_rate = UFT_LFDC_RATE_250;
        break;
    case UFT_LFDC_MEDIA_2D:
        dev->max_cylinder = 39;
        dev->max_head = 1;
        dev->params.data_rate = UFT_LFDC_RATE_250;
        break;
    case UFT_LFDC_MEDIA_1DD:
        dev->max_cylinder = 79;
        dev->max_head = 0;
        dev->params.data_rate = UFT_LFDC_RATE_250;
        break;
    case UFT_LFDC_MEDIA_1D:
        dev->max_cylinder = 39;
        dev->max_head = 0;
        dev->params.data_rate = UFT_LFDC_RATE_250;
        break;
    }
    
    return UFT_LFDC_OK;
}

int uft_lfdc_set_rate(uft_lfdc_device_t *dev, uint8_t rate) {
    if (!dev || dev->fd < 0) return UFT_LFDC_ERR_PARAM;
    
    dev->params.data_rate = rate;
    
    struct floppy_raw_cmd raw = {0};
    raw.rate = rate;
    raw.flags = FD_RAW_NEED_DISK;
    
    return UFT_LFDC_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Head Movement
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_lfdc_recalibrate(uft_lfdc_device_t *dev) {
    if (!dev || dev->fd < 0) return UFT_LFDC_ERR_PARAM;
    
    struct floppy_raw_cmd raw = {0};
    raw.flags = FD_RAW_NEED_DISK | FD_RAW_INTR;
    raw.cmd[0] = FDC_RECALIBRATE;
    raw.cmd[1] = 0;  /* Drive 0 */
    raw.cmd_count = 2;
    raw.rate = dev->params.data_rate;
    
    int ret = fdc_raw_cmd(dev, &raw);
    if (ret == UFT_LFDC_OK) {
        dev->current_cyl = 0;
    }
    
    return ret;
}

int uft_lfdc_seek(uft_lfdc_device_t *dev, uint8_t cylinder) {
    if (!dev || dev->fd < 0) return UFT_LFDC_ERR_PARAM;
    
    uint8_t phys_cyl = cylinder * dev->params.seek_multiplier;
    
    struct floppy_raw_cmd raw = {0};
    raw.flags = FD_RAW_NEED_DISK | FD_RAW_INTR;
    raw.cmd[0] = FDC_SEEK;
    raw.cmd[1] = (dev->current_head << 2) | 0;  /* Head + Drive */
    raw.cmd[2] = phys_cyl;
    raw.cmd_count = 3;
    raw.rate = dev->params.data_rate;
    raw.track = phys_cyl;
    
    int ret = fdc_raw_cmd(dev, &raw);
    if (ret == UFT_LFDC_OK) {
        dev->current_cyl = cylinder;
    }
    
    return ret;
}

int uft_lfdc_select_head(uft_lfdc_device_t *dev, uint8_t head) {
    if (!dev) return UFT_LFDC_ERR_PARAM;
    dev->current_head = head;
    return UFT_LFDC_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Read Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_lfdc_read_id(uft_lfdc_device_t *dev, uint8_t head,
                     uft_lfdc_track_info_t *info) {
    if (!dev || !info || dev->fd < 0) return UFT_LFDC_ERR_PARAM;
    
    memset(info, 0, sizeof(*info));
    info->cylinder = dev->current_cyl;
    info->head = head;
    
    uint8_t phys_cyl = dev->current_cyl * dev->params.seek_multiplier;
    
    /* Read multiple IDs to find all sectors */
    int found = 0;
    uint8_t seen[UFT_LFDC_MAX_SECTORS] = {0};
    
    for (int attempt = 0; attempt < 32 && found < UFT_LFDC_MAX_SECTORS; attempt++) {
        struct floppy_raw_cmd raw = {0};
        raw.flags = FD_RAW_NEED_DISK | FD_RAW_INTR;
        raw.cmd[0] = FDC_READ_ID;
        raw.cmd[1] = (head << 2) | 0;  /* Head + Drive */
        raw.cmd_count = 2;
        raw.rate = dev->params.data_rate;
        raw.track = phys_cyl;
        
        if (fdc_raw_cmd(dev, &raw) != UFT_LFDC_OK) {
            continue;
        }
        
        if (raw.reply_count >= 7) {
            uint8_t c = raw.reply[3];
            uint8_t h = raw.reply[4];
            uint8_t r = raw.reply[5];
            uint8_t n = raw.reply[6];
            
            /* Check if we've seen this sector */
            if (r > 0 && r <= UFT_LFDC_MAX_SECTORS && !seen[r-1]) {
                seen[r-1] = 1;
                info->sectors[found].cylinder = c;
                info->sectors[found].head = h;
                info->sectors[found].sector = r;
                info->sectors[found].size_code = n;
                found++;
            }
        }
    }
    
    if (found > 0) {
        info->sector_count = found;
        info->sector_size_code = info->sectors[0].size_code;
        info->sector_size = uft_lfdc_sector_size(info->sector_size_code);
        info->sectors_valid = true;
    }
    
    return found;
}

int uft_lfdc_read_sector(uft_lfdc_device_t *dev,
                         uint8_t cyl, uint8_t head, uint8_t sector,
                         uint8_t size_code, uint8_t *data, uint16_t *actual_size) {
    if (!dev || !data || dev->fd < 0) return UFT_LFDC_ERR_PARAM;
    
    uint16_t sector_size = uft_lfdc_sector_size(size_code);
    uint8_t phys_cyl = cyl * dev->params.seek_multiplier;
    
    /* Seek if needed */
    if (dev->current_cyl != cyl) {
        int ret = uft_lfdc_seek(dev, cyl);
        if (ret != UFT_LFDC_OK) return ret;
    }
    
    int last_error = UFT_LFDC_ERR_READ;
    
    for (int retry = 0; retry <= dev->params.retries; retry++) {
        struct floppy_raw_cmd raw = {0};
        raw.flags = FD_RAW_NEED_DISK | FD_RAW_READ | FD_RAW_INTR;
        raw.cmd[0] = dev->params.read_deleted ? FDC_READ_DELETED : FDC_READ_DATA;
        raw.cmd[1] = (head << 2) | 0;
        raw.cmd[2] = cyl;           /* C */
        raw.cmd[3] = head;          /* H */
        raw.cmd[4] = sector;        /* R */
        raw.cmd[5] = size_code;     /* N */
        raw.cmd[6] = sector;        /* EOT (end of track = same sector for single) */
        raw.cmd[7] = dev->params.gap3 ? dev->params.gap3 : 0x1B;  /* GPL */
        raw.cmd[8] = 0xFF;          /* DTL */
        raw.cmd_count = 9;
        raw.data = data;
        raw.length = sector_size;
        raw.rate = dev->params.data_rate;
        raw.track = phys_cyl;
        
        int ret = fdc_raw_cmd(dev, &raw);
        
        if (ret == UFT_LFDC_OK) {
            if (actual_size) *actual_size = sector_size;
            dev->sectors_read++;
            return UFT_LFDC_OK;
        }
        
        last_error = ret;
        dev->retries_used++;
    }
    
    dev->errors++;
    return last_error;
}

int uft_lfdc_read_track(uft_lfdc_device_t *dev,
                        uint8_t cyl, uint8_t head,
                        uft_lfdc_sector_data_t *sectors, int max_sectors) {
    if (!dev || !sectors) return UFT_LFDC_ERR_PARAM;
    
    /* First, read sector IDs */
    uft_lfdc_track_info_t info;
    int sector_count = uft_lfdc_read_id(dev, head, &info);
    if (sector_count <= 0) return sector_count;
    
    if (sector_count > max_sectors) sector_count = max_sectors;
    
    int read_count = 0;
    
    for (int i = 0; i < sector_count; i++) {
        sectors[i].id = info.sectors[i];
        sectors[i].size = uft_lfdc_sector_size(info.sectors[i].size_code);
        sectors[i].data = (uint8_t *)malloc(sectors[i].size);
        sectors[i].deleted = false;
        sectors[i].crc_error = false;
        
        if (!sectors[i].data) continue;
        
        uint16_t actual;
        int ret = uft_lfdc_read_sector(dev, cyl, head,
                                       info.sectors[i].sector,
                                       info.sectors[i].size_code,
                                       sectors[i].data, &actual);
        
        if (ret == UFT_LFDC_OK) {
            read_count++;
        } else if (ret == UFT_LFDC_ERR_CRC) {
            sectors[i].crc_error = true;
            read_count++;  /* Still count it */
        } else if (!dev->params.ignore_errors) {
            /* Free allocated data on error */
            free(sectors[i].data);
            sectors[i].data = NULL;
        }
    }
    
    return read_count;
}

int uft_lfdc_read_raw_track(uft_lfdc_device_t *dev,
                            uint8_t cyl, uint8_t head,
                            uint8_t *data, size_t data_size, size_t *actual_size) {
    if (!dev || !data || dev->fd < 0) return UFT_LFDC_ERR_PARAM;
    
    uint8_t phys_cyl = cyl * dev->params.seek_multiplier;
    
    if (dev->current_cyl != cyl) {
        int ret = uft_lfdc_seek(dev, cyl);
        if (ret != UFT_LFDC_OK) return ret;
    }
    
    struct floppy_raw_cmd raw = {0};
    raw.flags = FD_RAW_NEED_DISK | FD_RAW_READ | FD_RAW_INTR;
    raw.cmd[0] = FDC_READ_TRACK;
    raw.cmd[1] = (head << 2) | 0;
    raw.cmd[2] = cyl;
    raw.cmd[3] = head;
    raw.cmd[4] = 1;             /* Sector 1 */
    raw.cmd[5] = 2;             /* N=2 (512 bytes) - wird ignoriert */
    raw.cmd[6] = 18;            /* EOT - Sektoren pro Track */
    raw.cmd[7] = 0x1B;          /* GPL */
    raw.cmd[8] = 0xFF;          /* DTL */
    raw.cmd_count = 9;
    raw.data = data;
    raw.length = data_size;
    raw.rate = dev->params.data_rate;
    raw.track = phys_cyl;
    
    int ret = fdc_raw_cmd(dev, &raw);
    
    if (ret == UFT_LFDC_OK && actual_size) {
        *actual_size = raw.length;
    }
    
    return ret;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Write Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_lfdc_write_sector(uft_lfdc_device_t *dev,
                          uint8_t cyl, uint8_t head, uint8_t sector,
                          uint8_t size_code, const uint8_t *data, uint16_t size) {
    if (!dev || !data || dev->fd < 0) return UFT_LFDC_ERR_PARAM;
    
    if (uft_lfdc_write_protected(dev)) {
        return UFT_LFDC_ERR_PROTECTED;
    }
    
    uint8_t phys_cyl = cyl * dev->params.seek_multiplier;
    
    if (dev->current_cyl != cyl) {
        int ret = uft_lfdc_seek(dev, cyl);
        if (ret != UFT_LFDC_OK) return ret;
    }
    
    int last_error = UFT_LFDC_ERR_WRITE;
    
    for (int retry = 0; retry <= dev->params.retries; retry++) {
        struct floppy_raw_cmd raw = {0};
        raw.flags = FD_RAW_NEED_DISK | FD_RAW_WRITE | FD_RAW_INTR;
        raw.cmd[0] = FDC_WRITE_DATA;
        raw.cmd[1] = (head << 2) | 0;
        raw.cmd[2] = cyl;
        raw.cmd[3] = head;
        raw.cmd[4] = sector;
        raw.cmd[5] = size_code;
        raw.cmd[6] = sector;
        raw.cmd[7] = dev->params.gap3 ? dev->params.gap3 : 0x1B;
        raw.cmd[8] = 0xFF;
        raw.cmd_count = 9;
        raw.data = (uint8_t *)data;  /* Cast away const - Linux API */
        raw.length = size;
        raw.rate = dev->params.data_rate;
        raw.track = phys_cyl;
        
        int ret = fdc_raw_cmd(dev, &raw);
        
        if (ret == UFT_LFDC_OK) {
            dev->sectors_written++;
            return UFT_LFDC_OK;
        }
        
        last_error = ret;
        dev->retries_used++;
    }
    
    dev->errors++;
    return last_error;
}

int uft_lfdc_format_track(uft_lfdc_device_t *dev,
                          uint8_t cyl, uint8_t head,
                          const uft_lfdc_sector_id_t *sectors, int sector_count,
                          uint8_t fill_byte) {
    if (!dev || !sectors || dev->fd < 0) return UFT_LFDC_ERR_PARAM;
    
    if (uft_lfdc_write_protected(dev)) {
        return UFT_LFDC_ERR_PROTECTED;
    }
    
    uint8_t phys_cyl = cyl * dev->params.seek_multiplier;
    
    if (dev->current_cyl != cyl) {
        int ret = uft_lfdc_seek(dev, cyl);
        if (ret != UFT_LFDC_OK) return ret;
    }
    
    /* Build format buffer: 4 bytes per sector (C, H, R, N) */
    uint8_t *fmt_buf = (uint8_t *)malloc(sector_count * 4);
    if (!fmt_buf) return UFT_LFDC_ERR_PARAM;
    
    for (int i = 0; i < sector_count; i++) {
        fmt_buf[i * 4 + 0] = sectors[i].cylinder;
        fmt_buf[i * 4 + 1] = sectors[i].head;
        fmt_buf[i * 4 + 2] = sectors[i].sector;
        fmt_buf[i * 4 + 3] = sectors[i].size_code;
    }
    
    struct floppy_raw_cmd raw = {0};
    raw.flags = FD_RAW_NEED_DISK | FD_RAW_WRITE | FD_RAW_INTR;
    raw.cmd[0] = FDC_FORMAT_TRACK;
    raw.cmd[1] = (head << 2) | 0;
    raw.cmd[2] = sectors[0].size_code;  /* N */
    raw.cmd[3] = sector_count;          /* SC */
    raw.cmd[4] = dev->params.gap3 ? dev->params.gap3 : 0x54;  /* GPL for format */
    raw.cmd[5] = fill_byte;             /* D */
    raw.cmd_count = 6;
    raw.data = fmt_buf;
    raw.length = sector_count * 4;
    raw.rate = dev->params.data_rate;
    raw.track = phys_cyl;
    
    int ret = fdc_raw_cmd(dev, &raw);
    
    free(fmt_buf);
    return ret;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * D88 Support
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_lfdc_dump_d88(uft_lfdc_device_t *dev, const char *filename, bool verbose) {
    if (!dev || !filename || dev->fd < 0) return UFT_LFDC_ERR_PARAM;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return UFT_LFDC_ERR_OPEN;
    
    /* Prepare D88 header */
    uft_lfdc_d88_header_t header = {0};
    snprintf(header.name, sizeof(header.name), "UFT DUMP");
    header.write_protect = uft_lfdc_write_protected(dev) ? 0x10 : 0x00;
    
    switch (dev->media_type) {
    case UFT_LFDC_MEDIA_2D:
    case UFT_LFDC_MEDIA_1D:
        header.media_type = 0x00;
        break;
    case UFT_LFDC_MEDIA_2DD:
    case UFT_LFDC_MEDIA_1DD:
        header.media_type = 0x10;
        break;
    case UFT_LFDC_MEDIA_2HD:
    default:
        header.media_type = 0x20;
        break;
    }
    
    /* Reserve space for header */
    fseek(fp, sizeof(header), SEEK_SET);
    
    uint32_t current_offset = sizeof(header);
    int track_idx = 0;
    
    /* Read all tracks */
    for (uint8_t cyl = 0; cyl <= dev->max_cylinder; cyl++) {
        for (uint8_t head = 0; head <= dev->max_head; head++) {
            if (verbose) {
                printf("\rReading C:%02d H:%d", cyl, head);
                fflush(stdout);
            }
            
            /* Seek to track */
            if (uft_lfdc_seek(dev, cyl) != UFT_LFDC_OK) {
                continue;
            }
            
            /* Read sector IDs */
            uft_lfdc_track_info_t info;
            int sector_count = uft_lfdc_read_id(dev, head, &info);
            
            if (sector_count <= 0) {
                /* Empty track */
                header.track_offsets[track_idx++] = 0;
                continue;
            }
            
            header.track_offsets[track_idx++] = current_offset;
            
            /* Read and write each sector */
            for (int s = 0; s < sector_count; s++) {
                uint16_t sector_size = uft_lfdc_sector_size(info.sectors[s].size_code);
                uint8_t *sector_data = (uint8_t *)malloc(sector_size);
                if (!sector_data) continue;
                
                uint16_t actual;
                bool crc_error = false;
                
                int ret = uft_lfdc_read_sector(dev, cyl, head,
                                               info.sectors[s].sector,
                                               info.sectors[s].size_code,
                                               sector_data, &actual);
                
                if (ret == UFT_LFDC_ERR_CRC) {
                    crc_error = true;
                }
                
                /* Write D88 sector header */
                uint8_t sect_hdr[16] = {0};
                sect_hdr[0] = info.sectors[s].cylinder;
                sect_hdr[1] = info.sectors[s].head;
                sect_hdr[2] = info.sectors[s].sector;
                sect_hdr[3] = info.sectors[s].size_code;
                sect_hdr[4] = (uint8_t)(sector_count);   /* Number of sectors */
                sect_hdr[5] = 0x00;                      /* Density (MFM) */
                sect_hdr[6] = 0x00;                      /* Deleted mark */
                sect_hdr[7] = crc_error ? 0xB0 : 0x00;   /* Status */
                sect_hdr[8] = (uint8_t)(sector_size & 0xFF);
                sect_hdr[9] = (uint8_t)(sector_size >> 8);
                
                fwrite(sect_hdr, 1, 16, fp);
                fwrite(sector_data, 1, sector_size, fp);
                
                current_offset += 16 + sector_size;
                free(sector_data);
            }
        }
    }
    
    /* Update header with total size */
    header.disk_size = current_offset;
    
    /* Write header at beginning */
    fseek(fp, 0, SEEK_SET);
    fwrite(&header, 1, sizeof(header), fp);
    
    fclose(fp);
    
    if (verbose) {
        printf("\nDump complete: %u bytes\n", current_offset);
    }
    
    return UFT_LFDC_OK;
}

int uft_lfdc_restore_d88(uft_lfdc_device_t *dev, const char *filename, bool verbose) {
    if (!dev || !filename || dev->fd < 0) return UFT_LFDC_ERR_PARAM;
    
    if (uft_lfdc_write_protected(dev)) {
        return UFT_LFDC_ERR_PROTECTED;
    }
    
    FILE *fp = fopen(filename, "rb");
    if (!fp) return UFT_LFDC_ERR_OPEN;
    
    /* Read D88 header */
    uft_lfdc_d88_header_t header;
    if (fread(&header, 1, sizeof(header), fp) != sizeof(header)) {
        fclose(fp);
        return UFT_LFDC_ERR_READ;
    }
    
    int track_idx = 0;
    
    for (uint8_t cyl = 0; cyl <= dev->max_cylinder; cyl++) {
        for (uint8_t head = 0; head <= dev->max_head; head++) {
            uint32_t track_offset = header.track_offsets[track_idx++];
            
            if (track_offset == 0) continue;
            
            if (verbose) {
                printf("\rWriting C:%02d H:%d", cyl, head);
                fflush(stdout);
            }
            
            fseek(fp, track_offset, SEEK_SET);
            
            /* Read sectors until next track or end */
            while (1) {
                uint8_t sect_hdr[16];
                if (fread(sect_hdr, 1, 16, fp) != 16) break;
                
                uint8_t c = sect_hdr[0];
                uint8_t h = sect_hdr[1];
                uint8_t r = sect_hdr[2];
                uint8_t n = sect_hdr[3];
                uint16_t size = sect_hdr[8] | (sect_hdr[9] << 8);
                
                /* Check if we're still on same track */
                if (c != cyl || h != head) {
                    fseek(fp, -16, SEEK_CUR);
                    break;
                }
                
                uint8_t *data = (uint8_t *)malloc(size);
                if (!data) break;
                
                if (fread(data, 1, size, fp) != size) {
                    free(data);
                    break;
                }
                
                uft_lfdc_write_sector(dev, c, h, r, n, data, size);
                
                free(data);
            }
        }
    }
    
    fclose(fp);
    
    if (verbose) {
        printf("\nRestore complete\n");
    }
    
    return UFT_LFDC_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

const char *uft_lfdc_strerror(int error) {
    switch (error) {
    case UFT_LFDC_OK:           return "Success";
    case UFT_LFDC_ERR_OPEN:     return "Cannot open device";
    case UFT_LFDC_ERR_IOCTL:    return "IOCTL failed";
    case UFT_LFDC_ERR_SEEK:     return "Seek failed";
    case UFT_LFDC_ERR_READ:     return "Read failed";
    case UFT_LFDC_ERR_WRITE:    return "Write failed";
    case UFT_LFDC_ERR_CRC:      return "CRC error";
    case UFT_LFDC_ERR_NO_DATA:  return "Sector not found";
    case UFT_LFDC_ERR_NO_DISK:  return "No disk in drive";
    case UFT_LFDC_ERR_PROTECTED:return "Write protected";
    case UFT_LFDC_ERR_TIMEOUT:  return "Timeout";
    case UFT_LFDC_ERR_PARAM:    return "Invalid parameter";
    default:                    return "Unknown error";
    }
}

const char *uft_lfdc_media_str(uft_lfdc_media_type_t type) {
    switch (type) {
    case UFT_LFDC_MEDIA_2HD:    return "2HD";
    case UFT_LFDC_MEDIA_2DD:    return "2DD";
    case UFT_LFDC_MEDIA_2D:     return "2D";
    case UFT_LFDC_MEDIA_1DD:    return "1DD";
    case UFT_LFDC_MEDIA_1D:     return "1D";
    default:                    return "Unknown";
    }
}

#endif /* __linux__ */
