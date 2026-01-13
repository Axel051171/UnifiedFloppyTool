/**
 * @file uft_usb_qt_bridge.cpp
 * @brief Qt Bridge Implementation
 */

#include "uft_usb_qt_bridge.h"
#include "uft_usb_device.h"

#include <QDebug>

UftUsbManager::UftUsbManager(QObject *parent)
    : QObject(parent)
    , m_autoDetectTimer(new QTimer(this))
{
    connect(m_autoDetectTimer, &QTimer::timeout,
            this, &UftUsbManager::onAutoDetectTimer);
}

UftUsbManager::~UftUsbManager()
{
    stopAutoDetection();
}

QList<UftUsbDeviceInfo> UftUsbManager::enumerateAll()
{
    QList<UftUsbDeviceInfo> result;
    
    uft_usb_device_info_t devices[32];
    int count = uft_usb_enumerate(devices, 32);
    
    for (int i = 0; i < count; i++) {
        UftUsbDeviceInfo info;
        info.vendorId = devices[i].vendor_id;
        info.productId = devices[i].product_id;
        info.serialNumber = QString::fromUtf8(devices[i].serial_number);
        info.portName = QString::fromUtf8(devices[i].port_name);
        info.manufacturer = QString::fromUtf8(devices[i].manufacturer);
        info.name = QString::fromUtf8(uft_usb_type_name(devices[i].type));
        info.isConnected = true;
        
        switch (devices[i].iface_type) {
            case UFT_USB_IFACE_CDC: info.connectionType = "CDC"; break;
            case UFT_USB_IFACE_BULK: info.connectionType = "Bulk"; break;
            case UFT_USB_IFACE_WINUSB: info.connectionType = "WinUSB"; break;
            default: info.connectionType = "Unknown"; break;
        }
        
        switch (devices[i].type) {
            case UFT_USB_TYPE_GREASEWEAZLE: info.deviceType = "Greaseweazle"; break;
            case UFT_USB_TYPE_FLUXENGINE: info.deviceType = "FluxEngine"; break;
            case UFT_USB_TYPE_KRYOFLUX: info.deviceType = "KryoFlux"; break;
            case UFT_USB_TYPE_SUPERCARD_PRO: info.deviceType = "SuperCard Pro"; break;
            case UFT_USB_TYPE_FC5025: info.deviceType = "FC5025"; break;
            case UFT_USB_TYPE_XUM1541: info.deviceType = "XUM1541"; break;
            default: info.deviceType = "Unknown"; break;
        }
        
        result.append(info);
    }
    
    return result;
}

QList<UftUsbDeviceInfo> UftUsbManager::enumerateFloppyControllers()
{
    QList<UftUsbDeviceInfo> result;
    
    uft_usb_device_info_t devices[32];
    int count = uft_usb_enumerate_floppy_controllers(devices, 32);
    
    for (int i = 0; i < count; i++) {
        UftUsbDeviceInfo info;
        info.vendorId = devices[i].vendor_id;
        info.productId = devices[i].product_id;
        info.serialNumber = QString::fromUtf8(devices[i].serial_number);
        info.portName = QString::fromUtf8(devices[i].port_name);
        info.name = QString::fromUtf8(uft_usb_type_name(devices[i].type));
        info.isConnected = true;
        result.append(info);
    }
    
    return result;
}

UftUsbDeviceInfo UftUsbManager::findGreaseweazle()
{
    uft_usb_device_info_t dev;
    UftUsbDeviceInfo info;
    
    if (uft_usb_find_by_type(UFT_USB_TYPE_GREASEWEAZLE, &dev)) {
        info.vendorId = dev.vendor_id;
        info.productId = dev.product_id;
        info.portName = QString::fromUtf8(dev.port_name);
        info.serialNumber = QString::fromUtf8(dev.serial_number);
        info.name = "Greaseweazle";
        info.deviceType = "Greaseweazle";
        info.connectionType = "CDC";
        info.isConnected = true;
    }
    
    return info;
}

UftUsbDeviceInfo UftUsbManager::findFluxEngine()
{
    uft_usb_device_info_t dev;
    UftUsbDeviceInfo info;
    
    if (uft_usb_find_by_type(UFT_USB_TYPE_FLUXENGINE, &dev)) {
        info.vendorId = dev.vendor_id;
        info.productId = dev.product_id;
        info.portName = QString::fromUtf8(dev.port_name);
        info.serialNumber = QString::fromUtf8(dev.serial_number);
        info.name = "FluxEngine";
        info.deviceType = "FluxEngine";
        info.connectionType = "Bulk";
        info.isConnected = true;
    }
    
    return info;
}

UftUsbDeviceInfo UftUsbManager::findKryoFlux()
{
    uft_usb_device_info_t dev;
    UftUsbDeviceInfo info;
    
    if (uft_usb_find_by_type(UFT_USB_TYPE_KRYOFLUX, &dev)) {
        info.vendorId = dev.vendor_id;
        info.productId = dev.product_id;
        info.name = "KryoFlux";
        info.deviceType = "KryoFlux";
        info.connectionType = "Bulk";
        info.isConnected = true;
    }
    
    return info;
}

UftUsbDeviceInfo UftUsbManager::findSuperCardPro()
{
    uft_usb_device_info_t dev;
    UftUsbDeviceInfo info;
    
    if (uft_usb_find_by_type(UFT_USB_TYPE_SUPERCARD_PRO, &dev)) {
        info.vendorId = dev.vendor_id;
        info.productId = dev.product_id;
        info.portName = QString::fromUtf8(dev.port_name);
        info.name = "SuperCard Pro";
        info.deviceType = "SuperCard Pro";
        info.connectionType = "CDC";
        info.isConnected = true;
    }
    
    return info;
}

QString UftUsbManager::getPortName(quint16 vid, quint16 pid)
{
    char portName[64] = {0};
    if (uft_usb_get_port_name(vid, pid, portName, sizeof(portName))) {
        return QString::fromUtf8(portName);
    }
    return QString();
}

void UftUsbManager::startAutoDetection(int intervalMs)
{
    m_lastDevices = enumerateFloppyControllers();
    m_autoDetectTimer->start(intervalMs);
    qDebug() << "USB auto-detection started, interval:" << intervalMs << "ms";
}

void UftUsbManager::stopAutoDetection()
{
    m_autoDetectTimer->stop();
}

bool UftUsbManager::isAutoDetecting() const
{
    return m_autoDetectTimer->isActive();
}

void UftUsbManager::onAutoDetectTimer()
{
    QList<UftUsbDeviceInfo> currentDevices = enumerateFloppyControllers();
    compareAndEmitChanges(currentDevices);
    emit scanComplete(currentDevices.count());
}

void UftUsbManager::compareAndEmitChanges(const QList<UftUsbDeviceInfo> &newDevices)
{
    bool changed = false;
    
    // Check for disconnected devices
    for (const auto &old : m_lastDevices) {
        bool found = false;
        for (const auto &curr : newDevices) {
            if (old.vendorId == curr.vendorId && 
                old.productId == curr.productId &&
                old.serialNumber == curr.serialNumber) {
                found = true;
                break;
            }
        }
        if (!found) {
            emit deviceDisconnected(old);
            changed = true;
        }
    }
    
    // Check for newly connected devices
    for (const auto &curr : newDevices) {
        bool found = false;
        for (const auto &old : m_lastDevices) {
            if (old.vendorId == curr.vendorId && 
                old.productId == curr.productId &&
                old.serialNumber == curr.serialNumber) {
                found = true;
                break;
            }
        }
        if (!found) {
            emit deviceConnected(curr);
            changed = true;
        }
    }
    
    if (changed) {
        emit devicesChanged(newDevices);
    }
    
    m_lastDevices = newDevices;
}
