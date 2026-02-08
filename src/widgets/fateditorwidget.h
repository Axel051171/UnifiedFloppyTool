/**
 * @file fateditorwidget.h
 * @brief Qt Widget for FAT Filesystem Editing
 * 
 * Features:
 * - FAT12/16/32 filesystem visualization
 * - Cluster chain viewer
 * - Directory browser
 * - Hex editor for raw sectors
 * - Boot sector editor
 * - Bad cluster management
 * - Lost cluster recovery
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#ifndef UFT_FAT_EDITOR_WIDGET_H
#define UFT_FAT_EDITOR_WIDGET_H

#include <QWidget>
#include <QTreeWidget>
#include <QTableWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMenu>
#include <QAction>
#include <QMessageBox>

/* Forward declaration for C API */
extern "C" {
    struct uft_fat_context;
    typedef struct uft_fat_context uft_fat_t;
}

namespace UFT {

/**
 * @brief Cluster status colors
 */
struct ClusterColors {
    QColor free;        /**< Available clusters */
    QColor used;        /**< Used clusters */
    QColor bad;         /**< Bad clusters */
    QColor reserved;    /**< Reserved clusters */
    QColor chain;       /**< Current chain highlight */
    QColor selected;    /**< Selected cluster */
};

/**
 * @brief FAT Editor Widget
 */
class FatEditorWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit FatEditorWidget(QWidget *parent = nullptr);
    ~FatEditorWidget();
    
    /**
     * @brief Load FAT filesystem from image file
     */
    bool loadImage(const QString &path);
    
    /**
     * @brief Load FAT from memory buffer
     */
    bool loadFromBuffer(const uint8_t *data, size_t size);
    
    /**
     * @brief Save changes back to image
     */
    bool saveImage(const QString &path);
    
    /**
     * @brief Check if filesystem is loaded
     */
    bool isLoaded() const;
    
    /**
     * @brief Check if there are unsaved changes
     */
    bool hasChanges() const;
    
public slots:
    /**
     * @brief Refresh all views
     */
    void refresh();
    
    /**
     * @brief Select cluster by number
     */
    void selectCluster(uint32_t cluster);
    
    /**
     * @brief Show cluster chain for file
     */
    void showClusterChain(uint32_t startCluster);
    
    /**
     * @brief Mark selected cluster as bad
     */
    void markClusterBad();
    
    /**
     * @brief Mark selected cluster as free
     */
    void markClusterFree();
    
    /**
     * @brief Find lost clusters
     */
    void findLostClusters();
    
    /**
     * @brief Repair filesystem
     */
    void repairFilesystem();
    
signals:
    /**
     * @brief Emitted when cluster is selected
     */
    void clusterSelected(uint32_t cluster);
    
    /**
     * @brief Emitted when file is selected in directory view
     */
    void fileSelected(const QString &path);
    
    /**
     * @brief Emitted when filesystem is modified
     */
    void modified();
    
    /**
     * @brief Emitted for status messages
     */
    void statusMessage(const QString &msg);
    
private slots:
    void onDirectoryItemClicked(QTreeWidgetItem *item, int column);
    void onDirectoryItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onClusterMapClicked(int row, int col);
    void onClusterValueChanged();
    void onBootSectorFieldChanged();
    void onContextMenuRequested(const QPoint &pos);
    
private:
    void setupUi();
    void setupDirectoryView();
    void setupClusterMap();
    void setupBootSectorEditor();
    void setupHexView();
    void connectSignals();
    
    void updateDirectoryView();
    void updateClusterMap();
    void updateBootSectorView();
    void updateStatistics();
    void updateHexView(uint32_t cluster);
    
    void populateDirectory(QTreeWidgetItem *parent, uint32_t cluster);
    QColor getClusterColor(uint32_t cluster);
    QString formatSize(uint64_t size);
    QString formatDateTime(uint16_t date, uint16_t time);
    
    /* FAT context */
    uft_fat_t *m_fat;
    QString m_currentPath;
    bool m_modified;
    
    /* UI Components */
    QTabWidget *m_tabWidget;
    
    /* Directory Tab */
    QTreeWidget *m_directoryTree;
    QLabel *m_fileInfoLabel;
    QPushButton *m_extractButton;
    QPushButton *m_deleteButton;
    
    /* Cluster Map Tab */
    QTableWidget *m_clusterMap;
    QLabel *m_clusterInfoLabel;
    QSpinBox *m_clusterSpinBox;
    QLineEdit *m_clusterValueEdit;
    QPushButton *m_gotoClusterButton;
    QLabel *m_chainLabel;
    
    /* Boot Sector Tab */
    QLineEdit *m_oemNameEdit;
    QLineEdit *m_volumeLabelEdit;
    QSpinBox *m_bytesPerSectorSpin;
    QSpinBox *m_sectorsPerClusterSpin;
    QSpinBox *m_reservedSectorsSpin;
    QSpinBox *m_numFatsSpin;
    QSpinBox *m_rootEntriesSpin;
    QLabel *m_totalSectorsLabel;
    QLabel *m_fatSizeLabel;
    QPushButton *m_fixBootButton;
    
    /* Hex View Tab */
    QPlainTextEdit *m_hexEdit;
    QSpinBox *m_hexSectorSpin;
    QPushButton *m_readSectorButton;
    QPushButton *m_writeSectorButton;
    
    /* Statistics Panel */
    QLabel *m_fatTypeLabel;
    QLabel *m_totalClustersLabel;
    QLabel *m_freeClustersLabel;
    QLabel *m_usedClustersLabel;
    QLabel *m_badClustersLabel;
    QProgressBar *m_usageBar;
    
    /* Colors */
    ClusterColors m_colors;
    
    /* Current state */
    uint32_t m_selectedCluster;
    QVector<uint32_t> m_currentChain;
};

} // namespace UFT

#endif /* UFT_FAT_EDITOR_WIDGET_H */
