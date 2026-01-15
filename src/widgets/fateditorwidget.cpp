/**
 * @file fateditorwidget.cpp
 * @brief Qt Widget for FAT Filesystem Editing - Implementation
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#include "fateditorwidget.h"
#include <QHeaderView>
#include <QFileDialog>
#include <QScrollBar>
#include <QApplication>

extern "C" {
#include "uft/uft_fat_editor.h"
}

namespace UFT {

/*===========================================================================
 * CONSTRUCTOR / DESTRUCTOR
 *===========================================================================*/

FatEditorWidget::FatEditorWidget(QWidget *parent)
    : QWidget(parent)
    , m_fat(nullptr)
    , m_modified(false)
    , m_selectedCluster(0)
{
    /* Initialize colors */
    m_colors.free = QColor(0, 200, 0);       /* Green */
    m_colors.used = QColor(100, 100, 255);   /* Blue */
    m_colors.bad = QColor(255, 0, 0);        /* Red */
    m_colors.reserved = QColor(128, 128, 128); /* Gray */
    m_colors.chain = QColor(255, 255, 0);    /* Yellow */
    m_colors.selected = QColor(255, 165, 0); /* Orange */
    
    setupUi();
    connectSignals();
}

FatEditorWidget::~FatEditorWidget()
{
    if (m_fat) {
        uft_fat_close(m_fat);
        m_fat = nullptr;
    }
}

/*===========================================================================
 * UI SETUP
 *===========================================================================*/

void FatEditorWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);
    
    /* Top: Statistics Panel */
    QGroupBox *statsGroup = new QGroupBox(tr("Filesystem Statistics"));
    QGridLayout *statsLayout = new QGridLayout(statsGroup);
    
    statsLayout->addWidget(new QLabel(tr("Type:")), 0, 0);
    m_fatTypeLabel = new QLabel("-");
    m_fatTypeLabel->setStyleSheet("font-weight: bold;");
    statsLayout->addWidget(m_fatTypeLabel, 0, 1);
    
    statsLayout->addWidget(new QLabel(tr("Total Clusters:")), 0, 2);
    m_totalClustersLabel = new QLabel("-");
    statsLayout->addWidget(m_totalClustersLabel, 0, 3);
    
    statsLayout->addWidget(new QLabel(tr("Free:")), 1, 0);
    m_freeClustersLabel = new QLabel("-");
    m_freeClustersLabel->setStyleSheet("color: green;");
    statsLayout->addWidget(m_freeClustersLabel, 1, 1);
    
    statsLayout->addWidget(new QLabel(tr("Used:")), 1, 2);
    m_usedClustersLabel = new QLabel("-");
    m_usedClustersLabel->setStyleSheet("color: blue;");
    statsLayout->addWidget(m_usedClustersLabel, 1, 3);
    
    statsLayout->addWidget(new QLabel(tr("Bad:")), 1, 4);
    m_badClustersLabel = new QLabel("-");
    m_badClustersLabel->setStyleSheet("color: red;");
    statsLayout->addWidget(m_badClustersLabel, 1, 5);
    
    m_usageBar = new QProgressBar();
    m_usageBar->setTextVisible(true);
    m_usageBar->setFormat(tr("%p% used"));
    statsLayout->addWidget(m_usageBar, 2, 0, 1, 6);
    
    mainLayout->addWidget(statsGroup);
    
    /* Main Tab Widget */
    m_tabWidget = new QTabWidget();
    
    setupDirectoryView();
    setupClusterMap();
    setupBootSectorEditor();
    setupHexView();
    
    mainLayout->addWidget(m_tabWidget, 1);
}

void FatEditorWidget::setupDirectoryView()
{
    QWidget *dirTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(dirTab);
    
    m_directoryTree = new QTreeWidget();
    m_directoryTree->setHeaderLabels({tr("Name"), tr("Size"), tr("Date"), tr("Attr"), tr("Cluster")});
    m_directoryTree->setColumnWidth(0, 250);
    m_directoryTree->setAlternatingRowColors(true);
    layout->addWidget(m_directoryTree, 1);
    
    QHBoxLayout *infoLayout = new QHBoxLayout();
    m_fileInfoLabel = new QLabel();
    infoLayout->addWidget(m_fileInfoLabel, 1);
    
    m_extractButton = new QPushButton(tr("Extract"));
    m_extractButton->setEnabled(false);
    infoLayout->addWidget(m_extractButton);
    
    m_deleteButton = new QPushButton(tr("Delete"));
    m_deleteButton->setEnabled(false);
    infoLayout->addWidget(m_deleteButton);
    
    layout->addLayout(infoLayout);
    
    m_tabWidget->addTab(dirTab, tr("Directory"));
}

void FatEditorWidget::setupClusterMap()
{
    QWidget *clusterTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(clusterTab);
    
    QHBoxLayout *controlLayout = new QHBoxLayout();
    controlLayout->addWidget(new QLabel(tr("Cluster:")));
    m_clusterSpinBox = new QSpinBox();
    m_clusterSpinBox->setRange(2, 100000);
    controlLayout->addWidget(m_clusterSpinBox);
    
    m_gotoClusterButton = new QPushButton(tr("Go"));
    controlLayout->addWidget(m_gotoClusterButton);
    
    controlLayout->addWidget(new QLabel(tr("Value:")));
    m_clusterValueEdit = new QLineEdit();
    m_clusterValueEdit->setMaximumWidth(100);
    controlLayout->addWidget(m_clusterValueEdit);
    controlLayout->addStretch();
    layout->addLayout(controlLayout);
    
    m_clusterMap = new QTableWidget();
    m_clusterMap->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_clusterMap, 1);
    
    m_chainLabel = new QLabel(tr("Chain: -"));
    layout->addWidget(m_chainLabel);
    
    m_clusterInfoLabel = new QLabel();
    layout->addWidget(m_clusterInfoLabel);
    
    m_tabWidget->addTab(clusterTab, tr("Cluster Map"));
}

void FatEditorWidget::setupBootSectorEditor()
{
    QWidget *bootTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(bootTab);
    
    QGroupBox *bpbGroup = new QGroupBox(tr("BIOS Parameter Block"));
    QGridLayout *bpbLayout = new QGridLayout(bpbGroup);
    
    int row = 0;
    bpbLayout->addWidget(new QLabel(tr("OEM Name:")), row, 0);
    m_oemNameEdit = new QLineEdit();
    m_oemNameEdit->setMaxLength(8);
    bpbLayout->addWidget(m_oemNameEdit, row++, 1);
    
    bpbLayout->addWidget(new QLabel(tr("Volume Label:")), row, 0);
    m_volumeLabelEdit = new QLineEdit();
    m_volumeLabelEdit->setMaxLength(11);
    bpbLayout->addWidget(m_volumeLabelEdit, row++, 1);
    
    bpbLayout->addWidget(new QLabel(tr("Bytes/Sector:")), row, 0);
    m_bytesPerSectorSpin = new QSpinBox();
    m_bytesPerSectorSpin->setRange(128, 4096);
    m_bytesPerSectorSpin->setEnabled(false);
    bpbLayout->addWidget(m_bytesPerSectorSpin, row++, 1);
    
    bpbLayout->addWidget(new QLabel(tr("Sectors/Cluster:")), row, 0);
    m_sectorsPerClusterSpin = new QSpinBox();
    m_sectorsPerClusterSpin->setEnabled(false);
    bpbLayout->addWidget(m_sectorsPerClusterSpin, row++, 1);
    
    bpbLayout->addWidget(new QLabel(tr("Reserved Sectors:")), row, 0);
    m_reservedSectorsSpin = new QSpinBox();
    m_reservedSectorsSpin->setEnabled(false);
    bpbLayout->addWidget(m_reservedSectorsSpin, row++, 1);
    
    bpbLayout->addWidget(new QLabel(tr("Number of FATs:")), row, 0);
    m_numFatsSpin = new QSpinBox();
    m_numFatsSpin->setEnabled(false);
    bpbLayout->addWidget(m_numFatsSpin, row++, 1);
    
    bpbLayout->addWidget(new QLabel(tr("Root Entries:")), row, 0);
    m_rootEntriesSpin = new QSpinBox();
    m_rootEntriesSpin->setEnabled(false);
    bpbLayout->addWidget(m_rootEntriesSpin, row++, 1);
    
    bpbLayout->addWidget(new QLabel(tr("Total Sectors:")), row, 0);
    m_totalSectorsLabel = new QLabel("-");
    bpbLayout->addWidget(m_totalSectorsLabel, row++, 1);
    
    bpbLayout->addWidget(new QLabel(tr("FAT Size:")), row, 0);
    m_fatSizeLabel = new QLabel("-");
    bpbLayout->addWidget(m_fatSizeLabel, row++, 1);
    
    layout->addWidget(bpbGroup);
    
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_fixBootButton = new QPushButton(tr("Fix Boot Sector"));
    buttonLayout->addWidget(m_fixBootButton);
    buttonLayout->addStretch();
    layout->addLayout(buttonLayout);
    layout->addStretch();
    
    m_tabWidget->addTab(bootTab, tr("Boot Sector"));
}

void FatEditorWidget::setupHexView()
{
    QWidget *hexTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(hexTab);
    
    QHBoxLayout *controlLayout = new QHBoxLayout();
    controlLayout->addWidget(new QLabel(tr("Cluster:")));
    m_hexSectorSpin = new QSpinBox();
    m_hexSectorSpin->setRange(0, 100000);
    controlLayout->addWidget(m_hexSectorSpin);
    
    m_readSectorButton = new QPushButton(tr("Read"));
    controlLayout->addWidget(m_readSectorButton);
    
    m_writeSectorButton = new QPushButton(tr("Write"));
    m_writeSectorButton->setEnabled(false);
    controlLayout->addWidget(m_writeSectorButton);
    controlLayout->addStretch();
    layout->addLayout(controlLayout);
    
    m_hexEdit = new QPlainTextEdit();
    m_hexEdit->setFont(QFont("Courier New", 10));
    m_hexEdit->setReadOnly(true);
    layout->addWidget(m_hexEdit, 1);
    
    m_tabWidget->addTab(hexTab, tr("Hex View"));
}

void FatEditorWidget::connectSignals()
{
    connect(m_directoryTree, &QTreeWidget::itemClicked,
            this, &FatEditorWidget::onDirectoryItemClicked);
    connect(m_directoryTree, &QTreeWidget::itemDoubleClicked,
            this, &FatEditorWidget::onDirectoryItemDoubleClicked);
    
    connect(m_gotoClusterButton, &QPushButton::clicked, [this]() {
        selectCluster(m_clusterSpinBox->value());
    });
    
    connect(m_readSectorButton, &QPushButton::clicked, [this]() {
        updateHexView(m_hexSectorSpin->value());
    });
}

/*===========================================================================
 * LOADING
 *===========================================================================*/

bool FatEditorWidget::loadImage(const QString &path)
{
    if (m_fat) {
        uft_fat_close(m_fat);
        m_fat = nullptr;
    }
    
    m_fat = uft_fat_open_file(path.toUtf8().constData());
    if (!m_fat) {
        emit statusMessage(tr("Failed to open: %1").arg(path));
        return false;
    }
    
    m_currentPath = path;
    m_modified = false;
    refresh();
    emit statusMessage(tr("Loaded: %1").arg(path));
    return true;
}

bool FatEditorWidget::loadFromBuffer(const uint8_t *data, size_t size)
{
    if (m_fat) {
        uft_fat_close(m_fat);
        m_fat = nullptr;
    }
    
    m_fat = uft_fat_open(data, size);
    if (!m_fat) return false;
    
    m_currentPath.clear();
    m_modified = false;
    refresh();
    return true;
}

bool FatEditorWidget::saveImage(const QString &) { return false; }
bool FatEditorWidget::isLoaded() const { return m_fat != nullptr; }
bool FatEditorWidget::hasChanges() const { return m_modified; }

/*===========================================================================
 * REFRESH / UPDATE
 *===========================================================================*/

void FatEditorWidget::refresh()
{
    if (!m_fat) return;
    updateStatistics();
    updateDirectoryView();
    updateClusterMap();
    updateBootSectorView();
}

void FatEditorWidget::updateStatistics()
{
    if (!m_fat) return;
    
    uft_fat_stats_t stats;
    if (uft_fat_get_stats(m_fat, &stats) != 0) return;
    
    m_fatTypeLabel->setText(QString::fromUtf8(uft_fat_type_name(stats.type)));
    m_totalClustersLabel->setText(QString::number(stats.total_clusters));
    m_freeClustersLabel->setText(QString("%1 (%2)")
        .arg(stats.free_clusters).arg(formatSize(stats.free_size)));
    m_usedClustersLabel->setText(QString::number(stats.used_clusters));
    m_badClustersLabel->setText(QString::number(stats.bad_clusters));
    
    int usage = stats.total_clusters > 0 ? 
        (stats.used_clusters * 100) / stats.total_clusters : 0;
    m_usageBar->setValue(usage);
    
    m_clusterSpinBox->setMaximum(stats.total_clusters + 1);
}

void FatEditorWidget::updateDirectoryView()
{
    if (!m_fat) return;
    m_directoryTree->clear();
    
    uft_fat_file_info_t entries[256];
    int count = uft_fat_read_root(m_fat, entries, 256);
    
    for (int i = 0; i < count; i++) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_directoryTree);
        item->setText(0, QString::fromLatin1(entries[i].short_name));
        item->setText(1, (entries[i].attributes & 0x10) ? 
            tr("<DIR>") : formatSize(entries[i].file_size));
        item->setText(2, formatDateTime(entries[i].modify_date, entries[i].modify_time));
        item->setText(4, QString::number(entries[i].first_cluster));
        item->setData(0, Qt::UserRole, entries[i].first_cluster);
    }
}

void FatEditorWidget::updateClusterMap()
{
    if (!m_fat) return;
    
    uft_fat_stats_t stats;
    if (uft_fat_get_stats(m_fat, &stats) != 0) return;
    
    int cols = 64;
    int rows = (stats.total_clusters + cols - 1) / cols;
    
    m_clusterMap->setColumnCount(cols);
    m_clusterMap->setRowCount(rows);
    m_clusterMap->horizontalHeader()->hide();
    m_clusterMap->verticalHeader()->hide();
    
    for (uint32_t c = 2; c < stats.total_clusters + 2; c++) {
        int idx = c - 2;
        QTableWidgetItem *item = new QTableWidgetItem();
        item->setBackground(getClusterColor(c));
        item->setToolTip(QString("Cluster %1").arg(c));
        item->setData(Qt::UserRole, c);
        m_clusterMap->setItem(idx / cols, idx % cols, item);
    }
}

void FatEditorWidget::updateBootSectorView()
{
    if (!m_fat) return;
    const char *label = uft_fat_get_volume_label(m_fat);
    if (label) m_volumeLabelEdit->setText(QString::fromLatin1(label).trimmed());
}

void FatEditorWidget::updateHexView(uint32_t cluster)
{
    if (!m_fat) return;
    
    uint8_t buffer[4096];
    int read = uft_fat_read_cluster(m_fat, cluster, buffer, sizeof(buffer));
    if (read <= 0) {
        m_hexEdit->setPlainText(tr("Error reading cluster %1").arg(cluster));
        return;
    }
    
    QString hex;
    for (int i = 0; i < read; i += 16) {
        hex += QString("%1: ").arg(i, 4, 16, QChar('0')).toUpper();
        for (int j = 0; j < 16 && i + j < read; j++)
            hex += QString("%1 ").arg(buffer[i + j], 2, 16, QChar('0')).toUpper();
        hex += " ";
        for (int j = 0; j < 16 && i + j < read; j++) {
            char c = buffer[i + j];
            hex += (c >= 32 && c < 127) ? QChar(c) : QChar('.');
        }
        hex += "\n";
    }
    m_hexEdit->setPlainText(hex);
}

/*===========================================================================
 * SLOTS
 *===========================================================================*/

void FatEditorWidget::selectCluster(uint32_t cluster)
{
    m_selectedCluster = cluster;
    uint32_t value = uft_fat_get_cluster(m_fat, cluster);
    m_clusterValueEdit->setText(QString("0x%1").arg(value, 0, 16).toUpper());
    
    uft_cluster_status_t status = uft_fat_get_cluster_status(m_fat, cluster);
    QString statusStr;
    switch (status) {
        case UFT_CLUSTER_FREE: statusStr = tr("Free"); break;
        case UFT_CLUSTER_USED: statusStr = tr("Used -> %1").arg(value); break;
        case UFT_CLUSTER_BAD: statusStr = tr("Bad"); break;
        case UFT_CLUSTER_END: statusStr = tr("End of chain"); break;
        default: statusStr = tr("Reserved"); break;
    }
    m_clusterInfoLabel->setText(tr("Cluster %1: %2").arg(cluster).arg(statusStr));
    emit clusterSelected(cluster);
}

void FatEditorWidget::showClusterChain(uint32_t startCluster)
{
    if (!m_fat) return;
    
    uft_cluster_chain_t chain;
    if (uft_fat_get_chain(m_fat, startCluster, &chain) != 0) return;
    
    QString chainStr = tr("Chain: ");
    for (int i = 0; i < chain.cluster_count && i < 20; i++) {
        if (i > 0) chainStr += " -> ";
        chainStr += QString::number(chain.clusters[i]);
    }
    if (chain.cluster_count > 20)
        chainStr += QString(" ... (%1 more)").arg(chain.cluster_count - 20);
    
    m_chainLabel->setText(chainStr);
    m_currentChain.clear();
    for (int i = 0; i < chain.cluster_count; i++)
        m_currentChain.append(chain.clusters[i]);
    
    uft_fat_free_chain(&chain);
    updateClusterMap();
}

void FatEditorWidget::markClusterBad()
{
    if (!m_fat || m_selectedCluster < 2) return;
    if (uft_fat_mark_bad(m_fat, m_selectedCluster) == 0) {
        m_modified = true;
        emit modified();
        refresh();
    }
}

void FatEditorWidget::markClusterFree()
{
    if (!m_fat || m_selectedCluster < 2) return;
    if (uft_fat_mark_free(m_fat, m_selectedCluster) == 0) {
        m_modified = true;
        emit modified();
        refresh();
    }
}

void FatEditorWidget::findLostClusters()
{
    if (!m_fat) return;
    uint32_t clusters[1000];
    int count = uft_fat_find_lost_clusters(m_fat, clusters, 1000);
    
    QString msg = count > 0 ? tr("Found %1 lost clusters").arg(count) :
                              tr("No lost clusters found.");
    QMessageBox::information(this, tr("Lost Clusters"), msg);
}

void FatEditorWidget::repairFilesystem()
{
    if (!m_fat) return;
    if (uft_fat_repair(m_fat, true, true) == 0) {
        m_modified = true;
        emit modified();
        refresh();
        QMessageBox::information(this, tr("Repair"), tr("Filesystem repaired."));
    }
}

void FatEditorWidget::onDirectoryItemClicked(QTreeWidgetItem *item, int)
{
    uint32_t cluster = item->data(0, Qt::UserRole).toUInt();
    if (cluster >= 2) showClusterChain(cluster);
}

void FatEditorWidget::onDirectoryItemDoubleClicked(QTreeWidgetItem *item, int)
{
    uint32_t cluster = item->data(0, Qt::UserRole).toUInt();
    if (cluster >= 2) {
        m_hexSectorSpin->setValue(cluster);
        updateHexView(cluster);
        m_tabWidget->setCurrentIndex(3); /* Hex tab */
    }
}

void FatEditorWidget::onClusterMapClicked(int row, int col)
{
    QTableWidgetItem *item = m_clusterMap->item(row, col);
    if (item) selectCluster(item->data(Qt::UserRole).toUInt());
}

void FatEditorWidget::onClusterValueChanged() { /* TODO */ }
void FatEditorWidget::onBootSectorFieldChanged() { /* TODO */ }
void FatEditorWidget::onContextMenuRequested(const QPoint &) { /* TODO */ }

/*===========================================================================
 * HELPERS
 *===========================================================================*/

QColor FatEditorWidget::getClusterColor(uint32_t cluster)
{
    if (m_currentChain.contains(cluster)) return m_colors.chain;
    if (cluster == m_selectedCluster) return m_colors.selected;
    
    uft_cluster_status_t status = uft_fat_get_cluster_status(m_fat, cluster);
    switch (status) {
        case UFT_CLUSTER_FREE: return m_colors.free;
        case UFT_CLUSTER_USED:
        case UFT_CLUSTER_END: return m_colors.used;
        case UFT_CLUSTER_BAD: return m_colors.bad;
        default: return m_colors.reserved;
    }
}

QString FatEditorWidget::formatSize(uint64_t size)
{
    if (size < 1024) return QString("%1 B").arg(size);
    if (size < 1024 * 1024) return QString("%1 KB").arg(size / 1024);
    if (size < 1024 * 1024 * 1024) return QString("%1 MB").arg(size / (1024 * 1024));
    return QString("%1 GB").arg(size / (1024 * 1024 * 1024));
}

QString FatEditorWidget::formatDateTime(uint16_t date, uint16_t time)
{
    int year, month, day, hour, minute, second;
    uft_fat_decode_date(date, &year, &month, &day);
    uft_fat_decode_time(time, &hour, &minute, &second);
    return QString("%1-%2-%3 %4:%5")
        .arg(year).arg(month, 2, 10, QChar('0')).arg(day, 2, 10, QChar('0'))
        .arg(hour, 2, 10, QChar('0')).arg(minute, 2, 10, QChar('0'));
}

void FatEditorWidget::populateDirectory(QTreeWidgetItem *, uint32_t) { /* TODO */ }

} // namespace UFT
