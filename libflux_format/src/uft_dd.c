// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * uft_dd.c - Unified Floppy Tool DD Module Implementation
 * 
 * @version 1.0.0
 * @date 2025-01-01
 */

#include "uft_dd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>

#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
#else
#include <sys/ioctl.h>
#include <linux/fd.h>
#endif

/*============================================================================*
 * INTERNAL STATE
 *============================================================================*/

static struct {
    dd_config_t config;
    dd_status_t status;
    volatile bool running;
    volatile bool paused;
    volatile bool cancelled;
    
    /* Buffers */
    uint8_t *read_buffer;
    uint8_t *write_buffer;
    size_t buffer_size;
    
    /* Hash contexts (simplified) */
    uint8_t md5_ctx[128];
    uint8_t sha1_ctx[128];
    uint8_t sha256_ctx[128];
    
    /* File descriptors */
    int input_fd;
    int output_fd;
    
} dd_state;

/*============================================================================*
 * INITIALIZATION
 *============================================================================*/

void dd_config_init(dd_config_t *config) {
    if (!config) return;
    
    memset(config, 0, sizeof(*config));
    
    /* Block sizes */
    config->blocksize.soft_blocksize = DD_SOFT_BS_DEFAULT;
    config->blocksize.hard_blocksize = DD_HARD_BS_DEFAULT;
    config->blocksize.dio_blocksize = DD_DIO_BLOCKSIZE;
    config->blocksize.auto_adjust = true;
    
    /* Recovery */
    config->recovery.enabled = true;
    config->recovery.reverse = false;
    config->recovery.sparse = false;
    config->recovery.max_errors = DD_MAX_ERRORS_DEFAULT;
    config->recovery.retry_count = DD_RETRY_COUNT_DEFAULT;
    config->recovery.retry_delay_ms = DD_RETRY_DELAY_DEFAULT;
    config->recovery.continue_on_error = true;
    config->recovery.fill_on_error = true;
    config->recovery.fill_pattern = 0x00;
    
    /* Hash */
    config->hash.algorithms = HASH_NONE;
    config->hash.hash_input = false;
    config->hash.hash_output = false;
    config->hash.window_size = DD_HASH_WINDOW_DEFAULT;
    config->hash.verify_after = false;
    
    /* Wipe */
    config->wipe.enabled = false;
    config->wipe.pattern = WIPE_ZERO;
    config->wipe.passes = 1;
    
    /* Output */
    config->output.split_output = false;
    config->output.split_size = 0;
    config->output.append = false;
    config->output.truncate = false;
    config->output.direct_io = false;
    config->output.sync_writes = false;
    config->output.sync_frequency = 0;
    
    /* Floppy */
    config->floppy.enabled = false;
    config->floppy.type = FLOPPY_NONE;
    config->floppy.tracks = DD_FLOPPY_TRACKS_DEFAULT;
    config->floppy.heads = DD_FLOPPY_HEADS_DEFAULT;
    config->floppy.sectors_per_track = DD_FLOPPY_SPT_DEFAULT;
    config->floppy.sector_size = FLOPPY_SECTOR_SIZE;
    config->floppy.format_before = false;
    config->floppy.verify_sectors = true;
    config->floppy.write_retries = DD_FLOPPY_RETRIES_DEFAULT;
    config->floppy.skip_bad_sectors = false;
    config->floppy.step_delay_ms = DD_FLOPPY_STEP_DELAY_DEFAULT;
    config->floppy.settle_delay_ms = DD_FLOPPY_SETTLE_DELAY_DEFAULT;
    config->floppy.motor_delay_ms = DD_FLOPPY_MOTOR_DELAY_DEFAULT;
    
    /* Logging */
    config->log_level = 2;  /* Info */
    config->log_timestamps = true;
}

int dd_config_validate(const dd_config_t *config) {
    if (!config) return -1;
    
    /* Check block sizes */
    if (config->blocksize.soft_blocksize < DD_SOFT_BS_MIN ||
        config->blocksize.soft_blocksize > DD_SOFT_BS_MAX) {
        return 1;
    }
    
    if (config->blocksize.hard_blocksize < DD_HARD_BS_MIN ||
        config->blocksize.hard_blocksize > DD_HARD_BS_MAX) {
        return 2;
    }
    
    /* Soft must be >= hard */
    if (config->blocksize.soft_blocksize < config->blocksize.hard_blocksize) {
        return 3;
    }
    
    /* Check floppy parameters */
    if (config->floppy.enabled) {
        if (config->floppy.tracks < DD_FLOPPY_TRACKS_MIN ||
            config->floppy.tracks > DD_FLOPPY_TRACKS_MAX) {
            return 10;
        }
        if (config->floppy.heads < DD_FLOPPY_HEADS_MIN ||
            config->floppy.heads > DD_FLOPPY_HEADS_MAX) {
            return 11;
        }
        if (config->floppy.sectors_per_track < DD_FLOPPY_SPT_MIN ||
            config->floppy.sectors_per_track > DD_FLOPPY_SPT_MAX) {
            return 12;
        }
    }
    
    return 0;
}

/*============================================================================*
 * STATUS FUNCTIONS
 *============================================================================*/

void dd_get_status(dd_status_t *status) {
    if (!status) return;
    memcpy(status, &dd_state.status, sizeof(*status));
}

bool dd_is_running(void) {
    return dd_state.running;
}

void dd_pause(void) {
    dd_state.paused = true;
    dd_state.status.is_paused = true;
}

void dd_resume(void) {
    dd_state.paused = false;
    dd_state.status.is_paused = false;
}

void dd_cancel(void) {
    dd_state.cancelled = true;
}

/*============================================================================*
 * CORE I/O FUNCTIONS
 *============================================================================*/

static ssize_t safe_read(int fd, void *buf, size_t count) {
    ssize_t total = 0;
    uint8_t *ptr = buf;
    
    while (total < (ssize_t)count) {
        ssize_t n = read(fd, ptr + total, count - total);
        
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN) continue;
            return -1;
        }
        if (n == 0) break;  /* EOF */
        
        total += n;
    }
    
    return total;
}

static ssize_t safe_write(int fd, const void *buf, size_t count) {
    ssize_t total = 0;
    const uint8_t *ptr = buf;
    
    while (total < (ssize_t)count) {
        ssize_t n = write(fd, ptr + total, count - total);
        
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN) continue;
            return -1;
        }
        
        total += n;
    }
    
    return total;
}

/*============================================================================*
 * RECOVERY READ (from dd_rescue)
 *============================================================================*/

static ssize_t recovery_read(int fd, void *buf, size_t soft_bs, size_t hard_bs,
                            const dd_recovery_t *recovery) {
    ssize_t total = 0;
    uint8_t *ptr = buf;
    
    /* First try with soft block size */
    ssize_t n = safe_read(fd, ptr, soft_bs);
    
    if (n == (ssize_t)soft_bs || n >= 0) {
        return n;  /* Success or EOF */
    }
    
    /* Error - try smaller blocks */
    if (!recovery->enabled) return n;
    
    dd_state.status.errors_read++;
    
    /* Read with hard block size */
    size_t offset = 0;
    while (offset < soft_bs) {
        size_t to_read = (soft_bs - offset < hard_bs) ? 
                         (soft_bs - offset) : hard_bs;
        
        int retries = recovery->retry_count;
        ssize_t block_read = -1;
        
        while (retries >= 0 && block_read < 0) {
            block_read = safe_read(fd, ptr + offset, to_read);
            
            if (block_read < 0) {
                if (retries > 0) {
                    /* Retry delay */
                    if (recovery->retry_delay_ms > 0) {
                        usleep(recovery->retry_delay_ms * 1000);
                    }
                    
                    /* Seek back to retry position */
                    lseek(fd, -to_read, SEEK_CUR);
                }
                retries--;
            }
        }
        
        if (block_read < 0) {
            /* Unrecoverable - fill with pattern if enabled */
            if (recovery->fill_on_error) {
                memset(ptr + offset, recovery->fill_pattern, to_read);
                block_read = to_read;
            } else if (!recovery->continue_on_error) {
                return -1;
            } else {
                /* Skip this block */
                lseek(fd, to_read, SEEK_CUR);
                dd_state.status.sectors_skipped++;
                block_read = to_read;
            }
        }
        
        offset += block_read;
        total += block_read;
    }
    
    return total;
}

/*============================================================================*
 * FLOPPY DEVICE FUNCTIONS
 *============================================================================*/

#ifdef __linux__

static int floppy_open_device(const char *device) {
    int fd = open(device, O_RDWR | O_NDELAY);
    if (fd < 0) {
        return -1;
    }
    
    /* Clear O_NDELAY after open */
    int flags = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, flags & ~O_NDELAY);
    
    return fd;
}

int dd_floppy_write_sector(const dd_floppy_t *floppy,
                           int track, int head, int sector,
                           const uint8_t *data, size_t len) {
    if (!floppy || !data || !floppy->device) return -1;
    
    int fd = floppy_open_device(floppy->device);
    if (fd < 0) return -1;
    
    /* Calculate offset */
    size_t spt = floppy->sectors_per_track;
    size_t ss = floppy->sector_size;
    off_t offset = ((track * floppy->heads + head) * spt + (sector - 1)) * ss;
    
    /* Seek to position */
    if (lseek(fd, offset, SEEK_SET) != offset) {
        close(fd);
        return -1;
    }
    
    /* Write sector */
    ssize_t written = write(fd, data, len);
    
    /* Verify if enabled */
    if (floppy->verify_sectors && written == (ssize_t)len) {
        uint8_t verify_buf[1024];
        lseek(fd, offset, SEEK_SET);
        ssize_t verify_read = read(fd, verify_buf, len);
        
        if (verify_read != (ssize_t)len || memcmp(data, verify_buf, len) != 0) {
            close(fd);
            return -2;  /* Verify failed */
        }
    }
    
    close(fd);
    return (written == (ssize_t)len) ? 0 : -1;
}

int dd_floppy_read_sector(const dd_floppy_t *floppy,
                          int track, int head, int sector,
                          uint8_t *data, size_t len) {
    if (!floppy || !data || !floppy->device) return -1;
    
    int fd = floppy_open_device(floppy->device);
    if (fd < 0) return -1;
    
    /* Calculate offset */
    size_t spt = floppy->sectors_per_track;
    size_t ss = floppy->sector_size;
    off_t offset = ((track * floppy->heads + head) * spt + (sector - 1)) * ss;
    
    /* Seek to position */
    if (lseek(fd, offset, SEEK_SET) != offset) {
        close(fd);
        return -1;
    }
    
    /* Read sector */
    ssize_t rd = read(fd, data, len);
    
    close(fd);
    return (rd == (ssize_t)len) ? 0 : -1;
}

int dd_floppy_write_image(const dd_floppy_t *floppy,
                          const uint8_t *image, size_t image_size,
                          void (*progress)(int track, int head, void *user),
                          void *user_data) {
    if (!floppy || !image || !floppy->device) return -1;
    
    int fd = floppy_open_device(floppy->device);
    if (fd < 0) return -1;
    
    size_t sector_size = floppy->sector_size;
    size_t spt = floppy->sectors_per_track;
    size_t offset = 0;
    int errors = 0;
    
    for (int track = 0; track < floppy->tracks && offset < image_size; track++) {
        for (int head = 0; head < floppy->heads && offset < image_size; head++) {
            /* Report progress */
            if (progress) {
                progress(track, head, user_data);
            }
            
            for (int sector = 1; sector <= (int)spt && offset < image_size; sector++) {
                size_t to_write = (image_size - offset < sector_size) ?
                                  (image_size - offset) : sector_size;
                
                int retries = floppy->write_retries;
                ssize_t written = -1;
                
                while (retries >= 0 && written != (ssize_t)to_write) {
                    written = write(fd, image + offset, to_write);
                    
                    if (written != (ssize_t)to_write) {
                        /* Seek back for retry */
                        lseek(fd, offset, SEEK_SET);
                        retries--;
                    }
                }
                
                if (written != (ssize_t)to_write) {
                    errors++;
                    if (!floppy->skip_bad_sectors) {
                        close(fd);
                        return -1;
                    }
                }
                
                offset += sector_size;
            }
        }
    }
    
    close(fd);
    return errors;
}

int dd_floppy_read_image(const dd_floppy_t *floppy,
                         uint8_t *image, size_t max_size,
                         void (*progress)(int track, int head, void *user),
                         void *user_data) {
    if (!floppy || !image || !floppy->device) return -1;
    
    int fd = floppy_open_device(floppy->device);
    if (fd < 0) return -1;
    
    size_t sector_size = floppy->sector_size;
    size_t spt = floppy->sectors_per_track;
    size_t total_size = floppy->tracks * floppy->heads * spt * sector_size;
    size_t offset = 0;
    int errors = 0;
    
    if (max_size < total_size) {
        total_size = max_size;
    }
    
    for (int track = 0; track < floppy->tracks && offset < total_size; track++) {
        for (int head = 0; head < floppy->heads && offset < total_size; head++) {
            if (progress) {
                progress(track, head, user_data);
            }
            
            for (int sector = 1; sector <= (int)spt && offset < total_size; sector++) {
                ssize_t rd = read(fd, image + offset, sector_size);
                
                if (rd != (ssize_t)sector_size) {
                    errors++;
                    /* Fill with zeros on error */
                    memset(image + offset, 0, sector_size);
                }
                
                offset += sector_size;
            }
        }
    }
    
    close(fd);
    return errors;
}

int dd_floppy_detect(char **devices, int max_devices) {
    if (!devices || max_devices < 1) return 0;
    
    int found = 0;
    const char *floppy_devs[] = {
        "/dev/fd0", "/dev/fd1", "/dev/floppy", NULL
    };
    
    for (int i = 0; floppy_devs[i] && found < max_devices; i++) {
        if (access(floppy_devs[i], F_OK) == 0) {
            devices[found] = strdup(floppy_devs[i]);
            found++;
        }
    }
    
    return found;
}

#elif defined(_WIN32)

int dd_floppy_detect(char **devices, int max_devices) {
    if (!devices || max_devices < 1) return 0;
    
    int found = 0;
    
    /* Check A: and B: drives */
    DWORD drives = GetLogicalDrives();
    
    if ((drives & 1) && found < max_devices) {
        /* A: exists */
        UINT type = GetDriveTypeA("A:\\");
        if (type == DRIVE_REMOVABLE) {
            devices[found++] = strdup("\\\\.\\A:");
        }
    }
    
    if ((drives & 2) && found < max_devices) {
        /* B: exists */
        UINT type = GetDriveTypeA("B:\\");
        if (type == DRIVE_REMOVABLE) {
            devices[found++] = strdup("\\\\.\\B:");
        }
    }
    
    return found;
}

int dd_floppy_write_sector(const dd_floppy_t *floppy,
                           int track, int head, int sector,
                           const uint8_t *data, size_t len) {
    if (!floppy || !data || !floppy->device) return -1;
    
    HANDLE hDevice = CreateFileA(
        floppy->device,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );
    
    if (hDevice == INVALID_HANDLE_VALUE) return -1;
    
    /* Calculate offset */
    size_t spt = floppy->sectors_per_track;
    size_t ss = floppy->sector_size;
    LARGE_INTEGER offset;
    offset.QuadPart = ((track * floppy->heads + head) * spt + (sector - 1)) * ss;
    
    /* Seek */
    if (!SetFilePointerEx(hDevice, offset, NULL, FILE_BEGIN)) {
        CloseHandle(hDevice);
        return -1;
    }
    
    /* Write */
    DWORD written;
    BOOL result = WriteFile(hDevice, data, (DWORD)len, &written, NULL);
    
    CloseHandle(hDevice);
    return (result && written == len) ? 0 : -1;
}

#endif /* Platform-specific code */

/*============================================================================*
 * MAIN COPY FUNCTION
 *============================================================================*/

int dd_start(const dd_config_t *config) {
    if (!config) return -1;
    
    int result = dd_config_validate(config);
    if (result != 0) return result;
    
    /* Copy configuration */
    memcpy(&dd_state.config, config, sizeof(*config));
    memset(&dd_state.status, 0, sizeof(dd_state.status));
    
    /* Initialize state */
    dd_state.running = true;
    dd_state.paused = false;
    dd_state.cancelled = false;
    dd_state.status.is_running = true;
    dd_state.status.start_time = time(NULL);
    
    /* Allocate buffers */
    dd_state.buffer_size = config->blocksize.soft_blocksize;
    dd_state.read_buffer = malloc(dd_state.buffer_size);
    dd_state.write_buffer = malloc(dd_state.buffer_size);
    
    if (!dd_state.read_buffer || !dd_state.write_buffer) {
        free(dd_state.read_buffer);
        free(dd_state.write_buffer);
        return -1;
    }
    
    /* Open input */
    if (config->input_file) {
        dd_state.input_fd = open(config->input_file, O_RDONLY);
        if (dd_state.input_fd < 0) {
            free(dd_state.read_buffer);
            free(dd_state.write_buffer);
            return -1;
        }
        
        /* Get input size for progress */
        struct stat st;
        if (fstat(dd_state.input_fd, &st) == 0) {
            dd_state.status.total_size = st.st_size;
        }
    }
    
    /* Open output */
    if (config->output_file && !config->floppy.enabled) {
        int flags = O_WRONLY | O_CREAT;
        if (config->output.truncate) flags |= O_TRUNC;
        if (config->output.append) flags |= O_APPEND;
        
        dd_state.output_fd = open(config->output_file, flags, 0644);
        if (dd_state.output_fd < 0) {
            close(dd_state.input_fd);
            free(dd_state.read_buffer);
            free(dd_state.write_buffer);
            return -1;
        }
    }
    
    /* Skip input bytes */
    if (config->skip_bytes > 0 && dd_state.input_fd >= 0) {
        lseek(dd_state.input_fd, config->skip_bytes, SEEK_SET);
    }
    
    /* Seek output bytes */
    if (config->seek_bytes > 0 && dd_state.output_fd >= 0) {
        lseek(dd_state.output_fd, config->seek_bytes, SEEK_SET);
    }
    
    /* Main copy loop */
    uint64_t bytes_copied = 0;
    uint64_t max_bytes = config->max_bytes ? config->max_bytes : UINT64_MAX;
    
    while (!dd_state.cancelled && bytes_copied < max_bytes) {
        /* Handle pause */
        while (dd_state.paused && !dd_state.cancelled) {
            usleep(100000);  /* 100ms */
        }
        
        if (dd_state.cancelled) break;
        
        /* Read block */
        size_t to_read = dd_state.buffer_size;
        if (bytes_copied + to_read > max_bytes) {
            to_read = max_bytes - bytes_copied;
        }
        
        ssize_t nread = recovery_read(dd_state.input_fd, dd_state.read_buffer,
                                      to_read, config->blocksize.hard_blocksize,
                                      &config->recovery);
        
        if (nread <= 0) break;  /* EOF or error */
        
        dd_state.status.bytes_read += nread;
        
        /* Write block */
        ssize_t nwritten;
        
        if (config->floppy.enabled) {
            /* Write to floppy sector by sector */
            size_t ss = config->floppy.sector_size;
            size_t offset = 0;
            nwritten = 0;
            
            while (offset < (size_t)nread) {
                /* Calculate track/head/sector from current position */
                uint64_t sector_num = (bytes_copied + offset) / ss;
                int spt = config->floppy.sectors_per_track;
                int track = sector_num / (spt * config->floppy.heads);
                int head = (sector_num / spt) % config->floppy.heads;
                int sector = (sector_num % spt) + 1;
                
                size_t chunk = (nread - offset < ss) ? (nread - offset) : ss;
                
                if (dd_floppy_write_sector(&config->floppy, track, head, sector,
                                          dd_state.read_buffer + offset, chunk) == 0) {
                    nwritten += chunk;
                } else {
                    dd_state.status.errors_write++;
                    if (!config->floppy.skip_bad_sectors) {
                        break;
                    }
                }
                
                offset += ss;
                
                /* Update track/head/sector in status */
                dd_state.status.current_track = track;
                dd_state.status.current_head = head;
                dd_state.status.current_sector = sector;
            }
        } else {
            /* Regular file output */
            nwritten = safe_write(dd_state.output_fd, dd_state.read_buffer, nread);
            
            if (nwritten < 0) {
                dd_state.status.errors_write++;
                if (!config->recovery.continue_on_error) {
                    break;
                }
            }
        }
        
        if (nwritten > 0) {
            dd_state.status.bytes_written += nwritten;
            bytes_copied += nwritten;
            
            if ((size_t)nwritten == dd_state.buffer_size) {
                dd_state.status.blocks_full++;
            } else {
                dd_state.status.blocks_partial++;
            }
        }
        
        /* Sync if configured */
        if (config->output.sync_writes ||
            (config->output.sync_frequency > 0 &&
             dd_state.status.blocks_full % config->output.sync_frequency == 0)) {
            if (dd_state.output_fd >= 0) {
                fsync(dd_state.output_fd);
            }
        }
        
        /* Update progress */
        dd_state.status.current_time = time(NULL);
        dd_state.status.elapsed_seconds = difftime(dd_state.status.current_time,
                                                   dd_state.status.start_time);
        
        if (dd_state.status.elapsed_seconds > 0) {
            dd_state.status.bytes_per_second = 
                dd_state.status.bytes_written / dd_state.status.elapsed_seconds;
        }
        
        if (dd_state.status.total_size > 0) {
            dd_state.status.percent_complete = 
                (double)dd_state.status.bytes_read / dd_state.status.total_size * 100.0;
            
            if (dd_state.status.bytes_per_second > 0) {
                dd_state.status.eta_seconds = 
                    (dd_state.status.total_size - dd_state.status.bytes_read) /
                    dd_state.status.bytes_per_second;
            }
        }
        
        dd_state.status.current_offset = bytes_copied;
        
        /* Call progress callback */
        if (config->progress_callback) {
            config->progress_callback(&dd_state.status, config->user_data);
        }
    }
    
    /* Cleanup */
    if (dd_state.input_fd >= 0) close(dd_state.input_fd);
    if (dd_state.output_fd >= 0) {
        fsync(dd_state.output_fd);
        close(dd_state.output_fd);
    }
    
    free(dd_state.read_buffer);
    free(dd_state.write_buffer);
    
    dd_state.running = false;
    dd_state.status.is_running = false;
    
    return dd_state.cancelled ? 1 : 0;
}

/*============================================================================*
 * UTILITY FUNCTIONS
 *============================================================================*/

uint64_t dd_parse_size(const char *str) {
    if (!str) return 0;
    
    char *endptr;
    uint64_t value = strtoull(str, &endptr, 10);
    
    switch (*endptr) {
        case 'k': case 'K': value *= 1024; break;
        case 'm': case 'M': value *= 1024 * 1024; break;
        case 'g': case 'G': value *= 1024 * 1024 * 1024; break;
        case 't': case 'T': value *= 1024ULL * 1024 * 1024 * 1024; break;
    }
    
    return value;
}

static char size_buf[64];
const char *dd_format_size(uint64_t bytes) {
    if (bytes >= 1024ULL * 1024 * 1024 * 1024) {
        snprintf(size_buf, sizeof(size_buf), "%.2f TiB", 
                 bytes / (1024.0 * 1024 * 1024 * 1024));
    } else if (bytes >= 1024 * 1024 * 1024) {
        snprintf(size_buf, sizeof(size_buf), "%.2f GiB", 
                 bytes / (1024.0 * 1024 * 1024));
    } else if (bytes >= 1024 * 1024) {
        snprintf(size_buf, sizeof(size_buf), "%.2f MiB", 
                 bytes / (1024.0 * 1024));
    } else if (bytes >= 1024) {
        snprintf(size_buf, sizeof(size_buf), "%.2f KiB", 
                 bytes / 1024.0);
    } else {
        snprintf(size_buf, sizeof(size_buf), "%llu B", 
                 (unsigned long long)bytes);
    }
    
    return size_buf;
}

static char time_buf[64];
const char *dd_format_time(double seconds) {
    int h = (int)(seconds / 3600);
    int m = (int)((seconds - h * 3600) / 60);
    int s = (int)(seconds - h * 3600 - m * 60);
    
    if (h > 0) {
        snprintf(time_buf, sizeof(time_buf), "%d:%02d:%02d", h, m, s);
    } else {
        snprintf(time_buf, sizeof(time_buf), "%d:%02d", m, s);
    }
    
    return time_buf;
}
