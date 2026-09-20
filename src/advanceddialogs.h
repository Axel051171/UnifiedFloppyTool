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
