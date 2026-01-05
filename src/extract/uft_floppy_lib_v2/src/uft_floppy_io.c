/**
 * @file uft_floppy_io.c
 * @brief Cross-platform disk I/O implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "uft_floppy_io.h"
#include "uft_floppy_geometry.h"

/*===========================================================================
 * Platform-Specific Includes
 *===========================================================================*/

#ifdef UFT_PLATFORM_WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <winioctl.h>
#elif defined(UFT_PLATFORM_LINUX)
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/ioctl.h>
    #include <linux/fs.h>
#elif defined(UFT_PLATFORM_MACOS) || defined(UFT_PLATFORM_BSD)
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/disk.h>
#endif

/*===========================================================================
 * Internal Structures
 *===========================================================================*/

struct uft_disk_s {
    uft_disk_source_t source;       /**< Source type */
    uft_access_mode_t mode;         /**< Access mode */
    
    char path[256];                 /**< Device/file path */
    
#ifdef UFT_PLATFORM_WINDOWS
    HANDLE handle;                  /**< Windows handle */
#else
    int fd;                         /**< Unix file descriptor */
#endif
    
    FILE *image_fp;                 /**< Image file pointer (for image files) */
    
    uint64_t total_size;            /**< Total size in bytes */
    uint16_t sector_size;           /**< Sector size */
    uft_geometry_t geometry;        /**< Disk geometry */
    
    bool write_protected;           /**< Software write protect */
    bool hw_write_protected;        /**< Hardware write protect */
    bool is_open;                   /**< Currently open */
    
    uft_error_t last_error;         /**< Last error code */
};

/*===========================================================================
 * Static Variables
 *===========================================================================*/

static bool g_initialized = false;

/* Platform-specific drive paths */
#ifdef UFT_PLATFORM_WINDOWS
static const char* g_drive_paths[UFT_MAX_DRIVES] = {
    "\\\\.\\PhysicalDrive0", "\\\\.\\PhysicalDrive1",
    "\\\\.\\PhysicalDrive2", "\\\\.\\PhysicalDrive3",
    "\\\\.\\PhysicalDrive4", "\\\\.\\PhysicalDrive5",
    "\\\\.\\PhysicalDrive6", "\\\\.\\PhysicalDrive7",
    "\\\\.\\PhysicalDrive8", "\\\\.\\PhysicalDrive9"
};
/* Floppy drives */
static const char* g_floppy_paths[] = { "\\\\.\\A:", "\\\\.\\B:" };
#elif defined(UFT_PLATFORM_LINUX)
static const char* g_drive_paths[UFT_MAX_DRIVES] = {
    "/dev/sda", "/dev/sdb", "/dev/sdc", "/dev/sdd", "/dev/sde",
    "/dev/sdf", "/dev/sdg", "/dev/sdh", "/dev/sdi", "/dev/sdj"
};
/* Floppy drives */
static const char* g_floppy_paths[] = { "/dev/fd0", "/dev/fd1" };
#elif defined(UFT_PLATFORM_MACOS)
static const char* g_drive_paths[UFT_MAX_DRIVES] = {
    "/dev/disk0", "/dev/disk1", "/dev/disk2", "/dev/disk3", "/dev/disk4",
    "/dev/disk5", "/dev/disk6", "/dev/disk7", "/dev/disk8", "/dev/disk9"
};
static const char* g_floppy_paths[] = { "/dev/fd/0", "/dev/fd/1" };
#endif

/*===========================================================================
 * Error Messages
 *===========================================================================*/

static const char* g_error_messages[] = {
    "Success",
    "Invalid parameter",
    "Not initialized",
    "Drive not set",
    "Failed to open device",
    "Read operation failed",
    "Write operation failed",
    "Seek operation failed",
    "Permission denied",
    "Out of memory",
    "File or entry not found",
    "Invalid disk format",
    "CHS addressing overflow",
    "Invalid disk geometry",
    "Buffer too small",
    "End of file reached",
    "Disk full",
    "Directory not empty",
    "File already exists",
    "Write protected",
    "I/O error",
    "Operation not supported",
};

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

static uft_disk_t* disk_alloc(void) {
    uft_disk_t *disk = (uft_disk_t*)calloc(1, sizeof(uft_disk_t));
    if (disk) {
#ifdef UFT_PLATFORM_WINDOWS
        disk->handle = INVALID_HANDLE_VALUE;
#else
        disk->fd = -1;
#endif
        disk->image_fp = NULL;
        disk->sector_size = UFT_SECTOR_SIZE;
    }
    return disk;
}

static void disk_free(uft_disk_t *disk) {
    if (disk) {
        free(disk);
    }
}

/*===========================================================================
 * Platform-Specific Implementation
 *===========================================================================*/

#ifdef UFT_PLATFORM_WINDOWS

static uft_error_t win_open_drive(uft_disk_t *disk, const char *path, 
                                   uft_access_mode_t mode) {
    DWORD access = 0;
    
    if (mode & UFT_ACCESS_READ)  access |= GENERIC_READ;
    if (mode & UFT_ACCESS_WRITE) access |= GENERIC_WRITE;
    
    disk->handle = CreateFileA(path,
                               access,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL,
                               OPEN_EXISTING,
                               0,
                               NULL);
    
    if (disk->handle == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_ACCESS_DENIED) {
            return UFT_ERR_PERMISSION;
        }
        return UFT_ERR_OPEN_FAILED;
    }
    
    return UFT_OK;
}

static void win_close_drive(uft_disk_t *disk) {
    if (disk->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(disk->handle);
        disk->handle = INVALID_HANDLE_VALUE;
    }
}

static uft_error_t win_get_size(uft_disk_t *disk, uint64_t *size) {
    GET_LENGTH_INFORMATION li;
    DWORD returned;
    
    if (!DeviceIoControl(disk->handle, IOCTL_DISK_GET_LENGTH_INFO,
                         NULL, 0, &li, sizeof(li), &returned, NULL)) {
        return UFT_ERR_IO;
    }
    
    *size = li.Length.QuadPart;
    return UFT_OK;
}

static uft_error_t win_read_sectors(uft_disk_t *disk, void *buffer,
                                     uint64_t lba, uint32_t count) {
    LARGE_INTEGER offset;
    DWORD bytes_to_read = count * disk->sector_size;
    DWORD bytes_read;
    
    offset.QuadPart = lba * disk->sector_size;
    
    if (!SetFilePointerEx(disk->handle, offset, NULL, FILE_BEGIN)) {
        return UFT_ERR_SEEK_FAILED;
    }
    
    if (!ReadFile(disk->handle, buffer, bytes_to_read, &bytes_read, NULL)) {
        return UFT_ERR_READ_FAILED;
    }
    
    if (bytes_read != bytes_to_read) {
        return UFT_ERR_READ_FAILED;
    }
    
    return UFT_OK;
}

static uft_error_t win_write_sectors(uft_disk_t *disk, const void *buffer,
                                      uint64_t lba, uint32_t count) {
    LARGE_INTEGER offset;
    DWORD bytes_to_write = count * disk->sector_size;
    DWORD bytes_written;
    
    offset.QuadPart = lba * disk->sector_size;
    
    if (!SetFilePointerEx(disk->handle, offset, NULL, FILE_BEGIN)) {
        return UFT_ERR_SEEK_FAILED;
    }
    
    if (!WriteFile(disk->handle, buffer, bytes_to_write, &bytes_written, NULL)) {
        return UFT_ERR_WRITE_FAILED;
    }
    
    if (bytes_written != bytes_to_write) {
        return UFT_ERR_WRITE_FAILED;
    }
    
    return UFT_OK;
}

#elif defined(UFT_PLATFORM_LINUX) || defined(UFT_PLATFORM_MACOS) || defined(UFT_PLATFORM_BSD)

static uft_error_t unix_open_drive(uft_disk_t *disk, const char *path,
                                    uft_access_mode_t mode) {
    int flags = 0;
    
    if ((mode & UFT_ACCESS_READWRITE) == UFT_ACCESS_READWRITE) {
        flags = O_RDWR;
    } else if (mode & UFT_ACCESS_WRITE) {
        flags = O_WRONLY;
    } else {
        flags = O_RDONLY;
    }
    
    disk->fd = open(path, flags);
    if (disk->fd < 0) {
        if (errno == EACCES || errno == EPERM) {
            return UFT_ERR_PERMISSION;
        }
        return UFT_ERR_OPEN_FAILED;
    }
    
    return UFT_OK;
}

static void unix_close_drive(uft_disk_t *disk) {
    if (disk->fd >= 0) {
        close(disk->fd);
        disk->fd = -1;
    }
}

static uft_error_t unix_get_size(uft_disk_t *disk, uint64_t *size) {
#ifdef UFT_PLATFORM_LINUX
    if (ioctl(disk->fd, BLKGETSIZE64, size) < 0) {
        return UFT_ERR_IO;
    }
#elif defined(UFT_PLATFORM_MACOS)
    uint32_t block_size;
    uint64_t block_count;
    
    if (ioctl(disk->fd, DKIOCGETBLOCKSIZE, &block_size) < 0) {
        return UFT_ERR_IO;
    }
    if (ioctl(disk->fd, DKIOCGETBLOCKCOUNT, &block_count) < 0) {
        return UFT_ERR_IO;
    }
    *size = (uint64_t)block_size * block_count;
#else
    /* BSD - use lseek to find size */
    off_t end = lseek(disk->fd, 0, SEEK_END);
    if (end < 0) {
        return UFT_ERR_IO;
    }
    *size = (uint64_t)end;
    lseek(disk->fd, 0, SEEK_SET);
#endif
    return UFT_OK;
}

static uft_error_t unix_read_sectors(uft_disk_t *disk, void *buffer,
                                      uint64_t lba, uint32_t count) {
    off_t offset = lba * disk->sector_size;
    size_t bytes_to_read = count * disk->sector_size;
    ssize_t bytes_read;
    
    if (lseek(disk->fd, offset, SEEK_SET) < 0) {
        return UFT_ERR_SEEK_FAILED;
    }
    
    bytes_read = read(disk->fd, buffer, bytes_to_read);
    if (bytes_read < 0) {
        return UFT_ERR_READ_FAILED;
    }
    
    if ((size_t)bytes_read != bytes_to_read) {
        return UFT_ERR_READ_FAILED;
    }
    
    return UFT_OK;
}

static uft_error_t unix_write_sectors(uft_disk_t *disk, const void *buffer,
                                       uint64_t lba, uint32_t count) {
    off_t offset = lba * disk->sector_size;
    size_t bytes_to_write = count * disk->sector_size;
    ssize_t bytes_written;
    
    if (lseek(disk->fd, offset, SEEK_SET) < 0) {
        return UFT_ERR_SEEK_FAILED;
    }
    
    bytes_written = write(disk->fd, buffer, bytes_to_write);
    if (bytes_written < 0) {
        return UFT_ERR_WRITE_FAILED;
    }
    
    if ((size_t)bytes_written != bytes_to_write) {
        return UFT_ERR_WRITE_FAILED;
    }
    
    return UFT_OK;
}

#endif /* Platform */

/*===========================================================================
 * Image File Operations
 *===========================================================================*/

static uft_error_t image_open(uft_disk_t *disk, const char *path,
                               uft_access_mode_t mode) {
    const char *fmode;
    
    if ((mode & UFT_ACCESS_READWRITE) == UFT_ACCESS_READWRITE) {
        fmode = "r+b";
    } else if (mode & UFT_ACCESS_WRITE) {
        fmode = "r+b";  /* Can't write-only to existing file */
    } else {
        fmode = "rb";
    }
    
    disk->image_fp = fopen(path, fmode);
    if (!disk->image_fp) {
        if (errno == EACCES || errno == EPERM) {
            return UFT_ERR_PERMISSION;
        }
        return UFT_ERR_OPEN_FAILED;
    }
    
    /* Get file size */
    if (fseek(disk->image_fp, 0, SEEK_END) != 0) { /* seek error */ }
    disk->total_size = ftell(disk->image_fp);
    if (fseek(disk->image_fp, 0, SEEK_SET) != 0) { /* seek error */ }
    return UFT_OK;
}

static void image_close(uft_disk_t *disk) {
    if (disk->image_fp) {
        fclose(disk->image_fp);
        disk->image_fp = NULL;
    }
}

static uft_error_t image_read_sectors(uft_disk_t *disk, void *buffer,
                                       uint64_t lba, uint32_t count) {
    size_t bytes_to_read = count * disk->sector_size;
    size_t bytes_read;
    
    if (fseek(disk->image_fp, (long)(lba * disk->sector_size), SEEK_SET) != 0) {
        return UFT_ERR_SEEK_FAILED;
    }
    
    bytes_read = fread(buffer, 1, bytes_to_read, disk->image_fp);
    if (bytes_read != bytes_to_read) {
        if (feof(disk->image_fp)) {
            /* Partial read at end - zero fill */
            memset((uint8_t*)buffer + bytes_read, 0, bytes_to_read - bytes_read);
        } else {
            return UFT_ERR_READ_FAILED;
        }
    }
    
    return UFT_OK;
}

static uft_error_t image_write_sectors(uft_disk_t *disk, const void *buffer,
                                        uint64_t lba, uint32_t count) {
    size_t bytes_to_write = count * disk->sector_size;
    size_t bytes_written;
    
    if (fseek(disk->image_fp, (long)(lba * disk->sector_size), SEEK_SET) != 0) {
        return UFT_ERR_SEEK_FAILED;
    }
    
    bytes_written = fwrite(buffer, 1, bytes_to_write, disk->image_fp);
    if (bytes_written != bytes_to_write) {
        return UFT_ERR_WRITE_FAILED;
    }
    
    return UFT_OK;
}

/*===========================================================================
 * Public API Implementation
 *===========================================================================*/

uft_error_t uft_disk_init(void) {
    if (g_initialized) {
        return UFT_OK;
    }
    
    g_initialized = true;
    return UFT_OK;
}

void uft_disk_cleanup(void) {
    g_initialized = false;
}

int uft_disk_get_drive_count(void) {
    int count = 0;
    for (int i = 0; i < UFT_MAX_DRIVES; i++) {
        if (uft_disk_drive_exists(i)) {
            count++;
        }
    }
    return count;
}

bool uft_disk_drive_exists(int drive_index) {
    if (drive_index < 0 || drive_index >= UFT_MAX_DRIVES) {
        return false;
    }
    
#ifdef UFT_PLATFORM_WINDOWS
    HANDLE h = CreateFileA(g_drive_paths[drive_index],
                           GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        return false;
    }
    CloseHandle(h);
    return true;
#else
    int fd = open(g_drive_paths[drive_index], O_RDONLY);
    if (fd < 0) {
        return false;
    }
    close(fd);
    return true;
#endif
}

const char* uft_disk_get_drive_path(int drive_index) {
    if (drive_index < 0 || drive_index >= UFT_MAX_DRIVES) {
        return NULL;
    }
    return g_drive_paths[drive_index];
}

uft_error_t uft_disk_query_size(int drive_index, uint64_t *size_bytes) {
    if (!size_bytes) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    uft_disk_t *disk;
    uft_error_t err = uft_disk_open_drive(drive_index, UFT_ACCESS_READ, &disk);
    if (err != UFT_OK) {
        return err;
    }
    
    err = uft_disk_get_size(disk, size_bytes);
    uft_disk_close(disk);
    
    return err;
}

uft_error_t uft_disk_open_drive(int drive_index, uft_access_mode_t mode,
                                 uft_disk_t **disk) {
    if (!g_initialized) {
        return UFT_ERR_NOT_INITIALIZED;
    }
    
    if (!disk || drive_index < 0 || drive_index >= UFT_MAX_DRIVES) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    uft_disk_t *d = disk_alloc();
    if (!d) {
        return UFT_ERR_NO_MEMORY;
    }
    
    d->source = UFT_SOURCE_PHYSICAL;
    d->mode = mode;
    strncpy(d->path, g_drive_paths[drive_index], sizeof(d->path) - 1);
    
    uft_error_t err;
#ifdef UFT_PLATFORM_WINDOWS
    err = win_open_drive(d, d->path, mode);
    if (err == UFT_OK) {
        win_get_size(d, &d->total_size);
    }
#else
    err = unix_open_drive(d, d->path, mode);
    if (err == UFT_OK) {
        unix_get_size(d, &d->total_size);
    }
#endif
    
    if (err != UFT_OK) {
        disk_free(d);
        return err;
    }
    
    d->is_open = true;
    
    /* Detect geometry from size */
    uft_floppy_type_t type = uft_geometry_detect_type(d->total_size);
    if (type != UFT_FLOPPY_UNKNOWN) {
        uft_geometry_get_standard(type, &d->geometry);
    }
    
    *disk = d;
    return UFT_OK;
}

uft_error_t uft_disk_open_image(const char *path, uft_access_mode_t mode,
                                 uft_disk_t **disk) {
    if (!g_initialized) {
        return UFT_ERR_NOT_INITIALIZED;
    }
    
    if (!path || !disk) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    uft_disk_t *d = disk_alloc();
    if (!d) {
        return UFT_ERR_NO_MEMORY;
    }
    
    d->source = UFT_SOURCE_IMAGE;
    d->mode = mode;
    strncpy(d->path, path, sizeof(d->path) - 1);
    
    uft_error_t err = image_open(d, path, mode);
    if (err != UFT_OK) {
        disk_free(d);
        return err;
    }
    
    d->is_open = true;
    
    /* Detect geometry from size */
    uft_floppy_type_t type = uft_geometry_detect_type(d->total_size);
    if (type != UFT_FLOPPY_UNKNOWN) {
        uft_geometry_get_standard(type, &d->geometry);
    }
    
    *disk = d;
    return UFT_OK;
}

void uft_disk_close(uft_disk_t *disk) {
    if (!disk) return;
    
    if (disk->is_open) {
        if (disk->source == UFT_SOURCE_IMAGE) {
            image_close(disk);
        } else {
#ifdef UFT_PLATFORM_WINDOWS
            win_close_drive(disk);
#else
            unix_close_drive(disk);
#endif
        }
        disk->is_open = false;
    }
    
    disk_free(disk);
}

uft_error_t uft_disk_get_info(uft_disk_t *disk, uft_disk_info_t *info) {
    if (!disk || !info) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    memset(info, 0, sizeof(*info));
    
    info->source = disk->source;
    info->mode = disk->mode;
    strncpy(info->path, disk->path, sizeof(info->path) - 1);
    info->total_size = disk->total_size;
    info->total_sectors = (uint32_t)(disk->total_size / disk->sector_size);
    info->sector_size = disk->sector_size;
    info->geometry = disk->geometry;
    info->write_protected = disk->write_protected || disk->hw_write_protected;
    info->is_open = disk->is_open;
    
    return UFT_OK;
}

uft_error_t uft_disk_get_size(uft_disk_t *disk, uint64_t *sectors) {
    if (!disk || !sectors) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    *sectors = disk->total_size / disk->sector_size;
    return UFT_OK;
}

uint16_t uft_disk_get_sector_size(uft_disk_t *disk) {
    return disk ? disk->sector_size : 0;
}

bool uft_disk_is_write_protected(uft_disk_t *disk) {
    return disk ? (disk->write_protected || disk->hw_write_protected) : true;
}

uft_error_t uft_disk_read_sectors(uft_disk_t *disk, void *buffer,
                                   uint64_t lba, uint32_t count) {
    if (!disk || !buffer) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    if (!disk->is_open) {
        return UFT_ERR_DRIVE_NOT_SET;
    }
    
    uft_error_t err;
    
    if (disk->source == UFT_SOURCE_IMAGE) {
        err = image_read_sectors(disk, buffer, lba, count);
    } else {
#ifdef UFT_PLATFORM_WINDOWS
        err = win_read_sectors(disk, buffer, lba, count);
#else
        err = unix_read_sectors(disk, buffer, lba, count);
#endif
    }
    
    disk->last_error = err;
    return err;
}

uft_error_t uft_disk_write_sectors(uft_disk_t *disk, const void *buffer,
                                    uint64_t lba, uint32_t count) {
    if (!disk || !buffer) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    if (!disk->is_open) {
        return UFT_ERR_DRIVE_NOT_SET;
    }
    
    if (disk->write_protected || disk->hw_write_protected) {
        return UFT_ERR_PROTECTED;
    }
    
    if (!(disk->mode & UFT_ACCESS_WRITE)) {
        return UFT_ERR_PERMISSION;
    }
    
    uft_error_t err;
    
    if (disk->source == UFT_SOURCE_IMAGE) {
        err = image_write_sectors(disk, buffer, lba, count);
    } else {
#ifdef UFT_PLATFORM_WINDOWS
        err = win_write_sectors(disk, buffer, lba, count);
#else
        err = unix_write_sectors(disk, buffer, lba, count);
#endif
    }
    
    disk->last_error = err;
    return err;
}

uft_error_t uft_disk_read_bytes(uft_disk_t *disk, void *buffer,
                                 uint64_t offset, size_t size) {
    if (!disk || !buffer) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Calculate sector range */
    uint64_t start_sector = offset / disk->sector_size;
    uint64_t start_offset = offset % disk->sector_size;
    uint64_t end_offset = offset + size;
    uint64_t end_sector = (end_offset + disk->sector_size - 1) / disk->sector_size;
    uint32_t sector_count = (uint32_t)(end_sector - start_sector);
    
    /* Allocate temporary buffer */
    uint8_t *temp = (uint8_t*)malloc(sector_count * disk->sector_size);
    if (!temp) {
        return UFT_ERR_NO_MEMORY;
    }
    
    /* Read sectors */
    uft_error_t err = uft_disk_read_sectors(disk, temp, start_sector, sector_count);
    if (err == UFT_OK) {
        memcpy(buffer, temp + start_offset, size);
    }
    
    free(temp);
    return err;
}

uft_error_t uft_disk_write_bytes(uft_disk_t *disk, const void *buffer,
                                  uint64_t offset, size_t size) {
    if (!disk || !buffer) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Calculate sector range */
    uint64_t start_sector = offset / disk->sector_size;
    uint64_t start_offset = offset % disk->sector_size;
    uint64_t end_offset = offset + size;
    uint64_t end_sector = (end_offset + disk->sector_size - 1) / disk->sector_size;
    uint32_t sector_count = (uint32_t)(end_sector - start_sector);
    
    /* Allocate temporary buffer */
    uint8_t *temp = (uint8_t*)malloc(sector_count * disk->sector_size);
    if (!temp) {
        return UFT_ERR_NO_MEMORY;
    }
    
    /* Read-modify-write if not sector-aligned */
    uft_error_t err = uft_disk_read_sectors(disk, temp, start_sector, sector_count);
    if (err != UFT_OK) {
        free(temp);
        return err;
    }
    
    memcpy(temp + start_offset, buffer, size);
    
    err = uft_disk_write_sectors(disk, temp, start_sector, sector_count);
    
    free(temp);
    return err;
}

uft_error_t uft_disk_sync(uft_disk_t *disk) {
    if (!disk) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    if (disk->source == UFT_SOURCE_IMAGE) {
        if (disk->image_fp) {
            fflush(disk->image_fp);
        }
    } else {
#ifdef UFT_PLATFORM_WINDOWS
        if (disk->handle != INVALID_HANDLE_VALUE) {
            FlushFileBuffers(disk->handle);
        }
#else
        if (disk->fd >= 0) {
            fsync(disk->fd);
        }
#endif
    }
    
    return UFT_OK;
}

uft_error_t uft_disk_set_protection(uft_disk_t *disk, bool protect) {
    if (!disk) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    disk->write_protected = protect;
    return UFT_OK;
}

uft_error_t uft_disk_get_last_error(uft_disk_t *disk) {
    return disk ? disk->last_error : UFT_ERR_INVALID_PARAM;
}

const char* uft_disk_error_string(uft_error_t error) {
    int idx = -error;
    if (idx < 0 || idx >= (int)(sizeof(g_error_messages) / sizeof(g_error_messages[0]))) {
        return "Unknown error";
    }
    return g_error_messages[idx];
}
