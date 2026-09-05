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
#include "uft/uft_format_probe.h"
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

UftSaveOutcome uftSaveImageAs(const QString &source, const QString &target,
                              const QString &variante)
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

    /* MF-879: Ziel IST die Quelle — dann gibt es nichts zu schreiben.
     *
     * `MainWindow::onSave()` (Strg+S) ruft genau so auf:
     * `speichereNach(m_currentFile)` mit `m_currentFile` als Quelle UND
     * Ziel. Ohne diesen Zweig lief das in `kopiere()`, und dort ist die
     * Reihenfolge fuer diesen Fall gefaehrlich:
     *
     *     in.open(ReadOnly); daten = in.readAll(); in.close();
     *     out.open(WriteOnly);      <- KUERZT die Datei ... die Quelle
     *     ist = out.write(daten);
     *     if (ist != soll) QFile::remove(target);   <- LOESCHT die Quelle
     *
     * Im Normalfall entsteht dabei dieselbe Datei. Im Fehlerfall — volle
     * Platte, entzogenes Medium, Abbruch zwischen Kuerzen und Schreiben —
     * ist das geladene Abbild weg oder abgeschnitten, und die
     * Aufraeumzeile entfernt genau das Original, das sie schuetzen soll.
     * `kopiere()` ist fuer Quelle != Ziel geschrieben; dort ist das
     * Entfernen einer halben Zieldatei richtig (MF-571).
     *
     * Fuer ein Werkzeug mit dem Grundsatz „Kein Bit verloren" ist ein
     * Bedienweg, der beim Speichern das Original kuerzen KANN, nicht
     * hinnehmbar — auch wenn er es meistens nicht tut.
     *
     * Aenderungen an einem geoeffneten Abbild schreibt der Explorer-Reiter
     * unmittelbar und hinter dem Schreibtor (`gateBeforeModify()`,
     * `explorertab.cpp:442,540,661,746,846`). Die Datei auf der Platte ist
     * also bereits aktuell; „Speichern" hat nichts nachzutragen.
     *
     * Verglichen werden aufgeloeste Pfade, nicht Zeichenketten: „./x.d64"
     * und „x.d64" sind dieselbe Datei. */
    {
        const QString qKanon = QFileInfo(source).canonicalFilePath();
        const QString zKanon = QFileInfo(target).canonicalFilePath();
        if (!qKanon.isEmpty() && qKanon == zKanon) {
            r.ok = true;
            r.converted = false;
            r.message = QObject::tr(
                "%1 ist bereits gespeichert — es wurde nichts geschrieben.")
                    .arg(QFileInfo(target).fileName());
            return r;
        }
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

    /* MF-666: die gewaehlte Variante gegen das pruefen, was der
     * Schreiber wirklich kann.
     *
     * `uft_convert_file()` kennt keine Varianten — es nimmt ein Format.
     * Solange je Format genau EINE schreibbar ist, ist die Wahl
     * eindeutig. Eine andere anzunehmen und trotzdem die schreibbare zu
     * erzeugen waere ein Bedienelement ohne Wirkung; deshalb wird hier
     * abgelehnt statt ersetzt. */
    if (!variante.isEmpty()) {
        /* MF-666: NICHT uft_get_format_plugin() — das liefert das ERSTE
         * Plugin mit dieser Container-Kennung, und 82 von 88 teilen sich
         * UFT_FORMAT_DSK. Ein Tor im Baum weist genau darauf hin
         * (MF-445). uft_resolve_format_plugin() geht die Leiter
         * eindeutige Kennung -> Endung -> NULL und raet nie. */
        size_t kandidaten = 0;
        const uft_format_plugin_t *zielPlugin = uft_resolve_format_plugin(
            zielFmt, target.toUtf8().constData(), &kandidaten);
        const uft_format_variant_t *gewaehlt = nullptr;
        if (zielPlugin && zielPlugin->variants) {
            for (size_t i = 0; i < zielPlugin->variant_count; i++) {
                const char *n = zielPlugin->variants[i].name;
                if (n && variante == QString::fromUtf8(n)) {
                    gewaehlt = &zielPlugin->variants[i];
                    break;
                }
            }
        }
        if (!gewaehlt) {
            r.message = QObject::tr("Die Variante „%1\" gehoert nicht zu %2. "
                                    "Es wurde nichts geschrieben.")
                            .arg(variante,
                                 QString::fromUtf8(uft_format_get_name(zielFmt)));
            return r;
        }
        if (!gewaehlt->can_write) {
            QString m = QObject::tr("UFT schreibt „%1\" nicht.").arg(variante);
            if (gewaehlt->write_note && gewaehlt->write_note[0])
                m += "\n" + QString::fromUtf8(gewaehlt->write_note);
            m += "\n\n" + QObject::tr("Es wurde nichts geschrieben.");
            r.message = m;
            return r;
        }
    }

    /* Verschiedene Formate: durch den Wandler und sein Preflight-Tor.
     * Kein accept_data_loss — die Zustimmung gehoert an eine Stelle, an
     * der jemand sie geben kann, und "Speichern" ist keine. */
/* Vorgaben holen, nicht nullen (MF-672).
     *
     * Hier stand `memset(&opts, 0, sizeof(opts))`. Das ist nicht dasselbe
     * wie "keine besonderen Wuensche": `uft_convert_default_options()`
     * setzt zehn Werte, und einer davon aendert das Ergebnis.
     *
     *   `use_multiple_revs` steht per Vorgabe auf TRUE. Genullt ist es
     *   false, und `uft_format_convert_flux.c:964` liest
     *   `(!opts || opts->use_multiple_revs)` — ein Aufrufer, der NULL
     *   uebergibt, bekommt die Verschmelzung ueber alle Umdrehungen; ein
     *   Aufrufer, der eine genullte Struktur uebergibt, bekommt sie
     *   nicht.
     *
     * Die Oberflaeche war damit schlechter als gar keine Angabe: sie hat
     * SCP-Abbilder mit einer Umdrehung dekodiert, wo fuenf vorlagen, und
     * niemand konnte es sehen.
     *
     * `accept_data_loss` bleibt ausdruecklich aus — die Vorgabe laesst es
     * absichtlich ungesetzt (UFT-A05), und das gilt hier weiter. */
    uft_convert_options_t opts = uft_convert_default_options();
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
