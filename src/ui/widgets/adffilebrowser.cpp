/**
 * @file adffilebrowser.cpp
 * @brief ADF File Browser Implementation
 */

#include "adffilebrowser.h"
#include "adflib_wrapper/uft_adflib_wrapper.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QFileDialog>
#include <QMessageBox>
#include <QComboBox>
#include <QApplication>
#include <QDateTime>
#include <QMimeData>
#include <QDrag>
#include <QDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QFileInfo>

AdfFileBrowser::AdfFileBrowser(QWidget *parent)
    : QWidget(parent)
    , m_toolbar(nullptr)
    , m_treeView(nullptr)
    , m_model(nullptr)
    , m_pathEdit(nullptr)
    , m_statusLabel(nullptr)
    , m_volumeCombo(nullptr)
    , m_currentVolume(0)
    , m_showDeletedFiles(false)
    , m_adfContext(nullptr)
{
    setupUi();
    setupToolbar();
    setupConnections();
}

AdfFileBrowser::~AdfFileBrowser()
{
    closeImage();
}

void AdfFileBrowser::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(2);
    
    // Toolbar
    m_toolbar = new QToolBar(this);
    m_toolbar->setIconSize(QSize(16, 16));
    mainLayout->addWidget(m_toolbar);
    
    // Path bar
    QHBoxLayout *pathLayout = new QHBoxLayout();
    pathLayout->setContentsMargins(4, 0, 4, 0);
    
    m_volumeCombo = new QComboBox(this);
    m_volumeCombo->setMinimumWidth(100);
    m_volumeCombo->setVisible(false);
    pathLayout->addWidget(m_volumeCombo);
    
    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setReadOnly(true);
    m_pathEdit->setPlaceholderText(tr("No image loaded"));
    pathLayout->addWidget(m_pathEdit);
    
    mainLayout->addLayout(pathLayout);
    
    // Tree view
    m_treeView = new QTreeView(this);
    m_treeView->setRootIsDecorated(false);
    m_treeView->setAlternatingRowColors(true);
    m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeView->setSortingEnabled(true);
    m_treeView->setDragEnabled(true);
    mainLayout->addWidget(m_treeView);
    
    // Model
    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels({
        tr("Name"), tr("Size"), tr("Type"), tr("Date"), tr("Comment")
    });
    m_treeView->setModel(m_model);
    
    // Status bar
    m_statusLabel = new QLabel(this);
    m_statusLabel->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    mainLayout->addWidget(m_statusLabel);
    
    // Column widths
    m_treeView->header()->setStretchLastSection(true);
    m_treeView->setColumnWidth(0, 200);  // Name
    m_treeView->setColumnWidth(1, 80);   // Size
    m_treeView->setColumnWidth(2, 60);   // Type
    m_treeView->setColumnWidth(3, 120);  // Date
}

void AdfFileBrowser::setupToolbar()
{
    // Navigation
    m_actGoUp = m_toolbar->addAction(
        QIcon::fromTheme("go-up"), tr("Up"), this, &AdfFileBrowser::goUp);
    m_actGoRoot = m_toolbar->addAction(
        QIcon::fromTheme("go-home"), tr("Root"), this, &AdfFileBrowser::goToRoot);
    m_actRefresh = m_toolbar->addAction(
        QIcon::fromTheme("view-refresh"), tr("Refresh"), this, &AdfFileBrowser::refresh);
    
    m_toolbar->addSeparator();
    
    // File operations
    m_actExtract = m_toolbar->addAction(
        QIcon::fromTheme("document-save"), tr("Extract"), this, &AdfFileBrowser::extractSelected);
    m_actExtractAll = m_toolbar->addAction(
        QIcon::fromTheme("document-save-all"), tr("Extract All"), this, &AdfFileBrowser::extractAll);
    
    m_toolbar->addSeparator();
    
    m_actAdd = m_toolbar->addAction(
        QIcon::fromTheme("list-add"), tr("Add"), this, &AdfFileBrowser::addFiles);
    m_actDelete = m_toolbar->addAction(
        QIcon::fromTheme("list-remove"), tr("Delete"), this, &AdfFileBrowser::deleteSelected);
    
    m_toolbar->addSeparator();
    
    // Recovery
    m_actShowDeleted = m_toolbar->addAction(
        QIcon::fromTheme("edit-undo"), tr("Show Deleted"), this, &AdfFileBrowser::showDeleted);
    m_actShowDeleted->setCheckable(true);
    m_actRecover = m_toolbar->addAction(
        QIcon::fromTheme("edit-redo"), tr("Recover"), this, &AdfFileBrowser::recoverSelected);
    
    m_toolbar->addSeparator();
    
    m_actProperties = m_toolbar->addAction(
        QIcon::fromTheme("document-properties"), tr("Properties"), this, &AdfFileBrowser::showProperties);
    
    // Initial state
    setEnabled(false);
}

void AdfFileBrowser::setupConnections()
{
    connect(m_treeView, &QTreeView::doubleClicked,
            this, &AdfFileBrowser::onItemDoubleClicked);
    connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &AdfFileBrowser::onSelectionChanged);
    connect(m_treeView, &QTreeView::customContextMenuRequested,
            this, &AdfFileBrowser::onContextMenu);
    connect(m_volumeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AdfFileBrowser::setCurrentVolume);
}

bool AdfFileBrowser::openImage(const QString &path)
{
    closeImage();
    
    // Check if ADFlib is available
    if (!uft_adf_is_available()) {
        emit errorOccurred(tr("ADFlib support not available"));
        return false;
    }
    
    // Open the image
    m_adfContext = uft_adf_open(path.toUtf8().constData(), true);
    if (!m_adfContext) {
        emit errorOccurred(QString::fromUtf8(uft_adf_last_error()));
        return false;
    }
    
    m_imagePath = path;
    
    // Get volume info
    uft_adf_device_info_t devInfo;
    uft_adf_get_device_info((uft_adf_context_t*)m_adfContext, &devInfo);
    
    // Populate volume combo
    m_volumeCombo->clear();
    for (int i = 0; i < devInfo.num_volumes; i++) {
        uft_adf_volume_info_t volInfo;
        uft_adf_get_volume_info((uft_adf_context_t*)m_adfContext, i, &volInfo);
        m_volumeCombo->addItem(QString::fromUtf8(volInfo.name));
    }
    
    m_volumeCombo->setVisible(devInfo.num_volumes > 1);
    
    // Mount first volume
    if (uft_adf_mount_volume((uft_adf_context_t*)m_adfContext, 0) != 0) {
        emit errorOccurred(QString::fromUtf8(uft_adf_last_error()));
        closeImage();
        return false;
    }
    
    m_currentVolume = 0;
    m_currentPath = "/";
    
    setEnabled(true);
    populateDirectory();
    
    emit imageOpened(path);
    return true;
}

void AdfFileBrowser::closeImage()
{
    if (m_adfContext) {
        uft_adf_close((uft_adf_context_t*)m_adfContext);
        m_adfContext = nullptr;
    }
    
    m_model->removeRows(0, m_model->rowCount());
    m_imagePath.clear();
    m_currentPath.clear();
    m_pathEdit->clear();
    m_statusLabel->clear();
    m_volumeCombo->clear();
    m_volumeCombo->setVisible(false);
    
    setEnabled(false);
    emit imageClosed();
}

bool AdfFileBrowser::isImageOpen() const
{
    return m_adfContext != nullptr;
}

QString AdfFileBrowser::currentPath() const
{
    return m_currentPath;
}

void AdfFileBrowser::populateDirectory()
{
    if (!m_adfContext) return;
    
    m_model->removeRows(0, m_model->rowCount());
    
    // Get entries
    uft_adf_entry_t entries[256];
    int count = uft_adf_list_dir((uft_adf_context_t*)m_adfContext, entries, 256);
    
    for (int i = 0; i < count; i++) {
        const uft_adf_entry_t &e = entries[i];
        
        // Skip deleted unless showing
        if (e.is_deleted && !m_showDeletedFiles) continue;
        
        QList<QStandardItem*> row;
        
        // Name
        QStandardItem *nameItem = new QStandardItem(QString::fromUtf8(e.name));
        if (e.type == UFT_ADF_ENTRY_DIR) {
            nameItem->setIcon(QIcon::fromTheme("folder"));
        } else {
            nameItem->setIcon(QIcon::fromTheme("text-x-generic"));
        }
        if (e.is_deleted) {
            nameItem->setForeground(Qt::gray);
            nameItem->setToolTip(tr("Deleted"));
        }
        row.append(nameItem);
        
        // Size
        QStandardItem *sizeItem = new QStandardItem();
        if (e.type == UFT_ADF_ENTRY_FILE) {
            sizeItem->setText(QString::number(e.size));
            sizeItem->setData((qulonglong)e.size, Qt::UserRole);
        }
        row.append(sizeItem);
        
        // Type
        QString typeStr;
        switch (e.type) {
            case UFT_ADF_ENTRY_FILE: typeStr = tr("File"); break;
            case UFT_ADF_ENTRY_DIR: typeStr = tr("Dir"); break;
            case UFT_ADF_ENTRY_SOFTLINK: typeStr = tr("Link"); break;
            case UFT_ADF_ENTRY_HARDLINK: typeStr = tr("HLink"); break;
        }
        row.append(new QStandardItem(typeStr));
        
        // Date
        QDateTime dt(QDate(e.year, e.month, e.day), 
                     QTime(e.hour, e.minute, e.second));
        row.append(new QStandardItem(dt.toString("yyyy-MM-dd hh:mm")));
        
        // Comment
        row.append(new QStandardItem(QString::fromUtf8(e.comment)));
        
        // Store entry type and deleted flag
        nameItem->setData(e.type, Qt::UserRole);
        nameItem->setData(e.is_deleted, Qt::UserRole + 1);
        
        m_model->appendRow(row);
    }
    
    m_pathEdit->setText(m_currentPath);
    updateStatusBar();
}

void AdfFileBrowser::updateStatusBar()
{
    int count = m_model->rowCount();
    qint64 totalSize = 0;
    
    for (int i = 0; i < count; i++) {
        QStandardItem *sizeItem = m_model->item(i, 1);
        if (sizeItem) {
            totalSize += sizeItem->data(Qt::UserRole).toLongLong();
        }
    }
    
    m_statusLabel->setText(tr("%1 items, %2 bytes")
                          .arg(count)
                          .arg(totalSize));
}

void AdfFileBrowser::goUp()
{
    if (!m_adfContext || m_currentPath == "/") return;
    
    int lastSlash = m_currentPath.lastIndexOf('/');
    if (lastSlash > 0) {
        goToPath(m_currentPath.left(lastSlash));
    } else {
        goToRoot();
    }
}

void AdfFileBrowser::goToRoot()
{
    if (!m_adfContext) return;
    
    uft_adf_to_root((uft_adf_context_t*)m_adfContext);
    m_currentPath = "/";
    populateDirectory();
    emit directoryChanged(m_currentPath);
}

void AdfFileBrowser::goToPath(const QString &path)
{
    if (!m_adfContext) return;
    
    uft_adf_to_root((uft_adf_context_t*)m_adfContext);
    
    if (path != "/" && !path.isEmpty()) {
        QStringList parts = path.split('/', Qt::SkipEmptyParts);
        for (const QString &part : parts) {
            if (uft_adf_change_dir((uft_adf_context_t*)m_adfContext, 
                                  part.toUtf8().constData()) != 0) {
                emit errorOccurred(QString::fromUtf8(uft_adf_last_error()));
                return;
            }
        }
    }
    
    m_currentPath = path.isEmpty() ? "/" : path;
    populateDirectory();
    emit directoryChanged(m_currentPath);
}

void AdfFileBrowser::refresh()
{
    populateDirectory();
}

void AdfFileBrowser::onItemDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;
    
    QStandardItem *item = m_model->item(index.row(), 0);
    if (!item) return;
    
    int type = item->data(Qt::UserRole).toInt();
    if (type == UFT_ADF_ENTRY_DIR) {
        QString name = item->text();
        QString newPath = m_currentPath;
        if (!newPath.endsWith('/')) newPath += '/';
        newPath += name;
        goToPath(newPath);
    }
}

void AdfFileBrowser::onSelectionChanged()
{
    QModelIndexList selection = m_treeView->selectionModel()->selectedRows();
    
    bool hasSelection = !selection.isEmpty();
    bool hasFileSelected = false;
    bool hasDeletedSelected = false;
    
    for (const QModelIndex &idx : selection) {
        QStandardItem *item = m_model->item(idx.row(), 0);
        if (item) {
            int type = item->data(Qt::UserRole).toInt();
            if (type == UFT_ADF_ENTRY_FILE) hasFileSelected = true;
            if (item->data(Qt::UserRole + 1).toBool()) hasDeletedSelected = true;
        }
    }
    
    m_actExtract->setEnabled(hasSelection);
    m_actDelete->setEnabled(hasSelection && !hasDeletedSelected);
    m_actRecover->setEnabled(hasDeletedSelected);
    
    if (selection.count() == 1) {
        QStandardItem *item = m_model->item(selection.first().row(), 0);
        QStandardItem *sizeItem = m_model->item(selection.first().row(), 1);
        if (item && sizeItem) {
            emit fileSelected(item->text(), sizeItem->data(Qt::UserRole).toLongLong());
        }
    }
}

void AdfFileBrowser::onContextMenu(const QPoint &pos)
{
    QMenu menu(this);
    
    menu.addAction(m_actExtract);
    menu.addAction(m_actExtractAll);
    menu.addSeparator();
    menu.addAction(m_actAdd);
    menu.addAction(m_actDelete);
    menu.addSeparator();
    menu.addAction(m_actRecover);
    menu.addSeparator();
    menu.addAction(m_actProperties);
    
    menu.exec(m_treeView->mapToGlobal(pos));
}

void AdfFileBrowser::extractSelected()
{
    if (!m_adfContext) return;
    
    QModelIndexList selection = m_treeView->selectionModel()->selectedRows();
    if (selection.isEmpty()) return;
    
    QString destDir = QFileDialog::getExistingDirectory(
        this, tr("Select Destination Directory"));
    if (destDir.isEmpty()) return;
    
    int success = 0, failed = 0;
    emit extractionStarted(selection.count());
    
    for (int i = 0; i < selection.count(); i++) {
        QStandardItem *item = m_model->item(selection[i].row(), 0);
        if (!item) continue;
        
        QString name = item->text();
        QString destPath = destDir + "/" + name;
        
        emit extractionProgress(i + 1, selection.count(), name);
        QApplication::processEvents();
        
        if (uft_adf_extract_file((uft_adf_context_t*)m_adfContext,
                                 name.toUtf8().constData(),
                                 destPath.toUtf8().constData()) == 0) {
            success++;
        } else {
            failed++;
        }
    }
    
    emit extractionFinished(success, failed);
    
    QMessageBox::information(this, tr("Extraction Complete"),
        tr("Extracted %1 files successfully.\n%2 files failed.")
        .arg(success).arg(failed));
}

void AdfFileBrowser::extractAll()
{
    if (!m_adfContext) return;
    
    QString destDir = QFileDialog::getExistingDirectory(
        this, tr("Select Destination Directory"));
    if (destDir.isEmpty()) return;
    
    int result = uft_adf_extract_all((uft_adf_context_t*)m_adfContext,
                                     destDir.toUtf8().constData(), true);
    
    if (result >= 0) {
        QMessageBox::information(this, tr("Extraction Complete"),
            tr("Extracted %1 files.").arg(result));
    } else {
        emit errorOccurred(QString::fromUtf8(uft_adf_last_error()));
    }
}

void AdfFileBrowser::addFiles()
{
    if (!m_adfContext) return;

    QStringList paths = QFileDialog::getOpenFileNames(
        this, tr("Select Files to Add"));
    if (paths.isEmpty()) return;

    /* Confirm the write operation */
    if (QMessageBox::question(this, tr("Add Files"),
            tr("Add %1 file(s) to the disk image?\n\n"
               "This will modify the disk image on disk.")
            .arg(paths.size()),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    /* Re-open in read-write mode if needed */
    QString imagePath = m_imagePath;
    closeImage();

    void *rwCtx = uft_adf_open(imagePath.toUtf8().constData(), false);
    if (!rwCtx) {
        QMessageBox::warning(this, tr("Add Files"),
            tr("Read-only: filesystem write support not available.\n%1")
            .arg(QString::fromUtf8(uft_adf_last_error())));
        /* Re-open read-only */
        openImage(imagePath);
        return;
    }

    if (uft_adf_mount_volume((uft_adf_context_t*)rwCtx, m_currentVolume) != 0) {
        QMessageBox::warning(this, tr("Add Files"),
            tr("Failed to mount volume: %1")
            .arg(QString::fromUtf8(uft_adf_last_error())));
        uft_adf_close((uft_adf_context_t*)rwCtx);
        openImage(imagePath);
        return;
    }

    /* Navigate to current directory */
    if (m_currentPath != "/" && !m_currentPath.isEmpty()) {
        uft_adf_to_root((uft_adf_context_t*)rwCtx);
        QStringList parts = m_currentPath.split('/', Qt::SkipEmptyParts);
        for (const QString &part : parts) {
            uft_adf_change_dir((uft_adf_context_t*)rwCtx, part.toUtf8().constData());
        }
    }

    int success = 0, failed = 0;
    for (const QString &path : paths) {
        QFileInfo fi(path);
        QString adfName = fi.fileName();

        if (uft_adf_add_file((uft_adf_context_t*)rwCtx,
                              path.toUtf8().constData(),
                              adfName.toUtf8().constData()) == 0) {
            success++;
        } else {
            failed++;
        }
    }

    uft_adf_close((uft_adf_context_t*)rwCtx);

    /* Re-open read-only and refresh */
    openImage(imagePath);

    QMessageBox::information(this, tr("Add Files"),
        tr("Added %1 file(s) successfully.\n%2 file(s) failed.")
        .arg(success).arg(failed));
}

void AdfFileBrowser::deleteSelected()
{
    if (!m_adfContext) return;

    QModelIndexList selection = m_treeView->selectionModel()->selectedRows();
    if (selection.isEmpty()) return;

    /* Build list of names to delete */
    QStringList names;
    for (const QModelIndex &idx : selection) {
        QStandardItem *item = m_model->item(idx.row(), 0);
        if (item) names.append(item->text());
    }

    /* Confirm deletion */
    if (QMessageBox::question(this, tr("Delete Files"),
            tr("Delete %1 file(s) from the disk image?\n\n%2\n\n"
               "This will modify the disk image on disk.")
            .arg(names.size()).arg(names.join(", ")),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    /* Re-open in read-write mode */
    QString imagePath = m_imagePath;
    QString savedPath = m_currentPath;
    int savedVolume = m_currentVolume;
    closeImage();

    void *rwCtx = uft_adf_open(imagePath.toUtf8().constData(), false);
    if (!rwCtx) {
        QMessageBox::warning(this, tr("Delete"),
            tr("Read-only: filesystem write support not available.\n%1")
            .arg(QString::fromUtf8(uft_adf_last_error())));
        openImage(imagePath);
        return;
    }

    if (uft_adf_mount_volume((uft_adf_context_t*)rwCtx, savedVolume) != 0) {
        uft_adf_close((uft_adf_context_t*)rwCtx);
        openImage(imagePath);
        return;
    }

    /* Navigate to the directory containing the files */
    if (savedPath != "/" && !savedPath.isEmpty()) {
        uft_adf_to_root((uft_adf_context_t*)rwCtx);
        QStringList parts = savedPath.split('/', Qt::SkipEmptyParts);
        for (const QString &part : parts) {
            uft_adf_change_dir((uft_adf_context_t*)rwCtx, part.toUtf8().constData());
        }
    }

    int success = 0, failed = 0;
    for (const QString &name : names) {
        if (uft_adf_delete_file((uft_adf_context_t*)rwCtx,
                                 name.toUtf8().constData()) == 0) {
            success++;
        } else {
            failed++;
        }
    }

    uft_adf_close((uft_adf_context_t*)rwCtx);

    /* Re-open and navigate back */
    openImage(imagePath);
    if (savedPath != "/") goToPath(savedPath);

    QMessageBox::information(this, tr("Delete"),
        tr("Deleted %1 file(s) successfully.\n%2 file(s) failed.")
        .arg(success).arg(failed));
}

void AdfFileBrowser::showDeleted()
{
    m_showDeletedFiles = m_actShowDeleted->isChecked();
    populateDirectory();
}

void AdfFileBrowser::recoverSelected()
{
    if (!m_adfContext) return;

    QModelIndexList selection = m_treeView->selectionModel()->selectedRows();
    if (selection.isEmpty()) return;

    /* Collect deleted file names */
    QStringList names;
    for (const QModelIndex &idx : selection) {
        QStandardItem *item = m_model->item(idx.row(), 0);
        if (item && item->data(Qt::UserRole + 1).toBool()) {
            names.append(item->text());
        }
    }

    if (names.isEmpty()) {
        QMessageBox::information(this, tr("Recover"),
            tr("No deleted files selected."));
        return;
    }

    /* Confirm recovery */
    if (QMessageBox::question(this, tr("Recover Files"),
            tr("Attempt to recover %1 deleted file(s)?\n\n%2\n\n"
               "This will modify the disk image on disk.")
            .arg(names.size()).arg(names.join(", ")),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    /* Re-open in read-write mode */
    QString imagePath = m_imagePath;
    QString savedPath = m_currentPath;
    int savedVolume = m_currentVolume;
    closeImage();

    void *rwCtx = uft_adf_open(imagePath.toUtf8().constData(), false);
    if (!rwCtx) {
        QMessageBox::warning(this, tr("Recover"),
            tr("Read-only: filesystem write support not available.\n%1")
            .arg(QString::fromUtf8(uft_adf_last_error())));
        openImage(imagePath);
        return;
    }

    if (uft_adf_mount_volume((uft_adf_context_t*)rwCtx, savedVolume) != 0) {
        uft_adf_close((uft_adf_context_t*)rwCtx);
        openImage(imagePath);
        return;
    }

    /* Navigate to the directory */
    if (savedPath != "/" && !savedPath.isEmpty()) {
        uft_adf_to_root((uft_adf_context_t*)rwCtx);
        QStringList parts = savedPath.split('/', Qt::SkipEmptyParts);
        for (const QString &part : parts) {
            uft_adf_change_dir((uft_adf_context_t*)rwCtx, part.toUtf8().constData());
        }
    }

    int success = 0, failed = 0;
    for (const QString &name : names) {
        if (uft_adf_recover_file((uft_adf_context_t*)rwCtx,
                                  name.toUtf8().constData()) == 0) {
            success++;
        } else {
            failed++;
        }
    }

    uft_adf_close((uft_adf_context_t*)rwCtx);

    /* Re-open and navigate back */
    openImage(imagePath);
    if (savedPath != "/") goToPath(savedPath);

    QMessageBox::information(this, tr("Recover"),
        tr("Recovered %1 file(s) successfully.\n%2 file(s) failed.")
        .arg(success).arg(failed));
}

void AdfFileBrowser::showProperties()
{
    if (!m_adfContext) return;

    QModelIndexList selection = m_treeView->selectionModel()->selectedRows();
    if (selection.isEmpty()) return;

    int row = selection.first().row();
    QStandardItem *nameItem = m_model->item(row, 0);
    QStandardItem *sizeItem = m_model->item(row, 1);
    QStandardItem *typeItem = m_model->item(row, 2);
    QStandardItem *dateItem = m_model->item(row, 3);
    QStandardItem *commentItem = m_model->item(row, 4);
    if (!nameItem) return;

    QString name = nameItem->text();
    QString size = sizeItem ? sizeItem->text() : tr("N/A");
    QString type = typeItem ? typeItem->text() : tr("Unknown");
    QString date = dateItem ? dateItem->text() : tr("N/A");
    QString comment = commentItem ? commentItem->text() : QString();
    bool isDeleted = nameItem->data(Qt::UserRole + 1).toBool();

    /* Build properties dialog */
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Properties - %1").arg(name));
    dlg.setMinimumWidth(320);

    QVBoxLayout *layout = new QVBoxLayout(&dlg);

    QGroupBox *infoGroup = new QGroupBox(tr("File Information"), &dlg);
    QFormLayout *formLayout = new QFormLayout(infoGroup);

    formLayout->addRow(tr("Name:"), new QLabel(name));
    formLayout->addRow(tr("Type:"), new QLabel(type));
    formLayout->addRow(tr("Size:"), new QLabel(size.isEmpty() ? tr("N/A") :
        tr("%1 bytes").arg(size)));
    formLayout->addRow(tr("Date:"), new QLabel(date));

    if (!comment.isEmpty()) {
        formLayout->addRow(tr("Comment:"), new QLabel(comment));
    }

    if (isDeleted) {
        QLabel *deletedLabel = new QLabel(tr("Yes"));
        deletedLabel->setStyleSheet("color: red; font-weight: bold;");
        formLayout->addRow(tr("Deleted:"), deletedLabel);
    }

    /* Volume info */
    uft_adf_volume_info_t volInfo;
    if (uft_adf_get_volume_info((uft_adf_context_t*)m_adfContext,
                                 m_currentVolume, &volInfo) == 0) {
        formLayout->addRow(tr("Volume:"), new QLabel(QString::fromUtf8(volInfo.name)));
        formLayout->addRow(tr("Filesystem:"),
            new QLabel(QString::fromUtf8(uft_adf_fs_type_name(volInfo.fs_type))));
        formLayout->addRow(tr("Free Blocks:"),
            new QLabel(tr("%1 / %2").arg(volInfo.free_blocks).arg(volInfo.num_blocks)));
    }

    formLayout->addRow(tr("Path:"), new QLabel(m_currentPath));
    formLayout->addRow(tr("Image:"), new QLabel(m_imagePath));

    layout->addWidget(infoGroup);

    QPushButton *closeBtn = new QPushButton(tr("Close"), &dlg);
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    layout->addWidget(closeBtn);

    dlg.exec();
}

void AdfFileBrowser::setCurrentVolume(int index)
{
    if (!m_adfContext || index < 0) return;
    
    uft_adf_unmount_volume((uft_adf_context_t*)m_adfContext);
    
    if (uft_adf_mount_volume((uft_adf_context_t*)m_adfContext, index) != 0) {
        emit errorOccurred(QString::fromUtf8(uft_adf_last_error()));
        return;
    }
    
    m_currentVolume = index;
    m_currentPath = "/";
    populateDirectory();
}

int AdfFileBrowser::volumeCount() const
{
    return m_volumeCombo->count();
}

int AdfFileBrowser::currentVolume() const
{
    return m_currentVolume;
}

QStringList AdfFileBrowser::volumeNames() const
{
    QStringList names;
    for (int i = 0; i < m_volumeCombo->count(); i++) {
        names.append(m_volumeCombo->itemText(i));
    }
    return names;
}
