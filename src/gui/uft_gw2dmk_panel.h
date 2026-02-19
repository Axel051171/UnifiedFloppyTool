/**
 * @file uft_gw2dmk_panel.h
 * @brief GUI Panel for Direct Greaseweazle to DMK Reading
 * 
 * @version 1.0
 * @date 2026-01-16
 */

#ifndef UFT_GW2DMK_PANEL_H
#define UFT_GW2DMK_PANEL_H

#include <QWidget>
#include <QThread>
#include <QMutex>

class QLabel;
class QLineEdit;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QPushButton;
class QProgressBar;
class QGroupBox;
class QTextEdit;
class QTableWidget;

/**
 * @brief Worker thread for disk reading
 */
class UftGw2DmkWorker : public QThread
{
    Q_OBJECT

public:
    enum Operation {
        OP_NONE,
        OP_DETECT,          ///< Detect device
        OP_READ_TRACK,      ///< Read single track
        OP_READ_DISK        ///< Read entire disk
    };

    explicit UftGw2DmkWorker(QObject *parent = nullptr);
    ~UftGw2DmkWorker();

    void setOperation(Operation op);
    void setOutputPath(const QString &path);
    void setTrackRange(int start, int end);
    void setHeads(int heads);
    void setDiskType(int type);
    void setRetries(int retries);
    void setRevolutions(int revs);
    void setDevicePath(const QString &path);

    void requestStop();

signals:
    void deviceDetected(const QString &info);
    void deviceError(const QString &error);
    void progressChanged(int track, int head, int total, const QString &message);
    void trackRead(int track, int head, int sectors, int errors);
    void operationComplete(bool success, const QString &message);
    void fluxDataReady(int track, int head, const QByteArray &data);

protected:
    void run() override;

private:
    QMutex m_mutex;
    volatile bool m_stopRequested;
    
    Operation m_operation;
    QString m_outputPath;
    QString m_devicePath;
    int m_startTrack;
    int m_endTrack;
    int m_heads;
    int m_diskType;
    int m_retries;
    int m_revolutions;
};

/**
 * @brief Main GUI panel for Direct GW→DMK
 */
class UftGw2DmkPanel : public QWidget
{
    Q_OBJECT

public:
    explicit UftGw2DmkPanel(QWidget *parent = nullptr);
    ~UftGw2DmkPanel();

public slots:
    void detectDevice();
    void readDisk();
    void readTrack();
    void stopOperation();
    void browseOutput();
    void setPreset(int index);  ///< Set disk type preset

private slots:
    void onDeviceDetected(const QString &info);
    void onDeviceError(const QString &error);
    void onProgressChanged(int track, int head, int total, const QString &message);
    void onTrackRead(int track, int head, int sectors, int errors);
    void onOperationComplete(bool success, const QString &message);
    void onFluxDataReady(int track, int head, const QByteArray &data);
    void onDiskTypeChanged(int index);

signals:
    void trackReadComplete(int track, int head);
    void diskReadComplete(const QString &filename);
    void fluxHistogramRequested(const QByteArray &data);

private:
    void setupUi();
    void updateControlsState();
    void addLogMessage(const QString &msg, bool isError = false);

    // Worker thread
    UftGw2DmkWorker *m_worker;
    bool m_operationInProgress;

    // Device selection
    QComboBox *m_deviceCombo;
    QPushButton *m_detectBtn;
    QLabel *m_deviceInfoLabel;

    // Disk type presets
    QComboBox *m_diskTypeCombo;
    
    // Geometry
    QGroupBox *m_geometryGroup;
    QSpinBox *m_tracksSpin;
    QSpinBox *m_headsSpin;
    QSpinBox *m_startTrackSpin;
    QSpinBox *m_endTrackSpin;
    
    // Options
    QGroupBox *m_optionsGroup;
    QSpinBox *m_retriesSpin;
    QSpinBox *m_revolutionsSpin;
    QCheckBox *m_useIndexCheck;
    QCheckBox *m_joinReadsCheck;
    QCheckBox *m_detectDamCheck;
    QCheckBox *m_doubleStepCheck;
    
    // Encoding
    QComboBox *m_encodingCombo;
    QComboBox *m_rpmCombo;
    QComboBox *m_dataRateCombo;

    // Output
    QGroupBox *m_outputGroup;
    QLineEdit *m_outputPathEdit;
    QPushButton *m_browseBtn;

    // Control buttons
    QPushButton *m_readDiskBtn;
    QPushButton *m_readTrackBtn;
    QPushButton *m_stopBtn;

    // Progress
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;

    // Track status table
    QTableWidget *m_trackTable;

    // Log
    QTextEdit *m_logText;
};

#endif /* UFT_GW2DMK_PANEL_H */
