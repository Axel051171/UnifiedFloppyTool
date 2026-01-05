/**
 * @file uft_recovery_panel.h
 * @brief Recovery Panel - Disk Recovery, Repair, and Data Rescue
 */

#ifndef UFT_RECOVERY_PANEL_H
#define UFT_RECOVERY_PANEL_H

#include <QWidget>
#include <QGroupBox>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QPlainTextEdit>

class UftRecoveryPanel : public QWidget
{
    Q_OBJECT

public:
    explicit UftRecoveryPanel(QWidget *parent = nullptr);

    struct RecoveryParams {
        /* Read Retries */
        int max_retries;
        int retry_delay_ms;
        bool vary_head_position;
        double head_offset_range;
        
        /* Error Recovery */
        bool try_multiple_revolutions;
        int revolutions_to_try;
        bool use_best_revolution;
        bool merge_good_sectors;
        
        /* PLL Recovery */
        bool adaptive_pll;
        bool try_multiple_clocks;
        double clock_range_pct;
        int clock_steps;
        
        /* CRC Recovery */
        bool attempt_crc_repair;
        int max_bit_flips;
        bool brute_force_crc;
        
        /* Weak Bit Recovery */
        bool recover_weak_bits;
        int weak_bit_samples;
        bool statistical_recovery;
        
        /* Surface Analysis */
        bool analyze_surface;
        bool map_bad_sectors;
        bool find_spare_sectors;
        
        /* Output */
        bool create_recovery_log;
        bool create_error_map;
        bool save_partial_data;
    };

    RecoveryParams getParams() const;
    void setParams(const RecoveryParams &params);

signals:
    void paramsChanged();
    void recoveryStarted();
    void sectorRecovered(int track, int side, int sector, bool success);
    void recoveryFinished(int recovered, int failed);

public slots:
    void startRecovery();
    void stopRecovery();
    void analyzeImage();
    void repairImage();

private:
    void setupUi();
    void createRetryGroup();
    void createErrorRecoveryGroup();
    void createPllRecoveryGroup();
    void createCrcRecoveryGroup();
    void createWeakBitGroup();
    void createSurfaceGroup();
    void createOutputGroup();
    void createResultsView();

    /* Retries */
    QGroupBox *m_retryGroup;
    QSpinBox *m_maxRetries;
    QSpinBox *m_retryDelay;
    QCheckBox *m_varyHeadPosition;
    QDoubleSpinBox *m_headOffsetRange;
    
    /* Error Recovery */
    QGroupBox *m_errorRecoveryGroup;
    QCheckBox *m_multipleRevolutions;
    QSpinBox *m_revolutionsToTry;
    QCheckBox *m_useBestRevolution;
    QCheckBox *m_mergeGoodSectors;
    
    /* PLL Recovery */
    QGroupBox *m_pllRecoveryGroup;
    QCheckBox *m_adaptivePll;
    QCheckBox *m_multipleClocks;
    QDoubleSpinBox *m_clockRange;
    QSpinBox *m_clockSteps;
    
    /* CRC Recovery */
    QGroupBox *m_crcRecoveryGroup;
    QCheckBox *m_attemptCrcRepair;
    QSpinBox *m_maxBitFlips;
    QCheckBox *m_bruteForceCrc;
    
    /* Weak Bits */
    QGroupBox *m_weakBitGroup;
    QCheckBox *m_recoverWeakBits;
    QSpinBox *m_weakBitSamples;
    QCheckBox *m_statisticalRecovery;
    
    /* Surface */
    QGroupBox *m_surfaceGroup;
    QCheckBox *m_analyzeSurface;
    QCheckBox *m_mapBadSectors;
    QCheckBox *m_findSpareSectors;
    
    /* Output */
    QGroupBox *m_outputGroup;
    QCheckBox *m_createRecoveryLog;
    QCheckBox *m_createErrorMap;
    QCheckBox *m_savePartialData;
    
    /* Results */
    QTableWidget *m_resultsTable;
    QProgressBar *m_recoveryProgress;
    QPlainTextEdit *m_recoveryLog;
    QPushButton *m_startButton;
    QPushButton *m_stopButton;
};

#endif /* UFT_RECOVERY_PANEL_H */
