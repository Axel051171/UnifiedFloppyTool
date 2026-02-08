#include "hardwaremanager.h"

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

HardwareManager::HardwareManager(QObject *parent)
    : QObject(parent)
{
    // Default to Greaseweazle provider (most common)
    setProvider(std::make_unique<GreaseweazleHardwareProvider>(this));
}

void HardwareManager::setHardwareType(const QString &hardwareType)
{
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
    } else if (hardwareType.contains("ADF", Qt::CaseInsensitive)) {
        setProvider(std::make_unique<ADFCopyHardwareProvider>(this));
    } else {
        // Default to Greaseweazle for unknown types
        setProvider(std::make_unique<GreaseweazleHardwareProvider>(this));
    }
    
    applySettingsToProvider();
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
        m_provider->detectDrive();
    }
}

void HardwareManager::autoDetectDevice()
{
    if (m_provider) {
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
