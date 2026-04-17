/**
 * @file unified_hal_bridge.h
 * @brief Bridge between Qt HardwareProvider layer and the C HAL dispatcher.
 *
 * Phase H7 of the HAL unification. Lets any HardwareProvider that holds a
 * uft_hw_device_t* delegate its readDisk() to the unified dispatcher
 * uft_hw_read_tracks(), which transparently picks the backend's native
 * batch path or falls back to per-track reads.
 *
 * Ownership: the caller owns the device pointer. This bridge performs no
 * open/close — only read iteration and TrackData conversion.
 */
#ifndef UFT_UNIFIED_HAL_BRIDGE_H
#define UFT_UNIFIED_HAL_BRIDGE_H

#include "hardwareprovider.h"

extern "C" {
#include "uft/uft_hardware.h"
#include "uft/uft_format_plugin.h"
}

namespace uft_hal_bridge {

/**
 * @brief Convert a uft_track_t into a Qt TrackData snapshot.
 *
 * Copies cylinder/head/status plus any raw flux bytes the backend attached.
 * Sector-level data is not flattened — callers wanting decoded sectors
 * should run the format-plugin layer over the uft_track_t directly.
 */
TrackData toTrackData(const uft_track_t &src);

/**
 * @brief Run a disk capture via uft_hw_read_tracks() and return TrackData.
 *
 * @param device  Open HAL device; must not be NULL.
 * @param startCyl First cylinder (inclusive).
 * @param endCyl   Last cylinder (exclusive).
 * @param heads    1 or 2 (mapped to head_mask 0x01 / 0x03).
 * @param revolutions Multi-rev count (0 = backend default).
 * @return Vector of TrackData, one per captured track. Empty on failure.
 */
QVector<TrackData> readDiskViaUnifiedHal(uft_hw_device_t *device,
                                         int startCyl, int endCyl,
                                         int heads, int revolutions);

} // namespace uft_hal_bridge

#endif /* UFT_UNIFIED_HAL_BRIDGE_H */
