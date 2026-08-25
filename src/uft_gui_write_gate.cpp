/**
 * @file uft_gui_write_gate.cpp
 * @brief Siehe uft_gui_write_gate.h (MF-573).
 */

#include "uft_gui_write_gate.h"

#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QRegularExpression>

extern "C" {
#include "uft/policy/uft_write_gate.h"
}

/**
 * Wohin der Schnappschuss geht.
 *
 * Neben das Abbild, in `.uft-snapshots`. Absichtlich NICHT in ein
 * System-Temp-Verzeichnis: ein Sicherungsstand, den beim naechsten
 * Neustart jemand aufraeumt, ist kein Sicherungsstand. Er soll bei dem
 * Material liegen, zu dem er gehoert.
 */
static QString snapshotDirFor(const QString &imagePath)
{
    QDir d(QFileInfo(imagePath).absolutePath());
    const QString sub = QStringLiteral(".uft-snapshots");
    if (!d.exists(sub) && !d.mkpath(sub))
        return QString();
    return d.filePath(sub);
}

bool uftGuiWriteGateAllows(QWidget *parent,
                           const QString &imagePath,
                           const QByteArray &imageData,
                           const QString &what,
                           QString *snapshotOut)
{
    if (imageData.isEmpty()) {
        QMessageBox::critical(parent, QObject::tr("Write refused"),
            QObject::tr("No image data to snapshot before %1 — nothing was "
                        "written.").arg(what));
        return false;
    }

    const QString dir = snapshotDirFor(imagePath);
    if (dir.isEmpty()) {
        /* Kein Schnappschuss-Verzeichnis heisst kein Schnappschuss heisst
         * keine Aenderung. Das Prinzip kennt hier keinen Notausgang. */
        QMessageBox::critical(parent, QObject::tr("Write refused"),
            QObject::tr("Cannot create a snapshot directory next to\n%1\n\n"
                        "The image was NOT changed. \"No silent "
                        "modification\" means the state before the change "
                        "has to survive it.").arg(imagePath));
        return false;
    }

    const QByteArray prefix =
        (QFileInfo(imagePath).completeBaseName() + "_" +
         QString(what).replace(QRegularExpression("[^A-Za-z0-9]+"), "_"))
            .toUtf8();

    uft_write_gate_policy_t pol = UFT_GATE_POLICY_IMAGE_ONLY;
    uft_write_gate_result_t res;
    memset(&res, 0, sizeof(res));

    const uft_gate_status_t st = uft_write_gate_precheck(
        &pol,
        reinterpret_cast<const uint8_t *>(imageData.constData()),
        static_cast<size_t>(imageData.size()),
        dir.toUtf8().constData(),
        prefix.constData(),
        &res);

    if (st != UFT_GATE_OK) {
        QMessageBox::critical(parent, QObject::tr("Write refused"),
            QObject::tr("The write-safety gate refused %1:\n\n%2\n\n"
                        "The image was NOT changed.")
                .arg(what)
                .arg(QString::fromUtf8(res.decision_reason)));
        return false;
    }

    if (snapshotOut)
        *snapshotOut = QString::fromUtf8(res.snapshot.path);
    return true;
}
