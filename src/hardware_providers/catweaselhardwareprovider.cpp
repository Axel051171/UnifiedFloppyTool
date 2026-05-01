#include "catweaselhardwareprovider.h"

CatweaselHardwareProvider::CatweaselHardwareProvider(QObject *parent)
    : HardwareProvider(parent)
{
}

QString CatweaselHardwareProvider::displayName() const
{
    return QStringLiteral("Catweasel");
}

void CatweaselHardwareProvider::setHardwareType(const QString &hardwareType)
{
    m_hardwareType = hardwareType;
}

void CatweaselHardwareProvider::setDevicePath(const QString &devicePath)
{
    m_devicePath = devicePath;
}

void CatweaselHardwareProvider::setBaudRate(int baudRate)
{
    m_baudRate = baudRate;
}

void CatweaselHardwareProvider::detectDrive()
{
    /* MF-144 / HW-C: Catweasel has no UFT-side driver implementation.
     * The previous version emitted a synthetic driveDetected signal
     * with hardcoded "80 tracks / DD-HD / 300 RPM / Catweasel detected
     * drive" values regardless of whether any hardware was attached
     * (UFT-AUD-003: "Fake — gefährlich"). That was a forensic-integrity
     * lie: the UI flipped to "drive present" state without anyone
     * having spoken to a single piece of hardware.
     *
     * Honest behavior: emit only a status message saying detection
     * is not implemented. Do NOT emit driveDetected. The UI stays in
     * its "no drive" state, the user gets actionable text, and no
     * downstream code fires false-positive read attempts.
     *
     * If someone wires a real cwtool subprocess wrapper later
     * (analogous to KryoFlux/DTC), the implementation goes here. */
    emit statusMessage(tr("Catweasel: not implemented in this build. "
                          "Catweasel MK3/MK4 needs a userspace driver "
                          "(cwtool) which is not yet wired into UFT. "
                          "Use Greaseweazle or KryoFlux for flux capture "
                          "until then."));
}

void CatweaselHardwareProvider::autoDetectDevice()
{
    /* MF-144 / HW-C: same honest-stub treatment as detectDrive().
     * Previously emitted hardwareInfoUpdated with isReady=false but
     * filled-in vendor / product / connection strings — those imply
     * a probe ran. Now we only state the "not implemented" status
     * and let HardwareInfo stay default-constructed. */
    emit statusMessage(tr("Catweasel: autodetect not implemented "
                          "(no cwtool wrapper in this build)."));
}
