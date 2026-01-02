// SPDX-License-Identifier: MIT
/*
 * DDParamsWidget.h - Qt Widget for DD/Recovery Parameters
 * 
 * Comprehensive GUI controls for:
 *   - Block sizes and I/O modes
 *   - Recovery options (from dd_rescue)
 *   - Forensic hashing (from DC3DD)
 *   - Floppy output configuration
 * 
 * @version 1.0.0
 * @date 2025-01-01
 */

#ifndef DDPARAMSWIDGET_H
#define DDPARAMSWIDGET_H

#include <QWidget>
#include <QTabWidget>
#include <QGroupBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>

extern "C" {
#include "uft/uft_dd.h"
}

class DDParamsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DDParamsWidget(QWidget *parent = nullptr);
    ~DDParamsWidget();

    // Configuration access
    dd_config_t getConfig() const;
    void setConfig(const dd_config_t &config);

    // Status access
    dd_status_t getStatus() const;

signals:
    void configChanged();
    void operationStarted();
    void operationFinished(int result);
    void progressUpdated(double percent);
    void errorOccurred(const QString &message);

public slots:
    void startOperation();
    void pauseOperation();
    void cancelOperation();
    void resetToDefaults();
    void detectFloppyDevices();

private slots:
    void onValueChanged();
    void onBrowseInput();
    void onBrowseOutput();
    void updateStatus();
    void onFloppyEnabledChanged(bool enabled);

private:
    void setupUI();
    void createIOTab(QFormLayout *layout);
    void createBlockSizeTab(QFormLayout *layout);
    void createRecoveryTab(QFormLayout *layout);
    void createHashTab(QFormLayout *layout);
    void createFloppyTab(QFormLayout *layout);
    void createStatusGroup();
    void updateConfigFromWidgets();
    void updateWidgetsFromConfig();

    // Current configuration
    dd_config_t m_config;
    
    // Status update timer
    QTimer *m_statusTimer;

    // Tab widget
    QTabWidget *m_tabWidget;

    // I/O widgets
    QLineEdit *m_inputFile;
    QPushButton *m_browseInput;
    QLineEdit *m_outputFile;
    QPushButton *m_browseOutput;
    QSpinBox *m_skipBytes;
    QSpinBox *m_seekBytes;
    QSpinBox *m_maxBytes;
    QComboBox *m_maxBytesUnit;

    // Block size widgets
    QComboBox *m_softBlockSize;
    QComboBox *m_hardBlockSize;
    QCheckBox *m_autoAdjust;
    QCheckBox *m_directIO;
    QCheckBox *m_syncWrites;
    QSpinBox *m_syncFrequency;

    // Recovery widgets
    QCheckBox *m_recoveryEnabled;
    QCheckBox *m_reverseRead;
    QCheckBox *m_sparseOutput;
    QCheckBox *m_continueOnError;
    QCheckBox *m_fillOnError;
    QSpinBox *m_fillPattern;
    QSpinBox *m_maxErrors;
    QSpinBox *m_retryCount;
    QSpinBox *m_retryDelay;

    // Hash widgets
    QCheckBox *m_hashMD5;
    QCheckBox *m_hashSHA1;
    QCheckBox *m_hashSHA256;
    QCheckBox *m_hashSHA512;
    QCheckBox *m_hashInput;
    QCheckBox *m_hashOutput;
    QCheckBox *m_verifyAfter;
    QLineEdit *m_expectedHash;

    // Floppy widgets
    QCheckBox *m_floppyEnabled;
    QComboBox *m_floppyDevice;
    QPushButton *m_detectFloppy;
    QComboBox *m_floppyType;
    QSpinBox *m_floppyTracks;
    QSpinBox *m_floppyHeads;
    QSpinBox *m_floppySPT;
    QSpinBox *m_floppySectorSize;
    QCheckBox *m_floppyFormat;
    QCheckBox *m_floppyVerify;
    QSpinBox *m_floppyRetries;
    QCheckBox *m_floppySkipBad;
    QSpinBox *m_stepDelay;
    QSpinBox *m_settleDelay;
    QSpinBox *m_motorDelay;

    // Status widgets
    QGroupBox *m_statusGroup;
    QProgressBar *m_progressBar;
    QLabel *m_bytesRead;
    QLabel *m_bytesWritten;
    QLabel *m_errors;
    QLabel *m_speed;
    QLabel *m_eta;
    QLabel *m_currentPosition;
    QPushButton *m_startButton;
    QPushButton *m_pauseButton;
    QPushButton *m_cancelButton;
};

#endif // DDPARAMSWIDGET_H
