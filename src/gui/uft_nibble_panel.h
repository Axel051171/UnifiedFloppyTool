/**
 * @file uft_nibble_panel.h
 * @brief Nibble Panel - Low-Level Copy and Analysis
 */

#ifndef UFT_NIBBLE_PANEL_H
#define UFT_NIBBLE_PANEL_H

#include <QWidget>
#include <QGroupBox>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QPlainTextEdit>

class UftNibblePanel : public QWidget
{
    Q_OBJECT

public:
    explicit UftNibblePanel(QWidget *parent = nullptr);

    struct NibbleParams {
        /* Read Mode */
        int read_mode;              /* 0=Normal, 1=Raw, 2=Flux */
        int revolutions;
        bool read_between_index;
        double index_to_index_ms;
        
        /* GCR Settings (Commodore/Apple) */
        bool gcr_mode;
        int gcr_type;               /* 0=C64, 1=Apple II, 2=Apple 3.5 */
        bool decode_gcr;
        bool preserve_sync;
        int sync_length;
        
        /* Timing */
        bool preserve_timing;
        double bit_time_tolerance;
        bool detect_weak_bits;
        bool mark_weak_bits;
        
        /* Half Tracks */
        bool read_half_tracks;
        bool analyze_half_tracks;
        double half_track_offset;
        
        /* Density */
        bool variable_density;
        int density_zones;
        bool auto_detect_density;
        
        /* Output */
        bool create_nib_file;
        bool create_g64_file;
        bool include_timing_data;
        bool include_raw_flux;
    };

    NibbleParams getParams() const;
    void setParams(const NibbleParams &params);

signals:
    void paramsChanged();
    void trackAnalyzed(int track, int side);

private:
    void setupUi();
    void createReadModeGroup();
    void createGcrGroup();
    void createTimingGroup();
    void createHalfTrackGroup();
    void createDensityGroup();
    void createOutputGroup();
    void createAnalysisView();

    /* Read Mode */
    QGroupBox *m_readModeGroup;
    QComboBox *m_readMode;
    QSpinBox *m_revolutions;
    QCheckBox *m_readBetweenIndex;
    QDoubleSpinBox *m_indexToIndex;
    
    /* GCR */
    QGroupBox *m_gcrGroup;
    QCheckBox *m_gcrMode;
    QComboBox *m_gcrType;
    QCheckBox *m_decodeGcr;
    QCheckBox *m_preserveSync;
    QSpinBox *m_syncLength;
    
    /* Timing */
    QGroupBox *m_timingGroup;
    QCheckBox *m_preserveTiming;
    QDoubleSpinBox *m_bitTimeTolerance;
    QCheckBox *m_detectWeakBits;
    QCheckBox *m_markWeakBits;
    
    /* Half Tracks */
    QGroupBox *m_halfTrackGroup;
    QCheckBox *m_readHalfTracks;
    QCheckBox *m_analyzeHalfTracks;
    QDoubleSpinBox *m_halfTrackOffset;
    
    /* Density */
    QGroupBox *m_densityGroup;
    QCheckBox *m_variableDensity;
    QSpinBox *m_densityZones;
    QCheckBox *m_autoDetectDensity;
    
    /* Output */
    QGroupBox *m_outputGroup;
    QCheckBox *m_createNib;
    QCheckBox *m_createG64;
    QCheckBox *m_includeTiming;
    QCheckBox *m_includeRawFlux;
    
    /* Analysis */
    QTableWidget *m_trackTable;
    QPlainTextEdit *m_analysisLog;
};

#endif /* UFT_NIBBLE_PANEL_H */
