#ifndef ADVANCEDDIALOGS_H
#define ADVANCEDDIALOGS_H

#include <QDialog>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QDialogButtonBox>

// ============================================================================
// FLUX ADVANCED DIALOG
// ============================================================================
class FluxAdvancedDialog : public QDialog {
    Q_OBJECT
public:
    explicit FluxAdvancedDialog(QWidget *parent = nullptr);
    
    struct FluxAdvancedParams {
        // PLL Fine-Tuning
        double pllFrequency;      // Hz
        double pllBandwidth;      // 0.0 - 1.0
        double pllPhase;          // -1.0 to 1.0
        int pllLockThreshold;     // %
        
        // Timing
        double bitcellPeriod;     // µs
        double clockTolerance;    // %
        
        // Revolution Handling
        int revsToRead;
        int revsToUse;
        bool mergeRevs;
        int mergeMode;            // 0=First, 1=Best, 2=All
        
        // Detection Thresholds
        int weakBitThreshold;     // %
        int noFluxThreshold;      // µs
        
        // Index
        bool useIndex;
        double indexOffset;       // µs
        bool softIndex;
    };
    
    FluxAdvancedParams getParams() const;
    void setParams(const FluxAdvancedParams &params);

private:
    void setupUi();
    
    // PLL
    QDoubleSpinBox *m_pllFrequency;
    QDoubleSpinBox *m_pllBandwidth;
    QDoubleSpinBox *m_pllPhase;
    QSpinBox *m_pllLockThreshold;
    
    // Timing
    QDoubleSpinBox *m_bitcellPeriod;
    QDoubleSpinBox *m_clockTolerance;
    
    // Revolutions
    QSpinBox *m_revsToRead;
    QSpinBox *m_revsToUse;
    QCheckBox *m_mergeRevs;
    QComboBox *m_mergeMode;
    
    // Thresholds
    QSpinBox *m_weakBitThreshold;
    QSpinBox *m_noFluxThreshold;
    
    // Index
    QCheckBox *m_useIndex;
    QDoubleSpinBox *m_indexOffset;
    QCheckBox *m_softIndex;
};

// ============================================================================
// PLL ADVANCED DIALOG
// ============================================================================
class PLLAdvancedDialog : public QDialog {
    Q_OBJECT
public:
    explicit PLLAdvancedDialog(QWidget *parent = nullptr);
    
    struct PLLAdvancedParams {
        // Clock
        double clockRate;         // MHz
        double bitCell;           // µs
        
        // Filter
        int filterType;           // 0=Simple, 1=PID, 2=Adaptive
        int historyDepth;
        
        // Gain
        bool adaptiveGain;
        int tolerance;            // %
        
        // Lock Detection
        int lockThreshold;        // %
        int unlockThreshold;      // %
        
        // Weak Bits
        bool weakBitDetection;
        int weakBitWindow;        // bits
    };
    
    PLLAdvancedParams getParams() const;
    void setParams(const PLLAdvancedParams &params);

private:
    void setupUi();
    
    QDoubleSpinBox *m_clockRate;
    QDoubleSpinBox *m_bitCell;
    QComboBox *m_filterType;
    QSpinBox *m_historyDepth;
    QCheckBox *m_adaptiveGain;
    QSlider *m_tolerance;
    QLabel *m_toleranceLabel;
    QSpinBox *m_lockThreshold;
    QSpinBox *m_unlockThreshold;
    QCheckBox *m_weakBitDetection;
    QSpinBox *m_weakBitWindow;
};

// ============================================================================
// NIBBLE ADVANCED DIALOG
// ============================================================================
class NibbleAdvancedDialog : public QDialog {
    Q_OBJECT
public:
    explicit NibbleAdvancedDialog(QWidget *parent = nullptr);
    
    struct NibbleAdvancedParams {
        // GCR Settings
        int gcrVariant;           // 0=Standard, 1=Apple, 2=C64, 3=Victor
        bool rawNibble;           // No decoding
        bool decodeToSectors;
        
        // Track Options
        bool includeHalfTracks;
        bool includeQuarterTracks;
        int trackStep;            // 1, 2, 4
        
        // Sync Detection
        int syncPattern;          // Hex value
        int syncLength;           // bits
        bool autoDetectSync;
        
        // Error Handling
        bool ignoreBadGCR;
        bool fillBadSectors;
        uint8_t fillByte;
        
        // Output
        bool preserveGaps;
        bool preserveSync;
    };
    
    NibbleAdvancedParams getParams() const;
    void setParams(const NibbleAdvancedParams &params);

private:
    void setupUi();
    
    QComboBox *m_gcrVariant;
    QCheckBox *m_rawNibble;
    QCheckBox *m_decodeToSectors;
    QCheckBox *m_includeHalfTracks;
    QCheckBox *m_includeQuarterTracks;
    QSpinBox *m_trackStep;
    QLineEdit *m_syncPattern;
    QSpinBox *m_syncLength;
    QCheckBox *m_autoDetectSync;
    QCheckBox *m_ignoreBadGCR;
    QCheckBox *m_fillBadSectors;
    QSpinBox *m_fillByte;
    QCheckBox *m_preserveGaps;
    QCheckBox *m_preserveSync;
};

#endif // ADVANCEDDIALOGS_H
