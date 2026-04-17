/**
 * @file unified_hal_bridge.cpp
 * @brief Qt↔C-HAL bridge. See unified_hal_bridge.h.
 */
#include "unified_hal_bridge.h"

#include <QtGlobal>

#include <cstring>

extern "C" {
#include "uft/uft_error.h"
#include "uft/uft_progress.h"
}

namespace uft_hal_bridge {

TrackData toTrackData(const uft_track_t &src)
{
    TrackData td;
    td.cylinder = src.cylinder;
    td.head     = src.head;

    if (src.raw_data && src.raw_size > 0) {
        td.data = QByteArray(reinterpret_cast<const char *>(src.raw_data),
                             static_cast<int>(src.raw_size));
    }
    if (src.flux && src.flux_count > 0) {
        td.rawFlux = QByteArray(reinterpret_cast<const char *>(src.flux),
                                static_cast<int>(src.flux_count * sizeof(uint32_t)));
    }

    td.success = (src.error == 0);
    td.valid   = td.success;
    if (!td.success) {
        td.error        = QStringLiteral("HAL error %1").arg(src.error);
        td.errorMessage = td.error;
    }
    td.goodSectors = 0;
    td.badSectors  = src.errors;
    return td;
}

QVector<TrackData> readDiskViaUnifiedHal(uft_hw_device_t *device,
                                         int startCyl, int endCyl,
                                         int heads, int revolutions)
{
    QVector<TrackData> result;
    if (!device || endCyl <= startCyl) return result;

    const int head_mask = (heads >= 2) ? 0x03 : 0x01;
    const int track_count = (endCyl - startCyl) * ((head_mask == 0x03) ? 2 : 1);

    QVector<uft_track_t> tracks(track_count);
    std::memset(tracks.data(), 0, sizeof(uft_track_t) * track_count);
    for (auto &t : tracks) {
        t.cylinder = -1;
        t.head     = -1;
    }

    const uint8_t rev = (revolutions > 0 && revolutions <= 255)
                            ? static_cast<uint8_t>(revolutions) : 0;

    uft_error_t err = uft_hw_read_tracks(device, startCyl, endCyl, head_mask,
                                         tracks.data(), rev, nullptr, nullptr);
    if (err != UFT_OK) {
        qWarning("readDiskViaUnifiedHal: uft_hw_read_tracks failed (err=%d)", err);
        return result;
    }

    result.reserve(track_count);
    for (const auto &t : tracks)
        result.append(toTrackData(t));
    return result;
}

} // namespace uft_hal_bridge
