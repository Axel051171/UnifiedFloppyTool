/**
 * @file uft_compare_dialog.h
 * @brief Sector Compare Dialog - Compare two disk images sector-by-sector
 *
 * Provides a clean dialog with two file pickers, a large similarity label,
 * a per-sector diff tree, and text/JSON export.
 *
 * @version 1.0.0
 * @date 2026-04-10
 */

#ifndef UFT_COMPARE_DIALOG_H
#define UFT_COMPARE_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QTreeWidget>
#include <QProgressBar>

extern "C" {
#include "uft/analysis/uft_sector_compare.h"
}

class UftCompareDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UftCompareDialog(QWidget *parent = nullptr);

    /** Pre-fill path A (e.g. currently loaded image). */
    void setPathA(const QString &path);

private slots:
    void onBrowseA();
    void onBrowseB();
    void onCompare();
    void onExportText();
    void onExportJson();

private:
    void setupUi();
    void showResults(const uft_compare_result_t *result);

    /* File selection */
    QLineEdit   *m_pathA;
    QLineEdit   *m_pathB;
    QPushButton *m_browseA;
    QPushButton *m_browseB;

    /* Actions */
    QPushButton *m_compareBtn;

    /* Result display */
    QLabel       *m_resultLabel;
    QTreeWidget  *m_diffTree;
    QProgressBar *m_progress;

    /* Export */
    QPushButton *m_exportTextBtn;
    QPushButton *m_exportJsonBtn;

    /* Stored result for export */
    uft_compare_result_t m_lastResult;
    bool m_hasResult;
};

#endif /* UFT_COMPARE_DIALOG_H */
