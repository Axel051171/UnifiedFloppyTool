#include "hardwaremanager.h"

#include "mockhardwareprovider.h"

// Real providers (CLI-wrapper based)
#include "greaseweazlehardwareprovider.h"
#include "fc5025hardwareprovider.h"
#include "fluxenginehardwareprovider.h"
#include "kryofluxhardwareprovider.h"
#include "scphardwareprovider.h"
#include "applesaucehardwareprovider.h"
#include "adfcopyhardwareprovider.h"
#include "catweaselhardwareprovider.h"
#include "xum1541hardwareprovider.h"  // XUM1541/ZoomFloppy for C64

#include <utility>

HardwareManager::HardwareManager(QObject *parent)
    : QObject(parent)
{
    setProvider(std::make_unique<MockHardwareProvider>(this));
}

void HardwareManager::setHardwareType(const QString &hardwareType)
{
    if (m_hardwareType == hardwareType) {
        return;
    }
    m_hardwareType = hardwareType;

    // Provider selection is based on the UI string. Keep it robust by using
    // case-insensitive substring checks.
    const QString ht = m_hardwareType.toLower();

    if (ht.contains(QStringLiteral("greaseweazle"))) {
        setProvider(std::make_unique<GreaseweazleHardwareProvider>(this));
    } else if (ht.contains(QStringLiteral("supercard")) || ht.contains(QStringLiteral("scp"))) {
        setProvider(std::make_unique<SCPHardwareProvider>(this));
    } else if (ht.contains(QStringLiteral("applesauce")) || ht.contains(QStringLiteral("a2r")) || ht.contains(QStringLiteral("woz")) || ht.contains(QStringLiteral("moof"))) {
        setProvider(std::make_unique<ApplesauceHardwareProvider>(this));
    } else if (ht.contains(QStringLiteral("fluxengine"))) {
        setProvider(std::make_unique<FluxEngineHardwareProvider>(this));
    } else if (ht.contains(QStringLiteral("kryoflux")) || ht.contains(QStringLiteral("kryo flux"))) {
        setProvider(std::make_unique<KryoFluxHardwareProvider>(this));
    } else if (ht.contains(QStringLiteral("fc5025")) || ht.contains(QStringLiteral("fc-5025"))) {
        setProvider(std::make_unique<FC5025HardwareProvider>(this));
    } else if (ht.contains(QStringLiteral("xum1541")) || ht.contains(QStringLiteral("zoomfloppy")) || ht.contains(QStringLiteral("opencbm"))) {
        setProvider(std::make_unique<Xum1541HardwareProvider>(this));
    } else if (ht.contains(QStringLiteral("mock")) || ht.isEmpty()) {
        setProvider(std::make_unique<MockHardwareProvider>(this));
    } else {
        // Unknown -> keep mock, but tell the user clearly.
        emit statusMessage(QStringLiteral("Unknown hardware type '%1' – falling back to Mock Provider.").arg(m_hardwareType));
        setProvider(std::make_unique<MockHardwareProvider>(this));
    }

    applySettingsToProvider();
}

void HardwareManager::setDevicePath(const QString &devicePath)
{
    m_devicePath = devicePath;
    applySettingsToProvider();
}

void HardwareManager::setBaudRate(int baudRate)
{
    m_baudRate = baudRate;
    applySettingsToProvider();
}

void HardwareManager::detectDrive()
{
    if (!m_provider) {
        emit statusMessage(QStringLiteral("No hardware provider active."));
        return;
    }
    m_provider->detectDrive();
}

void HardwareManager::autoDetectDevice()
{
    if (!m_provider) {
        emit statusMessage(QStringLiteral("No hardware provider active."));
        return;
    }
    m_provider->autoDetectDevice();
}

void HardwareManager::setProvider(std::unique_ptr<HardwareProvider> provider)
{
    // Replace current provider
    m_provider = std::move(provider);

    if (!m_provider) {
        emit statusMessage(QStringLiteral("Failed to set hardware provider."));
        return;
    }

    // Forward provider signals to UI
    connect(m_provider.get(), &HardwareProvider::driveDetected, this, &HardwareManager::driveDetected);
    connect(m_provider.get(), &HardwareProvider::hardwareInfoUpdated, this, &HardwareManager::hardwareInfoUpdated);
    connect(m_provider.get(), &HardwareProvider::statusMessage, this, &HardwareManager::statusMessage);
    connect(m_provider.get(), &HardwareProvider::devicePathSuggested, this, &HardwareManager::devicePathSuggested);

    emit statusMessage(QStringLiteral("Active hardware provider: %1").arg(m_provider->displayName()));
}

void HardwareManager::applySettingsToProvider()
{
    if (!m_provider) {
        return;
    }
    m_provider->setHardwareType(m_hardwareType);
    m_provider->setDevicePath(m_devicePath);
    m_provider->setBaudRate(m_baudRate);
}
