/**
 * @file uft_xcopy_panel.h
 * @brief XCopy Panel - Disk Copy and Duplication Settings
 */

#ifndef UFT_XCOPY_PANEL_H
#define UFT_XCOPY_PANEL_H

#include <QWidget>
#include <QGroupBox>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>

class UftXCopyPanel : public QWidget
{
    Q_OBJECT

public:
    explicit UftXCopyPanel(QWidget *parent = nullptr);

    struct XCopyParams {
        /* Source/Destination */
        int source_drive;
        int dest_drive;
        bool source_is_image;
        bool dest_is_image;
        QString source_path;
        QString dest_path;
        
        /* Track Range */
        int start_track;
        int end_track;
        int sides;              /* 0=Side 0, 1=Side 1, 2=Both */
        
        /* Copy Mode */
        int copy_mode;          /* 0=Normal, 1=Sector, 2=Track, 3=Flux */
        bool verify_after_write;
        int verify_retries;
        
        /* Error Handling */
        bool ignore_errors;
        bool retry_on_error;
        int max_retries;
        bool skip_bad_sectors;
        bool fill_bad_sectors;
        uint8_t fill_byte;
        
        /* Speed */
        int read_speed;         /* 0=Normal, 1=Fast, 2=Maximum */
        int write_speed;
        bool buffer_entire_disk;
        
        /* Multiple Copies */
        int num_copies;
        bool auto_eject;
        bool wait_for_disk;
    };

    XCopyParams getParams() const;
    void setParams(const XCopyParams &params);

signals:
    void copyStarted();
    void copyProgress(int track, int side, int percent);
    void copyFinished(bool success);
    void paramsChanged();

public slots:
    void startCopy();
    void stopCopy();
    void selectSource();
    void selectDest();

private:
    void setupUi();
    void createSourceGroup();
    void createDestGroup();
    void createRangeGroup();
    void createModeGroup();
    void createErrorGroup();
    void createSpeedGroup();
    void createMultipleGroup();
    void createProgressGroup();

    /* Source */
    QGroupBox *m_sourceGroup;
    QComboBox *m_sourceDrive;
    QRadioButton *m_sourceIsDrive;
    QRadioButton *m_sourceIsImage;
    QLineEdit *m_sourcePath;
    QPushButton *m_browseSource;
    
    /* Destination */
    QGroupBox *m_destGroup;
    QComboBox *m_destDrive;
    QRadioButton *m_destIsDrive;
    QRadioButton *m_destIsImage;
    QLineEdit *m_destPath;
    QPushButton *m_browseDest;
    
    /* Range */
    QGroupBox *m_rangeGroup;
    QSpinBox *m_startTrack;
    QSpinBox *m_endTrack;
    QComboBox *m_sides;
    QCheckBox *m_allTracks;
    
    /* Mode */
    QGroupBox *m_modeGroup;
    QComboBox *m_copyMode;
    QCheckBox *m_verifyWrite;
    QSpinBox *m_verifyRetries;
    
    /* Error Handling */
    QGroupBox *m_errorGroup;
    QCheckBox *m_ignoreErrors;
    QCheckBox *m_retryErrors;
    QSpinBox *m_maxRetries;
    QCheckBox *m_skipBadSectors;
    QCheckBox *m_fillBadSectors;
    QSpinBox *m_fillByte;
    
    /* Speed */
    QGroupBox *m_speedGroup;
    QComboBox *m_readSpeed;
    QComboBox *m_writeSpeed;
    QCheckBox *m_bufferDisk;
    
    /* Multiple */
    QGroupBox *m_multipleGroup;
    QSpinBox *m_numCopies;
    QCheckBox *m_autoEject;
    QCheckBox *m_waitForDisk;
    
    /* Progress */
    QGroupBox *m_progressGroup;
    QProgressBar *m_totalProgress;
    QProgressBar *m_trackProgress;
    QLabel *m_statusLabel;
    QPushButton *m_startButton;
    QPushButton *m_stopButton;
};

#endif /* UFT_XCOPY_PANEL_H */
