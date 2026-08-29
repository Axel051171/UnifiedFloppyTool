/**
 * @file uft_save_image.cpp
 * @brief Umsetzung zu uft_save_image.h (MF-664)
 */

#include "uft_save_image.h"

#include <QFile>
#include <QFileInfo>
#include <QObject>

#include <cstring>

extern "C" {
#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_format_convert.h"
}

namespace {

/* Das Format, das eine Datei laut ihrem INHALT hat.
 *
 * Nicht nach der Endung. Die Endung ist die Absicht des Benutzers, der
 * Inhalt ist die Wahrheit — und MF-444 hat das im Kern schon so
 * entschieden: `uft_probe_format()` ignoriert den Dateinamen
 * ausdruecklich, weil "der Name kein Beweis ueber die Bytes" ist. */
uft_format_t formatVonInhalt(const QString &pfad)
{
    QFile f(pfad);
    if (!f.open(QIODevice::ReadOnly)) return UFT_FORMAT_UNKNOWN;
    const QByteArray kopf = f.read(64 * 1024);
    f.close();
    if (kopf.isEmpty()) return UFT_FORMAT_UNKNOWN;

    const uft_format_plugin_t *p = uft_probe_buffer_format(
        reinterpret_cast<const uint8_t *>(kopf.constData()),
        static_cast<size_t>(kopf.size()),
        static_cast<size_t>(QFileInfo(pfad).size()));
    return p ? static_cast<uft_format_t>(p->format) : UFT_FORMAT_UNKNOWN;
}

/* Byte-Kopie mit Pruefung. Die kurze Schreibung war in diesem Baum schon
 * einmal eine halbe Datei mit Erfolgsmeldung (MF-571) — hier wird sie
 * gemeldet und die Teildatei entfernt. */
UftSaveOutcome kopiere(const QString &source, const QString &target)
{
    UftSaveOutcome r;

    QFile in(source);
    if (!in.open(QIODevice::ReadOnly)) {
        r.message = QObject::tr("Die Quelldatei ist nicht lesbar:\n%1")
                        .arg(source);
        return r;
    }
    const QByteArray daten = in.readAll();
    in.close();

    QFile out(target);
    if (!out.open(QIODevice::WriteOnly)) {
        r.message = QObject::tr("Das Ziel ist nicht beschreibbar:\n%1")
                        .arg(target);
        return r;
    }
    const qint64 soll = daten.size();
    const qint64 ist  = out.write(daten);
    out.close();

    if (ist != soll) {
        QFile::remove(target);
        r.message = QObject::tr("Nur %1 von %2 Byte geschrieben — die "
                                "unvollständige Datei wurde entfernt.")
                        .arg(ist < 0 ? 0 : ist).arg(soll);
        return r;
    }

    r.ok = true;
    r.converted = false;
    r.message = QObject::tr("Gespeichert: %1 (%2 Byte, unverändert)")
                    .arg(QFileInfo(target).fileName()).arg(soll);
    return r;
}

} // namespace

UftSaveOutcome uftSaveImageAs(const QString &source, const QString &target)
{
    UftSaveOutcome r;

    if (source.isEmpty()) {
        r.message = QObject::tr("Es ist kein Abbild geladen.");
        return r;
    }
    if (!QFileInfo::exists(source)) {
        /* MF-664: hier stand frueher sinngemaess dieselbe Meldung — aber
         * fuer die FALSCHE Datei. onSaveAs() hatte m_currentFile schon
         * auf das Ziel gesetzt, bevor onSave() die Quelle suchte. Die
         * Quelle existierte, sie hiess nur nicht mehr so. */
        r.message = QObject::tr("Das geladene Abbild liegt nicht mehr "
                                "unter:\n%1").arg(source);
        return r;
    }
    if (target.isEmpty()) {
        r.message = QObject::tr("Kein Zielpfad angegeben.");
        return r;
    }

    const QString endung = QFileInfo(target).suffix();
    const uft_format_t zielFmt =
        endung.isEmpty() ? UFT_FORMAT_UNKNOWN
                         : uft_format_from_name(endung.toUtf8().constData());
    const uft_format_t quellFmt = formatVonInhalt(source);

    /* Gleiches Format — oder das Ziel traegt keine erkennbare Endung und
     * die Quelle wird unveraendert weitergereicht. Beides ist die
     * Identitaet; die Rundlauf-Matrix fuehrt sie seit MF-532 mit Messung
     * als verlustfrei. */
    if (zielFmt == UFT_FORMAT_UNKNOWN && quellFmt == UFT_FORMAT_UNKNOWN) {
        /* Wir wissen von KEINER Seite etwas. Eine Kopie ist dann das
         * Ehrlichste, was geht — sie behauptet nichts. */
        return kopiere(source, target);
    }
    if (zielFmt == UFT_FORMAT_UNKNOWN) {
        r.message = QObject::tr(
            "Die Endung „%1\" nennt kein bekanntes Format. Es wurde nichts "
            "geschrieben — eine Datei unter einem Namen abzulegen, dessen "
            "Format wir nicht kennen, wäre eine Behauptung.")
            .arg(endung.isEmpty() ? QObject::tr("(keine)") : endung);
        return r;
    }
    if (zielFmt == quellFmt) {
        return kopiere(source, target);
    }

    /* Verschiedene Formate: durch den Wandler und sein Preflight-Tor.
     * Kein accept_data_loss — die Zustimmung gehoert an eine Stelle, an
     * der jemand sie geben kann, und "Speichern" ist keine. */
    uft_convert_options_t opts;
    std::memset(&opts, 0, sizeof(opts));
    opts.accept_data_loss = false;

    uft_convert_result_t res;
    std::memset(&res, 0, sizeof(res));

    const uft_error_t rc = uft_convert_file(source.toUtf8().constData(),
                                            target.toUtf8().constData(),
                                            zielFmt, &opts, &res);
    if (rc != UFT_OK || !res.success) {
        QString m = QObject::tr("Speichern als %1 abgelehnt (Fehler %2).")
                        .arg(QString::fromUtf8(uft_format_get_name(zielFmt)))
                        .arg(static_cast<int>(rc));
        for (int i = 0; i < res.warning_count && i < 8; i++)
            m += "\n  " + QString::fromUtf8(res.warnings[i]);
        m += "\n\n" + QObject::tr("Es wurde nichts geschrieben.");
        r.message = m;
        return r;
    }

    r.ok = true;
    r.converted = true;
    QString m = QObject::tr("Gewandelt und gespeichert: %1 → %2")
                    .arg(QString::fromUtf8(uft_format_get_name(quellFmt)),
                         QString::fromUtf8(uft_format_get_name(zielFmt)));
    for (int i = 0; i < res.warning_count && i < 8; i++)
        m += "\n  " + QString::fromUtf8(res.warnings[i]);
    r.message = m;
    return r;
}
