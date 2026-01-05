/**
 * @file uft_flux_panel.h
 * @brief Flux Settings Panel - PLL, Timing, Bitrate, etc.
 */

#ifndef UFT_FLUX_PANEL_H
#define UFT_FLUX_PANEL_H

#include <QWidget>
#include <QGroupBox>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QSlider>
#include <QLabel>

class UftFluxPanel : public QWidget
{
    Q_OBJECT

public:
    explicit UftFluxPanel(QWidget *parent = nullptr);

    /* Get/Set Parameters */
    struct FluxParams {
        /* PLL Settings */
        double pll_frequency;       /* Hz */
        double pll_bandwidth;       /* 0.0 - 1.0 */
        double pll_phase_adjust;    /* -1.0 to 1.0 */
        int    pll_lock_threshold;
        bool   pll_adaptive;
        
        /* Timing */
        double bitcell_period_ns;
        double clock_tolerance_pct;
        int    sample_rate_mhz;
        
        /* Revolution */
        int    revolutions_to_read;
        int    revolutions_to_use;
        bool   merge_revolutions;
        int    merge_mode;          /* 0=First, 1=Best, 2=Consensus */
        
        /* Flux Analysis */
        bool   detect_weak_bits;
        int    weak_bit_threshold;
        bool   detect_no_flux_areas;
        int    no_flux_threshold_us;
        
        /* Index */
        bool   use_index_signal;
        double index_offset_us;
        bool   soft_index;
        
        /* Filtering */
        bool   filter_noise;
        int    noise_threshold_ns;
        bool   filter_spikes;
        int    spike_threshold_ns;
        
        /* Output */
        int    output_resolution;   /* bits */
        bool   preserve_timing;
        bool   normalize_flux;
    };

    FluxParams getParams() const;
    void setParams(const FluxParams &params);

signals:
    void paramsChanged();

private:
    void setupUi();
    void createPllGroup();
    void createTimingGroup();
    void createRevolutionGroup();
    void createAnalysisGroup();
    void createIndexGroup();
    void createFilterGroup();
    void createOutputGroup();

    /* PLL */
    QGroupBox *m_pllGroup;
    QDoubleSpinBox *m_pllFrequency;
    QDoubleSpinBox *m_pllBandwidth;
    QDoubleSpinBox *m_pllPhase;
    QSpinBox *m_pllLockThreshold;
    QCheckBox *m_pllAdaptive;
    
    /* Timing */
    QGroupBox *m_timingGroup;
    QDoubleSpinBox *m_bitcellPeriod;
    QDoubleSpinBox *m_clockTolerance;
    QComboBox *m_sampleRate;
    
    /* Revolution */
    QGroupBox *m_revolutionGroup;
    QSpinBox *m_revsToRead;
    QSpinBox *m_revsToUse;
    QCheckBox *m_mergeRevs;
    QComboBox *m_mergeMode;
    
    /* Analysis */
    QGroupBox *m_analysisGroup;
    QCheckBox *m_detectWeakBits;
    QSpinBox *m_weakBitThreshold;
    QCheckBox *m_detectNoFlux;
    QSpinBox *m_noFluxThreshold;
    
    /* Index */
    QGroupBox *m_indexGroup;
    QCheckBox *m_useIndex;
    QDoubleSpinBox *m_indexOffset;
    QCheckBox *m_softIndex;
    
    /* Filter */
    QGroupBox *m_filterGroup;
    QCheckBox *m_filterNoise;
    QSpinBox *m_noiseThreshold;
    QCheckBox *m_filterSpikes;
    QSpinBox *m_spikeThreshold;
    
    /* Output */
    QGroupBox *m_outputGroup;
    QComboBox *m_outputResolution;
    QCheckBox *m_preserveTiming;
    QCheckBox *m_normalizeFlux;
};

#endif /* UFT_FLUX_PANEL_H */
