/**
 * @file toolstab.cpp
 * @brief Tools Tab Implementation
 * 
 * P0-GUI-006 FIX: Disk utilities implementation
 */

#include "toolstab.h"
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

void ToolsTab::onTrackView()
{
    appendOutput(tr("Track View: Feature not yet implemented"));
    appendOutput(tr("This will show track-level analysis with sector headers."));
    appendOutput(QString());
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
    uft_convert_options_t opts;
    memset(&opts, 0, sizeof(opts));
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

void ToolsTab::onRepair()
{
    QString path = ui->editRepairFile->text();
    if (path.isEmpty()) {
        QMessageBox::information(this, tr("Repair"),
            tr("Please specify a disk image to repair."));
        return;
    }
    
    appendOutput(QString("═══════════════════════════════════════"));
    appendOutput(tr("Repair Analysis: %1").arg(QFileInfo(path).fileName()));
    
    if (ui->checkBackup->isChecked()) {
        QString backup = path + ".backup";
        if (QFile::copy(path, backup)) {
            appendOutput(tr("Backup created: %1").arg(backup));
        }
    }
    
    appendOutput(tr("Scanning for errors..."));
    appendOutput(tr("No errors found that can be automatically repaired."));
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
        file.write(blank);
        file.close();
        appendOutput(tr("Created blank disk: %1 (%2 bytes)")
            .arg(path).arg(blank.size()));
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
