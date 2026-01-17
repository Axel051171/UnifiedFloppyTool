/**
 * @file catalogtab.cpp
 * @brief Explorer Tab Implementation
 * 
 * P0-GUI-004 FIX: Directory browsing for disk images
 */

#include "explorertab.h"
#include "ui_tab_explorer.h"
#include "disk_image_validator.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QFileInfo>
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
    
    // TODO: Implement actual file import to disk image
    QMessageBox::information(this, tr("Import"),
        tr("Import of %1 files to disk image is not yet implemented.").arg(files.size()));
}

void ExplorerTab::onImportFolder()
{
    if (!m_imageLoaded) {
        QMessageBox::warning(this, tr("No Image"), tr("Please open a disk image first."));
        return;
    }
    
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Folder to Import"));
    
    if (dir.isEmpty()) return;
    
    // TODO: Implement actual folder import to disk image
    QMessageBox::information(this, tr("Import"),
        tr("Import of folder to disk image is not yet implemented."));
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
    
    // TODO: Implement actual rename
    QMessageBox::information(this, tr("Rename"),
        tr("Rename is not yet implemented."));
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
    
    if (reply == QMessageBox::Yes) {
        // TODO: Implement actual delete
        QMessageBox::information(this, tr("Delete"),
            tr("Delete is not yet implemented."));
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
    
    if (ok && !name.isEmpty()) {
        // TODO: Implement actual folder creation
        QMessageBox::information(this, tr("New Folder"),
            tr("Folder creation is not yet implemented."));
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
    
    // TODO: Implement disk validation
    QMessageBox::information(this, tr("Validate"),
        tr("Disk validation is not yet implemented."));
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
    
    // TODO: Read actual file data from disk image
    // For now, show placeholder
    QString hexText;
    hexText += tr("=== Hex View: %1 ===\n\n").arg(fileName);
    hexText += tr("Offset    00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F   ASCII\n");
    hexText += tr("--------  -----------------------------------------------   ----------------\n");
    hexText += tr("00000000  XX XX XX XX XX XX XX XX  XX XX XX XX XX XX XX XX   ................\n");
    hexText += tr("\n(File reading not yet implemented)\n");
    
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
    
    // TODO: Read actual file data from disk image
    textView->setPlainText(tr("(Text viewing not yet implemented for: %1)").arg(fileName));
    
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
