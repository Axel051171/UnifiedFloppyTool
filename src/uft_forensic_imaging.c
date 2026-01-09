#include "uft/compat/uft_platform.h"
/**
 * @file uft_forensic_imaging.c
 * @brief UnifiedFloppyTool - Forensic Imaging Implementation v3.1.4.009
 * 
 * Core algorithms extracted from dd_rescue, dc3dd, dcfldd.
 */

#include "uft/uft_forensic_imaging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#define open _open
#define close _close
#define read _read
#define write _write
#define lseek _lseeki64
#define fsync _commit
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#endif

#ifdef __linux__
#include <linux/fs.h>
#endif

/*===========================================================================
 * SIMD DETECTION
 *===========================================================================*/

static uft_fi_cpu_caps_t g_cpu_caps = {0};
static bool g_cpu_caps_detected = false;

void uft_fi_detect_cpu_caps(uft_fi_cpu_caps_t *caps) {
    if (!caps) return;
    
#if defined(__x86_64__) || defined(__i386__)
    uint32_t eax, ebx, ecx, edx;
    
    /* Check for CPUID support and get features */
    __asm__ volatile(
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1)
    );
    
    caps->has_sse2 = (edx & (1 << 26)) != 0;
    
    /* Check for AVX2 */
    if (eax >= 7) {
        __asm__ volatile(
            "cpuid"
            : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
            : "a"(7), "c"(0)
        );
        caps->has_avx2 = (ebx & (1 << 5)) != 0;
    }
#elif defined(__arm__) || defined(__aarch64__)
    caps->has_neon = true;  /* Most ARM64 has NEON */
#ifdef __ARM_FEATURE_SVE
    caps->has_sve = true;
#endif
#endif
    
    g_cpu_caps = *caps;
    g_cpu_caps_detected = true;
}

/*===========================================================================
 * SPARSE DETECTION - SIMD IMPLEMENTATIONS
 *===========================================================================*/

/* Bit manipulation helpers */
static inline int myffs(uint32_t val) {
    if (val == 0) return 0;
#ifdef __GNUC__
    return __builtin_ffs(val);
#else
    int pos = 1;
    while (!(val & 1)) { val >>= 1; pos++; }
    return pos;
#endif
}

/**
 * C reference implementation (from dd_rescue)
 */
size_t uft_fi_find_nonzero_c(const uint8_t *blk, size_t len) {
    const unsigned long *ptr = (const unsigned long *)blk;
    const unsigned long *bptr = ptr;
    
    /* Process word-aligned data */
    for (; (size_t)(ptr - bptr) < len / sizeof(*ptr); ++ptr) {
        if (*ptr) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            return sizeof(long) * (ptr - bptr) + 
                   sizeof(long) - ((__builtin_clzl(*ptr) + 7) >> 3);
#else
            return sizeof(long) * (ptr - bptr) + 
                   ((__builtin_ctzl(*ptr)) >> 3);
#endif
        }
    }
    
    /* Check remaining bytes */
    const uint8_t *bp = (const uint8_t *)ptr;
    for (size_t i = 0; i < len % sizeof(unsigned long); i++) {
        if (bp[i]) {
            return (ptr - bptr) * sizeof(unsigned long) + i;
        }
    }
    
    return len;
}

#ifdef __SSE2__
#include <emmintrin.h>

size_t uft_fi_find_nonzero_sse2(const uint8_t *blk, size_t len) {
    const __m128i zero = _mm_setzero_si128();
    size_t i = 0;
    
    /* Process 16-byte aligned blocks */
    for (; i + 16 <= len; i += 16) {
        __m128i v = _mm_loadu_si128((const __m128i *)(blk + i));
        __m128i cmp = _mm_cmpeq_epi8(v, zero);
        int mask = _mm_movemask_epi8(cmp);
        
        if (mask != 0xFFFF) {
            /* Found non-zero byte */
            int pos = myffs(~mask) - 1;
            return i + pos;
        }
    }
    
    /* Handle remaining bytes */
    for (; i < len; i++) {
        if (blk[i]) return i;
    }
    
    return len;
}
#endif

#ifdef __AVX2__
#include <immintrin.h>

size_t uft_fi_find_nonzero_avx2(const uint8_t *blk, size_t len) {
    const __m256i zero = _mm256_setzero_si256();
    size_t i = 0;
    
    /* Process 32-byte aligned blocks */
    for (; i + 32 <= len; i += 32) {
        __m256i v = _mm256_loadu_si256((const __m256i *)(blk + i));
        __m256i cmp = _mm256_cmpeq_epi8(v, zero);
        int mask = ~_mm256_movemask_epi8(cmp);
        
        if (mask) {
            int pos = myffs(mask) - 1;
            return i + pos;
        }
    }
    
    /* Handle remaining with SSE2 or scalar */
#ifdef __SSE2__
    size_t remain = uft_fi_find_nonzero_sse2(blk + i, len - i);
    return i + remain;
#else
    for (; i < len; i++) {
        if (blk[i]) return i;
    }
    return len;
#endif
}
#endif

/**
 * Auto-dispatch to best available SIMD
 */
size_t uft_fi_find_nonzero(const uint8_t *blk, size_t len) {
    if (!len || *blk) return 0;
    
    if (!g_cpu_caps_detected) {
        uft_fi_detect_cpu_caps(&g_cpu_caps);
    }
    
    /* Handle unaligned prefix */
    const size_t align_offset = (-(size_t)blk) & 0x1F;
    for (size_t i = 0; i < align_offset && i < len; i++) {
        if (blk[i]) return i;
    }
    
    size_t remaining = len - align_offset;
    const uint8_t *aligned_blk = blk + align_offset;
    
#ifdef __AVX2__
    if (g_cpu_caps.has_avx2) {
        size_t res = uft_fi_find_nonzero_avx2(aligned_blk, remaining);
        return align_offset + res;
    }
#endif
    
#ifdef __SSE2__
    if (g_cpu_caps.has_sse2) {
        size_t res = uft_fi_find_nonzero_sse2(aligned_blk, remaining);
        return align_offset + res;
    }
#endif
    
    return align_offset + uft_fi_find_nonzero_c(aligned_blk, remaining);
}

/**
 * Backward scan for sparse detection
 */
size_t uft_fi_find_nonzero_bkw(const uint8_t *blk_end, size_t len) {
    if (!len || blk_end[-1]) return 0;
    
    size_t offset = 0;
    const size_t chunk = 512;
    
    while (offset < len) {
        size_t seglen = (len - offset > chunk) ? chunk : (len - offset);
        size_t found = uft_fi_find_nonzero(blk_end - offset - seglen, seglen);
        
        if (found != seglen) {
            return offset;
        }
        offset += seglen;
    }
    
    return len;
}

/*===========================================================================
 * SPLIT FILE NAMING (from dcfldd)
 *===========================================================================*/

int uft_fi_split_extension(const char *format, int num, char *out, size_t outlen) {
    if (!format || !out || outlen < 4) return -1;
    
    size_t fmtlen = strlen(format);
    if (fmtlen == 0) return -1;
    
    /* Special formats */
    if (strcmp(format, "MAC") == 0) {
        if (num == 0) {
            snprintf(out, outlen, "dmg");
        } else {
            snprintf(out, outlen, "%03d.dmgpart", num + 1);
        }
        return 0;
    }
    
    if (strcmp(format, "WIN") == 0) {
        snprintf(out, outlen, "%03d", num + 1);
        return 0;
    }
    
    /* Generic format: 'a' for letters, digits for numbers */
    if (fmtlen >= outlen) return -1;
    
    for (int i = (int)fmtlen - 1; i >= 0; i--) {
        if (format[i] == 'a') {
            int x = num % 26;
            out[i] = 'a' + x;
            num /= 26;
        } else {
            int x = num % 10;
            out[i] = '0' + x;
            num /= 10;
        }
    }
    out[fmtlen] = '\0';
    
    return 0;
}

int uft_fi_split_max_count(const char *format) {
    if (!format) return 0;
    
    size_t fmtlen = strlen(format);
    int result = 1;
    
    if (strcmp(format, "MAC") == 0 || strcmp(format, "WIN") == 0) {
        return 999;  /* 001-999 */
    }
    
    for (size_t i = 0; i < fmtlen; i++) {
        result *= (format[i] == 'a') ? 26 : 10;
    }
    
    return result;
}

/*===========================================================================
 * SIZE PROBING (from dcfldd)
 *===========================================================================*/

uint64_t uft_fi_probe_size(int fd, bool is_device) {
    struct stat st;
    
    if (fstat(fd, &st) < 0) {
        return 0;
    }
    
    /* Regular file: use stat size */
    if (S_ISREG(st.st_mode)) {
        return st.st_size;
    }
    
    /* Block device: try ioctl first */
    if (S_ISBLK(st.st_mode) || is_device) {
#ifdef __linux__
        uint64_t size = 0;
        if (ioctl(fd, BLKGETSIZE64, &size) == 0) {
            return size;
        }
#endif
        
        /* Binary search fallback (from dcfldd get_dev_size) */
        uint64_t curr = 0, amount = 0;
        uint8_t buf[512];
        
        for (;;) {
            if (lseek(fd, curr, SEEK_SET) < 0) break;
            
            ssize_t nread = read(fd, buf, sizeof(buf));
            
            if (nread < (ssize_t)sizeof(buf)) {
                if (nread <= 0) {
                    if (curr == amount) {
                        lseek(fd, 0, SEEK_SET);
                        return amount;
                    }
                    curr = uft_fi_midpoint(amount, curr, sizeof(buf));
                } else {
                    lseek(fd, 0, SEEK_SET);
                    return amount + nread;
                }
            } else {
                amount = curr + sizeof(buf);
                curr = amount * 2;
            }
        }
        
        lseek(fd, 0, SEEK_SET);
        return amount;
    }
    
    return 0;
}

/*===========================================================================
 * JOB MANAGEMENT
 *===========================================================================*/

uft_fi_job_t* uft_fi_job_new(void) {
    uft_fi_job_t *job = calloc(1, sizeof(uft_fi_job_t));
    if (!job) return NULL;
    
    /* Set defaults */
    job->recovery.enable_recovery = true;
    job->recovery.max_retries = 3;
    job->recovery.retry_delay_ms = 100;
    job->recovery.soft_blocksize = UFT_FI_SOFT_BLOCKSIZE;
    job->recovery.hard_blocksize = UFT_FI_HARD_BLOCKSIZE;
    job->recovery.fill_byte = 0x00;
    
    job->input.fd = -1;
    job->input.sector_size = UFT_FI_DEFAULT_SECTOR_SZ;
    job->output.fd = -1;
    job->log_fd = -1;
    job->log_level = UFT_FI_LOG_INFO;
    
    job->state = UFT_FI_STATE_PENDING;
    job->exit_code = UFT_FI_EXIT_SUCCESS;
    
    return job;
}

void uft_fi_job_free(uft_fi_job_t *job) {
    if (!job) return;
    
    /* Close file descriptors */
    if (job->input.fd >= 0) close(job->input.fd);
    if (job->output.fd >= 0) close(job->output.fd);
    if (job->log_fd >= 0) close(job->log_fd);
    
    /* Free strings */
    free(job->input.path);
    free(job->output.path);
    free(job->log_path);
    
    /* Free split context */
    if (job->output.split) {
        free(job->output.split->base_name);
        free(job->output.split->format);
        if (job->output.split->current_fd >= 0) {
            close(job->output.split->current_fd);
        }
        free(job->output.split);
    }
    
    /* Free bad sector list */
    uft_fi_bad_sector_t *bs = job->bad_sector_list;
    while (bs) {
        uft_fi_bad_sector_t *next = bs->next;
        free(bs);
        bs = next;
    }
    
    /* Free hash outputs */
    uft_fi_hash_cleanup(job);
    
    free(job);
}

int uft_fi_set_input(uft_fi_job_t *job, const char *path) {
    if (!job || !path) return -1;
    
    free(job->input.path);
    job->input.path = strdup(path);
    
    return 0;
}

int uft_fi_set_output(uft_fi_job_t *job, const char *path) {
    if (!job || !path) return -1;
    
    free(job->output.path);
    job->output.path = strdup(path);
    
    return 0;
}

int uft_fi_set_split(uft_fi_job_t *job, uint64_t max_bytes, const char *format) {
    if (!job) return -1;
    
    /* Free existing split context */
    if (job->output.split) {
        free(job->output.split->base_name);
        free(job->output.split->format);
        if (job->output.split->current_fd >= 0) {
            close(job->output.split->current_fd);
        }
        free(job->output.split);
        job->output.split = NULL;
    }
    
    if (max_bytes == 0) return 0;  /* Disable split */
    
    job->output.split = calloc(1, sizeof(uft_fi_split_ctx_t));
    if (!job->output.split) return -1;
    
    job->output.split->max_bytes = max_bytes;
    job->output.split->format = strdup(format ? format : UFT_FI_SPLIT_FMT_DEFAULT);
    job->output.split->current_fd = -1;
    
    return 0;
}

/*===========================================================================
 * LOGGING
 *===========================================================================*/

void uft_fi_log(uft_fi_job_t *job, uft_fi_log_level_t level,
                const char *fmt, ...) {
    if (!job || level < job->log_level) return;
    
    static const char *level_str[] = {
        "", "[DEBUG] ", "[INFO] ", "[WARN] ", "[OK] ", "[FATAL] ", "[INPUT] "
    };
    
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    /* Call user callback if set */
    if (job->log_callback) {
        job->log_callback(level, buf, job->user_data);
    }
    
    /* Write to log file if open */
    if (job->log_fd >= 0) {
        dprintf(job->log_fd, "%s%s\n", level_str[level], buf);
    }
    
    /* Also stderr for warnings and above */
    if (level >= UFT_FI_LOG_WARN) {
        fprintf(stderr, "%s%s\n", level_str[level], buf);
    }
}

void uft_fi_log_header(uft_fi_job_t *job) {
    if (!job) return;
    
    time_t now = time(NULL);
    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    uft_fi_log(job, UFT_FI_LOG_INFO, "=== UnifiedFloppyTool Forensic Imaging ===");
    uft_fi_log(job, UFT_FI_LOG_INFO, "Start time: %s", timebuf);
    uft_fi_log(job, UFT_FI_LOG_INFO, "Source: %s", job->input.path ? job->input.path : "(none)");
    uft_fi_log(job, UFT_FI_LOG_INFO, "Source size: %lu bytes", (unsigned long)job->input.size);
    uft_fi_log(job, UFT_FI_LOG_INFO, "Destination: %s", job->output.path ? job->output.path : "(none)");
    
    if (job->output.split) {
        uft_fi_log(job, UFT_FI_LOG_INFO, "Split size: %lu bytes", 
                   (unsigned long)job->output.split->max_bytes);
    }
    
    /* List enabled hashes - FIXED: Use snprintf instead of strcat */
    char hash_str[128];
    size_t hash_pos = 0;
    hash_str[0] = '\0';
    
    if (job->hash_flags & UFT_FI_HASH_MD5) {
        int n = snprintf(hash_str + hash_pos, sizeof(hash_str) - hash_pos, "MD5 ");
        if (n > 0 && (size_t)n < sizeof(hash_str) - hash_pos) hash_pos += (size_t)n;
    }
    if (job->hash_flags & UFT_FI_HASH_SHA1) {
        int n = snprintf(hash_str + hash_pos, sizeof(hash_str) - hash_pos, "SHA1 ");
        if (n > 0 && (size_t)n < sizeof(hash_str) - hash_pos) hash_pos += (size_t)n;
    }
    if (job->hash_flags & UFT_FI_HASH_SHA256) {
        int n = snprintf(hash_str + hash_pos, sizeof(hash_str) - hash_pos, "SHA256 ");
        if (n > 0 && (size_t)n < sizeof(hash_str) - hash_pos) hash_pos += (size_t)n;
    }
    if (job->hash_flags & UFT_FI_HASH_SHA384) {
        int n = snprintf(hash_str + hash_pos, sizeof(hash_str) - hash_pos, "SHA384 ");
        if (n > 0 && (size_t)n < sizeof(hash_str) - hash_pos) hash_pos += (size_t)n;
    }
    if (job->hash_flags & UFT_FI_HASH_SHA512) {
        int n = snprintf(hash_str + hash_pos, sizeof(hash_str) - hash_pos, "SHA512 ");
        if (n > 0 && (size_t)n < sizeof(hash_str) - hash_pos) hash_pos += (size_t)n;
    }
    
    if (hash_str[0]) {
        uft_fi_log(job, UFT_FI_LOG_INFO, "Hashing: %s", hash_str);
    }
    
    uft_fi_log(job, UFT_FI_LOG_INFO, "Recovery: %s (retries: %d)", 
               job->recovery.enable_recovery ? "enabled" : "disabled",
               job->recovery.max_retries);
}

void uft_fi_log_footer(uft_fi_job_t *job) {
    if (!job) return;
    
    time_t now = time(NULL);
    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    uft_fi_log(job, UFT_FI_LOG_INFO, "=== Imaging Complete ===");
    uft_fi_log(job, UFT_FI_LOG_INFO, "End time: %s", timebuf);
    uft_fi_log(job, UFT_FI_LOG_INFO, "Bytes read: %lu", (unsigned long)job->progress.bytes_read);
    uft_fi_log(job, UFT_FI_LOG_INFO, "Bytes written: %lu", (unsigned long)job->progress.bytes_written);
    uft_fi_log(job, UFT_FI_LOG_INFO, "Bad sectors: %lu", (unsigned long)job->progress.bad_sectors);
    uft_fi_log(job, UFT_FI_LOG_INFO, "Recovered sectors: %lu", (unsigned long)job->progress.recovered_sectors);
    
    /* Log hash results */
    uft_fi_hash_output_t *ho = job->hash_outputs;
    while (ho) {
        if (ho->total_hash && ho->total_hash->result[0]) {
            uft_fi_log(job, UFT_FI_LOG_INFO, "%s: %s", 
                       ho->algorithm->name, ho->total_hash->result);
        }
        ho = ho->next;
    }
    
    const char *status = "Unknown";
    switch (job->exit_code) {
        case UFT_FI_EXIT_SUCCESS: status = "SUCCESS"; break;
        case UFT_FI_EXIT_COMPLETED: status = "COMPLETED"; break;
        case UFT_FI_EXIT_PARTIAL: status = "PARTIAL (with errors)"; break;
        case UFT_FI_EXIT_ABORTED: status = "ABORTED"; break;
        case UFT_FI_EXIT_FAILED: status = "FAILED"; break;
        case UFT_FI_EXIT_VERIFY_FAIL: status = "VERIFICATION FAILED"; break;
        default: /* status already "Unknown" */ break;
    }
    
    uft_fi_log(job, UFT_FI_LOG_INFO, "Status: %s", status);
}

void uft_fi_log_bad_sector(uft_fi_job_t *job, uint64_t sector, int error) {
    if (!job) return;
    
    uft_fi_bad_sector_t *bs = calloc(1, sizeof(uft_fi_bad_sector_t));
    if (!bs) return;
    
    bs->sector_number = sector;
    bs->lba_offset = sector * job->input.sector_size;
    bs->error_code = error;
    bs->timestamp = time(NULL);
    
    /* Add to list (prepend) */
    bs->next = job->bad_sector_list;
    job->bad_sector_list = bs;
    
    job->progress.bad_sectors++;
    
    uft_fi_log(job, UFT_FI_LOG_WARN, 
               "Bad sector %lu (offset 0x%lx): error %d",
               (unsigned long)sector, (unsigned long)bs->lba_offset, error);
}

const uft_fi_bad_sector_t* uft_fi_get_bad_sectors(const uft_fi_job_t *job) {
    return job ? job->bad_sector_list : NULL;
}

int uft_fi_export_bad_map(const uft_fi_job_t *job, const char *path) {
    if (!job || !path) return -1;
    
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    
    fprintf(f, "# UnifiedFloppyTool Bad Sector Map\n");
    fprintf(f, "# Source: %s\n", job->input.path ? job->input.path : "unknown");
    fprintf(f, "# Format: sector_number,byte_offset,error_code\n");
    
    const uft_fi_bad_sector_t *bs = job->bad_sector_list;
    uint64_t count = 0;
    
    while (bs) {
        fprintf(f, "%lu,%lu,%d\n",
                (unsigned long)bs->sector_number,
                (unsigned long)bs->lba_offset,
                bs->error_code);
        count++;
        bs = bs->next;
    }
    
    fprintf(f, "# Total: %lu bad sectors\n", (unsigned long)count);
    fclose(f);
    
    return 0;
}

/*===========================================================================
 * ERROR RECOVERY (dd_rescue style)
 *===========================================================================*/

uft_fi_error_t uft_fi_read_recover(uft_fi_job_t *job, uint8_t *buf,
                                    uint64_t offset, size_t len, size_t *actual) {
    if (!job || !buf || !actual) return UFT_FI_ERR_INVALID;
    
    *actual = 0;
    
    if (job->input.fd < 0) return UFT_FI_ERR_INVALID;
    
    /* Try normal read first */
    if (lseek(job->input.fd, offset, SEEK_SET) < 0) {
        return UFT_FI_ERR_SEEK;
    }
    
    ssize_t rd = read(job->input.fd, buf, len);
    
    if (rd >= 0 && (size_t)rd == len) {
        *actual = rd;
        return UFT_FI_ERR_SUCCESS;
    }
    
    /* Partial read or error - attempt recovery if enabled */
    if (!job->recovery.enable_recovery) {
        if (rd > 0) *actual = rd;
        return UFT_FI_ERR_IO;
    }
    
    uft_fi_log(job, UFT_FI_LOG_INFO, 
               "Read error at offset %lu (got %zd of %zu), attempting sector recovery",
               (unsigned long)offset, rd, len);
    
    /* Reduce to hard block size and try sector by sector */
    size_t hard_bs = job->recovery.hard_blocksize;
    size_t recovered = 0;
    size_t bad_count = 0;
    
    for (size_t off = 0; off < len; off += hard_bs) {
        size_t chunk = (len - off > hard_bs) ? hard_bs : (len - off);
        bool sector_ok = false;
        
        for (int retry = 0; retry <= job->recovery.max_retries; retry++) {
            if (lseek(job->input.fd, offset + off, SEEK_SET) < 0) {
                continue;
            }
            
            rd = read(job->input.fd, buf + off, chunk);
            
            if (rd == (ssize_t)chunk) {
                sector_ok = true;
                recovered += chunk;
                if (retry > 0) {
                    job->progress.recovered_sectors++;
                }
                break;
            }
            
            /* Wait before retry */
            if (retry < job->recovery.max_retries && job->recovery.retry_delay_ms > 0) {
                usleep(job->recovery.retry_delay_ms * 1000);
            }
        }
        
        if (!sector_ok) {
            /* Fill bad sector with pattern */
            if (job->recovery.fill_pattern) {
                memset(buf + off, job->recovery.fill_byte, chunk);
            }
            
            /* Log bad sector */
            uint64_t sector = (offset + off) / job->input.sector_size;
            uft_fi_log_bad_sector(job, sector, errno);
            
            recovered += chunk;  /* Count as "recovered" (filled) */
            bad_count++;
        }
    }
    
    *actual = recovered;
    return (bad_count > 0) ? UFT_FI_ERR_CRC : UFT_FI_ERR_SUCCESS;
}

/*===========================================================================
 * SPLIT FILE WRITE (from dcfldd)
 *===========================================================================*/

static int split_open_next(uft_fi_split_ctx_t *split) {
    if (!split || !split->base_name || !split->format) return -1;
    
    /* Close current file if open */
    if (split->current_fd >= 0) {
        close(split->current_fd);
        split->current_fd = -1;
    }
    
    /* Generate filename */
    int splitnum = split->total_bytes / split->max_bytes;
    char ext[32];
    
    if (uft_fi_split_extension(split->format, splitnum, ext, sizeof(ext)) < 0) {
        return -1;
    }
    
    char filename[1024];
    snprintf(filename, sizeof(filename), "%s.%s", split->base_name, ext);
    
    /* Open new file */
    mode_t perms = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
    split->current_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, perms);
    
    if (split->current_fd < 0) {
        return -1;
    }
    
    split->current_bytes = 0;
    split->split_count++;
    
    return 0;
}

static ssize_t split_write(uft_fi_split_ctx_t *split, const uint8_t *buf, size_t len) {
    if (!split) return -1;
    
    /* Open first file if needed */
    if (split->current_fd < 0 || split->current_bytes >= split->max_bytes) {
        if (split_open_next(split) < 0) {
            return -1;
        }
    }
    
    size_t written = 0;
    
    while (written < len) {
        size_t left_in_file = split->max_bytes - split->current_bytes;
        size_t to_write = (len - written > left_in_file) ? left_in_file : (len - written);
        
        ssize_t wr = write(split->current_fd, buf + written, to_write);
        if (wr < 0) {
            return -1;
        }
        
        written += wr;
        split->current_bytes += wr;
        split->total_bytes += wr;
        
        /* Open next file if current is full */
        if (split->current_bytes >= split->max_bytes && written < len) {
            if (split_open_next(split) < 0) {
                return written;
            }
        }
    }
    
    return written;
}

/*===========================================================================
 * HASH OPERATIONS (stub - requires linking with crypto library)
 *===========================================================================*/

int uft_fi_hash_init(uft_fi_job_t *job) {
    if (!job) return -1;
    
    /* TODO: Initialize hash contexts based on job->hash_flags */
    /* This would use OpenSSL, libgcrypt, or built-in implementations */
    
    return 0;
}

void uft_fi_hash_update(uft_fi_job_t *job, const void *data, size_t len) {
    if (!job || !data || !len) return;
    
    /* TODO: Update all active hash contexts */
    uft_fi_hash_output_t *ho = job->hash_outputs;
    while (ho) {
        if (ho->algorithm && ho->total_hash && ho->algorithm->update) {
            ho->algorithm->update(ho->total_hash->context, data, len);
            ho->total_hash->bytes_hashed += len;
        }
        ho = ho->next;
    }
}

void uft_fi_hash_finalize(uft_fi_job_t *job) {
    if (!job) return;
    
    /* TODO: Finalize all active hash contexts and generate results */
    uft_fi_hash_output_t *ho = job->hash_outputs;
    while (ho) {
        if (ho->algorithm && ho->total_hash && ho->algorithm->finish) {
            ho->algorithm->finish(ho->total_hash->context, ho->total_hash->sum);
            uft_fi_hash_to_hex(ho->total_hash->sum, ho->algorithm->sum_size,
                               ho->total_hash->result);
        }
        ho = ho->next;
    }
}

void uft_fi_hash_cleanup(uft_fi_job_t *job) {
    if (!job) return;
    
    uft_fi_hash_output_t *ho = job->hash_outputs;
    while (ho) {
        uft_fi_hash_output_t *next = ho->next;
        
        if (ho->total_hash) {
            free(ho->total_hash->context);
            free(ho->total_hash);
        }
        if (ho->window_hash) {
            free(ho->window_hash->context);
            free(ho->window_hash);
        }
        
        free(ho);
        ho = next;
    }
    
    job->hash_outputs = NULL;
}

/*===========================================================================
 * MAIN IMAGING EXECUTION
 *===========================================================================*/

uft_fi_exit_code_t uft_fi_execute(uft_fi_job_t *job) {
    if (!job) return UFT_FI_EXIT_FAILED;
    
    /* Open input */
    if (!job->input.path) {
        uft_fi_log(job, UFT_FI_LOG_FATAL, "No input path specified");
        return UFT_FI_EXIT_FAILED;
    }
    
    job->input.fd = open(job->input.path, O_RDONLY);
    if (job->input.fd < 0) {
        uft_fi_log(job, UFT_FI_LOG_FATAL, "Cannot open input: %s", strerror(errno));
        return UFT_FI_EXIT_FAILED;
    }
    
    /* Probe input size */
    struct stat st;
    if (fstat(job->input.fd, &st) == 0) {
        job->input.is_device = S_ISBLK(st.st_mode);
    }
    job->input.size = uft_fi_probe_size(job->input.fd, job->input.is_device);
    
    /* Open output */
    if (job->output.path) {
        if (job->output.split) {
            job->output.split->base_name = strdup(job->output.path);
        } else {
            mode_t perms = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
            int flags = O_WRONLY | O_CREAT;
            if (!job->output.append) flags |= O_TRUNC;
            
            job->output.fd = open(job->output.path, flags, perms);
            if (job->output.fd < 0) {
                uft_fi_log(job, UFT_FI_LOG_FATAL, "Cannot open output: %s", strerror(errno));
                close(job->input.fd);
                return UFT_FI_EXIT_FAILED;
            }
        }
    }
    
    /* Open log file */
    if (job->log_path) {
        job->log_fd = open(job->log_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    }
    
    /* Initialize hashing */
    uft_fi_hash_init(job);
    
    /* Log header */
    uft_fi_log_header(job);
    
    /* Initialize progress */
    job->progress.start_time = time(NULL);
    job->progress.sectors_total = job->input.size / job->input.sector_size;
    job->state = UFT_FI_STATE_ACTIVE;
    
    /* Allocate buffer */
    size_t bufsize = job->recovery.soft_blocksize;
    uint8_t *buffer = malloc(bufsize);
    if (!buffer) {
        uft_fi_log(job, UFT_FI_LOG_FATAL, "Cannot allocate buffer");
        job->exit_code = UFT_FI_EXIT_FAILED;
        goto cleanup;
    }
    
    /* Main copy loop */
    uint64_t offset = job->input.skip_sectors * job->input.sector_size;
    uint64_t max_bytes = job->input.max_sectors ? 
                         job->input.max_sectors * job->input.sector_size : 
                         job->input.size;
    
    while (offset < max_bytes && !job->progress.interrupted) {
        size_t to_read = bufsize;
        if (offset + to_read > max_bytes) {
            to_read = max_bytes - offset;
        }
        
        size_t actual = 0;
        uft_fi_error_t err = uft_fi_read_recover(job, buffer, offset, to_read, &actual);
        
        if (actual > 0) {
            /* Update hashes */
            uft_fi_hash_update(job, buffer, actual);
            
            /* Write output */
            ssize_t written = 0;
            if (job->output.split) {
                written = split_write(job->output.split, buffer, actual);
            } else if (job->output.fd >= 0) {
                written = write(job->output.fd, buffer, actual);
            }
            
            if (written < 0) {
                uft_fi_log(job, UFT_FI_LOG_FATAL, "Write error: %s", strerror(errno));
                job->exit_code = UFT_FI_EXIT_FAILED;
                break;
            }
            
            job->progress.bytes_read += actual;
            job->progress.bytes_written += written;
            job->progress.sectors_processed = job->progress.bytes_read / job->input.sector_size;
            
            offset += actual;
        }
        
        if (err != UFT_FI_ERR_SUCCESS && err != UFT_FI_ERR_CRC) {
            /* Fatal error */
            job->exit_code = UFT_FI_EXIT_FAILED;
            break;
        }
        
        /* Update progress callback */
        if (job->progress_callback) {
            time_t now = time(NULL);
            double elapsed = difftime(now, job->progress.start_time);
            if (elapsed > 0) {
                job->progress.transfer_rate = job->progress.bytes_read / elapsed;
            }
            job->progress_callback(&job->progress, job->user_data);
        }
    }
    
    free(buffer);
    
    /* Finalize hashes */
    uft_fi_hash_finalize(job);
    
    /* Determine exit code */
    if (job->progress.interrupted) {
        job->exit_code = UFT_FI_EXIT_ABORTED;
    } else if (job->progress.bad_sectors > 0) {
        job->exit_code = UFT_FI_EXIT_PARTIAL;
    } else if (job->exit_code == UFT_FI_EXIT_SUCCESS) {
        job->exit_code = UFT_FI_EXIT_COMPLETED;
    }
    
cleanup:
    job->state = UFT_FI_STATE_COMPLETE;
    
    /* Log footer */
    uft_fi_log_footer(job);
    
    /* Close files */
    if (job->input.fd >= 0) {
        close(job->input.fd);
        job->input.fd = -1;
    }
    if (job->output.fd >= 0) {
        fsync(job->output.fd);
        close(job->output.fd);
        job->output.fd = -1;
    }
    if (job->output.split && job->output.split->current_fd >= 0) {
        fsync(job->output.split->current_fd);
        close(job->output.split->current_fd);
        job->output.split->current_fd = -1;
    }
    
    return job->exit_code;
}

void uft_fi_cancel(uft_fi_job_t *job) {
    if (job) {
        job->progress.interrupted = true;
    }
}

/*===========================================================================
 * GUI INTEGRATION
 *===========================================================================*/

uft_fi_job_t* uft_fi_job_from_gui(const uft_fi_gui_params_t *params) {
    if (!params) return NULL;
    
    uft_fi_job_t *job = uft_fi_job_new();
    if (!job) return NULL;
    
    /* Source */
    job->input.path = strdup(params->source_path);
    job->input.is_device = params->source_is_device;
    job->input.sector_size = params->source_sector_size ? 
                             params->source_sector_size : UFT_FI_DEFAULT_SECTOR_SZ;
    job->input.skip_sectors = params->skip_input_sectors;
    job->input.max_sectors = params->max_sectors;
    
    /* Destination */
    job->output.path = strdup(params->dest_path);
    job->output.skip_sectors = params->skip_output_sectors;
    job->output.verify_mode = params->verify_mode;
    
    /* Split */
    if (params->dest_split && params->dest_split_size > 0) {
        uft_fi_set_split(job, params->dest_split_size, 
                         params->dest_split_format[0] ? params->dest_split_format : NULL);
    }
    
    /* Hashing */
    job->hash_flags = UFT_FI_HASH_NONE;
    if (params->hash_md5) job->hash_flags |= UFT_FI_HASH_MD5;
    if (params->hash_sha1) job->hash_flags |= UFT_FI_HASH_SHA1;
    if (params->hash_sha256) job->hash_flags |= UFT_FI_HASH_SHA256;
    if (params->hash_sha384) job->hash_flags |= UFT_FI_HASH_SHA384;
    if (params->hash_sha512) job->hash_flags |= UFT_FI_HASH_SHA512;
    job->hash_window_size = params->hash_window_size;
    
    /* Recovery */
    job->recovery.enable_recovery = params->recovery_enabled;
    job->recovery.max_retries = params->recovery_retries;
    job->recovery.fill_pattern = params->recovery_fill_zeros;
    job->recovery.fill_byte = 0x00;
    
    /* Logging */
    if (params->log_path[0]) {
        job->log_path = strdup(params->log_path);
    }
    job->log_level = params->log_verbose ? UFT_FI_LOG_DEBUG : UFT_FI_LOG_INFO;
    
    return job;
}

void uft_fi_get_gui_status(const uft_fi_job_t *job, uft_fi_gui_status_t *status) {
    if (!job || !status) return;
    
    memset(status, 0, sizeof(*status));
    
    status->state = job->state;
    status->bytes_processed = job->progress.bytes_read;
    status->bytes_total = job->input.size;
    status->bad_sectors = job->progress.bad_sectors;
    status->transfer_rate_mbps = job->progress.transfer_rate / (1024.0 * 1024.0);
    
    if (status->bytes_total > 0) {
        status->percent_complete = (int)((status->bytes_processed * 100) / 
                                         status->bytes_total);
    }
    
    int eta = uft_fi_calc_eta(&job->progress);
    uft_fi_format_eta(eta, status->eta_string, sizeof(status->eta_string));
    
    /* Copy current hash results if available */
    uft_fi_hash_output_t *ho = job->hash_outputs;
    while (ho) {
        if (ho->algorithm && ho->total_hash && ho->total_hash->result[0]) {
            if (ho->algorithm->flag == UFT_FI_HASH_MD5) {
                strncpy(status->current_hash_md5, ho->total_hash->result, 
                        sizeof(status->current_hash_md5) - 1);
            } else if (ho->algorithm->flag == UFT_FI_HASH_SHA1) {
                strncpy(status->current_hash_sha1, ho->total_hash->result,
                        sizeof(status->current_hash_sha1) - 1);
            } else if (ho->algorithm->flag == UFT_FI_HASH_SHA256) {
                strncpy(status->current_hash_sha256, ho->total_hash->result,
                        sizeof(status->current_hash_sha256) - 1);
            }
        }
        ho = ho->next;
    }
    
    /* Status message */
    switch (job->state) {
        case UFT_FI_STATE_PENDING:
            snprintf(status->status_message, sizeof(status->status_message),
                     "Waiting to start...");
            break;
        case UFT_FI_STATE_ACTIVE:
            snprintf(status->status_message, sizeof(status->status_message),
                     "Imaging... %d%% complete", status->percent_complete);
            break;
        case UFT_FI_STATE_COMPLETE:
            snprintf(status->status_message, sizeof(status->status_message),
                     "Complete! %lu bad sectors", 
                     (unsigned long)status->bad_sectors);
            break;
        case UFT_FI_STATE_ERROR:
            snprintf(status->status_message, sizeof(status->status_message),
                     "Error occurred");
            break;
        case UFT_FI_STATE_ABORTED:
            snprintf(status->status_message, sizeof(status->status_message),
                     "Cancelled by user");
            break;
        default:
            break;
    }
}
