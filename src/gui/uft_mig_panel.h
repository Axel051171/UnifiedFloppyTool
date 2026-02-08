/**
 * @file uft_mig_panel.h
 * @brief MIG-Flash Dumper Qt GUI Panel
 * 
 * @version 2.0.0
 * @date 2026-01-20
 */

#ifndef UFT_MIG_PANEL_H
#define UFT_MIG_PANEL_H

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QGroupBox>
#include <QCheckBox>
#include <QTimer>
#include <QThread>
#include <QMutex>

#include "uft/mig/mig_block_io.h"

/*============================================================================
 * WORKER THREAD
 *============================================================================*/

class MIGWorker : public QObject {
    Q_OBJECT
    
public:
    enum Operation {
        OP_NONE,
        OP_CONNECT,
        OP_AUTHENTICATE,
        OP_DUMP_XCI,
        OP_READ_UID,
        OP_READ_CERT
    };
    
    explicit MIGWorker(QObject *parent = nullptr);
    ~MIGWorker();
    
public slots:
    void connectDevice(const QString &path);
    void disconnectDevice();
    void authenticate();
    void dumpXCI(const QString &filename, bool trimmed);
    void readUID();
    void readCertificate(const QString &filename);
    void abort();
    
signals:
    void connected(const QString &firmwareVersion);
    void disconnected();
    void authenticated(quint64 totalSize, quint64 trimmedSize);
    void cartInserted();
    void cartRemoved();
    void progress(quint64 done, quint64 total, int speedKBps);
    void finished(bool success, const QString &message);
    void error(const QString &message);
    void uidRead(const QByteArray &uid);
    
private:
    mig_device_t *m_device;
    bool m_abort;
    QMutex m_mutex;
    
    static bool progressCallback(uint64_t done, uint64_t total, void *user);
};

/*============================================================================
 * MAIN PANEL
 *============================================================================*/

class UFTMIGPanel : public QWidget {
    Q_OBJECT
    
public:
    explicit UFTMIGPanel(QWidget *parent = nullptr);
    ~UFTMIGPanel();
    
public slots:
    void refreshDevices();
    void connectDevice();
    void disconnectDevice();
    void authenticate();
    void dumpXCI();
    void readUID();
    void readCertificate();
    void abortOperation();
    
private slots:
    void onWorkerConnected(const QString &firmwareVersion);
    void onWorkerDisconnected();
    void onWorkerAuthenticated(quint64 totalSize, quint64 trimmedSize);
    void onWorkerCartInserted();
    void onWorkerCartRemoved();
    void onWorkerProgress(quint64 done, quint64 total, int speedKBps);
    void onWorkerFinished(bool success, const QString &message);
    void onWorkerError(const QString &message);
    void onWorkerUIDRead(const QByteArray &uid);
    void onPollTimer();
    
signals:
    void deviceConnected(const QString &path, const QString &firmware);
    void deviceDisconnected();
    void cartridgeInserted();
    void cartridgeRemoved();
    void dumpStarted(const QString &filename);
    void dumpProgress(int percent, int speedKBps);
    void dumpComplete(const QString &filename);
    void statusMessage(const QString &message);
    
private:
    void setupUI();
    void updateUI();
    void setOperationInProgress(bool inProgress);
    QString formatSize(quint64 bytes);
    QString formatSpeed(int kbps);
    QString formatETA(quint64 done, quint64 total, int speedKBps);
    
    /* State */
    bool m_connected;
    bool m_cartInserted;
    bool m_authenticated;
    bool m_operationInProgress;
    quint64 m_cartTotalSize;
    quint64 m_cartTrimmedSize;
    
    /* Worker */
    QThread *m_workerThread;
    MIGWorker *m_worker;
    
    /* UI - Device Group */
    QGroupBox *m_deviceGroup;
    QComboBox *m_deviceCombo;
    QPushButton *m_refreshBtn;
    QPushButton *m_connectBtn;
    QLabel *m_firmwareLabel;
    
    /* UI - Cartridge Group */
    QGroupBox *m_cartGroup;
    QLabel *m_cartStatusLabel;
    QLabel *m_cartSizeLabel;
    QLabel *m_cartTrimmedLabel;
    QPushButton *m_authBtn;
    
    /* UI - Dump Group */
    QGroupBox *m_dumpGroup;
    QCheckBox *m_trimmedCheck;
    QPushButton *m_dumpBtn;
    QPushButton *m_uidBtn;
    QPushButton *m_certBtn;
    QPushButton *m_abortBtn;
    
    /* UI - Progress */
    QProgressBar *m_progressBar;
    QLabel *m_progressLabel;
    QLabel *m_speedLabel;
    QLabel *m_etaLabel;
    
    /* UI - Status */
    QLabel *m_statusLabel;
    
    /* Timer for cart detection */
    QTimer *m_pollTimer;
};

#endif /* UFT_MIG_PANEL_H */
