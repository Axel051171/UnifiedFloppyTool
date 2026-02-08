/**
 * @file AnalyzerToolbar.h
 * @brief Kompakte Analyzer-Toolbar für schnellen Überblick
 * 
 * Zeigt Analyse-Ergebnisse inline an und ermöglicht schnelle Aktionen.
 * Integriert sich nahtlos in MainWindow und XCopy-Panel.
 */

#ifndef ANALYZER_TOOLBAR_H
#define ANALYZER_TOOLBAR_H

#include <QToolBar>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QComboBox>
#include <QHBoxLayout>
#include <QFrame>

#include "uft_xcopy_panel.h"  /* For CopyMode enum */

/**
 * @brief Quick analysis result for toolbar display
 */
struct ToolbarAnalysisResult {
    QString platform;
    QString encoding;
    int sectorsPerTrack;
    bool protectionDetected;
    QString protectionName;
    CopyMode recommendedMode;
    int confidence;  // 0-100
};

/**
 * @brief Compact analyzer toolbar widget
 * 
 * Shows analysis results inline:
 * [Platform: Amiga DD] [Protection: Copylock] [Mode: Flux ▼] [Confidence: ████░░] [🔍 Analyze] [✓ Apply]
 */
class AnalyzerToolbar : public QToolBar
{
    Q_OBJECT

public:
    explicit AnalyzerToolbar(QWidget *parent = nullptr);
    ~AnalyzerToolbar() = default;

    /**
     * @brief Set analysis result to display
     */
    void setAnalysisResult(const ToolbarAnalysisResult &result);
    
    /**
     * @brief Clear all displayed data
     */
    void clearResult();
    
    /**
     * @brief Get currently selected copy mode
     */
    CopyMode getSelectedMode() const;
    
    /**
     * @brief Enable/disable apply button
     */
    void setApplyEnabled(bool enabled);
    
    /**
     * @brief Show "analyzing..." state
     */
    void setAnalyzing(bool analyzing);

signals:
    /**
     * @brief Emitted when user clicks "Analyze" button
     */
    void analyzeRequested();
    
    /**
     * @brief Emitted when user clicks "Full Analysis" in dropdown
     */
    void fullAnalysisRequested();
    
    /**
     * @brief Emitted when user clicks "Apply" button
     */
    void applyRequested(CopyMode mode);
    
    /**
     * @brief Emitted when user changes copy mode
     */
    void modeChanged(CopyMode mode);
    
    /**
     * @brief Emitted when user clicks platform label (show details)
     */
    void showDetailsRequested();

public slots:
    /**
     * @brief Update confidence animation
     */
    void updateConfidenceAnimation();

private:
    void setupUi();
    void setupConnections();
    QString getModeIcon(CopyMode mode) const;
    QString getModeName(CopyMode mode) const;
    QColor getConfidenceColor(int confidence) const;

    // UI Components
    QLabel *m_platformLabel;
    QLabel *m_encodingLabel;
    QLabel *m_protectionLabel;
    QComboBox *m_modeCombo;
    QProgressBar *m_confidenceBar;
    QPushButton *m_analyzeBtn;
    QPushButton *m_fullAnalyzeBtn;
    QPushButton *m_applyBtn;
    
    // Separators
    QFrame *m_sep1;
    QFrame *m_sep2;
    QFrame *m_sep3;
    
    // State
    ToolbarAnalysisResult m_result;
    bool m_hasResult;
    bool m_analyzing;
};

#endif /* ANALYZER_TOOLBAR_H */
