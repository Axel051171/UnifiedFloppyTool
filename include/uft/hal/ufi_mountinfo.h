/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file ufi_mountinfo.h
 * @brief Is a block device (or one of its partitions) mounted? — read from
 *        the text of /proc/self/mountinfo (P3-669, MF-1516).
 *
 * ── Why ──────────────────────────────────────────────────────────────────
 *
 * src/hal/ufi_linux.c opened a USB floppy with O_RDWR and nothing else, and
 * its write path is wired into the product (hardwaretab.cpp -> ufi_runners.
 * cpp). A drive the desktop auto-mounted was therefore written to past the
 * mounted file system — the kernel's page cache and the disk disagree, and
 * whichever writes last wins. Found by the review of the package
 * UFT_LinuxSectorImager_v1 (neue-ideen, t4).
 *
 * The first guard is the kernel's own: open(2), "on Linux 2.6 and later,
 * O_EXCL can be used without O_CREAT if path refers to a block device. If
 * the block device is in use by the system (e.g., mounted), open() fails
 * with the error EBUSY." ufi_linux.c now opens with O_EXCL.
 *
 * This module is the second guard, for what O_EXCL on the WHOLE disk is not
 * documented to cover: a mounted PARTITION of it. It is a pure function over
 * the TEXT of mountinfo, so it is tested on every platform without hardware
 * (tests/test_ufi_mountinfo.c); the Linux caller feeds it the real file.
 *
 * ── Reference ────────────────────────────────────────────────────────────
 *
 * proc_pid_mountinfo(5): one line per mount, fields separated by spaces;
 * field (3) is "major:minor: the value of st_dev for files on this
 * filesystem", field (5) the mount point; "the end of the optional fields
 * is marked by a single hyphen". Example from the page:
 *
 *   36 35 98:0 /mnt1 /mnt2 rw,noatime master:1 - ext3 /dev/root rw,...
 *
 * Only fields (3) and (5) are read. major:minor is compared as NUMBERS of a
 * whole token — "8:160" is not "8:16".
 */
#ifndef UFT_HAL_UFI_MOUNTINFO_H
#define UFT_HAL_UFI_MOUNTINFO_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Resolves a device number to the WHOLE disk it belongs to. Returns true and
 * sets *pmaj and *pmin when (maj, min) is a partition; false when it is a whole
 * disk or unknown. On Linux the caller answers it from
 * /sys/dev/block/<maj>:<min>/../dev; tests pass a table.
 */
typedef bool (*uft_ufi_eltern_fn)(unsigned maj, unsigned min,
                                  unsigned *pmaj, unsigned *pmin, void *ctx);

/**
 * Is the device (maj, min) — or a partition of it — mounted according to
 * the mountinfo text?
 *
 * @param text      contents of /proc/self/mountinfo (NUL-terminated)
 * @param maj,min   the whole device, from st_rdev of the opened node
 * @param eltern    partition -> whole disk, may be NULL (then only an exact
 *                  device match counts)
 * @param ctx       passed through to eltern
 * @param punkt     out, optional: mount point of the first hit (raw, with
 *                  mountinfo's octal escapes such as \040 left as they are)
 * @param punkt_len size of punkt
 * @return true if mounted; false if not, or if text is NULL
 */
bool uft_ufi_mountinfo_belegt(const char *text, unsigned maj, unsigned min,
                              uft_ufi_eltern_fn eltern, void *ctx,
                              char *punkt, size_t punkt_len);

#ifdef __linux__
/**
 * The same question on the running system: reads /proc/self/mountinfo and
 * resolves partitions through sysfs (/sys/dev/block/<maj>:<min>/partition
 * marks a partition, "../dev" names its whole disk).
 *
 * @return 1 mounted (punkt filled if given), 0 not mounted, -1 unknown —
 *         mountinfo unreadable. A caller about to WRITE treats -1 as a
 *         refusal: an unknown mount state is not a free device.
 */
int uft_ufi_linux_belegt(unsigned maj, unsigned min, char *punkt, size_t punkt_len);
#endif

#ifdef __cplusplus
}
#endif

#endif /* UFT_HAL_UFI_MOUNTINFO_H */
