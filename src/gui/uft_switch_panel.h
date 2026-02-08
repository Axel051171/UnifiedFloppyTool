/**
 * @file uft_switch_panel.h
 * @brief Nintendo Switch / MIG Dumper GUI Panel
 * 
 * @version 1.0.0
 * @date 2025-01-20
 */

#ifndef UFT_SWITCH_PANEL_H
#define UFT_SWITCH_PANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QTableWidget>
#include <QTreeWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QTimer>
#include <QThread>
#include <QFileDialog>
#include <QMessageBox>

/* Forward declarations */
struct uft_mig_device;
struct uft_xci_ctx;

/**
 * @class UftSwitchPanel
 * @brief Main panel for Nintendo Switch cartridge operations
 */
class UftSwitchPanel : public QWidget {
    Q_OBJECT

public:
    explicit UftSwitchPanel(QWidget *parent = nullptr);
    ~UftSwitchPanel();

signals:
    void deviceConnected(bool connected);
    void cartridgeInserted(bool inserted);
    void dumpStarted();
    void dumpProgress(int percent, double speedMbps);
    void dumpFinished(bool success, const QString &message);
    void logMessage(const QString &message);

public slots:
    void refreshDevices();
    void connectDevice();
    void disconnectDevice();
    void startDump();
    void abortDump();
    void browseXCI();
    void extractPartition();

private slots:
    void onDeviceTimer();
    void onDumpProgress(int percent, quint64 bytesRead, quint64 totalBytes, float speedMbps);
    void onDumpFinished(bool success);
    void onXciSelected(const QString &path);
    void onPartitionSelected(int index);

private:
    void setupUI();
    void setupDeviceGroup();
    void setupCartridgeGroup();
    void setupDumpGroup();
    void setupBrowserGroup();
    void updateDeviceStatus();
    void updateCartridgeInfo();
    void loadXciFile(const QString &path);

    /* Device Group */
    QGroupBox *m_deviceGroup;
    QComboBox *m_deviceCombo;
    QPushButton *m_refreshBtn;
    QPushButton *m_connectBtn;
    QLabel *m_deviceStatusLabel;
    QLabel *m_firmwareLabel;
    QLabel *m_serialLabel;

    /* Cartridge Group */
    QGroupBox *m_cartridgeGroup;
    QLabel *m_cartStatusLabel;
    QLabel *m_titleLabel;
    QLabel *m_titleIdLabel;
    QLabel *m_sizeLabel;
    QLabel *m_versionLabel;
    QPushButton *m_authBtn;

    /* Dump Group */
    QGroupBox *m_dumpGroup;
    QLineEdit *m_outputPathEdit;
    QPushButton *m_browseOutputBtn;
    QCheckBox *m_trimCheck;
    QCheckBox *m_dumpCertCheck;
    QCheckBox *m_dumpUidCheck;
    QPushButton *m_startDumpBtn;
    QPushButton *m_abortBtn;
    QProgressBar *m_progressBar;
    QLabel *m_progressLabel;
    QLabel *m_speedLabel;
    QLabel *m_etaLabel;

    /* Browser Group */
    QGroupBox *m_browserGroup;
    QLineEdit *m_xciPathEdit;
    QPushButton *m_browseXciBtn;
    QComboBox *m_partitionCombo;
    QTreeWidget *m_fileTree;
    QPushButton *m_extractBtn;
    QLabel *m_xciInfoLabel;

    /* State */
    struct uft_mig_device *m_device;
    struct uft_xci_ctx *m_xciCtx;
    QTimer *m_deviceTimer;
    bool m_dumping;
    QString m_currentXciPath;
};

/**
 * @class DumpWorker
 * @brief Background worker for XCI dumping
 */
class DumpWorker : public QThread {
    Q_OBJECT

public:
    DumpWorker(struct uft_mig_device *device, const QString &outputPath,
               bool trim, bool dumpCert, bool dumpUid, QObject *parent = nullptr);

signals:
    void progress(int percent, quint64 bytesRead, quint64 totalBytes, float speedMbps);
    void finished(bool success, const QString &message);
    void error(const QString &message);

protected:
    void run() override;

private:
    struct uft_mig_device *m_device;
    QString m_outputPath;
    bool m_trim;
    bool m_dumpCert;
    bool m_dumpUid;
};

#endif /* UFT_SWITCH_PANEL_H */
