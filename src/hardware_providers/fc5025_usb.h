/**
 * @file fc5025_usb.h
 * @brief FC5025 USB stub header for UnifiedFloppyTool
 * 
 * This is a placeholder. Real FC5025 support requires the 
 * Device Side Data FC5025 SDK/library.
 */

#ifndef FC5025_USB_H
#define FC5025_USB_H

#include <cstdint>

// Stub definitions - real implementation requires FC5025 SDK
namespace fc5025 {

struct DeviceHandle {
    void *handle = nullptr;
    bool connected = false;
};

inline bool init() { return false; }
inline void cleanup() {}
inline bool open(DeviceHandle &) { return false; }
inline void close(DeviceHandle &) {}
inline bool isConnected(const DeviceHandle &h) { return h.connected; }

} // namespace fc5025

#endif // FC5025_USB_H
