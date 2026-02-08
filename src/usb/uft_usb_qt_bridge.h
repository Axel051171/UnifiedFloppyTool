/**
 * @file uft_usb_qt_bridge.h
 * @brief Qt Bridge for USB Device Management
 * 
 * P1: Provides Qt-friendly interface to USB device enumeration
 * Integrates with Hardware Tab for automatic device discovery
 */

#ifndef UFT_USB_QT_BRIDGE_H
#define UFT_USB_QT_BRIDGE_H

#include <QObject>
#include <QString>
#include <QList>
#include <QTimer>

/**
 * @brief USB Device Information for Qt
 */
struct UftUsbDeviceInfo {
    QString name;
    QString portName;
    QString serialNumber;
    QString manufacturer;
    quint16 vendorId;
    quint16 productId;
    QString deviceType;      // "Greaseweazle", "FluxEngine", etc.
    QString connectionType;  // "CDC", "Bulk", "WinUSB"
    bool isConnected;
    
    QString displayName() const {
        if (!name.isEmpty()) return name;
        return QString("%1:%2").arg(vendorId, 4, 16, QChar('0'))
                               .arg(productId, 4, 16, QChar('0'));
    }
};

Q_DECLARE_METATYPE(UftUsbDeviceInfo)

/**
 * @brief USB Device Manager with Qt Signals
 */
class UftUsbManager : public QObject {
    Q_OBJECT
    
public:
    explicit UftUsbManager(QObject *parent = nullptr);
    ~UftUsbManager();
    
    // Device enumeration
    QList<UftUsbDeviceInfo> enumerateAll();
    QList<UftUsbDeviceInfo> enumerateFloppyControllers();
    
    // Find specific devices
    UftUsbDeviceInfo findGreaseweazle();
    UftUsbDeviceInfo findFluxEngine();
    UftUsbDeviceInfo findKryoFlux();
    UftUsbDeviceInfo findSuperCardPro();
    
    // Auto-detection
    void startAutoDetection(int intervalMs = 2000);
    void stopAutoDetection();
    bool isAutoDetecting() const;
    
    // Port name resolution
    QString getPortName(quint16 vid, quint16 pid);
    
signals:
    void deviceConnected(const UftUsbDeviceInfo &device);
    void deviceDisconnected(const UftUsbDeviceInfo &device);
    void devicesChanged(const QList<UftUsbDeviceInfo> &devices);
    void scanComplete(int deviceCount);
    
private slots:
    void onAutoDetectTimer();
    
private:
    QTimer *m_autoDetectTimer;
    QList<UftUsbDeviceInfo> m_lastDevices;
    
    void compareAndEmitChanges(const QList<UftUsbDeviceInfo> &newDevices);
};

#endif // UFT_USB_QT_BRIDGE_H
