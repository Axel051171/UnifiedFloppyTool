/**
 * @file protectiontab.cpp
 * @brief Protection Tab Widget - Implementation
 * 
 * P1-GUI-001: Complete backend integration for protection detection settings
 * 
 * @author UFT Team
 * @date 2026-01-05
 */

#include "protectiontab.h"
#include "gui/uft_spurausrichtung.h"
#include "gui/uft_schutz_auswahl.h"
#include <QAbstractButton>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QComboBox>
#include "ui_tab_protection.h"
#include <QMessageBox>

// ============================================================================
// Construction / Destruction
// ============================================================================

ProtectionTab::ProtectionTab(QWidget *parent) 
    : QWidget(parent)
    , ui(new Ui::TabProtection) 
{
    ui->setupUi(this);
    setupDefaults();
    setupConnections();
    verbindeAuswahl();
    loadSettings();
    wendeSperrenAn();   /* P3-635 Weg A, MF-1437 */
}

ProtectionTab::~ProtectionTab() {
    saveSettings();
    delete ui;
}

// ============================================================================
// Backend Integration - Get Configuration
// ============================================================================

void ProtectionTab::getConfig(uft_prot_config_t* config) const {
    if (!config) return;
    
    // Initialize with defaults
    uft_prot_config_init(config);
    
    // Analysis flags
    config->flags = getAnalysisFlags();
    
    // Platform hint
    config->platform_hint = getPlatformHint();
    
    // Thresholds
    config->confidence_threshold = getConfidenceThreshold();
    
    // Track range (0 = all)
    config->start_cylinder = 0;
    config->end_cylinder = 0;
}

void ProtectionTab::setConfig(const uft_prot_config_t* config) {
    if (!config) return;
    
    // Block signals during bulk update
    blockSignals(true);
    
    // Analysis flags → checkboxes
    ui->checkWeakBits->setChecked(config->flags & UFT_PROT_ANAL_WEAK_BITS);
    ui->checkHalfTrack->setChecked(config->flags & UFT_PROT_ANAL_HALF_TRACKS);
    ui->checkC64VarTiming->setChecked(config->flags & UFT_PROT_ANAL_TIMING);
    
    // Platform hint
    ui->checkAutoDetect->setChecked(config->platform_hint == UFT_PLATFORM_UNKNOWN);
    
    // C64-specific
    bool c64Enabled = (config->platform_hint == UFT_PLATFORM_C64 || 
                       config->platform_hint == UFT_PLATFORM_UNKNOWN);
    ui->checkC64Enable->setChecked(c64Enabled);
    syncC64Widgets(c64Enabled);
    
    blockSignals(false);
    emit configChanged();
    wendeSperrenAn();   /* P3-635 Weg A, MF-1437 */
}

uint32_t ProtectionTab::getAnalysisFlags() const {
    uint32_t flags = 0;
    
    // Generic flags
    if (ui->checkWeakBits->isChecked())    flags |= UFT_PROT_ANAL_WEAK_BITS;
    if (ui->checkHalfTrack->isChecked())   flags |= UFT_PROT_ANAL_HALF_TRACKS;
    if (ui->checkLongTrack->isChecked())   flags |= UFT_PROT_ANAL_DEEP;
    if (ui->checkShortTrack->isChecked())  flags |= UFT_PROT_ANAL_DEEP;
    if (ui->checkSyncAnomaly->isChecked()) flags |= UFT_PROT_ANAL_SIGNATURES;
    
    // C64 flags
    if (ui->checkC64Enable->isChecked()) {
        flags |= mapC64Flags();
    }
    
    // Expert mode → deep analysis
    if (ui->checkC64Expert->isChecked() || ui->checkDDExpertMode->isChecked()) {
        flags |= UFT_PROT_ANAL_DEEP | UFT_PROT_ANAL_ALL;
    }
    
    // Quick vs deep
    if (flags == 0) {
        flags = UFT_PROT_ANAL_QUICK;
    }
    
    return flags;
}

uft_platform_t ProtectionTab::getPlatformHint() const {
    if (ui->checkAutoDetect->isChecked()) {
        return UFT_PLATFORM_UNKNOWN;  // Auto-detect
    }
    
    // Check which platform tab is active/enabled
    if (ui->checkC64Enable->isChecked()) {
        return UFT_PLATFORM_C64;
    }
    
    // Check profile combo for platform hint
    QString profile = ui->comboProfile->currentText();
    if (profile.contains("Amiga"))    return UFT_PLATFORM_AMIGA;
    if (profile.contains("C64"))      return UFT_PLATFORM_C64;
    if (profile.contains("Atari"))    return UFT_PLATFORM_ATARI_ST;
    if (profile.contains("Apple"))    return UFT_PLATFORM_APPLE_II;
    if (profile.contains("PC"))       return UFT_PLATFORM_PC_DOS;
    
    return UFT_PLATFORM_UNKNOWN;
}

uint8_t ProtectionTab::getConfidenceThreshold() const {
    // Default 70, could be linked to a slider if added
    return 70;
}

bool ProtectionTab::isAutoDetectEnabled() const {
    return ui->checkAutoDetect->isChecked();
}

bool ProtectionTab::isPreserveEnabled() const {
    return ui->checkPreserveProtection->isChecked();
}

bool ProtectionTab::isReportEnabled() const {
    return ui->checkReportProtection->isChecked();
}

// ============================================================================
// Slots - Profile Management
// ============================================================================

void ProtectionTab::onProfileChanged(int index) {
    QString profileName = ui->comboProfile->itemText(index);
    applyProfile(profileName);
    emit profileChanged(profileName);
}

void ProtectionTab::applyProfile(const QString& profileName) {
    blockSignals(true);
    
    // Reset all
    resetDefaults();
    
    if (profileName.contains("Amiga Standard")) {
        ui->checkXCopyEnable->setChecked(true);
        ui->checkErr1->setChecked(true);
        ui->checkErr2->setChecked(true);
        ui->checkWeakBits->setChecked(true);
    }
    else if (profileName.contains("Amiga Advanced")) {
        ui->checkXCopyEnable->setChecked(true);
        ui->checkDDEnable->setChecked(true);
        ui->checkDDExpertMode->setChecked(true);
        ui->checkWeakBits->setChecked(true);
        ui->checkLongTrack->setChecked(true);
    }
    else if (profileName.contains("C64 Standard")) {
        ui->checkC64Enable->setChecked(true);
        ui->checkC64WeakBits->setChecked(true);
        syncC64Widgets(true);
    }
    else if (profileName.contains("C64 Advanced")) {
        ui->checkC64Enable->setChecked(true);
        ui->checkC64Expert->setChecked(true);
        ui->checkC64WeakBits->setChecked(true);
        ui->checkC64VarTiming->setChecked(true);
        /* MF-1436: checkC64Alignment bleibt aus - kein Erkenner (P3-602) */
        ui->checkHalfTrack->setChecked(true);
        syncC64Widgets(true);
    }
    else if (profileName.contains("Atari")) {
        ui->checkWeakBits->setChecked(true);
        ui->checkBadCRC->setChecked(true);
        ui->checkLongTrack->setChecked(true);
    }
    else if (profileName.contains("Apple")) {
        ui->checkWeakBits->setChecked(true);
        ui->checkSyncAnomaly->setChecked(true);
    }
    else if (profileName.contains("PC DOS")) {
        ui->checkWeakBits->setChecked(true);
        ui->checkBadCRC->setChecked(true);
    }
    
    blockSignals(false);
    emit configChanged();
    wendeSperrenAn();   /* P3-635 Weg A, MF-1437 */
}

void ProtectionTab::onSaveProfile() {
    QString name = ui->comboProfile->currentText();
    if (name.isEmpty() || !name.contains("Custom")) {
        QMessageBox::warning(this, "Save Profile", 
            "Please select 'Custom (User-Defined)' to save settings.");
        return;
    }
    saveSettings();
    QMessageBox::information(this, "Save Profile", "Profile saved successfully.");
}

void ProtectionTab::onLoadProfile() {
    loadSettings();
}

void ProtectionTab::onDeleteProfile() {
    // Only custom profiles can be deleted
    QMessageBox::information(this, "Delete Profile", 
        "Built-in profiles cannot be deleted.");
}

// ============================================================================
// Slots - Detection Settings
// ============================================================================

void ProtectionTab::onAutoDetectToggled(bool checked) {
    // When auto-detect is on, enable all platform tabs
    if (checked) {
        ui->checkC64Enable->setEnabled(true);
        ui->checkXCopyEnable->setEnabled(true);
        ui->checkDDEnable->setEnabled(true);
    }
    emit configChanged();
    wendeSperrenAn();   /* P3-635 Weg A, MF-1437 */
}

void ProtectionTab::onPreserveToggled(bool /*checked*/) {
    emit configChanged();
}

void ProtectionTab::onC64EnableToggled(bool checked) {
    syncC64Widgets(checked);
    emit configChanged();
    if (checked) {
        emit platformChanged(UFT_PLATFORM_C64);
    }
}

void ProtectionTab::onC64ExpertToggled(bool checked) {
    ui->groupC64ExpertParams->setEnabled(checked);
    emit configChanged();
    wendeSperrenAn();   /* P3-635 Weg A, MF-1437 */
}

void ProtectionTab::onDDEnableToggled(bool checked) {
    syncDDWidgets(checked);
    emit configChanged();
}

void ProtectionTab::onDDExpertToggled(bool checked) {
    ui->groupDDExpert->setEnabled(checked);
    emit configChanged();
    wendeSperrenAn();   /* P3-635 Weg A, MF-1437 */
}

void ProtectionTab::onXCopyEnableToggled(bool checked) {
    syncXCopyWidgets(checked);
    emit configChanged();
}

void ProtectionTab::onAnyCheckboxChanged() {
    emit configChanged();
}

void ProtectionTab::onSliderChanged(int /*value*/) {
    emit configChanged();
}

void ProtectionTab::onSpinboxChanged(int /*value*/) {
    emit configChanged();
}

// ============================================================================
// Settings Persistence
// ============================================================================

void ProtectionTab::loadSettings() {
    QSettings settings;
    settings.beginGroup(SETTINGS_GROUP);
    
    // Profile
    int profileIdx = settings.value("profile", 0).toInt();
    if (profileIdx >= 0 && profileIdx < ui->comboProfile->count()) {
        ui->comboProfile->setCurrentIndex(profileIdx);
    }
    
    // Detection flags
    ui->checkAutoDetect->setChecked(settings.value("autoDetect", true).toBool());
    ui->checkPreserveProtection->setChecked(settings.value("preserve", true).toBool());
    ui->checkReportProtection->setChecked(settings.value("report", true).toBool());
    ui->checkLogDetails->setChecked(settings.value("logDetails", false).toBool());
    
    // Generic flags
    ui->checkWeakBits->setChecked(settings.value("weakBits", true).toBool());
    /* MF-1437: Filter der laufenden Analyse, eigener Schluessel, voreingestellt
     * "zeigen" (uft_schutz_auswahl.h) — nicht der alte "halfTrack" (false). */
    ui->checkHalfTrack->setChecked(settings.value("zeigeHalbspur", true).toBool());
    ui->checkLongTrack->setChecked(settings.value("longTrack", false).toBool());
    ui->checkShortTrack->setChecked(settings.value("shortTrack", false).toBool());
    ui->checkBadCRC->setChecked(settings.value("badCRC", true).toBool());
    ui->checkSyncAnomaly->setChecked(settings.value("syncAnomaly", true).toBool());
    
    // C64 flags
    ui->checkC64Enable->setChecked(settings.value("c64Enable", false).toBool());
    ui->checkC64Expert->setChecked(settings.value("c64Expert", false).toBool());
    ui->checkC64WeakBits->setChecked(settings.value("c64WeakBits", true).toBool());
    ui->checkC64VarTiming->setChecked(settings.value("c64VarTiming", true).toBool());
    uftSperreSpurausrichtung(ui->checkC64Alignment);  /* MF-1436: kein Erkenner, ein gespeicherter Haken gilt nicht */
    ui->checkC64SectorCount->setChecked(settings.value("c64SectorCount", true).toBool());
    
    settings.endGroup();
    
    // Update dependent widgets
    syncC64Widgets(ui->checkC64Enable->isChecked());
    syncDDWidgets(ui->checkDDEnable->isChecked());
    syncXCopyWidgets(ui->checkXCopyEnable->isChecked());
}

void ProtectionTab::saveSettings() {
    QSettings settings;
    settings.beginGroup(SETTINGS_GROUP);
    
    // Profile
    settings.setValue("profile", ui->comboProfile->currentIndex());
    
    // Detection flags
    settings.setValue("autoDetect", ui->checkAutoDetect->isChecked());
    settings.setValue("preserve", ui->checkPreserveProtection->isChecked());
    settings.setValue("report", ui->checkReportProtection->isChecked());
    settings.setValue("logDetails", ui->checkLogDetails->isChecked());
    
    // Generic flags
    settings.setValue("weakBits", ui->checkWeakBits->isChecked());
    settings.setValue("zeigeHalbspur", ui->checkHalfTrack->isChecked());  /* MF-1437 */
    settings.setValue("longTrack", ui->checkLongTrack->isChecked());
    settings.setValue("shortTrack", ui->checkShortTrack->isChecked());
    settings.setValue("badCRC", ui->checkBadCRC->isChecked());
    settings.setValue("syncAnomaly", ui->checkSyncAnomaly->isChecked());
    
    // C64 flags
    settings.setValue("c64Enable", ui->checkC64Enable->isChecked());
    settings.setValue("c64Expert", ui->checkC64Expert->isChecked());
    settings.setValue("c64WeakBits", ui->checkC64WeakBits->isChecked());
    settings.setValue("c64VarTiming", ui->checkC64VarTiming->isChecked());
    settings.setValue("c64Alignment", ui->checkC64Alignment->isChecked());
    settings.setValue("c64SectorCount", ui->checkC64SectorCount->isChecked());
    
    settings.endGroup();
    settings.sync();
}

void ProtectionTab::resetDefaults() {
    ui->checkAutoDetect->setChecked(true);
    ui->checkPreserveProtection->setChecked(true);
    ui->checkReportProtection->setChecked(true);
    ui->checkLogDetails->setChecked(false);
    
    ui->checkWeakBits->setChecked(true);
    ui->checkHalfTrack->setChecked(true);   /* MF-1437: Filter, voreingestellt "zeigen" */
    ui->checkLongTrack->setChecked(false);
    ui->checkShortTrack->setChecked(false);
    ui->checkBadCRC->setChecked(true);
    ui->checkSyncAnomaly->setChecked(true);
    
    ui->checkC64Enable->setChecked(false);
    ui->checkC64Expert->setChecked(false);
    ui->checkDDEnable->setChecked(false);
    ui->checkDDExpertMode->setChecked(false);
    ui->checkXCopyEnable->setChecked(false);
    
    syncC64Widgets(false);
    syncDDWidgets(false);
    syncXCopyWidgets(false);
}

// ============================================================================
// Internal Helpers
// ============================================================================

void ProtectionTab::setupConnections() {
    // Profile combo
    connect(ui->comboProfile, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ProtectionTab::onProfileChanged);
    connect(ui->btnSaveProfile, &QPushButton::clicked, this, &ProtectionTab::onSaveProfile);
    connect(ui->btnLoadProfile, &QPushButton::clicked, this, &ProtectionTab::onLoadProfile);
    connect(ui->btnDeleteProfile, &QPushButton::clicked, this, &ProtectionTab::onDeleteProfile);
    
    // Detection checkboxes
    connect(ui->checkAutoDetect, &QCheckBox::toggled, this, &ProtectionTab::onAutoDetectToggled);
    connect(ui->checkPreserveProtection, &QCheckBox::toggled, this, &ProtectionTab::onPreserveToggled);
    connect(ui->checkReportProtection, &QCheckBox::toggled, this, &ProtectionTab::onAnyCheckboxChanged);
    connect(ui->checkLogDetails, &QCheckBox::toggled, this, &ProtectionTab::onAnyCheckboxChanged);
    
    // Platform enables
    connect(ui->checkC64Enable, &QCheckBox::toggled, this, &ProtectionTab::onC64EnableToggled);
    connect(ui->checkC64Expert, &QCheckBox::toggled, this, &ProtectionTab::onC64ExpertToggled);
    connect(ui->checkDDEnable, &QCheckBox::toggled, this, &ProtectionTab::onDDEnableToggled);
    connect(ui->checkDDExpertMode, &QCheckBox::toggled, this, &ProtectionTab::onDDExpertToggled);
    connect(ui->checkXCopyEnable, &QCheckBox::toggled, this, &ProtectionTab::onXCopyEnableToggled);
    
    // Generic flags
    connect(ui->checkWeakBits, &QCheckBox::toggled, this, &ProtectionTab::onAnyCheckboxChanged);
    connect(ui->checkHalfTrack, &QCheckBox::toggled, this, &ProtectionTab::onAnyCheckboxChanged);
    connect(ui->checkLongTrack, &QCheckBox::toggled, this, &ProtectionTab::onAnyCheckboxChanged);
    connect(ui->checkShortTrack, &QCheckBox::toggled, this, &ProtectionTab::onAnyCheckboxChanged);
    connect(ui->checkBadCRC, &QCheckBox::toggled, this, &ProtectionTab::onAnyCheckboxChanged);
    connect(ui->checkSyncAnomaly, &QCheckBox::toggled, this, &ProtectionTab::onAnyCheckboxChanged);
    
    // C64 flags
    connect(ui->checkC64WeakBits, &QCheckBox::toggled, this, &ProtectionTab::onAnyCheckboxChanged);
    connect(ui->checkC64VarTiming, &QCheckBox::toggled, this, &ProtectionTab::onAnyCheckboxChanged);
    connect(ui->checkC64Alignment, &QCheckBox::toggled, this, &ProtectionTab::onAnyCheckboxChanged);
    connect(ui->checkC64SectorCount, &QCheckBox::toggled, this, &ProtectionTab::onAnyCheckboxChanged);
}

/* P3-635 Weg A (MF-1437).
 *
 * Gemessen: 54 Bedienelemente, keines erreichte eine Analyse. Die EINZIGE
 * Eingabe mit einem Merkmal in der laufenden Analyse ist "Enable Half-Track
 * Detection" (Tafel in uft_schutz_auswahl.h). Sie und die Profilsteuerung
 * (die sie setzt, speichert und laedt) bleiben bedienbar; alles andere
 * wird abgeschaltet und sagt warum.
 *
 * Die Menge "alles andere" kommt aus dem Formular (findChildren), nicht
 * aus einer gepflegten Liste — ein neues Kaestchen ohne Leser ist damit
 * von selbst gesperrt (Grundsatz MF-636). Gerufen am Ende jeder Funktion,
 * die Elemente an- oder abschaltet, weil die alten sync*-Funktionen
 * einzelne Elemente direkt wieder einschalten. */
void ProtectionTab::wendeSperrenAn() {
    QStringList verdrahtet = { QStringLiteral("comboProfile"),
                               QStringLiteral("btnSaveProfile"),
                               QStringLiteral("btnLoadProfile"),
                               QStringLiteral("btnDeleteProfile") };
    for (int i = 0; i < UFT_SCHUTZ_AUSWAHL_ANZAHL; i++)
        verdrahtet << QString::fromLatin1(UFT_SCHUTZ_AUSWAHL[i].kaestchen);

    /* 1. Verdrahtete samt ihren Behaeltern einschalten */
    for (const QString &n : verdrahtet) {
        QWidget *w = findChild<QWidget *>(n);
        for (QWidget *p = w; p && p != this; p = p->parentWidget())
            p->setEnabled(true);
    }

    /* 2. Jede andere Eingabe ausschalten */
    const QString anzeige = tr(
        "Ergebnisanzeige ohne Quelle: dieser Reiter fuehrt keine Analyse aus. "
        "Die Schutzanalyse laeuft im Status-Reiter (P3-635).");
    const QString ohneLeser = tr(
        "Ohne Wirkung: keine Analyse im Baum liest diese Einstellung. "
        "Wirksam ist hier nur \"Enable Half-Track Detection\" (P3-635).");
    for (QWidget *e : findChildren<QWidget *>()) {
        if (!qobject_cast<QAbstractButton *>(e) && !qobject_cast<QComboBox *>(e) &&
            !qobject_cast<QAbstractSpinBox *>(e) && !qobject_cast<QAbstractSlider *>(e))
            continue;
        const QString n = e->objectName();
        if (n.isEmpty() || n.startsWith(QLatin1String("qt_")) || verdrahtet.contains(n))
            continue;
        e->setEnabled(false);
        if (e == ui->checkC64Alignment)
            continue;                          /* behaelt seinen Grund (P3-602) */
        bool inAnzeige = false;
        for (QWidget *p = e->parentWidget(); p && p != this; p = p->parentWidget())
            if (p == ui->groupFlags) inAnzeige = true;
        e->setToolTip(inAnzeige ? anzeige : ohneLeser);
    }
}

/* Die Wahl muss die laufende Analyse SOFORT erreichen — gespeichert wurde
 * bisher nur im Destruktor, und das Fenster lebt bis zum Programmende. */
void ProtectionTab::verbindeAuswahl() {
    for (int i = 0; i < UFT_SCHUTZ_AUSWAHL_ANZAHL; i++) {
        auto *k = findChild<QCheckBox *>(QString::fromLatin1(UFT_SCHUTZ_AUSWAHL[i].kaestchen));
        if (!k) continue;
        const QString schluessel = QString::fromLatin1(UFT_SCHUTZ_AUSWAHL[i].schluessel);
        connect(k, &QCheckBox::toggled, this, [schluessel](bool an) {
            QSettings s;
            s.beginGroup(QStringLiteral(UFT_SCHUTZ_AUSWAHL_GRUPPE));
            s.setValue(schluessel, an);
            s.endGroup();
            s.sync();
        });
    }
}

void ProtectionTab::setupDefaults() {
    // Ensure expert groups are initially disabled
    ui->groupC64ExpertParams->setEnabled(false);
    ui->groupDDExpert->setEnabled(false);
    uftSperreSpurausrichtung(ui->checkC64Alignment);
}

void ProtectionTab::syncC64Widgets(bool enabled) {
    ui->groupGCR->setEnabled(enabled);
    ui->groupHalfTrack->setEnabled(enabled);
    ui->groupC64Protection->setEnabled(enabled);
    ui->groupC64Output->setEnabled(enabled);
    ui->checkC64Expert->setEnabled(enabled);
    ui->groupC64ExpertParams->setEnabled(enabled && ui->checkC64Expert->isChecked());
    wendeSperrenAn();   /* P3-635 Weg A, MF-1437 */
}

void ProtectionTab::syncDDWidgets(bool enabled) {
    ui->checkDD1->setEnabled(enabled);
    ui->checkDD2->setEnabled(enabled);
    ui->checkDD3->setEnabled(enabled);
    ui->checkDD4->setEnabled(enabled);
    ui->checkDD5->setEnabled(enabled);
    ui->checkDDExpertMode->setEnabled(enabled);
    ui->groupDDExpert->setEnabled(enabled && ui->checkDDExpertMode->isChecked());
    wendeSperrenAn();   /* P3-635 Weg A, MF-1437 */
}

void ProtectionTab::syncXCopyWidgets(bool enabled) {
    ui->checkErr1->setEnabled(enabled);
    ui->checkErr2->setEnabled(enabled);
    ui->checkErr3->setEnabled(enabled);
    ui->checkErr4->setEnabled(enabled);
    ui->checkErr5->setEnabled(enabled);
    ui->checkErr6->setEnabled(enabled);
    ui->checkErr7->setEnabled(enabled);
    ui->checkErr8->setEnabled(enabled);
    wendeSperrenAn();   /* P3-635 Weg A, MF-1437 */
}

uint32_t ProtectionTab::mapC64Flags() const {
    uint32_t flags = 0;
    
    if (ui->checkC64WeakBits->isChecked())  flags |= UFT_PROT_ANAL_WEAK_BITS;
    if (ui->checkC64VarTiming->isChecked()) flags |= UFT_PROT_ANAL_TIMING;
    if (ui->checkHalfTrack->isChecked())    flags |= UFT_PROT_ANAL_HALF_TRACKS;
    if (ui->checkC64Expert->isChecked())    flags |= UFT_PROT_ANAL_DEEP | UFT_PROT_ANAL_SIGNATURES;
    
    return flags;
}

uint32_t ProtectionTab::mapAmigaFlags() const {
    uint32_t flags = 0;
    
    if (ui->checkXCopyEnable->isChecked()) flags |= UFT_PROT_ANAL_WEAK_BITS;
    if (ui->checkDDEnable->isChecked())    flags |= UFT_PROT_ANAL_DEEP;
    if (ui->checkLongTrack->isChecked())   flags |= UFT_PROT_ANAL_TIMING;
    
    return flags;
}

uint32_t ProtectionTab::mapAtariFlags() const {
    uint32_t flags = UFT_PROT_ANAL_WEAK_BITS;
    
    if (ui->checkBadCRC->isChecked())    flags |= UFT_PROT_ANAL_SIGNATURES;
    if (ui->checkLongTrack->isChecked()) flags |= UFT_PROT_ANAL_TIMING;
    
    return flags;
}

uint32_t ProtectionTab::mapGenericFlags() const {
    uint32_t flags = 0;
    
    if (ui->checkWeakBits->isChecked())    flags |= UFT_PROT_ANAL_WEAK_BITS;
    if (ui->checkHalfTrack->isChecked())   flags |= UFT_PROT_ANAL_HALF_TRACKS;
    if (ui->checkLongTrack->isChecked())   flags |= UFT_PROT_ANAL_TIMING;
    if (ui->checkShortTrack->isChecked())  flags |= UFT_PROT_ANAL_TIMING;
    if (ui->checkBadCRC->isChecked())      flags |= UFT_PROT_ANAL_SIGNATURES;
    if (ui->checkSyncAnomaly->isChecked()) flags |= UFT_PROT_ANAL_SIGNATURES;
    
    return flags;
}

QString ProtectionTab::currentProfileName() const {
    return ui->comboProfile->currentText();
}

void ProtectionTab::populateProfileCombo() {
    // Already populated in .ui file
}

QStringList ProtectionTab::savedProfiles() const {
    QSettings settings;
    settings.beginGroup(SETTINGS_GROUP);
    QStringList profiles = settings.childGroups();
    settings.endGroup();
    return profiles;
}
