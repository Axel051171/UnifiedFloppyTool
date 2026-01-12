/**
 * @file settingsdialog.h
 * @brief Settings Dialog mit Theme-Auswahl
 */

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog() override = default;

private slots:
    void onThemeChanged(int index);
    void onApplyClicked();
    void onResetClicked();

private:
    void setupUI();
    void loadSettings();
    void saveSettings();

    // Appearance
    QComboBox* m_cmbTheme = nullptr;
    QCheckBox* m_chkAnimations = nullptr;
    
    // Performance
    QSpinBox* m_spnThreads = nullptr;
    QCheckBox* m_chkSIMD = nullptr;
    
    // Buttons
    QPushButton* m_btnApply = nullptr;
    QPushButton* m_btnReset = nullptr;
    QPushButton* m_btnClose = nullptr;
};

#endif // SETTINGSDIALOG_H
