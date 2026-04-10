/**
 * @file uft_smart_export_dialog.h
 * @brief Smart Export Dialog with Intelligent Format Suggestion
 *
 * Replaces the simple "Save As" with a recommendation-driven export that
 * shows suitability scores, preservation flags, and data-loss warnings
 * for each candidate output format.
 *
 * @version 4.1.0
 */

#ifndef UFT_SMART_EXPORT_DIALOG_H
#define UFT_SMART_EXPORT_DIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>

#ifdef __cplusplus
extern "C" {
#endif
#include "uft/analysis/uft_format_suggest.h"
#ifdef __cplusplus
}
#endif

class UftSmartExportDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Construct a Smart Export dialog
     * @param inputPath      Path to the currently loaded disk image
     * @param hasProtection  True if copy protection was detected
     * @param hasWeakBits    True if weak / fuzzy bits are present
     * @param parent         Parent widget
     */
    explicit UftSmartExportDialog(const QString &inputPath,
                                   bool hasProtection, bool hasWeakBits,
                                   QWidget *parent = nullptr);

    /** @brief Return the format name chosen by the user (empty if cancelled) */
    QString selectedFormat() const;

    /** @brief Return the output file path chosen by the user */
    QString selectedPath() const;

private slots:
    void onFormatSelected(int index);
    void onBrowse();
    void onExport();

private:
    void buildSuggestions();
    QWidget *createFormatCard(const uft_format_suggestion_t &s, bool recommended);

    QListWidget  *m_formatList;
    QLabel       *m_previewLabel;
    QLabel       *m_warningLabel;
    QPushButton  *m_exportBtn;
    QLineEdit    *m_outputPath;

    uft_suggest_result_t m_suggestions;
    QString              m_inputPath;
    bool                 m_hasProtection;
    bool                 m_hasWeakBits;
    QString              m_selectedFormat;
};

#endif /* UFT_SMART_EXPORT_DIALOG_H */
