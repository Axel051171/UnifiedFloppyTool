/**
 * @file uft_recovery_dialog.h
 * @brief Recovery Wizard Dialog - Guided step-by-step disk recovery
 *
 * 5-page QWizard:
 *   1. Assessment   - Quality score + traffic-light triage
 *   2. Strategy     - Choose recovery strategy (ranked by probability)
 *   3. Execute      - Run recovery with per-track progress
 *   4. Verify       - Before/after sector comparison
 *   5. Export       - Format suggestion + save
 *
 * @version 1.0.0
 * @date 2026-04-10
 */

#ifndef UFT_RECOVERY_DIALOG_H
#define UFT_RECOVERY_DIALOG_H

#include <QWizard>
#include <QWizardPage>
#include <QLabel>
#include <QProgressBar>
#include <QRadioButton>
#include <QButtonGroup>
#include <QTreeWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QTimer>

extern "C" {
#include "uft/recovery/uft_recovery_wizard.h"
#include "uft/analysis/uft_format_suggest.h"
}

/* Forward declarations for pages */
class UftRecAssessPage;
class UftRecStrategyPage;
class UftRecExecutePage;
class UftRecVerifyPage;
class UftRecExportPage;

/* ============================================================================
 * Main Wizard
 * ============================================================================ */

class UftRecoveryDialog : public QWizard
{
    Q_OBJECT

public:
    explicit UftRecoveryDialog(const QString &diskPath,
                               QWidget *parent = nullptr);
    ~UftRecoveryDialog();

    /** Access the C wizard state (read-only). */
    const uft_recovery_wizard_t *wizardState() const { return m_wizard; }
    uft_recovery_wizard_t       *wizardState()       { return m_wizard; }

    QString diskPath() const { return m_diskPath; }

    enum PageId {
        Page_Assess,
        Page_Strategy,
        Page_Execute,
        Page_Verify,
        Page_Export
    };

private:
    QString                  m_diskPath;
    uft_recovery_wizard_t   *m_wizard;

    UftRecAssessPage    *m_assessPage;
    UftRecStrategyPage  *m_strategyPage;
    UftRecExecutePage   *m_executePage;
    UftRecVerifyPage    *m_verifyPage;
    UftRecExportPage    *m_exportPage;

    void setupPages();
};

/* ============================================================================
 * Page 1 - Assessment
 * ============================================================================ */

class UftRecAssessPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit UftRecAssessPage(UftRecoveryDialog *wizard);

    void initializePage() override;
    bool isComplete() const override;

private slots:
    void runTriage();

private:
    UftRecoveryDialog *m_wiz;

    /* Traffic light */
    QLabel *m_trafficLight;
    QLabel *m_qualityScore;

    /* Sector counts */
    QLabel *m_sectorsOk;
    QLabel *m_sectorsBad;
    QLabel *m_sectorsWeak;

    QProgressBar *m_progress;
    QLabel       *m_statusLabel;
    bool          m_assessed;
};

/* ============================================================================
 * Page 2 - Strategy Selection
 * ============================================================================ */

class UftRecStrategyPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit UftRecStrategyPage(UftRecoveryDialog *wizard);

    void initializePage() override;
    bool isComplete() const override;

    int selectedStrategyIndex() const;

private:
    UftRecoveryDialog *m_wiz;
    QButtonGroup      *m_group;
    QVBoxLayout       *m_stratLayout;
    QLabel            *m_adviceLabel;

    /* Dynamic radio buttons (re-created each initializePage) */
    QList<QRadioButton *> m_radios;
    QList<QLabel *>       m_probLabels;
};

/* ============================================================================
 * Page 3 - Execute Recovery
 * ============================================================================ */

class UftRecExecutePage : public QWizardPage
{
    Q_OBJECT

public:
    explicit UftRecExecutePage(UftRecoveryDialog *wizard);

    void initializePage() override;
    bool isComplete() const override;

private slots:
    void startRecovery();

private:
    UftRecoveryDialog *m_wiz;

    QProgressBar *m_progress;
    QLabel       *m_trackLabel;
    QLabel       *m_statusLabel;
    QPushButton  *m_cancelBtn;
    bool          m_done;
    bool          m_cancelled;
};

/* ============================================================================
 * Page 4 - Verify
 * ============================================================================ */

class UftRecVerifyPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit UftRecVerifyPage(UftRecoveryDialog *wizard);

    void initializePage() override;

private:
    UftRecoveryDialog *m_wiz;

    QLabel *m_beforeLabel;
    QLabel *m_afterLabel;
    QLabel *m_improvedLabel;
    QLabel *m_crcLabel;
};

/* ============================================================================
 * Page 5 - Export
 * ============================================================================ */

class UftRecExportPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit UftRecExportPage(UftRecoveryDialog *wizard);

    void initializePage() override;

private slots:
    void onExport();

private:
    UftRecoveryDialog *m_wiz;

    QTreeWidget *m_formatTree;
    QPushButton *m_exportBtn;
    QLabel      *m_summaryLabel;
};

#endif /* UFT_RECOVERY_DIALOG_H */
