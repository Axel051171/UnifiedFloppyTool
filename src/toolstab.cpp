/**
 * @file toolstab.cpp
 * @brief Tools Tab Implementation
 * 
 * P0-GUI-006 FIX: Disk utilities implementation
 */

#include "toolstab.h"
#include "uft/core/uft_copy_plan.h"   /* MF-1265: uft_copy_plan_current() */

#include <uft/uft_core.h>
#include <uft/uft_format_plugin.h>
#include "ui_tab_tools.h"
#include "disk_image_validator.h"

extern "C" {
#include "uft/uft_format_convert.h"
}


#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QDateTime>
#include <QInputDialog>
#include "rawformatdialog.h"
#include "visualdiskdialog.h"
/* MF-1197: die Spuransicht haengt am fertigen Spurraster. */
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVector>
#include "widgets/trackgridwidget.h"

ToolsTab::ToolsTab(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TabTools)
    , m_batchRunning(false)
{
    ui->setupUi(this);
    setupFormatConversionMap();
    setupConnections();
}

ToolsTab::~ToolsTab()
{
    delete ui;
}

void ToolsTab::setupConnections()
{
    // Analysis tools
    connect(ui->btnDiskInfo, &QPushButton::clicked, this, &ToolsTab::onDiskInfo);
    connect(ui->btnHexView, &QPushButton::clicked, this, &ToolsTab::onHexView);
    connect(ui->btnTrackView, &QPushButton::clicked, this, &ToolsTab::onTrackView);
    connect(ui->btnFluxView, &QPushButton::clicked, this, &ToolsTab::onFluxView);
    connect(ui->btnSectorEdit, &QPushButton::clicked, this, &ToolsTab::onSectorEdit);
    connect(ui->btnAnalyze, &QPushButton::clicked, this, &ToolsTab::onAnalyze);
    
    // Conversion tools
    connect(ui->btnConvert, &QPushButton::clicked, this, &ToolsTab::onConvert);
    connect(ui->btnRepair, &QPushButton::clicked, this, &ToolsTab::onRepair);
    connect(ui->btnCompare, &QPushButton::clicked, this, &ToolsTab::onCompare);
    connect(ui->btnCreateBlank, &QPushButton::clicked, this, &ToolsTab::onCreateBlank);
    
    // Batch
    connect(ui->btnBatchStart, &QPushButton::clicked, this, &ToolsTab::onBatchStart);
    connect(ui->btnBatchStop, &QPushButton::clicked, this, &ToolsTab::onBatchStop);
    
    // Browse buttons
    connect(ui->btnBrowseConvertSource, &QPushButton::clicked, this, &ToolsTab::onBrowseConvertSource);
    connect(ui->btnBrowseConvertTarget, &QPushButton::clicked, this, &ToolsTab::onBrowseConvertTarget);
    connect(ui->btnBrowseRepair, &QPushButton::clicked, this, &ToolsTab::onBrowseRepair);
    connect(ui->btnBrowseCompareA, &QPushButton::clicked, this, &ToolsTab::onBrowseCompareA);
    connect(ui->btnBrowseCompareB, &QPushButton::clicked, this, &ToolsTab::onBrowseCompareB);
    connect(ui->btnBrowseAnalyze, &QPushButton::clicked, this, &ToolsTab::onBrowseAnalyze);
    connect(ui->btnBrowseBatch, &QPushButton::clicked, this, &ToolsTab::onBrowseBatch);
    
    // Output
    connect(ui->btnClearOutput, &QPushButton::clicked, this, &ToolsTab::onClearOutput);
    connect(ui->btnSaveOutput, &QPushButton::clicked, this, &ToolsTab::onSaveOutput);
    
    // Format conversion dependencies
    connect(ui->comboConvertFrom, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ToolsTab::onConvertFromChanged);
    connect(ui->comboBatchAction, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ToolsTab::onBatchActionChanged);
}

void ToolsTab::appendOutput(const QString& text)
{
    ui->textOutput->appendPlainText(text);
}

void ToolsTab::onDiskInfo()
{
    QString path = ui->editAnalyzeFile->text();
    if (path.isEmpty()) {
        path = QFileDialog::getOpenFileName(this, tr("Select Disk Image"),
            QString(), DiskImageValidator::fileDialogFilter());
        if (path.isEmpty()) return;
        ui->editAnalyzeFile->setText(path);
    }
    
    DiskImageInfo info = DiskImageValidator::validate(path);
    
    appendOutput(QString("═══════════════════════════════════════"));
    appendOutput(tr("Disk Info: %1").arg(QFileInfo(path).fileName()));
    appendOutput(QString("═══════════════════════════════════════"));
    
    if (!info.isValid) {
        appendOutput(tr("Error: %1").arg(info.errorMessage));
        return;
    }
    
    appendOutput(tr("Format:    %1").arg(info.formatName));
    appendOutput(tr("Platform:  %1").arg(info.platform));
    appendOutput(tr("Size:      %1 bytes").arg(info.fileSize));
    
    if (info.tracks > 0) {
        appendOutput(tr("Tracks:    %1").arg(info.tracks));
        appendOutput(tr("Heads:     %1").arg(info.heads));
        appendOutput(tr("Sectors:   %1 per track").arg(info.sectorsPerTrack));
        appendOutput(tr("Sec Size:  %1 bytes").arg(info.sectorSize));
    }
    
    appendOutput(tr("Flux:      %1").arg(info.isFluxFormat ? "Yes" : "No"));
    appendOutput(QString());
    
    emit statusMessage(tr("Disk info: %1").arg(info.formatName));
}

void ToolsTab::onHexView()
{
    QString path = ui->editAnalyzeFile->text();
    if (path.isEmpty()) {
        QMessageBox::information(this, tr("Hex View"),
            tr("Please select a disk image first."));
        return;
    }
    
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        appendOutput(tr("Cannot open file: %1").arg(file.errorString()));
        return;
    }
    
    QByteArray data = file.read(512); // First sector
    file.close();
    
    appendOutput(QString("═══════════════════════════════════════"));
    appendOutput(tr("Hex View: %1 (first 512 bytes)").arg(QFileInfo(path).fileName()));
    appendOutput(QString("═══════════════════════════════════════"));
    
    for (int i = 0; i < data.size(); i += 16) {
        QString hex, ascii;
        for (int j = 0; j < 16 && (i+j) < data.size(); j++) {
            quint8 b = static_cast<quint8>(data[i+j]);
            hex += QString("%1 ").arg(b, 2, 16, QChar('0')).toUpper();
            ascii += (b >= 32 && b < 127) ? QChar(b) : '.';
        }
        appendOutput(QString("%1: %2 %3")
            .arg(i, 4, 16, QChar('0')).toUpper()
            .arg(hex, -48)
            .arg(ascii));
    }
    appendOutput(QString());
}

namespace {

/** Was EIN Lesedurchgang ueber EINE Spur weiss. */
struct SpurBefund {
    int  cyl = 0, head = 0;
    bool lesbar = false;
    long sektoren = 0, crc_fehler = 0, schwach = 0, ohne_daten = 0;
};

/** Was er ueber die ganze Diskette weiss. */
struct DiskBefund {
    bool        offen = false;
    QString     fehler;        ///< Warum nicht - in Bedienersprache.
    QString     pluginName;
    int         zylinder = 0, koepfe = 0;
    QVector<SpurBefund> spuren;
    long        sektoren = 0, crc_fehler = 0, schwach = 0, ohne_daten = 0;
    long        gelesen = 0, stumm = 0;
    QStringList auffaellig;
};

/**
 * @brief Liest eine Diskette EINMAL und gibt je Spur einen Befund zurueck.
 *
 * MF-1197: Diese Schleife stand bis hierher im Rumpf von `onRepair()`.
 * `onTrackView()` braucht dieselben Zahlen - sie ein zweites Mal zu
 * schreiben waere MF-1177 gewesen ("eine Groesse, eine Rechnung; wer sie
 * zweimal rechnet, hat sie nicht gemessen"). Seither ruft **jeder** der
 * beiden Knoepfe diese eine Funktion.
 *
 * Sie urteilt nicht und meldet nichts an die Oberflaeche; sie zaehlt.
 */
DiskBefund scanne_spuren(const QString &path)
{
    DiskBefund b;

    uft_disk_t *disk = uft_disk_open(path.toUtf8().constData(), true);
    if (!disk) {
        b.fehler = QObject::tr(
            "Nicht lesbar: kein Plugin konnte diese Datei oeffnen. Ueber "
            "Fehler auf der Diskette ist damit NICHTS bekannt - das ist "
            "kein Freispruch.");
        return b;
    }

    const uft_format_plugin_t *plugin = uft_disk_plugin(disk);
    if (!plugin || !plugin->read_track) {
        b.fehler = QObject::tr(
            "Das Plugin \"%1\" liest keine ganzen Spuren. Ueber einzelne "
            "Sektoren ist damit nichts bekannt - das ist kein Freispruch.")
                .arg(plugin && plugin->name ? QString::fromUtf8(plugin->name)
                                            : QObject::tr("(unbenannt)"));
        uft_disk_close(disk);
        return b;
    }

    uft_geometry_t geom;
    memset(&geom, 0, sizeof(geom));
    uft_disk_get_geometry(disk, &geom);

    b.offen      = true;
    b.pluginName = plugin->name ? QString::fromUtf8(plugin->name)
                                : QObject::tr("(unbenannt)");
    b.zylinder   = geom.cylinders > 0 ? geom.cylinders : 0;
    b.koepfe     = geom.heads > 0 ? geom.heads : 1;

    for (int c = 0; c < b.zylinder; c++) {
        for (int h = 0; h < b.koepfe; h++) {
            SpurBefund s;
            s.cyl = c;
            s.head = h;

            uft_track_t spur;
            memset(&spur, 0, sizeof(spur));
            if (plugin->read_track(disk, c, h, &spur) != UFT_OK) {
                b.stumm++;
                if (b.auffaellig.size() < 20)
                    b.auffaellig << QObject::tr("  Spur %1/%2: nicht lesbar")
                                        .arg(c).arg(h);
                b.spuren.push_back(s);   /* lesbar bleibt false */
                continue;
            }
            s.lesbar = true;
            b.gelesen++;

            for (size_t i = 0; i < spur.sector_count; i++) {
                const uft_sector_t *sek = &spur.sectors[i];
                s.sektoren++;
                if (!sek->crc_ok) {
                    s.crc_fehler++;
                    if (b.auffaellig.size() < 20)
                        b.auffaellig << QObject::tr(
                            "  Spur %1/%2 Sektor %3: CRC falsch")
                                .arg(c).arg(h).arg(sek->id.sector);
                }
                if (sek->weak) {
                    s.schwach++;
                    if (b.auffaellig.size() < 20)
                        b.auffaellig << QObject::tr(
                            "  Spur %1/%2 Sektor %3: schwache Bits")
                                .arg(c).arg(h).arg(sek->id.sector);
                }
                if (!sek->data || sek->data_len == 0) s.ohne_daten++;
                free(sek->data);
            }
            free(spur.sectors);

            b.sektoren   += s.sektoren;
            b.crc_fehler += s.crc_fehler;
            b.schwach    += s.schwach;
            b.ohne_daten += s.ohne_daten;
            b.spuren.push_back(s);
        }
    }

    uft_disk_close(disk);
    return b;
}

}  // namespace

void ToolsTab::onTrackView()
{
    /* MF-1197: hier standen drei Platzhalterzeilen, die dem Benutzer
     * sagten, es gebe die Faehigkeit noch nicht. Gemessen war das eine
     * UNTERTREIBUNG - `TrackGridWidget` (882 Z.) lag fertig im Binary und
     * hatte null Aufrufer (Klasse P3-204/MF-930, gehalten von Tor 68).
     * Bewacht von tests/test_tools_tab_track_view.cpp.
     *
     * Die alte Zeichenkette wird hier ABSICHTLICH nicht zitiert: Tor 34
     * sucht genau sie im Quelltext und haelt ein Zitat fuer eine Zusage.
     * Das ist kein Fehler des Tores - es soll Kommentare lesen, das ist
     * seine Messung (MF-569/735). Ein Zitat einer ENTFERNTEN Zusage kann
     * es nicht von der Zusage selbst unterscheiden, also gehoert das
     * Zitat weg und nicht die Messung. */
    const QString path = ui->editAnalyzeFile->text();
    if (path.isEmpty()) {
        QMessageBox::information(this, tr("Track View"),
            tr("Please select a disk image first."));
        return;
    }

    appendOutput(QString("======================================="));
    appendOutput(tr("Spuransicht: %1").arg(QFileInfo(path).fileName()));

    const DiskBefund b = scanne_spuren(path);
    if (!b.offen) {
        appendOutput(b.fehler);
        appendOutput(QString());
        return;
    }

    appendOutput(tr("Plugin: %1, %2 Zylinder x %3 Kopf/Koepfe")
                     .arg(b.pluginName).arg(b.zylinder).arg(b.koepfe));
    appendOutput(tr("%1 Spuren gelesen, %2 stumm, %3 Sektoren.")
                     .arg(b.gelesen).arg(b.stumm).arg(b.sektoren));
    appendOutput(QString());

    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle(tr("Spuransicht - %1").arg(QFileInfo(path).fileName()));
    dlg->setMinimumSize(760, 520);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout *layout = new QVBoxLayout(dlg);

    QLabel *kopf = new QLabel(
        tr("<b>%1</b> &mdash; %2 Zylinder &times; %3 Kopf/Koepfe, "
           "%4 Sektoren gelesen")
            .arg(b.pluginName).arg(b.zylinder).arg(b.koepfe).arg(b.sektoren),
        dlg);
    kopf->setWordWrap(true);
    layout->addWidget(kopf);

    /* Der Vorbehalt gehoert dorthin, wo jemand ihn liest (MF-569): das
     * Raster zeigt EINE Lesung dieser Datei, keine Aussage ueber die
     * Diskette, von der sie stammt. */
    QLabel *vorbehalt = new QLabel(
        tr("<i>Gezeigt wird diese eine Lesung des Abbilds. Spuren, die das "
           "Plugin nicht liefert, bleiben grau &mdash; sie sind damit nicht "
           "als fehlerfrei erwiesen, sondern ungelesen.</i>"), dlg);
    vorbehalt->setWordWrap(true);
    layout->addWidget(vorbehalt);

    TrackGridWidget *grid = new TrackGridWidget(dlg);
    grid->setDiskGeometry(b.zylinder, b.koepfe);
    grid->reset();
    for (const SpurBefund &s : b.spuren) {
        TrackStatus st;
        if (!s.lesbar)                      st = TrackStatus::ERROR;
        else if (s.crc_fehler || s.schwach) st = TrackStatus::WARNING;
        else if (s.sektoren == 0)           st = TrackStatus::WARNING;
        else                                st = TrackStatus::GOOD;

        const int gut = static_cast<int>(s.sektoren - s.crc_fehler - s.ohne_daten);
        grid->updateTrackStatus(s.cyl, s.head, st,
                                gut > 0 ? gut : 0,
                                static_cast<int>(s.sektoren));
    }
    layout->addWidget(grid, 1);

    QPushButton *closeBtn = new QPushButton(tr("Close"), dlg);
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::close);
    layout->addWidget(closeBtn, 0, Qt::AlignRight);

    dlg->show();
}

void ToolsTab::onFluxView()
{
    appendOutput(tr("Flux View: Feature not yet implemented"));
    appendOutput(tr("This will show flux timing histograms and PLL analysis."));
    appendOutput(QString());
}

void ToolsTab::onSectorEdit()
{
    appendOutput(tr("Sector Editor: Feature not yet implemented"));
    appendOutput(tr("This will allow hex editing of individual sectors."));
    appendOutput(QString());
}

void ToolsTab::onAnalyze()
{
    onDiskInfo();
    
    QString path = ui->editAnalyzeFile->text();
    if (path.isEmpty()) return;
    
    // Calculate hash if checkbox is checked
    if (ui->checkCalcHashes->isChecked()) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            file.close();
            
            QByteArray md5 = QCryptographicHash::hash(data, QCryptographicHash::Md5);
            appendOutput(tr("MD5: %1").arg(QString(md5.toHex())));
        }
    }
}

void ToolsTab::onConvert()
{
    QString source = ui->editConvertSource->text();
    QString target = ui->editConvertTarget->text();
    
    if (source.isEmpty() || target.isEmpty()) {
        QMessageBox::information(this, tr("Convert"),
            tr("Please specify source and target paths."));
        return;
    }
    
    appendOutput(QString("═══════════════════════════════════════"));
    appendOutput(tr("Converting: %1").arg(QFileInfo(source).fileName()));
    appendOutput(tr("       To:  %1").arg(target));
    
    /* -- MF-568: hier stand `QFile::copy()` -----------------------------
     *
     * Woertlich:
     *
     *     // For now, just copy the file
     *     // Real implementation would use format conversion
     *     if (QFile::copy(source, target))
     *         appendOutput(tr("Conversion complete!"));
     *
     * Wer SCP -> D64 waehlte, bekam eine byteweise Kopie der SCP-Datei
     * unter dem Namen `.d64` -- und die Meldung "Conversion complete!".
     * Das Preflight-Tor, die Rundlauf-Matrix, die Verlustmeldung und
     * saemtliche Wandler wurden dabei nicht einmal beruehrt.
     *
     * Jetzt geht der Knopf durch `uft_convert_file()` -- denselben
     * Engpass, den MF-263/UFT-A01 als einzigen vorsieht und den MF-567
     * auch fuer den Speicher-Weg dichtgemacht hat. */
    uft_format_t dstFmt = uft_format_from_name(
        ui->comboConvertTo->currentText().toUtf8().constData());
    if (dstFmt == UFT_FORMAT_UNKNOWN) {
        dstFmt = uft_format_from_name(
            QFileInfo(target).suffix().toUtf8().constData());
    }
    if (dstFmt == UFT_FORMAT_UNKNOWN) {
        appendOutput(tr("Unknown target format - nothing was written."));
        appendOutput(QString());
        emit statusMessage(tr("Conversion refused: unknown target format"));
        return;
    }

    /* Verlustbehaftete Wandlungen verlangen ein ausdrueckliches Ja. WAS
     * verloren geht, sagt die Matrix -- nicht diese Datei. */
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

    /* MF-1265 (`P3-509`, zweiter Halbsatz): der Kopierplan gilt auch
     * hier. Dieser Reiter laeuft im GUI-Faden, darf die Auskunft des
     * Kerns also direkt nehmen — anders als `DecodeJob`, der nach
     * `moveToThread()` eine KOPIE mitbekommt. Der Unterschied ist
     * nicht Geschmack, sondern der Faden.
     *
     * Gefragt wird der KERN, nicht der Formatreiter: eine erste
     * Fassung rief `FormatTab::aktuellerPlan()`, und der Binder hat
     * daran gezeigt, dass zwei Qt-Tests `toolstab.cpp` ohne
     * `formattab.cpp` binden. */
    uft_copy_plan_t plan = uft_copy_plan_current();

    /* MF-1311 — hier, und nur hier, kennt der Baum BEIDE Formate.
     *
     * Das war der eigentliche Fund der Messung: `dstFmt` steht weiter
     * oben und ist hier noch im Gueltigkeitsbereich, und die Quelle
     * laesst sich aus der Datei selbst bestimmen. Die Faehigkeitsmaske
     * ist an dieser Stelle also MESSBAR — waehrend
     * `FormatTab::copyPlanCaps()` sie nur aus dem QUELLformat rechnet
     * und sie dort ohnehin stirbt, weil sie den Plan nie verlaesst.
     *
     * `uft_probe_file_format()` gibt fuer die Quelle direkt ein Plugin
     * zurueck — keine Mehrdeutigkeit. Fuer das Ziel ist sie moeglich
     * (gemessen teilen sich 82 von 88 Plugins ihre Container-ID),
     * deshalb `uft_resolve_format_plugin()` mit Pfadhinweis und
     * Kandidatenzahl: bleibt mehr als einer uebrig, ist die Maske NICHT
     * ermittelt, und der Plan sagt das mit `caps_bekannt = false`. */
    {
        const uft_format_plugin_t *qpl =
            uft_probe_file_format(source.toUtf8().constData());

        size_t kandidaten = 0;
        const uft_format_plugin_t *zpl = uft_resolve_format_plugin(
            dstFmt, target.toUtf8().constData(), &kandidaten);
        if (kandidaten != 1) zpl = nullptr;

        uint32_t maske = 0u;
        plan.caps_bekannt = uft_copy_caps_von_plugins(qpl, zpl, &maske);
        plan.caps = maske;
    }

    /* MF-1309/MF-1311: erst das Tor. Hier steht ein Bediener davor, also
     * bekommt er den Grund zu sehen statt einer stillen Ablehnung — und
     * bei "nicht gemessen" wird weder gesperrt noch stillschweigend
     * durchgelassen, sondern gefragt. */
    {
        const char *grund = nullptr;
        const uft_copy_verdict_t urteil =
            uft_copy_plan_gate_caps(&plan, &grund);
        const QString g = QString::fromUtf8(grund ? grund : "unbekannt");

        if (urteil == UFT_COPY_DENY) {
            QMessageBox::warning(
                this, tr("Convert"),
                tr("Der Kopierplan ist nicht ausfuehrbar: %1").arg(g));
            return;
        }
        if (urteil == UFT_COPY_NEEDS_MEASUREMENT) {
            const auto antwort = QMessageBox::question(
                this, tr("Convert"),
                tr("Der Plan verlangt '%1', und ob Quell- und Zielformat das "
                   "tragen, konnte nicht gemessen werden.\n\n"
                   "Ungemessen heisst weder ja noch nein. Trotzdem "
                   "fortfahren?").arg(g),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (antwort != QMessageBox::Yes) return;
        }
    }
    uft_copy_plan_to_convert_options(&plan, &opts);

    /* NACH dem Plan: kein Plan erteilt sich selbst eine Zustimmung
     * (UFT-A05). */
    opts.accept_data_loss = false;

    uft_convert_result_t res;
    memset(&res, 0, sizeof(res));
    uft_error_t rc = uft_convert_file(source.toUtf8().constData(),
                                      target.toUtf8().constData(),
                                      dstFmt, &opts, &res);

    if (rc != UFT_OK && res.warning_count > 0) {
        /* Das Tor hat abgelehnt. Liegt es am Verlust, darf der Benutzer
         * entscheiden -- aber er entscheidet es, nicht der Code. */
        bool needsConsent = false;
        for (int i = 0; i < res.warning_count && i < 8; i++) {
            if (QString::fromUtf8(res.warnings[i])
                    .contains("accept_data_loss")) {
                needsConsent = true;
            }
        }

        if (needsConsent) {
            QString what;
            for (int i = 0; i < res.warning_count && i < 8; i++) {
                what += QString::fromUtf8(res.warnings[i]) + "\n";
            }
            if (QMessageBox::question(this, tr("Lossy conversion"),
                    tr("This conversion loses information:\n\n%1\n"
                       "Proceed anyway?").arg(what))
                == QMessageBox::Yes) {
                memset(&res, 0, sizeof(res));
                opts.accept_data_loss = true;
                rc = uft_convert_file(source.toUtf8().constData(),
                                      target.toUtf8().constData(),
                                      dstFmt, &opts, &res);
            }
        }
    }

    for (int i = 0; i < res.warning_count && i < 8; i++) {
        appendOutput(QString("  ! ") + QString::fromUtf8(res.warnings[i]));
    }

    if (rc == UFT_OK && res.success) {
        appendOutput(tr("Conversion complete: %1 tracks, %2 sectors "
                        "(%3 failed)")
                     .arg(res.tracks_converted)
                     .arg(res.sectors_converted)
                     .arg(res.sectors_failed));
        emit statusMessage(tr("Conversion complete"));
    } else {
        /* Kein "complete" ohne Tat. Sein Ziel raeumt der Wandler selbst
         * auf (MF-545); hier bleibt die ehrliche Meldung. */
        appendOutput(tr("Conversion refused or failed (error %1) - "
                        "no output written.").arg((int)rc));
        emit statusMessage(tr("Conversion failed"));
    }
    appendOutput(QString());
}

/**
 * Was auf der Diskette steht — und was dieses Werkzeug NICHT tut.
 *
 * ── Was hier stand (MF-893) ──────────────────────────────────────────────
 *
 *     appendOutput(tr("Scanning for errors..."));
 *     appendOutput(tr("No errors found that can be automatically "
 *                     "repaired."));
 *
 * Zwei Zeilen, unmittelbar nacheinander, ohne dass eine Datei geoeffnet
 * wurde. Jede Datei bekam einen Freispruch — auch eine, die gar kein
 * Abbild ist. Das ist eine Unbedenklichkeitsbescheinigung ohne Messung,
 * dieselbe Klasse wie MF-892 (die gruene Belegungskarte) und MF-570.
 *
 * Zwischen den beiden Zeilen stand der einzige echte Vorgang der
 * Funktion: eine Sicherungskopie. Die scheiterte STILL, wenn die
 * Zieldatei schon existierte — `QFile::copy()` gibt dann false zurueck,
 * und die Erfolgsmeldung wurde einfach uebersprungen. Wer eine Sicherung
 * angehakt hatte, konnte nicht erkennen, dass keine entstand.
 *
 * ── Was jetzt passiert ───────────────────────────────────────────────────
 *
 * Die Spuren werden wirklich gelesen (`plugin->read_track()`) und
 * gezaehlt, was das Plugin meldet: fehlerhafte CRC, schwache Bits,
 * Sektoren ohne Daten, nicht lesbare Spuren. Der Bericht nennt, WIE VIEL
 * untersucht wurde — eine Aussage ueber Fehler ohne die Zahl der
 * geprueften Sektoren ist nicht nachpruefbar.
 *
 * Und er sagt klar, dass eine automatische Reparatur NICHT umgesetzt
 * ist. Der Knopf heisst „Repair"; solange er nur sucht, muss das
 * dastehen.
 */
void ToolsTab::onRepair()
{
    QString path = ui->editRepairFile->text();
    if (path.isEmpty()) {
        QMessageBox::information(this, tr("Repair"),
            tr("Please specify a disk image to repair."));
        return;
    }

    appendOutput(QString("======================================="));
    appendOutput(tr("Pruefung: %1").arg(QFileInfo(path).fileName()));

    /* MF-893: eine gescheiterte Sicherung wird gemeldet, nicht
     * uebersprungen. `QFile::copy()` scheitert unter anderem, wenn die
     * Zieldatei schon da ist. */
    if (ui->checkBackup->isChecked()) {
        const QString backup = path + ".backup";
        if (QFile::exists(backup)) {
            appendOutput(tr("Sicherung NICHT angelegt: %1 existiert bereits.")
                             .arg(backup));
        } else if (QFile::copy(path, backup)) {
            appendOutput(tr("Sicherung angelegt: %1").arg(backup));
        } else {
            appendOutput(tr("Sicherung FEHLGESCHLAGEN: %1 konnte nicht "
                            "geschrieben werden.").arg(backup));
        }
    }

    /* MF-1197: der Lesedurchgang liegt seit hier in `scanne_spuren()` und
     * wird von `onTrackView()` mitbenutzt. Zwei Kopien derselben Schleife
     * waeren MF-1177 gewesen. Die Ausgabe darunter ist unveraendert. */
    const DiskBefund befund = scanne_spuren(path);
    if (!befund.offen) {
        appendOutput(befund.fehler);
        appendOutput(QString());
        return;
    }

    appendOutput(tr("Plugin: %1, %2 Zylinder x %3 Kopf/Koepfe")
                     .arg(befund.pluginName)
                     .arg(befund.zylinder).arg(befund.koepfe));

    const long sektoren       = befund.sektoren;
    const long crc_fehler     = befund.crc_fehler;
    const long schwach        = befund.schwach;
    const long ohne_daten     = befund.ohne_daten;
    const long spuren_gelesen = befund.gelesen;
    const long spuren_stumm   = befund.stumm;
    const QStringList auffaellig = befund.auffaellig;

    appendOutput(tr("Untersucht: %1 Spuren gelesen, %2 stumm, %3 Sektoren.")
                     .arg(spuren_gelesen).arg(spuren_stumm).arg(sektoren));

    if (crc_fehler || schwach || ohne_daten || spuren_stumm) {
        appendOutput(tr("Auffaellig: %1 mit falscher CRC, %2 mit schwachen "
                        "Bits, %3 ohne Daten.")
                         .arg(crc_fehler).arg(schwach).arg(ohne_daten));
        for (const QString &z : auffaellig) appendOutput(z);
        if (auffaellig.size() >= 20)
            appendOutput(tr("  (weitere nicht aufgefuehrt)"));
    } else if (sektoren > 0) {
        appendOutput(tr("Keine Auffaelligkeit in den %1 gelesenen Sektoren. "
                        "Das ist eine Aussage ueber DIESE Lesung, nicht "
                        "ueber die Diskette.").arg(sektoren));
    } else {
        appendOutput(tr("Kein einziger Sektor gelesen - ueber Fehler ist "
                        "damit nichts bekannt."));
    }

    /* MF-893: der Knopf heisst „Repair". Solange er nur sucht, gehoert
     * das dahin, wo der Benutzer hinsieht. */
    appendOutput(tr("Eine automatische Reparatur ist NICHT umgesetzt; es "
                    "wurde nichts veraendert."));
    appendOutput(QString());
}

void ToolsTab::onCompare()
{
    QString pathA = ui->editCompareA->text();
    QString pathB = ui->editCompareB->text();
    
    if (pathA.isEmpty() || pathB.isEmpty()) {
        QMessageBox::information(this, tr("Compare"),
            tr("Please specify two files to compare."));
        return;
    }
    
    QFile fa(pathA), fb(pathB);
    if (!fa.open(QIODevice::ReadOnly) || !fb.open(QIODevice::ReadOnly)) {
        appendOutput(tr("Cannot open files for comparison."));
        return;
    }
    
    QByteArray da = fa.readAll();
    QByteArray db = fb.readAll();
    fa.close();
    fb.close();
    
    appendOutput(QString("═══════════════════════════════════════"));
    appendOutput(tr("Comparing files:"));
    appendOutput(tr("  A: %1 (%2 bytes)").arg(pathA).arg(da.size()));
    appendOutput(tr("  B: %1 (%2 bytes)").arg(pathB).arg(db.size()));
    
    if (da == db) {
        appendOutput(tr("Result: Files are IDENTICAL"));
    } else {
        int diffs = 0;
        int minSize = qMin(da.size(), db.size());
        for (int i = 0; i < minSize; i++) {
            if (da[i] != db[i]) diffs++;
        }
        appendOutput(tr("Result: Files DIFFER (%1 byte differences)").arg(diffs));
    }
    appendOutput(QString());
}

void ToolsTab::onCreateBlank()
{
    // Ask user for format (comboBlankFormat not in UI)
    QStringList formats = {"ADF", "D64", "IMG", "ST", "DSK"};
    bool ok;
    QString format = QInputDialog::getItem(this, tr("Create Blank Disk"),
        tr("Select format:"), formats, 0, false, &ok);
    if (!ok || format.isEmpty()) return;
    
    QString path = QFileDialog::getSaveFileName(this, tr("Create Blank Disk"),
        QString(), QString("%1 (*.%2)").arg(format).arg(format.toLower()));
    
    if (path.isEmpty()) return;
    
    // Create blank disk based on format
    QByteArray blank;
    
    if (format == "ADF") {
        blank.fill(0, 901120); // DD ADF
    } else if (format == "D64") {
        blank.fill(0, 174848); // Standard D64
    } else if (format == "IMG") {
        blank.fill(0xF6, 1474560); // 1.44MB formatted
    } else {
        blank.fill(0, 737280); // 720K default
    }
    
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        /* MF-571: der Rueckgabewert von write() wurde verworfen, und
         * gemeldet wurde `blank.size()` — die BEABSICHTIGTE Groesse.
         *
         * Bei voller Platte oder E/A-Fehler entstand damit eine
         * abgeschnittene Datei plus die Meldung, es seien 174848 Byte
         * geschrieben worden. Dieselbe Form wie MF-122 (dort im
         * Kopier-Pfad) und MF-550 (dort bei verworfenen Flusswechseln):
         * eine gemeldete Zahl, die nicht die gemessene ist.
         *
         * Eine halbe Datei ist schlimmer als keine — sie hat den
         * richtigen Namen und die richtige Endung. */
        qint64 wrote = file.write(blank);
        file.close();
        if (wrote != blank.size()) {
            QFile::remove(path);
            appendOutput(tr("Failed to create %1: only %2 of %3 bytes "
                            "written — partial file removed.")
                         .arg(path).arg(wrote < 0 ? 0 : wrote)
                         .arg(blank.size()));
            appendOutput(QString());
            return;
        }
        appendOutput(tr("Created blank disk: %1 (%2 bytes)")
            .arg(path).arg(wrote));
    } else {
        appendOutput(tr("Failed to create: %1").arg(file.errorString()));
    }
}

void ToolsTab::onBatchStart()
{
    if (m_batchRunning) return;
    
    QString folder = ui->editBatchFolder->text();
    if (folder.isEmpty()) {
        QMessageBox::information(this, tr("Batch"),
            tr("Please specify a folder for batch processing."));
        return;
    }
    
    m_batchRunning = true;
    ui->btnBatchStart->setEnabled(false);
    ui->btnBatchStop->setEnabled(true);
    
    appendOutput(QString("═══════════════════════════════════════"));
    appendOutput(tr("Batch processing: %1").arg(folder));
    appendOutput(tr("(Batch processing not yet implemented)"));
    
    m_batchRunning = false;
    ui->btnBatchStart->setEnabled(true);
    ui->btnBatchStop->setEnabled(false);
}

void ToolsTab::onBatchStop()
{
    m_batchRunning = false;
    ui->btnBatchStart->setEnabled(true);
    ui->btnBatchStop->setEnabled(false);
    appendOutput(tr("Batch processing stopped."));
}

// Browse button implementations
void ToolsTab::onBrowseConvertSource()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select Source"),
        QString(), DiskImageValidator::fileDialogFilter());
    if (!path.isEmpty()) ui->editConvertSource->setText(path);
}

void ToolsTab::onBrowseConvertTarget()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Select Target"));
    if (!path.isEmpty()) ui->editConvertTarget->setText(path);
}

void ToolsTab::onBrowseRepair()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select Image"),
        QString(), DiskImageValidator::fileDialogFilter());
    if (!path.isEmpty()) ui->editRepairFile->setText(path);
}

void ToolsTab::onBrowseCompareA()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select First File"),
        QString(), DiskImageValidator::fileDialogFilter());
    if (!path.isEmpty()) ui->editCompareA->setText(path);
}

void ToolsTab::onBrowseCompareB()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select Second File"),
        QString(), DiskImageValidator::fileDialogFilter());
    if (!path.isEmpty()) ui->editCompareB->setText(path);
}

void ToolsTab::onBrowseAnalyze()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select Image"),
        QString(), DiskImageValidator::fileDialogFilter());
    if (!path.isEmpty()) ui->editAnalyzeFile->setText(path);
}

void ToolsTab::onBrowseBatch()
{
    QString path = QFileDialog::getExistingDirectory(this, tr("Select Folder"));
    if (!path.isEmpty()) ui->editBatchFolder->setText(path);
}

void ToolsTab::onClearOutput()
{
    ui->textOutput->clear();
}

void ToolsTab::onSaveOutput()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Save Output"),
        QString(), "Text (*.txt);;Log (*.log)");
    
    if (!path.isEmpty()) {
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(ui->textOutput->toPlainText().toUtf8());
            file.close();
            appendOutput(tr("Output saved to: %1").arg(path));
        }
    }
}

void ToolsTab::setupFormatConversionMap()
{
    /* -- MF-568: die Handliste ist WEG -----------------------------------
     *
     * Hier standen 40 Zeilen `m_conversionMap["X"] = {"Y", "Z"}` -- die
     * VIERTE Aufzaehlung dessen, was gewandelt werden kann, nach
     * Wandlungstabelle, Rundlauf-Matrix und Verteiler. Sie war die
     * einzige, die der Benutzer je sah, und sie bot Paare an, die die
     * Maschine nicht hat: SCP->ATR, SCP->WOZ, TRD->SCL, D64->TAP.
     *
     * `populateConvertToFormats()` fragt jetzt
     * `uft_convert_list_targets()` -- die Tabelle selbst.
     *
     * Die leere Funktion bleibt stehen, weil der Konstruktor sie ruft und
     * die Kopfdatei sie fuehrt; sie zu loeschen waere eine zweite
     * Aenderung an einer Stelle, die diese Sitzung nicht misst. Was hier
     * NICHT wieder hineingehoert, ist eine Liste. */
}

void ToolsTab::onConvertFromChanged(int index)
{
    QString format = ui->comboConvertFrom->itemText(index);
    populateConvertToFormats(format);
}

void ToolsTab::populateConvertToFormats(const QString& fromFormat)
{
    ui->comboConvertTo->blockSignals(true);
    ui->comboConvertTo->clear();
    
    /* -- MF-568: die Liste kommt jetzt aus der Maschine -----------------
     *
     * Hier stand `m_conversionMap` -- eine von Hand gepflegte Liste, die
     * VIERTE Aufzaehlung dessen, was gewandelt werden kann (nach
     * Wandlungstabelle, Rundlauf-Matrix und Verteiler) und die einzige,
     * die der Benutzer je zu sehen bekam. Sie bot Paare an, die es nicht
     * gibt: SCP->ATR, SCP->WOZ, TRD->SCL, D64->TAP.
     *
     * `uft_convert_list_targets()` liest die Tabelle selbst. Was hier
     * nicht auftaucht, kann das Werkzeug nicht -- und sagt es, bevor
     * jemand klickt. */
    uft_format_t srcFmt =
        uft_format_from_name(fromFormat.toUtf8().constData());
    QStringList targets;
    if (srcFmt != UFT_FORMAT_UNKNOWN) {
        const uft_conversion_path_t* paths[64];
        int n = uft_convert_list_targets(srcFmt, paths, 64);
        for (int i = 0; i < n; i++) {
            if (paths[i]->target == srcFmt) continue;   /* Identitaet */
            QString name = QString::fromUtf8(
                uft_format_get_name(paths[i]->target));
            if (!targets.contains(name)) targets << name;
        }
    }
    if (targets.isEmpty()) {
        /* Nichts anzubieten ist eine Aussage, kein Fehler. Ein
         * Ausweichvorschlag waere geraten. */
        ui->comboConvertTo->addItem(tr("(no conversion available)"));
        ui->comboConvertTo->setEnabled(false);
    } else {
        targets.sort();
        ui->comboConvertTo->addItems(targets);
        ui->comboConvertTo->setEnabled(true);
    }
    
    ui->comboConvertTo->blockSignals(false);
    
    // Show hint
    int targetCount = ui->comboConvertTo->count();
    appendOutput(tr("Format %1 can convert to %2 target format(s)")
                .arg(fromFormat).arg(targetCount));
}

void ToolsTab::onBatchActionChanged(int index)
{
    QString action = ui->comboBatchAction->itemText(index);
    updateBatchOptions(action);
}

void ToolsTab::updateBatchOptions(const QString& action)
{
    // Enable/disable options based on batch action
    bool isConvert = action.contains("Convert", Qt::CaseInsensitive);
    bool isAnalyze = action.contains("Analyze", Qt::CaseInsensitive) || 
                     action.contains("Verify", Qt::CaseInsensitive);
    bool isHash = action.contains("Hash", Qt::CaseInsensitive) ||
                  action.contains("Checksum", Qt::CaseInsensitive);
    
    // Target format only for conversion
    ui->comboConvertTo->setEnabled(isConvert);
    
    // Hash options
    ui->checkCalcHashes->setEnabled(isAnalyze || isHash);
    
    // Log options
    ui->checkBatchLog->setEnabled(true);  // Always available
    
    // Subfolder processing
    ui->checkBatchSubfolders->setEnabled(true);  // Always available
}

// ============================================================================
// RAW Format Configuration
// ============================================================================

void ToolsTab::onRawFormatConfig()
{
    RawFormatDialog dlg(this);
    
    connect(&dlg, &RawFormatDialog::configurationApplied, this, [this](const RawFormatDialog::RawConfig& cfg) {
        appendOutput(tr("═══════════════════════════════════════"));
        appendOutput(tr("RAW Format Configuration Applied"));
        appendOutput(tr("═══════════════════════════════════════"));
        appendOutput(tr("Track Type: %1").arg(cfg.trackType));
        appendOutput(tr("Geometry: %1 tracks × %2 sides × %3 sectors").arg(cfg.tracks).arg(cfg.sides).arg(cfg.sectorsPerTrack));
        appendOutput(tr("Sector Size: %1 bytes").arg(cfg.sectorSize));
        appendOutput(tr("Bitrate: %1 bps").arg(cfg.bitrate));
        appendOutput(tr("Total Size: %1 bytes (%2 KB)").arg(cfg.totalSize).arg(cfg.totalSize / 1024));
    });
    
    dlg.exec();
}

// ============================================================================
// Visual Disk Viewer
// ============================================================================

void ToolsTab::onVisualDisk()
{
    QString path = ui->editAnalyzeFile->text();
    
    VisualDiskDialog dlg(this);
    
    if (!path.isEmpty()) {
        dlg.loadDiskImage(path);
    }
    
    dlg.exec();
}
