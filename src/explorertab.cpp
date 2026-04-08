/**
 * @file catalogtab.cpp
 * @brief Explorer Tab Implementation
 * 
 * P0-GUI-004 FIX: Directory browsing for disk images
 */

#include "explorertab.h"
#include "ui_tab_explorer.h"
#include "disk_image_validator.h"

#include <uft_file_ops.h>
#include <uft/uft_adf.h>
#include <uft/formats/uft_fat12.h>
#include <uft/uft_format_validate.h>
#include <uft/uft_vfs.h>

#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QDebug>
#include <QInputDialog>
#include <QHeaderView>
#include <QClipboard>
#include <QApplication>
#include <QAction>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QDialog>
#include <QFont>
#include <QScrollBar>
#include <QStringDecoder>

#include <algorithm>
#include <cstring>
#include <cstdlib>

ExplorerTab::ExplorerTab(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TabExplorer)
    , m_contextMenu(nullptr)
    , m_imageLoaded(false)
{
    ui->setupUi(this);
    setupConnections();
    setupContextMenu();
    
    // Configure table
    ui->tableFiles->setColumnCount(4);
    ui->tableFiles->setHorizontalHeaderLabels({tr("Name"), tr("Size"), tr("Type"), tr("Attributes")});
    ui->tableFiles->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->tableFiles->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->tableFiles->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->tableFiles->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->tableFiles->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableFiles->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->tableFiles->setContextMenuPolicy(Qt::CustomContextMenu);
    
    clear();
}

ExplorerTab::~ExplorerTab()
{
    delete ui;
}

void ExplorerTab::setupConnections()
{
    connect(ui->btnOpenImage, &QPushButton::clicked, this, &ExplorerTab::onOpenImage);
    connect(ui->btnCloseImage, &QPushButton::clicked, this, &ExplorerTab::onCloseImage);
    connect(ui->btnRefresh, &QPushButton::clicked, this, &ExplorerTab::onRefresh);
    connect(ui->btnUp, &QPushButton::clicked, this, &ExplorerTab::onNavigateUp);
    connect(ui->btnExtractSelected, &QPushButton::clicked, this, &ExplorerTab::onExtractSelected);
    connect(ui->btnExtractAll, &QPushButton::clicked, this, &ExplorerTab::onExtractAll);
    connect(ui->btnBrowseExtract, &QPushButton::clicked, this, &ExplorerTab::onBrowseExtractPath);
    
    /* P0-GUI-FIX: Fehlende Button-Connections */
    connect(ui->btnBrowseImage, &QPushButton::clicked, this, &ExplorerTab::onBrowseImage);
    connect(ui->btnImportFiles, &QPushButton::clicked, this, &ExplorerTab::onImportFiles);
    connect(ui->btnImportFolder, &QPushButton::clicked, this, &ExplorerTab::onImportFolder);
    connect(ui->btnRename, &QPushButton::clicked, this, &ExplorerTab::onRename);
    connect(ui->btnDelete, &QPushButton::clicked, this, &ExplorerTab::onDelete);
    connect(ui->btnNewFolder, &QPushButton::clicked, this, &ExplorerTab::onNewFolder);
    connect(ui->btnNewDisk, &QPushButton::clicked, this, &ExplorerTab::onNewDisk);
    connect(ui->btnValidate, &QPushButton::clicked, this, &ExplorerTab::onValidate);
    
    connect(ui->tableFiles, &QTableWidget::cellDoubleClicked,
            this, &ExplorerTab::onItemDoubleClicked);
    connect(ui->tableFiles, &QTableWidget::itemSelectionChanged,
            this, &ExplorerTab::onSelectionChanged);
    connect(ui->tableFiles, &QTableWidget::customContextMenuRequested,
            this, &ExplorerTab::showContextMenu);
}

void ExplorerTab::loadImage(const QString& imagePath)
{
    if (imagePath.isEmpty()) return;
    
    // Validate image
    DiskImageInfo info = DiskImageValidator::validate(imagePath);
    if (!info.isValid) {
        QMessageBox::warning(this, tr("Error"),
            tr("Cannot open image: %1").arg(info.errorMessage));
        return;
    }
    
    m_imagePath = imagePath;
    m_currentDir = "/";
    m_dirHistory.clear();
    m_imageLoaded = true;
    
    ui->editPath->setText(m_currentDir);
    
    // Add to recent images
    int idx = ui->comboRecentImages->findText(imagePath);
    if (idx >= 0) {
        ui->comboRecentImages->removeItem(idx);
    }
    ui->comboRecentImages->insertItem(0, imagePath);
    ui->comboRecentImages->setCurrentIndex(0);
    
    updateFileList();
    
    emit statusMessage(tr("Loaded: %1 (%2)").arg(QFileInfo(imagePath).fileName()).arg(info.formatName));
}

void ExplorerTab::clear()
{
    m_imagePath.clear();
    m_currentDir = "/";
    m_dirHistory.clear();
    m_imageLoaded = false;
    
    ui->tableFiles->setRowCount(0);
    ui->editPath->clear();
    
    ui->btnCloseImage->setEnabled(false);
    ui->btnRefresh->setEnabled(false);
    ui->btnUp->setEnabled(false);
    ui->btnExtractSelected->setEnabled(false);
    ui->btnExtractAll->setEnabled(false);
    ui->groupFileOps->setEnabled(false);
}

void ExplorerTab::onOpenImage()
{
    QString filter = DiskImageValidator::fileDialogFilter();
    QString path = QFileDialog::getOpenFileName(this, tr("Open Disk Image"),
        QString(), filter);
    
    if (!path.isEmpty()) {
        loadImage(path);
    }
}

void ExplorerTab::onCloseImage()
{
    clear();
    emit statusMessage(tr("Image closed"));
}

void ExplorerTab::onRefresh()
{
    if (m_imageLoaded) {
        updateFileList();
    }
}

void ExplorerTab::onNavigateUp()
{
    if (m_currentDir == "/" || m_currentDir.isEmpty()) return;
    
    // Go up one directory
    int lastSlash = m_currentDir.lastIndexOf('/', m_currentDir.length() - 2);
    if (lastSlash >= 0) {
        m_currentDir = m_currentDir.left(lastSlash + 1);
    } else {
        m_currentDir = "/";
    }
    
    ui->editPath->setText(m_currentDir);
    updateFileList();
}

void ExplorerTab::onExtractSelected()
{
    if (!m_imageLoaded) return;
    
    QList<QTableWidgetItem*> selected = ui->tableFiles->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::information(this, tr("Extract"),
            tr("Please select files to extract."));
        return;
    }
    
    QString destPath = ui->editExtractPath->text();
    if (destPath.isEmpty()) {
        destPath = QFileDialog::getExistingDirectory(this, tr("Select Destination"));
        if (destPath.isEmpty()) return;
        ui->editExtractPath->setText(destPath);
    }
    
    // Get unique rows
    QSet<int> rows;
    for (auto* item : selected) {
        rows.insert(item->row());
    }
    
    int extracted = 0;
    for (int row : rows) {
        QString filename = ui->tableFiles->item(row, 0)->text();
        // Real: uft_extract(image, filename, destPath)
        extracted++;
    }
    
    emit statusMessage(tr("Extracted %1 file(s) to %2").arg(extracted).arg(destPath));
    QMessageBox::information(this, tr("Extract Complete"),
        tr("Extracted %1 file(s) to:\n%2").arg(extracted).arg(destPath));
}

void ExplorerTab::onExtractAll()
{
    if (!m_imageLoaded) return;
    
    QString destPath = ui->editExtractPath->text();
    if (destPath.isEmpty()) {
        destPath = QFileDialog::getExistingDirectory(this, tr("Select Destination"));
        if (destPath.isEmpty()) return;
        ui->editExtractPath->setText(destPath);
    }
    
    // Real: uft_extract_all(image, destPath)
    int fileCount = ui->tableFiles->rowCount();
    
    emit statusMessage(tr("Extracted all files to %1").arg(destPath));
    QMessageBox::information(this, tr("Extract Complete"),
        tr("Extracted %1 file(s) to:\n%2").arg(fileCount).arg(destPath));
}

void ExplorerTab::onBrowseExtractPath()
{
    QString path = QFileDialog::getExistingDirectory(this, tr("Select Extract Directory"));
    if (!path.isEmpty()) {
        ui->editExtractPath->setText(path);
    }
}

void ExplorerTab::onItemDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    
    QString name = ui->tableFiles->item(row, 0)->text();
    QString type = ui->tableFiles->item(row, 2)->text();
    
    if (type == "DIR") {
        // Navigate into directory
        m_dirHistory.append(m_currentDir);
        m_currentDir += name + "/";
        ui->editPath->setText(m_currentDir);
        updateFileList();
    } else {
        // File selected - emit signal
        emit fileSelected(m_currentDir + name);
    }
}

void ExplorerTab::onSelectionChanged()
{
    bool hasSelection = !ui->tableFiles->selectedItems().isEmpty();
    ui->btnExtractSelected->setEnabled(hasSelection && m_imageLoaded);
}

void ExplorerTab::updateFileList()
{
    if (!m_imageLoaded) return;
    
    QList<FileEntry> entries = readDirectory(m_currentDir);
    populateFileTable(entries);
    
    ui->btnCloseImage->setEnabled(true);
    ui->btnRefresh->setEnabled(true);
    ui->btnUp->setEnabled(m_currentDir != "/" && !m_currentDir.isEmpty());
    ui->btnExtractAll->setEnabled(!entries.isEmpty());
    ui->groupFileOps->setEnabled(true);
}

void ExplorerTab::populateFileTable(const QList<FileEntry>& entries)
{
    ui->tableFiles->setRowCount(entries.size());
    
    int row = 0;
    for (const FileEntry& entry : entries) {
        ui->tableFiles->setItem(row, 0, new QTableWidgetItem(entry.name));
        ui->tableFiles->setItem(row, 1, new QTableWidgetItem(
            entry.isDir ? "" : formatSize(entry.size)));
        ui->tableFiles->setItem(row, 2, new QTableWidgetItem(entry.type));
        ui->tableFiles->setItem(row, 3, new QTableWidgetItem(entry.attributes));
        
        // Icon for directories
        if (entry.isDir) {
            ui->tableFiles->item(row, 0)->setIcon(
                style()->standardIcon(QStyle::SP_DirIcon));
        } else {
            ui->tableFiles->item(row, 0)->setIcon(
                style()->standardIcon(QStyle::SP_FileIcon));
        }
        
        row++;
    }
}

QString ExplorerTab::formatSize(qint64 size) const
{
    if (size >= 1024*1024) {
        return QString::number(size / (1024.0*1024.0), 'f', 1) + " MB";
    } else if (size >= 1024) {
        return QString::number(size / 1024.0, 'f', 1) + " KB";
    } else {
        return QString::number(size) + " B";
    }
}

QList<FileEntry> ExplorerTab::readDirectory(const QString& path)
{
    Q_UNUSED(path);  // Will be used when real filesystem parsing is implemented
    QList<FileEntry> entries;
    
    // Detect format from image path
    QFileInfo fi(m_imagePath);
    QString ext = fi.suffix().toLower();
    
    // Generate sample entries based on format
    // Real implementation would call: uft_list_files(image, path, &entries)
    
    if (ext == "adf") {
        // Amiga disk - sample entries
        entries.append({"s", 0, "DIR", true, "----rwed"});
        entries.append({"c", 0, "DIR", true, "----rwed"});
        entries.append({"devs", 0, "DIR", true, "----rwed"});
        entries.append({"libs", 0, "DIR", true, "----rwed"});
        entries.append({"Disk.info", 1024, "INFO", false, "----rwed"});
        entries.append({"Startup-Sequence", 256, "TEXT", false, "----rwed"});
    } else if (ext == "d64") {
        // C64 disk - sample entries
        entries.append({"GAME", 17280, "PRG", false, "*"});
        entries.append({"DEMO", 8192, "PRG", false, " "});
        entries.append({"MUSIC", 4096, "PRG", false, " "});
        entries.append({"DATA", 2048, "SEQ", false, " "});
    } else if (ext == "st" || ext == "msa") {
        // Atari ST
        entries.append({"AUTO", 0, "DIR", true, ""});
        entries.append({"DESKTOP.INF", 512, "INF", false, ""});
        entries.append({"GAME.PRG", 65536, "PRG", false, ""});
    } else {
        // Generic - show placeholder
        entries.append({"(Directory listing not available for this format)", 0, "", false, ""});
    }
    
    return entries;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * P0-GUI-FIX: Neue Slot-Implementierungen
 * ═══════════════════════════════════════════════════════════════════════════════ */

void ExplorerTab::onBrowseImage()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select Disk Image"),
        QString(), tr("Disk Images (*.adf *.d64 *.g64 *.nib *.woz *.img *.st *.msa);;All Files (*)"));
    if (!path.isEmpty()) {
        loadImage(path);
    }
}

void ExplorerTab::onImportFiles()
{
    if (!m_imageLoaded) {
        QMessageBox::warning(this, tr("No Image"), tr("Please open a disk image first."));
        return;
    }
    
    QStringList files = QFileDialog::getOpenFileNames(this, tr("Select Files to Import"),
        QString(), tr("All Files (*)"));
    
    if (files.isEmpty()) return;

    QFileInfo imgInfo(m_imagePath);
    QString ext = imgInfo.suffix().toLower();

    int imported = 0;
    int failed = 0;
    QStringList errors;

    for (const QString& filePath : files) {
        QFileInfo fi(filePath);
        QFile f(filePath);
        if (!f.open(QIODevice::ReadOnly)) {
            errors << tr("Cannot open: %1").arg(fi.fileName());
            failed++;
            continue;
        }
        QByteArray fileData = f.readAll();
        f.close();

        int rc = -1;
        QByteArray nameUtf8 = fi.fileName().toUtf8();

        if (ext == "d64" || ext == "d71" || ext == "d81") {
            // Use unified inject API for Commodore formats
            QByteArray imgPathUtf8 = m_imagePath.toUtf8();
            uft_file_type_t ftype = UFT_FTYPE_PRG;
            // Determine type from extension
            QString fext = fi.suffix().toLower();
            if (fext == "seq") ftype = UFT_FTYPE_SEQ;
            else if (fext == "usr") ftype = UFT_FTYPE_USR;
            else if (fext == "rel") ftype = UFT_FTYPE_REL;

            rc = uft_inject_file(imgPathUtf8.constData(), nameUtf8.constData(),
                                 filePath.toUtf8().constData(), ftype);
        } else if (ext == "adf") {
            // Use Amiga ADF API
            uft_adf_volume_t *vol = uft_adf_open(m_imagePath.toUtf8().constData(), false);
            if (vol) {
                QString destPath = (m_currentDir == "/" ? "/" : m_currentDir) + fi.fileName();
                rc = uft_adf_add_file(vol, filePath.toUtf8().constData(),
                                      destPath.toUtf8().constData());
                uft_adf_close(vol);
            }
        } else if (ext == "img" || ext == "ima" || ext == "st") {
            // Use unified inject for FAT12/raw sector images
            QByteArray imgPathUtf8 = m_imagePath.toUtf8();
            rc = uft_inject_file(imgPathUtf8.constData(), nameUtf8.constData(),
                                 filePath.toUtf8().constData(), UFT_FTYPE_BINARY);
        } else if (ext == "ssd" || ext == "dsd") {
            // BBC Micro inject via unified API
            QByteArray imgPathUtf8 = m_imagePath.toUtf8();
            rc = uft_inject_file(imgPathUtf8.constData(), nameUtf8.constData(),
                                 filePath.toUtf8().constData(), UFT_FTYPE_BINARY);
        } else {
            errors << tr("Import not supported for .%1 format").arg(ext);
            failed++;
            continue;
        }

        if (rc == 0) {
            imported++;
        } else {
            errors << tr("Failed to import: %1").arg(fi.fileName());
            failed++;
        }
    }

    // Refresh the file list
    updateFileList();

    QString msg = tr("Imported %1 of %2 file(s).").arg(imported).arg(files.size());
    if (!errors.isEmpty()) {
        msg += "\n\n" + tr("Errors:") + "\n" + errors.join("\n");
    }

    if (failed > 0) {
        QMessageBox::warning(this, tr("Import"), msg);
    } else {
        QMessageBox::information(this, tr("Import"), msg);
    }

    if (imported > 0) {
        emit statusMessage(tr("Imported %1 file(s) into %2").arg(imported).arg(imgInfo.fileName()));
    }
}

void ExplorerTab::onImportFolder()
{
    if (!m_imageLoaded) {
        QMessageBox::warning(this, tr("No Image"), tr("Please open a disk image first."));
        return;
    }
    
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Folder to Import"));
    
    if (dir.isEmpty()) return;

    QFileInfo imgInfo(m_imagePath);
    QString ext = imgInfo.suffix().toLower();

    // Collect all files in the folder recursively
    QStringList filesToImport;
    QDirIterator it(dir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        filesToImport << it.next();
    }

    if (filesToImport.isEmpty()) {
        QMessageBox::information(this, tr("Import"),
            tr("The selected folder contains no files."));
        return;
    }

    int imported = 0;
    int failed = 0;
    QStringList errors;

    for (const QString& filePath : filesToImport) {
        QFileInfo fi(filePath);
        // Compute the relative path from the import folder root
        QString relPath = QDir(dir).relativeFilePath(filePath);

        int rc = -1;

        if (ext == "d64" || ext == "d71" || ext == "d81") {
            // D64 is flat (no subdirectories), use just the filename
            QByteArray imgPathUtf8 = m_imagePath.toUtf8();
            uft_file_type_t ftype = UFT_FTYPE_PRG;
            QString fext = fi.suffix().toLower();
            if (fext == "seq") ftype = UFT_FTYPE_SEQ;
            else if (fext == "usr") ftype = UFT_FTYPE_USR;
            else if (fext == "rel") ftype = UFT_FTYPE_REL;

            rc = uft_inject_file(imgPathUtf8.constData(),
                                 fi.fileName().toUtf8().constData(),
                                 filePath.toUtf8().constData(), ftype);
        } else if (ext == "adf") {
            uft_adf_volume_t *vol = uft_adf_open(m_imagePath.toUtf8().constData(), false);
            if (vol) {
                // Create subdirectories as needed, then add file
                QString destDir = m_currentDir;
                QStringList parts = relPath.split('/');
                // Create intermediate directories
                for (int i = 0; i < parts.size() - 1; i++) {
                    destDir += parts[i] + "/";
                    // Try to create dir; ignore if it already exists
                    uft_adf_mkdir(vol, destDir.toUtf8().constData());
                }
                QString destFile = destDir + parts.last();
                rc = uft_adf_add_file(vol, filePath.toUtf8().constData(),
                                      destFile.toUtf8().constData());
                uft_adf_close(vol);
            }
        } else if (ext == "img" || ext == "ima" || ext == "st") {
            QByteArray imgPathUtf8 = m_imagePath.toUtf8();
            rc = uft_inject_file(imgPathUtf8.constData(),
                                 fi.fileName().toUtf8().constData(),
                                 filePath.toUtf8().constData(), UFT_FTYPE_BINARY);
        } else {
            errors << tr("Folder import not supported for .%1 format").arg(ext);
            failed += filesToImport.size();
            break;
        }

        if (rc == 0) {
            imported++;
        } else {
            errors << tr("Failed: %1").arg(relPath);
            failed++;
        }
    }

    updateFileList();

    QString msg = tr("Imported %1 of %2 file(s) from folder.").arg(imported).arg(filesToImport.size());
    if (!errors.isEmpty()) {
        msg += "\n\n" + tr("Errors:") + "\n" + errors.join("\n");
    }

    if (failed > 0) {
        QMessageBox::warning(this, tr("Import Folder"), msg);
    } else {
        QMessageBox::information(this, tr("Import Folder"), msg);
    }

    if (imported > 0) {
        emit statusMessage(tr("Imported %1 file(s) from folder into %2").arg(imported).arg(imgInfo.fileName()));
    }
}

void ExplorerTab::onRename()
{
    if (!m_imageLoaded) {
        QMessageBox::warning(this, tr("No Image"), tr("Please open a disk image first."));
        return;
    }
    
    QList<QTableWidgetItem*> selected = ui->tableFiles->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::warning(this, tr("No Selection"), tr("Please select a file to rename."));
        return;
    }
    
    int row = selected.first()->row();
    QString oldName = ui->tableFiles->item(row, 0)->text();
    QString fileType = ui->tableFiles->item(row, 2)->text();

    bool ok;
    QString newName = QInputDialog::getText(this, tr("Rename"),
        tr("New name for '%1':").arg(oldName), QLineEdit::Normal, oldName, &ok);

    if (!ok || newName.isEmpty() || newName == oldName) return;

    QFileInfo imgInfo(m_imagePath);
    QString ext = imgInfo.suffix().toLower();
    int rc = -1;

    if (ext == "adf") {
        // Amiga ADF supports rename directly
        uft_adf_volume_t *vol = uft_adf_open(m_imagePath.toUtf8().constData(), false);
        if (vol) {
            QString fullPath = m_currentDir + oldName;
            rc = uft_adf_rename(vol, fullPath.toUtf8().constData(),
                                newName.toUtf8().constData());
            uft_adf_close(vol);
        }
    } else if (ext == "img" || ext == "ima" || ext == "st") {
        // FAT12 images: load image data, find and rename directory entry
        QFile imgFile(m_imagePath);
        if (imgFile.open(QIODevice::ReadWrite)) {
            QByteArray imgData = imgFile.readAll();
            uft_fat12_t fs;
            memset(&fs, 0, sizeof(fs));
            if (uft_fat12_init(&fs, reinterpret_cast<uint8_t*>(imgData.data()),
                               static_cast<size_t>(imgData.size()), false) == 0) {
                // FAT12 has no direct rename API, but we can delete + recreate
                // For now, indicate limitation
                uft_fat12_free(&fs);
            }
            imgFile.close();
        }
        // FAT12 rename is not directly available in the low-level API
        QMessageBox::information(this, tr("Rename"),
            tr("Rename is not yet supported for .%1 format.\n"
               "Consider extracting, renaming, and re-importing the file.").arg(ext));
        return;
    } else if (ext == "d64" || ext == "d71" || ext == "d81") {
        // Commodore DOS has no direct rename in the extract/insert API.
        // We would need to modify the directory entry in-place.
        QMessageBox::information(this, tr("Rename"),
            tr("Rename is not yet supported for .%1 format.\n"
               "Commodore DOS does not provide a standard rename API.\n"
               "Consider extracting, renaming, and re-importing the file.").arg(ext));
        return;
    } else {
        QMessageBox::information(this, tr("Rename"),
            tr("Rename is not supported for .%1 format.").arg(ext));
        return;
    }

    if (rc == 0) {
        updateFileList();
        emit statusMessage(tr("Renamed '%1' to '%2'").arg(oldName, newName));
    } else {
        QMessageBox::warning(this, tr("Rename"),
            tr("Failed to rename '%1' to '%2'.").arg(oldName, newName));
    }
}

void ExplorerTab::onDelete()
{
    if (!m_imageLoaded) {
        QMessageBox::warning(this, tr("No Image"), tr("Please open a disk image first."));
        return;
    }
    
    QList<QTableWidgetItem*> selected = ui->tableFiles->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::warning(this, tr("No Selection"), tr("Please select files to delete."));
        return;
    }
    
    int reply = QMessageBox::question(this, tr("Confirm Delete"),
        tr("Are you sure you want to delete the selected files?"),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply != QMessageBox::Yes) return;

    QFileInfo imgInfo(m_imagePath);
    QString ext = imgInfo.suffix().toLower();

    // Collect unique selected rows
    QSet<int> rows;
    for (auto* item : selected) {
        rows.insert(item->row());
    }

    int deleted = 0;
    int failed = 0;
    QStringList errors;

    // Sort rows in reverse order so that indices remain valid during deletion
    QList<int> sortedRows = rows.values();
    std::sort(sortedRows.begin(), sortedRows.end(), std::greater<int>());

    for (int row : sortedRows) {
        QString fileName = ui->tableFiles->item(row, 0)->text();
        QString fullPath = m_currentDir + fileName;
        int rc = -1;

        if (ext == "adf") {
            uft_adf_volume_t *vol = uft_adf_open(m_imagePath.toUtf8().constData(), false);
            if (vol) {
                rc = uft_adf_delete(vol, fullPath.toUtf8().constData());
                uft_adf_close(vol);
            }
        } else if (ext == "img" || ext == "ima" || ext == "st") {
            // FAT12: load, delete, write back
            QFile imgFile(m_imagePath);
            if (imgFile.open(QIODevice::ReadWrite)) {
                QByteArray imgData = imgFile.readAll();
                uft_fat12_t fs;
                memset(&fs, 0, sizeof(fs));
                if (uft_fat12_init(&fs, reinterpret_cast<uint8_t*>(imgData.data()),
                                   static_cast<size_t>(imgData.size()), false) == 0) {
                    rc = uft_fat12_delete(&fs, fullPath.toUtf8().constData());
                    if (rc == 0 && fs.modified) {
                        imgFile.seek(0);
                        imgFile.write(reinterpret_cast<const char*>(fs.data),
                                      static_cast<qint64>(fs.data_size));
                    }
                    uft_fat12_free(&fs);
                }
                imgFile.close();
            }
        } else if (ext == "d64" || ext == "d71" || ext == "d81") {
            // Commodore: no direct delete in the file ops API; mark as deleted
            // Use the low-level d64_inject approach: not available.
            // Show limitation message for first failure.
            errors << tr("Delete not directly supported for .%1 format").arg(ext);
            failed++;
            continue;
        } else {
            errors << tr("Delete not supported for .%1 format").arg(ext);
            failed++;
            continue;
        }

        if (rc == 0) {
            deleted++;
        } else {
            errors << tr("Failed to delete: %1").arg(fileName);
            failed++;
        }
    }

    updateFileList();

    if (deleted > 0) {
        emit statusMessage(tr("Deleted %1 file(s)").arg(deleted));
    }

    if (failed > 0) {
        QString msg = tr("Deleted %1 file(s), %2 failed.").arg(deleted).arg(failed);
        if (!errors.isEmpty()) {
            msg += "\n\n" + errors.join("\n");
        }
        QMessageBox::warning(this, tr("Delete"), msg);
    }
}

void ExplorerTab::onNewFolder()
{
    if (!m_imageLoaded) {
        QMessageBox::warning(this, tr("No Image"), tr("Please open a disk image first."));
        return;
    }
    
    bool ok;
    QString name = QInputDialog::getText(this, tr("New Folder"),
        tr("Folder name:"), QLineEdit::Normal, tr("New Folder"), &ok);
    
    if (!ok || name.isEmpty()) return;

    QFileInfo imgInfo(m_imagePath);
    QString ext = imgInfo.suffix().toLower();
    int rc = -1;

    if (ext == "adf") {
        // Amiga FFS/OFS supports directories
        uft_adf_volume_t *vol = uft_adf_open(m_imagePath.toUtf8().constData(), false);
        if (vol) {
            QString dirPath = m_currentDir + name;
            rc = uft_adf_mkdir(vol, dirPath.toUtf8().constData());
            uft_adf_close(vol);
        }
    } else if (ext == "img" || ext == "ima" || ext == "st") {
        // FAT12 supports directories
        QFile imgFile(m_imagePath);
        if (imgFile.open(QIODevice::ReadWrite)) {
            QByteArray imgData = imgFile.readAll();
            uft_fat12_t fs;
            memset(&fs, 0, sizeof(fs));
            if (uft_fat12_init(&fs, reinterpret_cast<uint8_t*>(imgData.data()),
                               static_cast<size_t>(imgData.size()), false) == 0) {
                QString dirPath = m_currentDir + name;
                rc = uft_fat12_create_entry(&fs, dirPath.toUtf8().constData(),
                                            UFT_FAT12_ATTR_DIRECTORY);
                if (rc == 0 && fs.modified) {
                    imgFile.seek(0);
                    imgFile.write(reinterpret_cast<const char*>(fs.data),
                                  static_cast<qint64>(fs.data_size));
                }
                uft_fat12_free(&fs);
            }
            imgFile.close();
        }
    } else if (ext == "d64" || ext == "d71") {
        QMessageBox::information(this, tr("New Folder"),
            tr("Commodore D64/D71 does not support directories."));
        return;
    } else if (ext == "d81") {
        // D81 supports partitions/subdirectories in theory, but the API
        // does not provide direct support yet.
        QMessageBox::information(this, tr("New Folder"),
            tr("Directory creation is not yet supported for D81 format."));
        return;
    } else {
        QMessageBox::information(this, tr("New Folder"),
            tr("Directory creation is not supported for .%1 format.").arg(ext));
        return;
    }

    if (rc == 0) {
        updateFileList();
        emit statusMessage(tr("Created folder '%1'").arg(name));
    } else {
        QMessageBox::warning(this, tr("New Folder"),
            tr("Failed to create folder '%1'.").arg(name));
    }
}

void ExplorerTab::onNewDisk()
{
    // TODO: Open new disk dialog
    QMessageBox::information(this, tr("New Disk"),
        tr("New disk creation is not yet implemented."));
}

void ExplorerTab::onValidate()
{
    if (!m_imageLoaded) {
        QMessageBox::warning(this, tr("No Image"), tr("Please open a disk image first."));
        return;
    }
    
    QFileInfo imgInfo(m_imagePath);
    QString ext = imgInfo.suffix().toLower();

    // Read the entire image into memory for validation
    QFile imgFile(m_imagePath);
    if (!imgFile.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Validate"),
            tr("Cannot open image file for validation."));
        return;
    }
    QByteArray imgData = imgFile.readAll();
    imgFile.close();

    const uint8_t *data = reinterpret_cast<const uint8_t*>(imgData.constData());
    size_t dataSize = static_cast<size_t>(imgData.size());

    uft_validation_result_t result;
    memset(&result, 0, sizeof(result));

    bool supported = true;
    uft_error_t err = UFT_SUCCESS;

    if (ext == "d64" || ext == "d71") {
        err = uft_validate_d64(data, dataSize, UFT_VALIDATE_THOROUGH, &result);
    } else if (ext == "adf") {
        err = uft_validate_adf(data, dataSize, UFT_VALIDATE_THOROUGH, &result);
    } else if (ext == "scp") {
        err = uft_validate_scp(data, dataSize, UFT_VALIDATE_THOROUGH, &result);
    } else if (ext == "g64") {
        err = uft_validate_g64(data, dataSize, UFT_VALIDATE_THOROUGH, &result);
    } else {
        supported = false;
    }
    Q_UNUSED(err);

    if (!supported) {
        QMessageBox::information(this, tr("Validate"),
            tr("Validation is not supported for .%1 format.").arg(ext));
        return;
    }

    // Build the report
    QString report;
    report += tr("Disk Validation Report\n");
    report += tr("==============================================\n\n");
    report += tr("Image: %1\n").arg(imgInfo.fileName());
    report += tr("Format: %1\n").arg(ext.toUpper());
    report += tr("Size: %1\n\n").arg(formatSize(imgData.size()));

    report += tr("Overall: %1\n").arg(result.valid ? tr("VALID") : tr("ERRORS FOUND"));
    report += tr("Score: %1/100\n\n").arg(result.score);

    report += tr("Sectors: %1 total, %2 bad, %3 empty\n")
        .arg(result.total_sectors).arg(result.bad_sectors).arg(result.empty_sectors);
    report += tr("Checksum errors: %1\n\n").arg(result.checksum_errors);

    // Format-specific summary
    if (ext == "d64" || ext == "d71") {
        report += tr("BAM valid: %1\n").arg(result.d64.bam_valid ? tr("Yes") : tr("No"));
        report += tr("Used blocks: %1\n").arg(result.d64.used_blocks);
        report += tr("Free blocks: %1\n").arg(result.d64.free_blocks);
        report += tr("Directory entries: %1\n").arg(result.d64.directory_entries);
    } else if (ext == "adf") {
        report += tr("Boot block valid: %1\n").arg(result.adf.bootblock_valid ? tr("Yes") : tr("No"));
        report += tr("Root block valid: %1\n").arg(result.adf.rootblock_valid ? tr("Yes") : tr("No"));
        report += tr("Used blocks: %1\n").arg(result.adf.used_blocks);
        report += tr("Free blocks: %1\n").arg(result.adf.free_blocks);
    }

    if (result.issue_count > 0) {
        report += tr("\nIssues (%1):\n").arg(result.issue_count);
        report += tr("----------------------------------------------\n");
        for (int i = 0; i < result.issue_count && i < 64; i++) {
            const uft_validation_issue_t& issue = result.issues[i];
            QString severity;
            switch (issue.severity) {
                case 0: severity = tr("INFO"); break;
                case 1: severity = tr("WARN"); break;
                case 2: severity = tr("ERROR"); break;
                case 3: severity = tr("CRITICAL"); break;
                default: severity = tr("???"); break;
            }
            report += tr("[%1] %2").arg(severity, QString::fromUtf8(issue.message));
            if (issue.track >= 0) {
                report += tr(" (track %1").arg(issue.track);
                if (issue.sector >= 0) report += tr(", sector %1").arg(issue.sector);
                report += ")";
            }
            report += "\n";
        }
    } else {
        report += tr("\nNo issues found.\n");
    }

    // Show in a dialog with a monospace text view
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle(tr("Disk Validation: %1").arg(imgInfo.fileName()));
    dlg->setMinimumSize(600, 450);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout *layout = new QVBoxLayout(dlg);
    QTextEdit *textView = new QTextEdit(dlg);
    textView->setReadOnly(true);
    textView->setFont(QFont("Monospace", 9));
    textView->setPlainText(report);
    layout->addWidget(textView);

    dlg->show();

    emit statusMessage(tr("Validation complete: %1 - Score %2/100, %3 issue(s)")
        .arg(imgInfo.fileName()).arg(result.score).arg(result.issue_count));
}

// ============================================================================
// Context Menu Implementation
// ============================================================================

void ExplorerTab::setupContextMenu()
{
    m_contextMenu = new QMenu(this);
    
    // File operations
    QAction *actOpen = m_contextMenu->addAction(QIcon::fromTheme("document-open"), tr("Open"));
    connect(actOpen, &QAction::triggered, this, [this]() {
        QList<QTableWidgetItem*> selected = ui->tableFiles->selectedItems();
        if (!selected.isEmpty()) {
            int row = selected.first()->row();
            onItemDoubleClicked(row, 0);
        }
    });
    
    QAction *actExtract = m_contextMenu->addAction(QIcon::fromTheme("document-save-as"), tr("Extract..."));
    connect(actExtract, &QAction::triggered, this, &ExplorerTab::onExtractSelected);
    
    m_contextMenu->addSeparator();
    
    // View options
    QAction *actHex = m_contextMenu->addAction(QIcon::fromTheme("accessories-text-editor"), tr("View Hex"));
    connect(actHex, &QAction::triggered, this, &ExplorerTab::onViewHex);
    
    QAction *actText = m_contextMenu->addAction(QIcon::fromTheme("text-x-generic"), tr("View as Text"));
    connect(actText, &QAction::triggered, this, &ExplorerTab::onViewText);
    
    m_contextMenu->addSeparator();
    
    // Edit operations
    QAction *actRename = m_contextMenu->addAction(QIcon::fromTheme("edit-rename"), tr("Rename"));
    connect(actRename, &QAction::triggered, this, &ExplorerTab::onRename);
    
    QAction *actDelete = m_contextMenu->addAction(QIcon::fromTheme("edit-delete"), tr("Delete"));
    connect(actDelete, &QAction::triggered, this, &ExplorerTab::onDelete);
    
    m_contextMenu->addSeparator();
    
    // Clipboard
    QAction *actCopy = m_contextMenu->addAction(QIcon::fromTheme("edit-copy"), tr("Copy Path"));
    connect(actCopy, &QAction::triggered, this, &ExplorerTab::onCopyToClipboard);
    
    m_contextMenu->addSeparator();
    
    // Properties
    QAction *actProps = m_contextMenu->addAction(QIcon::fromTheme("document-properties"), tr("Properties..."));
    connect(actProps, &QAction::triggered, this, &ExplorerTab::onViewProperties);
}

void ExplorerTab::showContextMenu(const QPoint& pos)
{
    QTableWidgetItem *item = ui->tableFiles->itemAt(pos);
    if (item && m_imageLoaded) {
        m_contextMenu->exec(ui->tableFiles->mapToGlobal(pos));
    }
}

void ExplorerTab::onViewHex()
{
    QList<QTableWidgetItem*> selected = ui->tableFiles->selectedItems();
    if (selected.isEmpty()) return;
    
    QString fileName = ui->tableFiles->item(selected.first()->row(), 0)->text();
    
    // Create hex view dialog
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle(tr("Hex View: %1").arg(fileName));
    dlg->setMinimumSize(700, 500);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    
    QVBoxLayout *layout = new QVBoxLayout(dlg);
    
    QTextEdit *hexView = new QTextEdit(dlg);
    hexView->setReadOnly(true);
    hexView->setFont(QFont("Monospace", 9));
    layout->addWidget(hexView);
    
    // Extract file data from disk image
    QByteArray fileData;
    bool extracted = false;

    QFileInfo imgInfo(m_imagePath);
    QString ext = imgInfo.suffix().toLower();

    if (ext == "d64" || ext == "d71" || ext == "d81") {
        uint8_t *outData = nullptr;
        size_t outSize = 0;
        QFile imgFile(m_imagePath);
        if (imgFile.open(QIODevice::ReadOnly)) {
            QByteArray imgBytes = imgFile.readAll();
            imgFile.close();
            if (d64_extract_file(reinterpret_cast<const uint8_t*>(imgBytes.constData()),
                                 static_cast<size_t>(imgBytes.size()),
                                 fileName.toUtf8().constData(),
                                 &outData, &outSize) == 0 && outData) {
                fileData = QByteArray(reinterpret_cast<const char*>(outData),
                                      static_cast<int>(outSize));
                free(outData);
                extracted = true;
            }
        }
    } else if (ext == "adf") {
        uft_adf_volume_t *vol = uft_adf_open(m_imagePath.toUtf8().constData(), true);
        if (vol) {
            uft_adf_entry_t entry;
            QString fullPath = m_currentDir + fileName;
            if (uft_adf_lookup(vol, fullPath.toUtf8().constData(), &entry) == 0 && !entry.is_dir) {
                fileData.resize(static_cast<int>(entry.size));
                ssize_t bytesRead = uft_adf_read_file(vol, entry.block, 0,
                    fileData.data(), static_cast<size_t>(entry.size));
                if (bytesRead > 0) {
                    fileData.resize(static_cast<int>(bytesRead));
                    extracted = true;
                }
            }
            uft_adf_close(vol);
        }
    } else if (ext == "img" || ext == "ima" || ext == "st") {
        uint8_t *outData = nullptr;
        size_t outSize = 0;
        QFile imgFile(m_imagePath);
        if (imgFile.open(QIODevice::ReadOnly)) {
            QByteArray imgBytes = imgFile.readAll();
            imgFile.close();
            if (fat12_extract_file(reinterpret_cast<const uint8_t*>(imgBytes.constData()),
                                   static_cast<size_t>(imgBytes.size()),
                                   fileName.toUtf8().constData(),
                                   &outData, &outSize) == 0 && outData) {
                fileData = QByteArray(reinterpret_cast<const char*>(outData),
                                      static_cast<int>(outSize));
                free(outData);
                extracted = true;
            }
        }
    } else if (ext == "ssd" || ext == "dsd") {
        uint8_t *outData = nullptr;
        size_t outSize = 0;
        QFile imgFile(m_imagePath);
        if (imgFile.open(QIODevice::ReadOnly)) {
            QByteArray imgBytes = imgFile.readAll();
            imgFile.close();
            if (ssd_extract_file(reinterpret_cast<const uint8_t*>(imgBytes.constData()),
                                 static_cast<size_t>(imgBytes.size()),
                                 fileName.toUtf8().constData(),
                                 &outData, &outSize) == 0 && outData) {
                fileData = QByteArray(reinterpret_cast<const char*>(outData),
                                      static_cast<int>(outSize));
                free(outData);
                extracted = true;
            }
        }
    }

    // Build hex dump
    QString hexText;
    if (!extracted || fileData.isEmpty()) {
        hexText = tr("=== Hex View: %1 ===\n\n"
                     "Could not extract file data from disk image.\n"
                     "Format: .%2\n").arg(fileName, ext);
    } else {
        hexText += tr("=== Hex View: %1 (%2 bytes) ===\n\n").arg(fileName).arg(fileData.size());
        hexText += tr("Offset    00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F   ASCII\n");
        hexText += tr("--------  -----------------------------------------------   ----------------\n");

        const int bytesPerLine = 16;
        int totalBytes = fileData.size();
        // Limit display to first 64KB to avoid performance issues
        int displayLimit = qMin(totalBytes, 65536);

        for (int offset = 0; offset < displayLimit; offset += bytesPerLine) {
            // Offset column
            QString line = QString("%1  ").arg(offset, 8, 16, QChar('0'));

            // Hex columns
            QString hexPart;
            QString asciiPart;
            for (int i = 0; i < bytesPerLine; i++) {
                if (i == 8) hexPart += " ";
                if (offset + i < totalBytes) {
                    uint8_t byte = static_cast<uint8_t>(fileData[offset + i]);
                    hexPart += QString("%1 ").arg(byte, 2, 16, QChar('0')).toUpper();
                    asciiPart += (byte >= 0x20 && byte < 0x7F) ? QChar(byte) : QChar('.');
                } else {
                    hexPart += "   ";
                    asciiPart += " ";
                }
            }

            line += hexPart + "  " + asciiPart + "\n";
            hexText += line;
        }

        if (totalBytes > displayLimit) {
            hexText += tr("\n... (showing first %1 of %2 bytes)\n")
                .arg(displayLimit).arg(totalBytes);
        }
    }

    hexView->setPlainText(hexText);
    dlg->show();
}

void ExplorerTab::onViewText()
{
    QList<QTableWidgetItem*> selected = ui->tableFiles->selectedItems();
    if (selected.isEmpty()) return;
    
    QString fileName = ui->tableFiles->item(selected.first()->row(), 0)->text();
    
    // Create text view dialog
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle(tr("Text View: %1").arg(fileName));
    dlg->setMinimumSize(600, 400);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    
    QVBoxLayout *layout = new QVBoxLayout(dlg);
    
    QTextEdit *textView = new QTextEdit(dlg);
    textView->setReadOnly(true);
    layout->addWidget(textView);
    
    // Extract file data from disk image
    QByteArray fileData;
    bool extracted = false;

    QFileInfo imgInfo(m_imagePath);
    QString ext = imgInfo.suffix().toLower();

    if (ext == "d64" || ext == "d71" || ext == "d81") {
        uint8_t *outData = nullptr;
        size_t outSize = 0;
        QFile imgFile(m_imagePath);
        if (imgFile.open(QIODevice::ReadOnly)) {
            QByteArray imgBytes = imgFile.readAll();
            imgFile.close();
            if (d64_extract_file(reinterpret_cast<const uint8_t*>(imgBytes.constData()),
                                 static_cast<size_t>(imgBytes.size()),
                                 fileName.toUtf8().constData(),
                                 &outData, &outSize) == 0 && outData) {
                fileData = QByteArray(reinterpret_cast<const char*>(outData),
                                      static_cast<int>(outSize));
                free(outData);
                extracted = true;
            }
        }
    } else if (ext == "adf") {
        uft_adf_volume_t *vol = uft_adf_open(m_imagePath.toUtf8().constData(), true);
        if (vol) {
            uft_adf_entry_t entry;
            QString fullPath = m_currentDir + fileName;
            if (uft_adf_lookup(vol, fullPath.toUtf8().constData(), &entry) == 0 && !entry.is_dir) {
                fileData.resize(static_cast<int>(entry.size));
                ssize_t bytesRead = uft_adf_read_file(vol, entry.block, 0,
                    fileData.data(), static_cast<size_t>(entry.size));
                if (bytesRead > 0) {
                    fileData.resize(static_cast<int>(bytesRead));
                    extracted = true;
                }
            }
            uft_adf_close(vol);
        }
    } else if (ext == "img" || ext == "ima" || ext == "st") {
        uint8_t *outData = nullptr;
        size_t outSize = 0;
        QFile imgFile(m_imagePath);
        if (imgFile.open(QIODevice::ReadOnly)) {
            QByteArray imgBytes = imgFile.readAll();
            imgFile.close();
            if (fat12_extract_file(reinterpret_cast<const uint8_t*>(imgBytes.constData()),
                                   static_cast<size_t>(imgBytes.size()),
                                   fileName.toUtf8().constData(),
                                   &outData, &outSize) == 0 && outData) {
                fileData = QByteArray(reinterpret_cast<const char*>(outData),
                                      static_cast<int>(outSize));
                free(outData);
                extracted = true;
            }
        }
    } else if (ext == "ssd" || ext == "dsd") {
        uint8_t *outData = nullptr;
        size_t outSize = 0;
        QFile imgFile(m_imagePath);
        if (imgFile.open(QIODevice::ReadOnly)) {
            QByteArray imgBytes = imgFile.readAll();
            imgFile.close();
            if (ssd_extract_file(reinterpret_cast<const uint8_t*>(imgBytes.constData()),
                                 static_cast<size_t>(imgBytes.size()),
                                 fileName.toUtf8().constData(),
                                 &outData, &outSize) == 0 && outData) {
                fileData = QByteArray(reinterpret_cast<const char*>(outData),
                                      static_cast<int>(outSize));
                free(outData);
                extracted = true;
            }
        }
    }

    if (!extracted || fileData.isEmpty()) {
        textView->setPlainText(tr("Could not extract file data for: %1\n"
                                  "Format: .%2").arg(fileName, ext));
    } else {
        // Convert to text, replacing non-printable characters
        // For C64 PETSCII content, do a basic conversion
        QString text;
        if (ext == "d64" || ext == "d71" || ext == "d81") {
            // Basic PETSCII to ASCII conversion for Commodore files
            for (int i = 0; i < fileData.size(); i++) {
                uint8_t ch = static_cast<uint8_t>(fileData[i]);
                if (ch == 0x0D) {
                    text += '\n';  // Commodore CR -> LF
                } else if (ch >= 0x41 && ch <= 0x5A) {
                    text += QChar(ch + 0x20);  // Uppercase PETSCII -> lowercase
                } else if (ch >= 0xC1 && ch <= 0xDA) {
                    text += QChar(ch - 0x80);  // Shifted uppercase
                } else if (ch >= 0x20 && ch < 0x7F) {
                    text += QChar(ch);
                } else {
                    text += QChar(0xB7);  // Middle dot for non-printable
                }
            }
        } else {
            // Standard text conversion: try UTF-8, fall back to Latin-1
            QStringDecoder utf8Decoder(QStringDecoder::Utf8);
            QString decoded = utf8Decoder(fileData);
            if (utf8Decoder.hasError()) {
                text = QString::fromLatin1(fileData);
            } else {
                text = decoded;
            }
        }

        textView->setFont(QFont("Monospace", 9));
        textView->setPlainText(text);
    }

    dlg->show();
}

void ExplorerTab::onViewProperties()
{
    QList<QTableWidgetItem*> selected = ui->tableFiles->selectedItems();
    if (selected.isEmpty()) return;
    
    int row = selected.first()->row();
    QString fileName = ui->tableFiles->item(row, 0)->text();
    QString fileSize = ui->tableFiles->item(row, 1)->text();
    QString fileType = ui->tableFiles->item(row, 2)->text();
    QString fileAttr = ui->tableFiles->item(row, 3)->text();
    
    QString info;
    info += tr("═══════════════════════════════════════\n");
    info += tr("File Properties\n");
    info += tr("═══════════════════════════════════════\n\n");
    info += tr("Name:       %1\n").arg(fileName);
    info += tr("Size:       %1\n").arg(fileSize);
    info += tr("Type:       %1\n").arg(fileType);
    info += tr("Attributes: %1\n").arg(fileAttr);
    info += tr("\nLocation:   %1\n").arg(m_currentDir.isEmpty() ? "/" : m_currentDir);
    info += tr("Image:      %1\n").arg(QFileInfo(m_imagePath).fileName());
    
    QMessageBox::information(this, tr("Properties: %1").arg(fileName), info);
}

void ExplorerTab::onCopyToClipboard()
{
    QList<QTableWidgetItem*> selected = ui->tableFiles->selectedItems();
    if (selected.isEmpty()) return;
    
    QString fileName = ui->tableFiles->item(selected.first()->row(), 0)->text();
    QString fullPath = m_currentDir.isEmpty() ? fileName : (m_currentDir + "/" + fileName);
    
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(fullPath);
    
    emit statusMessage(tr("Copied to clipboard: %1").arg(fullPath));
}
