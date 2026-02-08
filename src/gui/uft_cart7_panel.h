/**
 * @file uft_cart7_panel.h
 * @brief 7-in-1 Cartridge Reader GUI Panel
 * 
 * Qt-based GUI panel for the multi-system cartridge reader.
 * 
 * @version 1.0.0
 * @date 2026-01-20
 */

#ifndef UFT_CART7_PANEL_H
#define UFT_CART7_PANEL_H

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QLineEdit>
#include <QGroupBox>
#include <QTimer>
#include <QThread>
#include <QTreeWidget>
#include <QCheckBox>
#include <QSpinBox>
#include <QTabWidget>

/* Forward declarations */
struct cart7_device;
typedef struct cart7_device cart7_device_t;

/**
 * @brief Worker thread for cartridge operations
 */
class Cart7Worker : public QThread {
    Q_OBJECT
    
public:
    enum Operation {
        OP_NONE,
        OP_DUMP_ROM,
        OP_DUMP_SAVE,
        OP_WRITE_SAVE,
        OP_VERIFY
    };
    
    Cart7Worker(QObject *parent = nullptr);
    ~Cart7Worker();
    
    void setDevice(cart7_device_t *dev) { m_device = dev; }
    void setOperation(Operation op) { m_operation = op; }
    void setOutputPath(const QString &path) { m_outputPath = path; }
    void setInputPath(const QString &path) { m_inputPath = path; }
    void abort() { m_abort = true; }
    
signals:
    void progressChanged(quint64 current, quint64 total, quint32 speed);
    void finished(bool success, const QString &message);
    void statusChanged(const QString &status);
    
protected:
    void run() override;
    
private:
    cart7_device_t *m_device;
    Operation m_operation;
    QString m_outputPath;
    QString m_inputPath;
    volatile bool m_abort;
};

/**
 * @brief Main panel for 7-in-1 Cartridge Reader
 */
class Cart7Panel : public QWidget {
    Q_OBJECT
    
public:
    explicit Cart7Panel(QWidget *parent = nullptr);
    ~Cart7Panel();
    
public slots:
    void refreshDevices();
    void connectDevice();
    void disconnectDevice();
    void selectSlot(int index);
    void startDump();
    void startSaveBackup();
    void startSaveRestore();
    void abortOperation();
    void browseOutputPath();
    void browseInputPath();
    
private slots:
    void onPollTimer();
    void onWorkerProgress(quint64 current, quint64 total, quint32 speed);
    void onWorkerFinished(bool success, const QString &message);
    void onWorkerStatus(const QString &status);
    
private:
    void setupUI();
    void setupDeviceGroup();
    void setupSlotGroup();
    void setupCartridgeGroup();
    void setupDumpGroup();
    void setupSaveGroup();
    void updateUIState();
    void displayCartridgeInfo();
    QString formatSize(quint64 bytes);
    QString generateFilename();
    
    /* UI Elements - Device */
    QGroupBox *m_deviceGroup;
    QComboBox *m_deviceCombo;
    QPushButton *m_refreshBtn;
    QPushButton *m_connectBtn;
    QPushButton *m_disconnectBtn;
    QLabel *m_fwVersionLabel;
    QLabel *m_serialLabel;
    
    /* UI Elements - Slot Selection */
    QGroupBox *m_slotGroup;
    QComboBox *m_slotCombo;
    QLabel *m_voltageLabel;
    QCheckBox *m_autoVoltageCheck;
    
    /* UI Elements - Cartridge Info */
    QGroupBox *m_cartGroup;
    QLabel *m_cartStatusLabel;
    QLabel *m_systemLabel;
    QTreeWidget *m_infoTree;
    
    /* UI Elements - ROM Dump */
    QGroupBox *m_dumpGroup;
    QLineEdit *m_outputPathEdit;
    QPushButton *m_browseOutBtn;
    QCheckBox *m_trimCheck;
    QCheckBox *m_verifyCheck;
    QPushButton *m_dumpBtn;
    QPushButton *m_abortBtn;
    QProgressBar *m_progressBar;
    QLabel *m_speedLabel;
    QLabel *m_etaLabel;
    
    /* UI Elements - Save Management */
    QGroupBox *m_saveGroup;
    QPushButton *m_backupSaveBtn;
    QPushButton *m_restoreSaveBtn;
    QLineEdit *m_savePathEdit;
    QPushButton *m_browseSaveBtn;
    
    /* State */
    cart7_device_t *m_device;
    QTimer *m_pollTimer;
    Cart7Worker *m_worker;
    bool m_connected;
    bool m_cartPresent;
    int m_currentSlot;
    quint64 m_romSize;
    quint64 m_saveSize;
    QString m_currentSystem;
};

#endif /* UFT_CART7_PANEL_H */
