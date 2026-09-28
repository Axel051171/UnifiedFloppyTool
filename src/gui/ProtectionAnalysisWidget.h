// SPDX-License-Identifier: MIT
/*
 * ProtectionAnalysisWidget.h - Qt Widget for Copy Protection Analysis
 * 
 * Provides visual analysis of copy protection traits on C64/CBM disks:
 *   - Trait detection (weak bits, long tracks, etc.)
 *   - Scheme identification (RapidLok, EA Loader, etc.)
 *   - Track-by-track heatmap visualization
 *   - Multi-revolution comparison
 * 
 * @version 1.0.0
 * @date 2025-01-01
 */

#ifndef PROTECTIONANALYSISWIDGET_H
#define PROTECTIONANALYSISWIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QProgressBar>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QHeaderView>

extern "C" {
#include "uft/core/uft_gcr.h"
#include "uft/protection/ufm_c64_protection_taxonomy.h"
#include "uft/protection/ufm_c64_scheme_detect.h"
#include "uft/protection/ufm_cbm_protection_methods.h"
}

/*============================================================================*
 * TRAIT COLORS
 *============================================================================*/

// Visual severity colors
#define TRAIT_COLOR_NONE        "#2d2d2d"
#define TRAIT_COLOR_LOW         "#4a9e4a"   // Green
#define TRAIT_COLOR_MEDIUM      "#e6b800"   // Yellow
#define TRAIT_COLOR_HIGH        "#e65c00"   // Orange
#define TRAIT_COLOR_CRITICAL    "#cc0000"   // Red

/*============================================================================*
 * WIDGET CLASS
 *============================================================================*/

class ProtectionAnalysisWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ProtectionAnalysisWidget(QWidget *parent = nullptr);
    ~ProtectionAnalysisWidget();

    // Load flux/G64 data for analysis
    void loadFluxData(const uint8_t *data, size_t len);
    void loadG64(const char *path);
    
    // Get analysis results
    int getConfidence() const { return m_confidence; }
    QString getSummary() const { return m_summary; }

signals:
    void analysisComplete(int confidence, const QString &summary);
    void traitDetected(int track, const QString &trait, int severity);

public slots:
    void runAnalysis();
    void clearResults();
    void exportReport();

private slots:
    void onTrackSelected(int row, int col);
    void onSchemeFilterChanged(int index);

private:
    void setupUI();
    void createTraitHeatmap();
    void createSchemePanel();
    void createDetailPanel();
    void updateHeatmap();
    void updateSchemeList();
    void updateDetailView(int track);

    // Results storage
    int m_confidence;
    QString m_summary;
    QVector<ufm_c64_track_metrics_t> m_trackMetrics;
    QVector<ufm_c64_prot_hit_t> m_hits;

    /* MF-1511: je Spur der Anteil gueltiger Kodewoerter unter jedem
     * Codec des Registers, bester zuerst. Gemessen BEIM LADEN — die
     * Rohbits sind nach `g64_free()` fort, und sie nur fuer diese Zahl
     * zu behalten waere ein Vielfaches an Speicher fuer drei Werte.
     * Gleicher Index wie `m_trackMetrics`. */
    QVector<QVector<uft_gcr_erkennung_t>> m_gcrKandidaten;
    
    // UI Elements
    QSplitter *m_mainSplitter;
    
    // Heatmap (Track x Trait matrix)
    QTableWidget *m_heatmapTable;
    QStringList m_traitNames;
    
    // Scheme detection panel
    QGroupBox *m_schemeGroup;
    QTableWidget *m_schemeTable;
    QComboBox *m_schemeFilter;
    QLabel *m_confidenceLabel;
    QProgressBar *m_confidenceBar;
    
    // Detail panel
    QGroupBox *m_detailGroup;
    QTextEdit *m_detailText;
    QComboBox *m_gcrCodec;      /**< MF-1511: Codec-Auswahl der Spur */
    QLabel *m_selectedTrackLabel;
    
    // Controls
    QPushButton *m_analyzeButton;
    QPushButton *m_exportButton;
    QPushButton *m_clearButton;
};

#endif // PROTECTIONANALYSISWIDGET_H
