#include "hardwaremanager.h"

#include <QDebug>

// All hardware providers
#include "mockhardwareprovider.h"
#include "greaseweazlehardwareprovider.h"
#include "fluxenginehardwareprovider.h"
#include "kryofluxhardwareprovider.h"
#include "scphardwareprovider.h"
#include "applesaucehardwareprovider.h"
#include "fc5025hardwareprovider.h"
#include "xum1541hardwareprovider.h"
#include "catweaselhardwareprovider.h"
#include "adfcopyhardwareprovider.h"
#include "usbfloppyhardwareprovider.h"

/**
 * @brief RAII guard that sets m_operationActive for the duration of a scope.
 *
 * On construction it locks m_hwMutex, sets m_operationActive = true, then
 * unlocks. On destruction it locks again, clears the flag, and unlocks.
 */
class HwOperationGuard {
public:
    explicit HwOperationGuard(QMutex &mutex, bool &flag)
        : m_mutex(mutex), m_flag(flag)
    {
        QMutexLocker lk(&m_mutex);
        m_flag = true;
    }
    ~HwOperationGuard()
    {
        QMutexLocker lk(&m_mutex);
        m_flag = false;
    }
private:
    QMutex &m_mutex;
    bool   &m_flag;
};

HardwareManager::HardwareManager(QObject *parent)
    : QObject(parent)
{
    // Default to Greaseweazle provider (most common)
    setProvider(std::make_unique<GreaseweazleHardwareProvider>(this));
}

bool HardwareManager::setHardwareType(const QString &hardwareType)
{
    {
        QMutexLocker locker(&m_hwMutex);
        if (m_operationActive) {
            qWarning() << "HardwareManager: cannot change hardware type while operation is active";
            return false;
        }
    }

    m_hardwareType = hardwareType;

    // Switch provider based on type
    if (hardwareType.contains("Mock", Qt::CaseInsensitive) ||
        hardwareType.contains("Test", Qt::CaseInsensitive)) {
        setProvider(std::make_unique<MockHardwareProvider>(this));
    } else if (hardwareType.contains("Greaseweazle", Qt::CaseInsensitive)) {
        setProvider(std::make_unique<GreaseweazleHardwareProvider>(this));
    } else if (hardwareType.contains("FluxEngine", Qt::CaseInsensitive)) {
        setProvider(std::make_unique<FluxEngineHardwareProvider>(this));
    } else if (hardwareType.contains("KryoFlux", Qt::CaseInsensitive)) {
        setProvider(std::make_unique<KryoFluxHardwareProvider>(this));
    } else if (hardwareType.contains("SuperCard", Qt::CaseInsensitive) ||
               hardwareType.contains("SCP", Qt::CaseInsensitive)) {
        setProvider(std::make_unique<SCPHardwareProvider>(this));
    } else if (hardwareType.contains("Applesauce", Qt::CaseInsensitive)) {
        setProvider(std::make_unique<ApplesauceHardwareProvider>(this));
    } else if (hardwareType.contains("FC5025", Qt::CaseInsensitive)) {
        setProvider(std::make_unique<FC5025HardwareProvider>(this));
    } else if (hardwareType.contains("XUM1541", Qt::CaseInsensitive) ||
               hardwareType.contains("ZoomFloppy", Qt::CaseInsensitive)) {
        setProvider(std::make_unique<Xum1541HardwareProvider>(this));
    } else if (hardwareType.contains("Catweasel", Qt::CaseInsensitive)) {
        setProvider(std::make_unique<CatweaselHardwareProvider>(this));
    } else if (hardwareType.contains("ADF-Copy", Qt::CaseInsensitive) ||
               hardwareType.contains("ADFCopy", Qt::CaseInsensitive) ||
               hardwareType.contains("ADF-Drive", Qt::CaseInsensitive) ||
               hardwareType.contains("adfcopy", Qt::CaseInsensitive)) {
        setProvider(std::make_unique<ADFCopyHardwareProvider>(this));
    } else {
        // Default to Greaseweazle for unknown types
        setProvider(std::make_unique<GreaseweazleHardwareProvider>(this));
    }

    applySettingsToProvider();
    return true;
}

bool HardwareManager::isOperationActive() const
{
    QMutexLocker locker(&m_hwMutex);
    return m_operationActive;
}

void HardwareManager::setDevicePath(const QString &devicePath)
{
    m_devicePath = devicePath;
    if (m_provider) {
        m_provider->setDevicePath(devicePath);
    }
}

void HardwareManager::setBaudRate(int baudRate)
{
    m_baudRate = baudRate;
    if (m_provider) {
        m_provider->setBaudRate(baudRate);
    }
}

void HardwareManager::detectDrive()
{
    if (m_provider) {
        HwOperationGuard guard(m_hwMutex, m_operationActive);
        m_provider->detectDrive();
    }
}

void HardwareManager::autoDetectDevice()
{
    if (m_provider) {
        HwOperationGuard guard(m_hwMutex, m_operationActive);
        m_provider->autoDetectDevice();
    }
}

void HardwareManager::setProvider(std::unique_ptr<HardwareProvider> provider)
{
    // Disconnect old provider signals
    if (m_provider) {
        disconnect(m_provider.get(), nullptr, this, nullptr);
    }
    
    m_provider = std::move(provider);
    
    if (m_provider) {
        // Forward provider signals
        connect(m_provider.get(), &HardwareProvider::driveDetected,
                this, &HardwareManager::driveDetected);
        connect(m_provider.get(), &HardwareProvider::hardwareInfoUpdated,
                this, &HardwareManager::hardwareInfoUpdated);
        connect(m_provider.get(), &HardwareProvider::statusMessage,
                this, &HardwareManager::statusMessage);
        connect(m_provider.get(), &HardwareProvider::devicePathSuggested,
                this, &HardwareManager::devicePathSuggested);
    }
}

void HardwareManager::applySettingsToProvider()
{
    if (m_provider) {
        m_provider->setHardwareType(m_hardwareType);
        m_provider->setDevicePath(m_devicePath);
        m_provider->setBaudRate(m_baudRate);
    }
}
