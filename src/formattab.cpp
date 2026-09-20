/**
 * @file formattab.cpp
 * @brief Format/Settings Tab Widget - Complete Implementation
 * 
 * ALL UI Dependencies + 25+ System/Format Mappings
 * 
 * @author UFT Team
 * @date 2026-01-12
 */

#include "formattab.h"
#include "ui_tab_format.h"
#include "uft_gw2dmk_panel.h"
#include <uft/uft_format_plugin.h>  /* MF-661: Faehigkeits-Manifest */
#include <uft/uft_format_probe.h>   /* MF-1231: uft_format_variant_t */
#include <QDebug>
#include <QFileDialog>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QDialog>
#include <QVBoxLayout>
#include <QPushButton>          /* MF-1237 */
#include <QStandardItemModel>    /* MF-1237: Eintraege sperren */
#include <QPlainTextEdit>        /* MF-1238: JSON-Ansicht */
#include <QClipboard>            /* MF-1238 */
#include <QGuiApplication>       /* MF-1238 */
#include <QMessageBox>           /* MF-1238 */
#include <QGridLayout>           /* MF-1282: Beschriftung links im Gitter */
#include <QFormLayout>           /* MF-1282: Beschriftung in der LabelRole */
#include <QBoxLayout>            /* MF-1282: Beschriftung davor im Kasten */
#include <QLabel>                /* MF-1282 */

// ============================================================================
// Construction / Destruction
// ============================================================================

FormatTab::FormatTab(QWidget *parent) 
    : QWidget(parent)
    , ui(new Ui::TabFormat)

{
    ui->setupUi(this);
    
    setupFormatDatabase();
    
    // Initialize presets
    setupBuiltinPresets();
    loadPresetsFromFile();
    setupConnections();
    
    // Populate system combo with all systems
    populateSystemCombo();
    
    // Initialize with first system
    onSystemChanged(0);
    setupInitialState();
    loadSettings();
    /* MF-1237: der Reiter oeffnet in einem BENANNTEN Modus, nicht in
     * einer beliebigen Achsenstellung. StandardCopy ist der Modus ohne
     * Anspruch — er verlangt keine Faehigkeit und gilt fuer jedes
     * Format (gemessen: `braucht == 0`, `behelf_formate == NULL`). */
    wendeProfilAn(QStringLiteral("standardcopy"));
    /* MF-1233: der Plan gilt ab dem ersten Bild, nicht erst nach der
     * ersten Aenderung. Ein Reiter, der die Regeln erst beim Anfassen
     * anwendet, zeigt beim Oeffnen einen Zustand, den der Plan nicht
     * erlaubt. */
    applyCopyPlan();

    /* MF-1265 (`P3-509`, zweiter Halbsatz): der Reiter MELDET seinen
     * Plan beim Kern an, statt dass andere Reiter ihn befragen — eine
     * FUNKTION und kein hinterlegter Wert, damit die Auskunft nicht
     * veralten kann, sobald der Bediener etwas umstellt.
     *
     * ZULETZT, nicht als erstes: `copyPlan()` liest die vier
     * Auswahlfelder, und vor `setupUi()` gibt es die nicht. Eine
     * Anmeldung am Anfang des Konstruktors waere eine Auskunft ueber
     * einen halb gebauten Reiter.
     *
     * Gemessen gibt es genau EINE Instanz: `git grep "new FormatTab"`
     * hat einen Treffer. */
    uft_copy_plan_set_quelle(
        [](void *ctx) -> uft_copy_plan_t {
            return static_cast<FormatTab *>(ctx)->copyPlan();
        },
        this);
}

FormatTab::~FormatTab() {
    /* MF-1265: abmelden, BEVOR irgendetwas freigegeben wird. Ein
     * Zeiger auf einen halb abgebauten Reiter ist die einzige Gefahr
     * dieser Bauform — der Rotbeweis zielt genau darauf. */
    uft_copy_plan_set_quelle(nullptr, nullptr);
    saveSettings();
    delete ui;
}



// ============================================================================
// Format Database Setup - 25+ Systems, 100+ Formats
// ============================================================================

void FormatTab::setupFormatDatabase() {
    // ========================================================================
    // COMMODORE FAMILY
    // ========================================================================
    // MF-635: fuenf Eintraege sind hier gestrichen — NIB, NBZ, P64, X64, T64,
    // TAP. Gemessen gegen die Plugin-SSOT (scripts/gen_format_list.py, 88
    // ausgeschriebene + 49 DSK_PLUGIN-Auspraegungen):
    //
    //   NBZ, P64, X64, T64, TAP  -> kein Plugin, weder ueber Namen noch
    //                               ueber Dateiendung
    //   NIB                      -> es gibt ein Plugin dieses Namens, aber es
    //                               ist src/formats/nib/uft_nib.c, "Apple II
    //                               Nibble", dessen Probe exakt 232960 Byte
    //                               verlangt (35 x 6656). Eine Commodore-
    //                               MNIB-Datei trifft sie nie.
    //
    // Der ausgewaehlte Text wandert als `p.format` weiter (:1137), also war
    // eine Beschriftung wie "NIB (nicht lesbar)" keine Option — sie haette
    // den Wert verfaelscht. Wer eine dieser Dateien oeffnen will, bekommt
    // jetzt keinen Eintrag statt eines Versprechens.
    //
    // Der C64-NIB-Leser existierte (src/formats/c64/uft_nib_format.c, 1100
    // Zeilen) und war unerreichbar; er ist in MF-635 geloescht. Der Neubau
    // kommt mit dem Commodore-NIB-Baustein, Oracle-first gegen nibscan.
    m_systemFormats["Commodore 64/128"] = {
        "D64", "G64", "D71", "D81"
    };
    
    m_systemFormats["Commodore Plus/4"] = {
        "D64", "D71", "TAP"
    };
    
    m_systemFormats["Commodore VIC-20"] = {
        "D64", "TAP", "PRG"
    };
    
    m_systemFormats["Commodore PET/CBM"] = {
        "D64", "D80", "D82", "D67"
    };
    
    // ========================================================================
    // AMIGA
    // ========================================================================
    m_systemFormats["Amiga"] = {
        "ADF", "ADZ", "HDF", "DMS", "IPF"
    };
    
    // ========================================================================
    // APPLE FAMILY
    // ========================================================================
    m_systemFormats["Apple II"] = {
        "WOZ", "A2R", "NIB", "PO", "DO", "2IMG", "DSK", "D13"
    };
    
    m_systemFormats["Apple III"] = {
        "PO", "2IMG", "DSK"
    };
    
    m_systemFormats["Macintosh (400K/800K)"] = {
        "DC42", "IMG", "DART"
    };
    
    // ========================================================================
    // ATARI FAMILY
    // ========================================================================
    m_systemFormats["Atari ST/STE"] = {
        "ST", "STX", "MSA", "DIM", "STT", "IPF"
    };
    
    m_systemFormats["Atari 8-bit (400/800/XL/XE)"] = {
        "ATR", "ATX", "XFD", "DCM", "PRO", "XEX"
    };
    
    // ========================================================================
    // SINCLAIR / SPECTRUM
    // ========================================================================
    m_systemFormats["ZX Spectrum"] = {
        "TRD", "SCL", "TZX", "TAP", "DSK", "FDI", "TD0", "UDI", "OPD", "MGT"
    };
    
    m_systemFormats["SAM Coupé"] = {
        "MGT", "SAD", "DSK"
    };
    
    // ========================================================================
    // AMSTRAD
    // ========================================================================
    m_systemFormats["Amstrad CPC"] = {
        "DSK", "EDSK", "RAW", "IPF", "SCP"
    };
    
    m_systemFormats["Amstrad PCW"] = {
        "DSK", "EDSK", "IMG"
    };
    
    // ========================================================================
    // MSX
    // ========================================================================
    m_systemFormats["MSX"] = {
        "DSK", "DMK", "IMG", "DIM"
    };
    
    // ========================================================================
    // BBC / ACORN
    // ========================================================================
    m_systemFormats["BBC Micro"] = {
        "SSD", "DSD", "ADF", "ADL", "UEF"
    };
    
    m_systemFormats["Acorn Archimedes"] = {
        "ADF", "ADL"
    };
    
    // ========================================================================
    // PC / DOS
    // ========================================================================
    m_systemFormats["PC/DOS"] = {
        "IMG", "IMA", "XDF", "DMF", "2M", "TD0", "IMD", "CQM", 
        "360K", "720K", "1.2M", "1.44M", "2.88M", "86F"
    };
    
    // ========================================================================
    // JAPANESE SYSTEMS
    // ========================================================================
    m_systemFormats["NEC PC-98"] = {
        "D88", "D77", "NFD", "FDI", "HDM", "XDF"
    };
    
    m_systemFormats["Sharp X68000"] = {
        "XDF", "DIM", "D88"
    };
    
    m_systemFormats["FM Towns"] = {
        "D88", "D77", "IMG"
    };
    
    // ========================================================================
    // TRS-80
    // ========================================================================
    m_systemFormats["TRS-80 (Model I/III/4)"] = {
        "DMK", "JV1", "JV3", "DSK", "IMD"
    };
    
    m_systemFormats["TRS-80 Color Computer"] = {
        "VDK", "DSK", "DMK", "JVC"
    };
    
    // ========================================================================
    // TEXAS INSTRUMENTS
    // ========================================================================
    m_systemFormats["TI-99/4A"] = {
        "DSK", "V9T9", "PC99"
    };
    
    // ========================================================================
    // FRENCH SYSTEMS
    // ========================================================================
    m_systemFormats["Thomson MO/TO"] = {
        "SAP", "HFE"
    };
    
    m_systemFormats["Oric Atmos"] = {
        "DSK", "TAP"
    };
    
    // ========================================================================
    // CP/M SYSTEMS
    // ========================================================================
    m_systemFormats["Kaypro"] = {
        "IMG", "TD0", "IMD", "DSK"
    };
    
    m_systemFormats["Osborne"] = {
        "IMG", "TD0", "IMD"
    };
    
    m_systemFormats["North Star"] = {
        "NorthStar", "IMG", "TD0"
    };
    
    // ========================================================================
    // DEC
    // ========================================================================
    m_systemFormats["DEC PDP/VAX"] = {
        "IMG", "TD0", "IMD"
    };
    
    // ========================================================================
    // OTHER SYSTEMS
    // ========================================================================
    m_systemFormats["Heathkit/Zenith"] = {
        "IMG", "TD0", "IMD"
    };
    
    m_systemFormats["Victor 9000"] = {
        "IMG", "TD0", "SCP"
    };
    
    // ========================================================================
    // FLUX / RAW FORMATS
    // ========================================================================
    m_systemFormats["Flux (raw)"] = {
        "SCP", "HFE", "RAW", "KFRAW", "GWRAW", "A2R", "WOZ", "IPF", "FDI", "MFM", "MFI", "86F"
    };

    // ========================================================================
    // Format Info Database (Key formats with metadata)
    // ========================================================================
    
    // Commodore
    m_formatInfo["D64"] = {"D64", "C64/1541 Disk Image", {"Standard", "35 Track", "40 Track", "42 Track"},
        false, false, false, true, false, 35, 21, 256};
    m_formatInfo["G64"] = {"G64", "C64 GCR Disk Image", {"Standard", "Extended"},
        true, true, false, true, false, 42, 21, 256};
    m_formatInfo["D71"] = {"D71", "C128/1571 Disk Image", {"Standard"},
        false, false, false, true, false, 70, 21, 256};
    m_formatInfo["D81"] = {"D81", "C128/1581 Disk Image", {"Standard"},
        false, false, false, false, true, 80, 10, 512};
    m_formatInfo["NIB"] = {"NIB", "Nibble Image", {"C64", "Apple"},
        true, true, false, true, false, 35, 0, 0};
        
    // Amiga
    m_formatInfo["ADF"] = {"ADF", "Amiga Disk File", {"DD (880K)", "HD (1.76M)"},
        false, false, false, false, true, 80, 11, 512};
    m_formatInfo["IPF"] = {"IPF", "Interchangeable Preservation Format", {"Standard"},
        true, true, true, true, true, 84, 0, 0};
        
    // Apple
    m_formatInfo["WOZ"] = {"WOZ", "Apple II Flux Image", {"WOZ 1.0", "WOZ 2.0"},
        true, true, true, true, false, 35, 16, 256};
    m_formatInfo["A2R"] = {"A2R", "Applesauce Flux", {"Standard"},
        true, true, true, true, false, 35, 16, 256};
    m_formatInfo["PO"] = {"PO", "ProDOS Order", {"140K", "800K"},
        false, false, false, true, false, 35, 16, 256};
        
    // Atari
    m_formatInfo["ST"] = {"ST", "Atari ST Sector Image", {"SS/DD", "DS/DD", "DS/HD"},
        false, false, false, false, true, 80, 9, 512};
    m_formatInfo["STX"] = {"STX", "Atari ST Extended", {"Standard"},
        true, true, false, false, true, 80, 9, 512};
    m_formatInfo["ATR"] = {"ATR", "Atari 8-bit Image", {"SD (90K)", "ED (130K)", "DD (180K)"},
        false, false, false, false, true, 40, 18, 128};
        
    // Spectrum
    m_formatInfo["TRD"] = {"TRD", "TR-DOS Image", {"DS/DD 640K", "SS/DD 320K"},
        false, false, false, false, true, 80, 16, 256};
    m_formatInfo["SCL"] = {"SCL", "Sinclair Container", {"Standard"},
        false, false, false, false, true, 0, 0, 0};
    m_formatInfo["TZX"] = {"TZX", "ZX Spectrum Tape", {"Standard"},
        false, false, false, false, false, 0, 0, 0};
        
    // Amstrad
    m_formatInfo["DSK"] = {"DSK", "Amstrad/Spectrum DSK", {"Standard", "Extended (EDSK)"},
        false, false, false, false, true, 40, 9, 512};
    m_formatInfo["EDSK"] = {"EDSK", "Extended DSK", {"Standard"},
        true, true, false, false, true, 42, 9, 512};
        
    // PC
    m_formatInfo["IMG"] = {"IMG", "Raw Sector Image", {"360K", "720K", "1.2M", "1.44M", "2.88M"},
        false, false, false, false, true, 80, 18, 512};
    m_formatInfo["XDF"] = {"XDF", "Extended Density Format", {"XDF 5.25\" (1.86M)", "XDF 3.5\" (1.86M)"},
        false, false, false, false, true, 80, 23, 512};
    m_formatInfo["DMF"] = {"DMF", "Distribution Media Format", {"DMF 1.68M", "DMF 1.72M"},
        false, false, false, false, true, 80, 21, 512};
    m_formatInfo["TD0"] = {"TD0", "Teledisk Image", {"Normal", "Advanced"},
        true, true, false, false, true, 80, 18, 512};
        
    // Japanese
    m_formatInfo["D88"] = {"D88", "PC-98/X1 Image", {"2D", "2DD", "2HD"},
        false, false, false, false, true, 80, 16, 256};
        
    // Flux
    m_formatInfo["SCP"] = {"SCP", "SuperCard Pro Flux", {"Single Rev", "Multi Rev"},
        true, true, true, true, true, 84, 0, 0};
    m_formatInfo["HFE"] = {"HFE", "HxC Floppy Emulator", {"HFE v1", "HFE v3"},
        true, true, true, true, true, 84, 0, 0};
}

// ============================================================================
// Populate System Combo
// ============================================================================

void FormatTab::populateSystemCombo() {
    ui->comboSystem->blockSignals(true);
    ui->comboSystem->clear();
    
    // Add systems in logical order
    QStringList systemOrder = {
        // Commodore
        "Commodore 64/128", "Commodore Plus/4", "Commodore VIC-20", "Commodore PET/CBM",
        // Amiga
        "Amiga",
        // Apple
        "Apple II", "Apple III", "Macintosh (400K/800K)",
        // Atari
        "Atari ST/STE", "Atari 8-bit (400/800/XL/XE)",
        // Sinclair
        "ZX Spectrum", "SAM Coupé",
        // Amstrad
        "Amstrad CPC", "Amstrad PCW",
        // MSX
        "MSX",
        // BBC/Acorn
        "BBC Micro", "Acorn Archimedes",
        // PC
        "PC/DOS",
        // Japanese
        "NEC PC-98", "Sharp X68000", "FM Towns",
        // TRS-80
        "TRS-80 (Model I/III/4)", "TRS-80 Color Computer",
        // TI
        "TI-99/4A",
        // French
        "Thomson MO/TO", "Oric Atmos",
        // CP/M
        "Kaypro", "Osborne", "North Star",
        // DEC
        "DEC PDP/VAX",
        // Other
        "Heathkit/Zenith", "Victor 9000",
        // Flux
        "Flux (raw)"
    };
    
    for (const QString& sys : systemOrder) {
        if (m_systemFormats.contains(sys)) {
            ui->comboSystem->addItem(sys);
        }
    }
    
    ui->comboSystem->blockSignals(false);
}

// ============================================================================
// Connection Setup - ALL Dependencies
// ============================================================================

/* ── Der Kopierplan in der Oberflaeche (MF-1233) ─────────────────────
 *
 * Die vier Auswahlfelder werden NICHT im `.ui` gefuellt, sondern hier
 * aus `uft_copy_level_name()` und seinen drei Geschwistern. Damit gibt
 * es die Wertelisten genau einmal — im Kern.
 *
 * Genau daran scheitert der alte Weg, und das ist gemessen:
 * `comboXCopyMode` (dieses .ui) fuehrt Sector/Track/„Index",
 * `comboCopyMode` (tab_xcopy.ui) fuehrt Sector/Track/„Flux/Nibble".
 * Zwei Listen, zwei Wahrheiten — und die erste hat ueber den ganzen
 * Baum gemessen **0 Leser**.
 */
static void fuelleAus(QComboBox *box, int anzahl,
                      const char *(*name)(int), int vorgabe)
{
    if (!box) return;
    box->blockSignals(true);
    box->clear();
    for (int i = 0; i < anzahl; i++) {
        const char *n = name(i);
        if (n) box->addItem(QString::fromUtf8(n), i);
    }
    const int idx = box->findData(vorgabe);
    if (idx >= 0) box->setCurrentIndex(idx);
    box->blockSignals(false);
}

/* Die Namensfunktionen nehmen ihre eigenen Aufzaehlungstypen; fuer die
 * gemeinsame Fuellfunktion braucht es je eine Bruecke. Sie sind
 * absichtlich winzig — eine Umwandlung, keine zweite Tafel. */
static const char *nameLevel(int v)
{ return uft_copy_level_name(static_cast<uft_copy_level_t>(v)); }
static const char *nameStrategy(int v)
{ return uft_copy_strategy_name(static_cast<uft_read_strategy_t>(v)); }
static const char *namePreserve(int v)
{ return uft_copy_preservation_name(static_cast<uft_preservation_t>(v)); }
static const char *namePolicy(int v)
{ return uft_copy_policy_name(static_cast<uft_copy_policy_t>(v)); }
static const char *nameTrackMode(int v)
{ return uft_copy_track_mode_name(static_cast<uft_track_mode_t>(v)); }
static const char *nameFileSpecial(int v)
{ return uft_copy_file_special_name(static_cast<uft_file_special_t>(v)); }
static const char *nameGcr(int v)
{ return uft_copy_gcr_name(static_cast<uft_gcr_variant_t>(v)); }
static const char *nameVote(int v)
{ return uft_copy_vote_name(static_cast<uft_vote_method_t>(v)); }
static const char *nameExact(int v)
{ return uft_copy_exact_name(static_cast<uft_bitexact_kind_t>(v)); }

/* MF-1282: die Anordnung finden, die dieses Element WIRKLICH enthaelt.
 *
 * `w->parentWidget()->layout()` liefert nur die oberste Anordnung des
 * Elternwidgets; liegt das Element in einer geschachtelten Anordnung
 * darin, meldet `indexOf()` dort -1. Deshalb wird abgestiegen. */
static QLayout *layoutVon(QLayout *lay, QWidget *w, int *index)
{
    if (!lay) return nullptr;
    const int i = lay->indexOf(w);
    if (i >= 0) { *index = i; return lay; }
    for (int k = 0; k < lay->count(); ++k)
        if (QLayout *sub = lay->itemAt(k)->layout())
            if (QLayout *f = layoutVon(sub, w, index)) return f;
    return nullptr;
}

/* MF-1282: die Beschriftung eines Bedienelements.
 *
 * Der Kopierplan versteckt Felder, die auf dieser Ebene nichts bedeuten
 * (MF-1234). Bisher verschwand nur das FELD — das „Sectors:" davor blieb
 * stehen und zeigte auf nichts. Eine Beschriftung ohne ihr Feld ist eine
 * Aussage ueber eine Einstellung, die es hier nicht gibt.
 *
 * DREI Lagen kommen in `forms/tab_format.ui` vor, alle gemessen:
 *   im QGridLayout steht die Beschriftung in der Zelle LINKS daneben
 *     (spinSectors Zelle(1,1), links QLabel „Sectors:"),
 *   im QFormLayout in der LabelRole derselben Zeile
 *     (comboFormat Zeile 1, links QLabel „Format:"),
 *   im QBoxLayout als VORHERIGES Element
 *     (comboFluxMerge, comboSampleRate, comboXCopySides).
 * Steht dort kein QLabel, gibt es keine Beschriftung und nichts passiert —
 * gemessen ist das der Fall bei `checkDecodeGCR` (davor steht das Haekchen
 * `checkNibbleMode`) und `checkHashSha512` (davor `checkHashSha256`). Ein
 * Nachbar ist keine Beschriftung, und ihn mit zu verstecken waere schlimmer
 * als die haengende Beschriftung, die behoben werden soll.
 * Dieselbe Regel wendet die Vorschau des Entwurfs an (test-gui). */
static QWidget *beschriftungVon(QWidget *w)
{
    if (!w || !w->parentWidget()) return nullptr;
    int i = -1;
    QLayout *lay = layoutVon(w->parentWidget()->layout(), w, &i);
    if (!lay || i < 0) return nullptr;
    if (auto *f = qobject_cast<QFormLayout *>(lay)) {
        int r = -1;
        QFormLayout::ItemRole rolle = QFormLayout::FieldRole;
        f->getWidgetPosition(w, &r, &rolle);
        if (r < 0 || rolle != QFormLayout::FieldRole) return nullptr;
        QLayoutItem *it = f->itemAt(r, QFormLayout::LabelRole);
        return it ? qobject_cast<QLabel *>(it->widget()) : nullptr;
    }
    if (auto *g = qobject_cast<QGridLayout *>(lay)) {
        int r = 0, c = 0, rs = 0, cs = 0;
        g->getItemPosition(i, &r, &c, &rs, &cs);
        if (c <= 0) return nullptr;
        QLayoutItem *it = g->itemAtPosition(r, c - 1);
        return it ? qobject_cast<QLabel *>(it->widget()) : nullptr;
    }
    if (qobject_cast<QBoxLayout *>(lay)) {
        if (i == 0) return nullptr;
        QLayoutItem *it = lay->itemAt(i - 1);
        return it ? qobject_cast<QLabel *>(it->widget()) : nullptr;
    }
    return nullptr;
}

void FormatTab::setupConnections() {
    // Kopierplan (MF-1233) — vier Achsen, aus dem Kern gefuellt
    fuelleAus(ui->comboPlanLevel,    UFT_COPY_LEVEL_N,    nameLevel,
              UFT_COPY_SECTOR);
    fuelleAus(ui->comboPlanStrategy, UFT_READ_STRATEGY_N, nameStrategy,
              UFT_READ_STANDARD);
    fuelleAus(ui->comboPlanPreserve, UFT_PRESERVE_N,      namePreserve,
              UFT_PRESERVE_LOGICAL);
    fuelleAus(ui->comboPlanPolicy,   UFT_POLICY_N,        namePolicy,
              UFT_POLICY_NORMAL);
    connect(ui->comboPlanLevel, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FormatTab::onCopyPlanChanged);
    connect(ui->comboPlanStrategy, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FormatTab::onCopyPlanChanged);
    connect(ui->comboPlanPreserve, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FormatTab::onCopyPlanChanged);
    connect(ui->comboPlanPolicy, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FormatTab::onCopyPlanChanged);

    // MF-1235: die Feinheiten — Spurart, Dateiart, GCR, Abstimmung,
    // Genauigkeit. Auch sie aus dem Kern, nicht aus dem .ui.
    fuelleAus(ui->comboPlanTrackMode,   UFT_TRACK_MODE_N,   nameTrackMode,
              UFT_TRACK_DECODED);
    fuelleAus(ui->comboPlanFileSpecial, UFT_FILE_SPECIAL_N, nameFileSpecial,
              UFT_FILE_GENERIC);
    fuelleAus(ui->comboPlanGcr,         UFT_GCR_N,          nameGcr,
              UFT_GCR_COMMODORE);
    fuelleAus(ui->comboPlanVote,        UFT_VOTE_N,         nameVote,
              UFT_VOTE_STRICT_MAJORITY);
    fuelleAus(ui->comboPlanExact,       UFT_EXACT_N,        nameExact,
              UFT_EXACT_SECTOR);
    for (QComboBox *b : { ui->comboPlanTrackMode, ui->comboPlanFileSpecial,
                          ui->comboPlanGcr, ui->comboPlanVote,
                          ui->comboPlanExact })
        connect(b, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &FormatTab::onCopyPlanChanged);
    for (QCheckBox *c : { ui->checkHashSha512, ui->checkHashCrc32 })
        connect(c, &QCheckBox::toggled, this, &FormatTab::onCopyPlanToggled);

    // MF-1237: der benannte Kopiermodus. Die Liste kommt aus dem Kern.
    connect(ui->comboCopyProfile, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FormatTab::onCopyProfileChanged);
    connect(ui->btnPlanAnpassen, &QPushButton::clicked,
            this, &FormatTab::onPlanAnpassen);
    /* MF-1238: die JSON-Ansicht */
    connect(ui->btnPlanJson, &QPushButton::toggled,
            this, &FormatTab::onPlanJsonToggled);
    connect(ui->btnPlanJsonKopieren, &QPushButton::clicked,
            this, &FormatTab::onPlanJsonKopieren);
    connect(ui->btnPlanJsonSichern, &QPushButton::clicked,
            this, &FormatTab::onPlanJsonSichern);

    // System/Format cascade
    connect(ui->comboSystem, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FormatTab::onSystemChanged);
    connect(ui->comboFormat, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FormatTab::onFormatChanged);
    connect(ui->comboVersion, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FormatTab::onVersionChanged);
    connect(ui->comboEncoding, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FormatTab::onEncodingChanged);
    
    // XCopy
    connect(ui->checkAllTracks, &QCheckBox::toggled,
            this, &FormatTab::onAllTracksToggled);
    connect(ui->spinStartTrack, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { emit formatSettingsChanged(); });
    connect(ui->spinEndTrack, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { emit formatSettingsChanged(); });
    
    // Nibble/GCR
    connect(ui->comboGCRType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FormatTab::onGCRTypeChanged);
    
    // Logging
    connect(ui->checkLogToFile, &QCheckBox::toggled,
            this, &FormatTab::onLogToFileToggled);
    connect(ui->btnBrowseLog, &QPushButton::clicked,
            this, &FormatTab::onBrowseLogPath);
    
    // Protection
    connect(ui->checkDetectAll, &QCheckBox::toggled,
            this, &FormatTab::onDetectAllToggled);
    connect(ui->comboPlatform, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { emit formatSettingsChanged(); });
    
    // Advanced dialog buttons
    connect(ui->btnNibbleAdvanced, &QPushButton::clicked,
            this, &FormatTab::onNibbleAdvanced);
    
    // Presets
    connect(ui->btnLoadPreset, &QPushButton::clicked,
            this, &FormatTab::onLoadPreset);
    connect(ui->btnSavePreset, &QPushButton::clicked,
            this, &FormatTab::onSavePreset);
    connect(ui->comboPreset, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { emit formatSettingsChanged(); });
    
    // Format parameters
    connect(ui->spinTracks, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { emit formatSettingsChanged(); });
    connect(ui->spinSides, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { emit formatSettingsChanged(); });
    connect(ui->spinSectors, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { emit formatSettingsChanged(); });
    connect(ui->comboSectorSize, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { emit formatSettingsChanged(); });
    connect(ui->comboRPM, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { emit formatSettingsChanged(); });
    
    // Flux parameters
    connect(ui->comboFluxSpeed, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { emit formatSettingsChanged(); });
    connect(ui->spinRevolutions, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { emit formatSettingsChanged(); });
    connect(ui->comboFluxErrors, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { emit formatSettingsChanged(); });
    connect(ui->comboFluxMerge, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { emit formatSettingsChanged(); });
    connect(ui->checkWeakBits, &QCheckBox::toggled,
            this, [this](bool) { emit formatSettingsChanged(); });
    connect(ui->checkNoFluxAreas, &QCheckBox::toggled,
            this, [this](bool) { emit formatSettingsChanged(); });
    
    // PLL parameters
    connect(ui->comboSampleRate, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { emit formatSettingsChanged(); });
    connect(ui->checkAdaptivePLL, &QCheckBox::toggled,
            this, [this](bool) { emit formatSettingsChanged(); });
    connect(ui->checkUseIndex, &QCheckBox::toggled,
            this, [this](bool) { emit formatSettingsChanged(); });
    
    // GW→DMK Direct
    connect(ui->btnGw2DmkOpen, &QPushButton::clicked,
            this, &FormatTab::onGw2DmkOpenClicked);
}


// ============================================================================
// Initial State Setup
// ============================================================================

void FormatTab::setupInitialState() {
    updateXCopyTrackRange(ui->checkAllTracks->isChecked());
    updateNibbleOptions(ui->comboGCRType->currentText());
    updateLogFileOptions(ui->checkLogToFile->isChecked());
    syncProtectionWidgets(ui->checkDetectAll->isChecked());
    // PLL handled by embedded panel
}

// ============================================================================
// OUTPUT FORMAT - System/Format Cascade
// ============================================================================

void FormatTab::onSystemChanged(int index) {
    QString system = ui->comboSystem->itemText(index);
    populateFormatsForSystem(system);
    emit systemChanged(system);
    emit formatSettingsChanged();
}

void FormatTab::onFormatChanged(int index) {
    if (index < 0) return;
    
    QString format = ui->comboFormat->itemText(index);
    populateVersionsForFormat(format);
    updateFormatSpecificOptions(format);
    /* MF-1233: die Faehigkeiten haengen am FORMAT - ein Planbefund wie
     * "Consensus braucht CAP_MULTI_REV" gilt je Format verschieden. */
    applyCopyPlan();
    emit formatChanged(format);
    emit formatSettingsChanged();
}

void FormatTab::onVersionChanged(int /*index*/) {
    emit formatSettingsChanged();
}

void FormatTab::onEncodingChanged(int index) {
    QString encoding = ui->comboEncoding->itemText(index);
    bool isGCR = encoding.contains("GCR");
    updateGCROptions(isGCR);
    emit formatSettingsChanged();
}

void FormatTab::populateFormatsForSystem(const QString& system) {
    ui->comboFormat->blockSignals(true);
    ui->comboFormat->clear();
    
    if (m_systemFormats.contains(system)) {
        ui->comboFormat->addItems(m_systemFormats[system]);
    }
    
    ui->comboFormat->blockSignals(false);
    
    if (ui->comboFormat->count() > 0) {
        onFormatChanged(0);
    }
}

void FormatTab::populateVersionsForFormat(const QString& format) {
    ui->comboVersion->blockSignals(true);
    ui->comboVersion->clear();

    /* ── MF-1231: der KERN hat Vorrang, wo er etwas weiss ────────────
     *
     * `m_formatInfo` fuehrt eine von Hand gepflegte Variantenliste —
     * 25 Formate, 50 Namen. Gemessen war sie an drei Stellen schwaecher
     * als das Plugin selbst:
     *
     *   IMG  Oberflaeche 5 Groessen, `known_geometries[]` **13**
     *        (160K, 180K, 320K und die vier ED-Spezialformate fehlten,
     *        und die DMF-Groesse stand als eigenes „Format" ohne Plugin)
     *   D64  „Standard" und „35 Track" sind dieselbe Diskette; die
     *        41-Spur-Fassung fehlte
     *   TRD  die 40-Spur-Diskette mit 163 840 Byte fehlte
     *
     * Zwei Listen derselben Sache driften (MF-1177). Seit MF-1231
     * fuehren die Plugins ihre Varianten selbst, mit `can_write` und
     * einer Begruendung, wo nur gelesen werden kann — und diese Anzeige
     * liest von dort. `m_formatInfo` bleibt der Rueckfall fuer die
     * Formate, deren Plugin noch keine Tafel traegt; er verschwindet,
     * sobald sie eine hat.
     *
     * Was eine nicht schreibbare Variante angeht: sie steht sichtbar in
     * der Liste, aber mit ihrem Grund dahinter. Sie zu verschweigen
     * waere eine stille Auslassung; sie ohne Grund anzubieten die
     * Zusage, die MF-665 an HFEv3 verhindert hat. */
    const uft_format_plugin_t *plugin =
        uft_get_format_plugin_by_name(format.toUtf8().constData());

    if (plugin && plugin->variants && plugin->variant_count > 0) {
        for (size_t i = 0; i < plugin->variant_count; i++) {
            const uft_format_variant_t &v = plugin->variants[i];
            QString text = QString::fromUtf8(v.name);
            if (!v.can_write) {
                text += (v.write_note && v.write_note[0])
                            ? tr(" — nur lesen: %1").arg(QString::fromUtf8(v.write_note))
                            : tr(" — nur lesen");
            }
            ui->comboVersion->addItem(text, QString::fromUtf8(v.name));
            if (v.is_write_default)
                ui->comboVersion->setCurrentIndex(ui->comboVersion->count() - 1);
        }
    } else if (m_formatInfo.contains(format)) {
        for (const QString &v : m_formatInfo[format].versions)
            ui->comboVersion->addItem(v, v);
    } else {
        /* Kein Plugin-Wissen und kein Eintrag: dann steht hier nichts.
         * Bis MF-1231 stand „Standard" — ein Name, den weder das Format
         * noch der Kern je gesagt hat. */
        ui->comboVersion->addItem(tr("— keine Variante benannt —"), QString());
    }

    ui->comboVersion->blockSignals(false);
}

// ============================================================================
// Faehigkeits-Manifest -> Bedienelemente  (MF-661)
// ============================================================================
//
// Stufe 2 aus docs/plans/VARIANTEN_UND_FAEHIGKEITEN.md.
//
// Bis hierher entschied `m_formatInfo` — eine handgepflegte Tabelle mit 25
// Eintraegen — was die Oberflaeche anbietet. Die 88 Plugins fuehren daneben
// ihr eigenes Manifest, und die beiden widersprechen sich an fuenf Stellen
// gemessen (MF-660): EDSK, G64, NIB, SCP und TD0 versprechen hier
// Weak-Bit-Faehigkeit, die das Plugin als UNSUPPORTED fuehrt.
//
// Die beiden beantworten verschiedene Fragen — "kann das FORMAT das
// tragen?" gegen "tut UNSER CODE das?". Fuer ein Bedienelement zaehlt die
// zweite: ein Element, das ein format-theoretisches Koennen anbietet,
// welches unser Leser nicht einloest, ist ein toter Knopf.
//
// Die Zuordnung Gruppe -> Merkmal ist eine FESTLEGUNG, keine Messung, und
// steht deshalb hier an einer Stelle statt verstreut:

namespace {

struct GruppenMerkmal {
    const char *gruppe;    // objectName des Widgets im .ui
    const char *merkmal;   // Name im Faehigkeits-Manifest
    const char *warum;
};

// groupNibble fehlt bewusst: GCR hat kein eigenes Manifest-Merkmal, und
// eines zu erfinden hiesse, 88 Manifeste um eine ungepruefte Zeile zu
// erweitern. Die Gruppe bleibt sichtbar, bis es ein belegtes Merkmal gibt.
const GruppenMerkmal kZuordnung[] = {
    { "groupFlux",       "Flux",      "arbeitet am Flussband" },
    { "groupPLL",        "Flux",      "PLL taktet den Flussstrom" },
    { "groupWrite",      "Write",     "direkt" },
    { "groupProtection", "Weak Bits", "Kopierschutz haengt daran" },
};

} // namespace

void FormatTab::applyPluginCapabilities(const QString& format)
{
    const uft_format_plugin_t *plugin =
        uft_get_format_plugin_by_name(format.toUtf8().constData());

    for (const GruppenMerkmal &z : kZuordnung) {
        QWidget *w = findChild<QWidget *>(QString::fromLatin1(z.gruppe));
        if (!w) continue;

        // Kein Plugin gefunden: NICHTS ausblenden. Auf Unwissen zu
        // verstecken naehme dem Benutzer Funktion wegen eines
        // Nachschlagefehlers von uns — dieselbe Regel wie "nicht
        // ermittelt" bei der Variantenanzeige.
        if (!plugin) {
            w->setVisible(true);
            w->setEnabled(true);
            w->setToolTip(QString());
            continue;
        }

        switch (uft_plugin_control_visibility(plugin, z.merkmal)) {
        case UFT_CONTROL_SHOW:
            w->setVisible(true);
            w->setEnabled(true);
            w->setToolTip(QString());
            break;

        case UFT_CONTROL_SHOW_LIMITED: {
            // PARTIAL wird gezeigt, nicht versteckt: eine eingeschraenkte
            // Faehigkeit zu verbergen naehme dem Benutzer eine Information,
            // die er hat. Der Header verlangt fuer PARTIAL eine
            // Begruendung — die wird hier der Hinweistext.
            w->setVisible(true);
            w->setEnabled(true);
            const char *note = uft_plugin_feature_note(plugin, z.merkmal);
            w->setToolTip(note && *note
                ? tr("Eingeschränkt: %1").arg(QString::fromUtf8(note))
                : tr("Eingeschränkt unterstützt."));
            break;
        }

        case UFT_CONTROL_HIDE:
        default:
            w->setVisible(false);
            w->setEnabled(false);
            w->setToolTip(QString());
            break;
        }
    }
}

void FormatTab::updateFormatSpecificOptions(const QString& format) {
    // MF-661: VOR dem fruehen return. `m_formatInfo` kennt 25 von 88
    // Formaten; fuer die uebrigen 63 kehrte diese Funktion bisher sofort
    // zurueck, und die Bedienelemente behielten den Zustand des ZULETZT
    // gewaehlten Formats. Das Manifest gilt fuer alle.
    applyPluginCapabilities(format);

    if (!m_formatInfo.contains(format)) {
        return;
    }
    
    const FormatInfo& info = m_formatInfo[format];
    
    ui->checkHalfTracks->setEnabled(info.supportsHalfTracks);
    if (!info.supportsHalfTracks) ui->checkHalfTracks->setChecked(false);
    
    
    updateFluxOptions(info.supportsFlux);
    updateGCROptions(info.supportsGCR);
    
    if (info.defaultTracks > 0) ui->spinTracks->setValue(info.defaultTracks);
    
    if (info.defaultSectorSize > 0) {
    }
    
    if (format == "XDF" || format == "DMF" || format == "2M") {
    } else {
    }
}

// ============================================================================
// XCOPY Dependencies
// ============================================================================

// ============================================================================
// Kopierplan (MF-1233) und der benannte Modus (MF-1237)
// ============================================================================

/* Das Auswahlfeld fuellen — aus dem Kern, und mit dem GRUND, warum ein
 * Profil nicht geht.
 *
 * Nicht verfuegbare Profile werden SICHTBAR und unanwaehlbar, nicht
 * entfernt: wer „BAMCopy" sucht und nicht findet, weiss nicht, ob es das
 * nicht gibt oder ob es hier nur nicht passt. Dieselbe Regel wie beim
 * Varianten-Waehler (MF-666). */
void FormatTab::fuelleProfile() {
    if (!ui->comboCopyProfile) return;
    const QString fmt = getSelectedFormat();
    const uint32_t caps = copyPlanCaps();
    const QByteArray f = fmt.toUtf8();

    ui->comboCopyProfile->blockSignals(true);
    ui->comboCopyProfile->clear();

    for (size_t i = 0; i < uft_copy_profile_count(); i++) {
        const uft_copy_profile_t *p = uft_copy_profile(i);
        if (!p) continue;
        const char *grund = nullptr;
        const bool ok = uft_copy_profile_available(
            p, caps, fmt.isEmpty() ? nullptr : f.constData(), &grund);

        QString text = QString::fromUtf8(p->name);
        if (!ok)
            text += tr(" — nicht verfügbar: %1")
                        .arg(QString::fromUtf8(grund ? grund : ""));
        ui->comboCopyProfile->addItem(text, QString::fromUtf8(p->id));
        const int idx = ui->comboCopyProfile->count() - 1;
        if (!ok) {
            if (auto *m = qobject_cast<QStandardItemModel *>(
                    ui->comboCopyProfile->model()))
                if (QStandardItem *it = m->item(idx)) it->setEnabled(false);
        } else if (p->text) {
            ui->comboCopyProfile->setItemData(idx, QString::fromUtf8(p->text),
                                              Qt::ToolTipRole);
        }
    }
    /* „Benutzerdefiniert" steht NICHT im Kern — es ist die Abwesenheit
     * eines Profils und damit ein Zustand dieser Oberflaeche. */
    ui->comboCopyProfile->addItem(tr("Benutzerdefiniert"), QString());

    const int wahl = ui->comboCopyProfile->findData(
        m_profil.isEmpty() ? QVariant(QString()) : QVariant(m_profil));
    if (wahl >= 0) ui->comboCopyProfile->setCurrentIndex(wahl);
    ui->comboCopyProfile->blockSignals(false);
}

/* Die vier Achsen sperren oder freigeben. */
void FormatTab::setPlanFrei(bool frei) {
    m_planFrei = frei;
    for (QComboBox *b : { ui->comboPlanLevel, ui->comboPlanStrategy,
                          ui->comboPlanPreserve, ui->comboPlanPolicy })
        if (b) b->setEnabled(frei);
    if (ui->btnPlanAnpassen)
        ui->btnPlanAnpassen->setText(frei ? tr("Modus verwenden")
                                          : tr("Plan anpassen"));
}

/* Ein Profil anwenden: es SETZT die vier Achsen und sperrt sie. */
void FormatTab::wendeProfilAn(const QString &id) {
    if (id.isEmpty()) {          /* Benutzerdefiniert */
        m_profil.clear();
        setPlanFrei(true);
        return;
    }
    const uft_copy_profile_t *p =
        uft_copy_profile_by_id(id.toUtf8().constData());
    if (!p) return;

    m_profil = id;
    m_basisProfil = id;

    auto setze = [](QComboBox *b, int wert) {
        if (!b) return;
        const int i = b->findData(wert);
        if (i >= 0) { b->blockSignals(true); b->setCurrentIndex(i);
                      b->blockSignals(false); }
    };
    setze(ui->comboPlanLevel,    p->plan.level);
    setze(ui->comboPlanStrategy, p->plan.strategy);
    setze(ui->comboPlanPreserve, p->plan.preservation);
    setze(ui->comboPlanPolicy,   p->plan.policy);
    setze(ui->comboPlanTrackMode,   p->plan.track_mode);
    setze(ui->comboPlanFileSpecial, p->plan.file_special);
    setze(ui->comboPlanGcr,         p->plan.gcr);
    setze(ui->comboPlanVote,        p->plan.vote);
    setze(ui->comboPlanExact,       p->plan.exact_kind);
    setPlanFrei(false);
}

/* ── MF-1238: was das Format traegt ──────────────────────────────────
 *
 * Die vier Flaggen unten kann DIESER Reiter nicht messen: er kennt kein
 * geoeffnetes Abbild, also weiss er nichts ueber Dateisystem, BAM, GCR
 * oder einen Bitstromzugang. `copyPlanCaps()` setzt sie deshalb nie.
 *
 * Sie als „traegt nicht" anzuzeigen waere eine Messung behauptet, die
 * es nicht gibt — derselbe Fehler, den MF-980 benannt hat: „das Format
 * sagt 0xE5" und „hier wurde 0xE5 gelesen" sind zwei Aussagen. */
static uint32_t nicht_messbar()
{
    return (uint32_t)UFT_CAP_BITSTREAM_IO | (uint32_t)UFT_CAP_FILESYSTEM
         | (uint32_t)UFT_CAP_CBM_BAM      | (uint32_t)UFT_CAP_GCR;
}

void FormatTab::zeigeCaps() {
    if (!ui->labelPlanCaps) return;

    if (getSelectedFormat().isEmpty()) {
        ui->labelPlanCaps->setText(tr("Kein Format gewählt."));
        return;
    }

    const uint32_t caps = copyPlanCaps();
    const uint32_t offen = nicht_messbar();
    QStringList traegt, traegtNicht, unbekannt;

    /* Ueber die Aufzaehlung des KERNS laufen, nicht ueber eine Liste
     * hier — eine gepflegte Aufzaehlung veraltet still (MF-636). */
    for (size_t i = 0; i < uft_copy_cap_count(); i++) {
        const uft_copy_caps_t c = uft_copy_cap_at(i);
        const char *n = uft_copy_cap_name(c);
        if (!n) continue;
        const QString name = QString::fromUtf8(n);
        if (offen & (uint32_t)c)        unbekannt   << name;
        else if (caps & (uint32_t)c)    traegt      << name;
        else                            traegtNicht << name;
    }

    QStringList zeilen;
    zeilen << (traegt.isEmpty()
                   ? tr("Das Format trägt: —")
                   : tr("Das Format trägt: %1").arg(traegt.join(", ")));
    if (!traegtNicht.isEmpty())
        zeilen << tr("trägt nicht: %1").arg(traegtNicht.join(", "));
    if (!unbekannt.isEmpty())
        zeilen << tr("nicht feststellbar (dieser Reiter öffnet kein "
                     "Abbild): %1").arg(unbekannt.join(", "));
    ui->labelPlanCaps->setText(zeilen.join(QStringLiteral(" · ")));
}

/* ── MF-1238: der Plan als JSON ──────────────────────────────────────
 *
 * Gezeigt wird der AUFGELOESTE Plan — der, der laufen wuerde —, nicht
 * die rohe Achsenstellung. Sonst stuende bei „Automatisch" eine Ebene
 * im Text, die niemand ausfuehrt.
 *
 * Die Laenge kommt vom Kern (Nullzeiger-Abfrage, Zusage K24). Ein
 * fester Puffer waere eine stille Kuerzung, sobald ein Plan mehr
 * erzwingt als hineinpasst — und still gekuerzte Ausgaben sind in
 * diesem Baum eine eigene Fehlerklasse. */
QString FormatTab::planJson() const {
    const uft_copy_plan_t roh = copyPlan();
    const uft_copy_plan_t p   = uft_copy_plan_resolve(&roh, copyPlanCaps());

    const size_t n = uft_copy_plan_to_json(&p, nullptr, 0);
    if (n == 0) return QString();

    QByteArray b(int(n) + 1, '\0');
    (void)uft_copy_plan_to_json(&p, b.data(), (size_t)b.size());
    return QString::fromUtf8(b.constData());
}

void FormatTab::onPlanJsonToggled(bool checked) {
    if (ui->textPlanJson) {
        ui->textPlanJson->setVisible(checked);
        if (checked) ui->textPlanJson->setPlainText(planJson());
    }
    if (ui->btnPlanJson)
        ui->btnPlanJson->setText(checked ? tr("JSON verbergen")
                                         : tr("Plan als JSON zeigen"));
}

void FormatTab::onPlanJsonKopieren() {
    if (QClipboard *c = QGuiApplication::clipboard())
        c->setText(planJson());
}

void FormatTab::onPlanJsonSichern() {
    /* Ohne Dialog wird nichts geschrieben. Ein Werkzeug, das
     * ungefragt Dateien anlegt, ist in diesem Baum ein Befund. */
    const QString pfad = QFileDialog::getSaveFileName(
        this, tr("Kopierplan sichern"), QStringLiteral("kopierplan.json"),
        tr("JSON (*.json);;Alle Dateien (*)"));
    if (pfad.isEmpty()) return;

    QFile f(pfad);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Kopierplan sichern"),
                             tr("Die Datei ließ sich nicht schreiben:\n%1")
                                 .arg(f.errorString()));
        return;
    }
    const QByteArray roh = planJson().toUtf8();
    if (f.write(roh) != roh.size())
        QMessageBox::warning(this, tr("Kopierplan sichern"),
                             tr("Es wurden nicht alle Daten geschrieben:\n%1")
                                 .arg(f.errorString()));
    f.close();
}

void FormatTab::onCopyProfileChanged(int index) {
    if (index < 0 || !ui->comboCopyProfile) return;
    wendeProfilAn(ui->comboCopyProfile->itemData(index).toString());
    applyCopyPlan();
    emit formatSettingsChanged();
}

void FormatTab::onPlanAnpassen() {
    if (m_planFrei) {
        /* Zurueck zum Profil, von dem ausgegangen wurde. */
        wendeProfilAn(m_basisProfil);
    } else {
        /* Ab jetzt ohne Profil — aber mit den Werten, die es gesetzt
         * hat. Der Bediener faengt nicht bei Null an. */
        m_profil.clear();
        setPlanFrei(true);
    }
    applyCopyPlan();
    emit formatSettingsChanged();
}

uft_copy_plan_t FormatTab::copyPlan() const {
    uft_copy_plan_t p = uft_copy_plan_default();
    auto lies = [](QComboBox *b, int vorgabe) {
        if (!b || b->currentIndex() < 0) return vorgabe;
        const QVariant v = b->currentData();
        return v.isValid() ? v.toInt() : vorgabe;
    };
    p.level = static_cast<uft_copy_level_t>(
        lies(ui->comboPlanLevel, UFT_COPY_SECTOR));
    p.strategy = static_cast<uft_read_strategy_t>(
        lies(ui->comboPlanStrategy, UFT_READ_STANDARD));
    p.preservation = static_cast<uft_preservation_t>(
        lies(ui->comboPlanPreserve, UFT_PRESERVE_LOGICAL));
    p.policy = static_cast<uft_copy_policy_t>(
        lies(ui->comboPlanPolicy, UFT_POLICY_NORMAL));

    /* MF-1235: die Feinheiten kommen jetzt aus Bedienelementen.
     *
     * Bis hierher leitete ich `exact_kind` aus der Ebene ab — eine
     * Rateregel, die genau die drei Faelle der Vorgabe nicht
     * unterscheidet (SCP->SCP fluxnah, SCP->HFE Bitstrom ohne
     * Flusszeiten, SCP->IMG nur Sektoren). Sie ist ersetzt. */
    p.track_mode = static_cast<uft_track_mode_t>(
        lies(ui->comboPlanTrackMode, UFT_TRACK_DECODED));
    p.file_special = static_cast<uft_file_special_t>(
        lies(ui->comboPlanFileSpecial, UFT_FILE_GENERIC));
    p.gcr = static_cast<uft_gcr_variant_t>(
        lies(ui->comboPlanGcr, UFT_GCR_COMMODORE));
    p.vote = static_cast<uft_vote_method_t>(
        lies(ui->comboPlanVote, UFT_VOTE_STRICT_MAJORITY));
    p.exact_kind = static_cast<uft_bitexact_kind_t>(
        lies(ui->comboPlanExact, UFT_EXACT_SECTOR));

    /* SHA-256 steht fest — das Ankreuzfeld ist gesperrt, und der Wert
     * wird hier nicht aus ihm gelesen, sondern gesetzt. Ein gesperrtes
     * Feld auszulesen hiesse, sich auf einen Zustand zu verlassen, den
     * niemand mehr aendern kann; steht er einmal falsch, faellt es nie
     * auf. */
    p.hashes = (uint32_t)UFT_HASH_SHA256;
    if (ui->checkHashSha512 && ui->checkHashSha512->isChecked())
        p.hashes |= (uint32_t)UFT_HASH_SHA512;
    if (ui->checkHashCrc32 && ui->checkHashCrc32->isChecked())
        p.hashes |= (uint32_t)UFT_HASH_CRC32;
    return p;
}

uint32_t FormatTab::copyPlanCaps() const {
    /* Was das gewaehlte Format zusagt — und NUR das.
     *
     * Dateisystem, GCR und CBM-BAM bleiben ungesetzt, weil dieser Reiter
     * sie nicht messen kann: er kennt kein geoeffnetes Abbild. Das
     * fuehrt dazu, dass die Dateiebene einen Befund meldet, und der ist
     * wahr — `FormatTab` hat keine Dateisystemerkennung. Sie
     * hilfsweise als „vorhanden" anzunehmen waere genau die stille
     * Zusage, gegen die der ganze Plan gebaut ist. */
    uint32_t caps = 0;
    const QString fmt = getSelectedFormat();
    if (fmt.isEmpty()) return caps;

    const uft_format_plugin_t *pl =
        uft_get_format_plugin_by_name(fmt.toUtf8().constData());
    if (!pl) return caps;

    if (pl->capabilities & UFT_FORMAT_CAP_FLUX)      caps |= UFT_CAP_FLUX_IO;
    if (pl->capabilities & UFT_FORMAT_CAP_TIMING)    caps |= UFT_CAP_TIMING;
    if (pl->capabilities & UFT_FORMAT_CAP_WEAK_BITS) caps |= UFT_CAP_WEAK_BITS;
    if (pl->capabilities & UFT_FORMAT_CAP_MULTI_REV) caps |= UFT_CAP_MULTI_REV;
    return caps;
}

void FormatTab::applyCopyPlan() {
    const uint32_t caps = copyPlanCaps();
    /* MF-1237: die Verfuegbarkeit haengt am FORMAT - also neu fuellen,
     * bevor irgendetwas angezeigt wird. */
    fuelleProfile();
    if (ui->labelProfileText) {
        const uft_copy_profile_t *pr = m_profil.isEmpty()
            ? nullptr
            : uft_copy_profile_by_id(m_profil.toUtf8().constData());
        const uft_copy_profile_t *basis =
            uft_copy_profile_by_id(m_basisProfil.toUtf8().constData());
        if (pr && pr->text)
            ui->labelProfileText->setText(QString::fromUtf8(pr->text));
        else if (basis)
            ui->labelProfileText->setText(
                tr("Benutzerdefiniert — ausgehend von %1. Die vier Achsen "
                   "sind entsperrt.").arg(QString::fromUtf8(basis->name)));
        else
            ui->labelProfileText->setText(tr("Benutzerdefiniert."));
    }
    /* MF-1234: „Automatisch" wird VOR jeder Regel aufgeloest - sonst
     * greift keine ebenenabhaengige Regel, und die Oberflaeche zeigte
     * einen Zustand, den der Plan gar nicht beurteilt hat. */
    const uft_copy_plan_t gewaehlt = copyPlan();
    const uft_copy_plan_t plan = uft_copy_plan_resolve(&gewaehlt, caps);

    /* 1. Die Befunde — harte zuerst, weil sie den Auftrag sperren. */
    uft_copy_finding_t f[16];
    size_t n = uft_copy_plan_check(&plan, caps, f, 16);
    const size_t gezeigt = (n > 16) ? 16 : n;
    QStringList hart, weich;
    for (size_t i = 0; i < gezeigt; i++) {
        const QString t = QString::fromUtf8(f[i].text ? f[i].text : "");
        (f[i].hard ? hart : weich) << t;
    }
    QString befunde;
    if (!hart.isEmpty())
        befunde = tr("So nicht ausführbar: ") + hart.join(QStringLiteral("  "));
    if (!weich.isEmpty()) {
        if (!befunde.isEmpty()) befunde += QStringLiteral("\n");
        befunde += tr("Hinweis: ") + weich.join(QStringLiteral("  "));
    }
    if (n > gezeigt)
        befunde += tr("\n(%1 weitere Befunde)").arg(n - gezeigt);
    if (gewaehlt.level == UFT_COPY_AUTO) {
        const char *abgeleitet = uft_copy_level_name(plan.level);
        const QString zeile = tr("Automatisch aufgelöst zu: %1")
                                  .arg(QString::fromUtf8(abgeleitet ? abgeleitet : "?"));
        befunde = befunde.isEmpty()
                      ? zeile
                      : (zeile + QChar('\n') + befunde);
    }
    if (ui->labelPlanFindings) ui->labelPlanFindings->setText(befunde);

    /* 2. Was der Plan erzwingt. Sichtbar, nicht versteckt — sonst
     *    wundert sich der Bediener, warum ein Feld nicht reagiert. */
    uft_copy_enforced_t e[96];
    size_t m = uft_copy_plan_enforced(&plan, e, 96);
    const size_t emax = (m > 96) ? 96 : m;
    QStringList zeilen;
    for (size_t i = 0; i < emax; i++)
        zeilen << QStringLiteral("%1 = %2")
                      .arg(QString::fromUtf8(e[i].param))
                      .arg(QString::fromUtf8(e[i].value));
    if (ui->labelPlanForced)
        ui->labelPlanForced->setText(
            zeilen.isEmpty() ? tr("Der Plan erzwingt nichts.")
                             : tr("Der Plan setzt fest: ")
                                   + zeilen.join(QStringLiteral(", ")));

    /* 2b. Die Feinheiten: sichtbar nur, wo sie etwas bedeuten (MF-1235).
     *
     * Ausgeblendet, nicht ausgegraut — dieselbe Regel wie bei den
     * Parametern. Ein gesperrtes „Commodore GCR" auf der Sektorebene
     * sagt „spaeter vielleicht"; dort bedeutet es aber gar nichts.
     *
     * Massgeblich ist die AUFGELOESTE Ebene: wer „Automatisch" waehlt
     * und auf einem Flussformat landet, soll die Flussfelder sehen. */
    struct { QWidget *w; bool zeigen; } planZeilen[] = {
        { ui->rowPlanTrackMode,   plan.level == UFT_COPY_TRACK },
        { ui->rowPlanFileSpecial, plan.level == UFT_COPY_FILE },
        { ui->rowPlanGcr,         plan.level == UFT_COPY_NIBBLE },
        { ui->rowPlanVote,        plan.strategy == UFT_READ_CONSENSUS },
        { ui->rowPlanExact,       plan.preservation == UFT_PRESERVE_BIT_EXACT },
        { ui->rowPlanHash,        plan.policy == UFT_POLICY_EVIDENCE },
    };
    for (const auto &z : planZeilen)
        if (z.w) z.w->setVisible(z.zeigen);

    /* Und „Commodore BAM" verschwindet als EINTRAG, wenn das Format
     * keine BAM zusagt. Sonst koennte man etwas waehlen, das der Kern
     * im selben Atemzug als harten Befund zurueckweist — eine Auswahl,
     * die nur dazu da ist, abgelehnt zu werden. */
    if (ui->comboPlanFileSpecial) {
        const bool bam = (caps & (uint32_t)UFT_CAP_CBM_BAM) &&
                         (caps & (uint32_t)UFT_CAP_FILESYSTEM);
        const int idx = ui->comboPlanFileSpecial->findData(UFT_FILE_BAM);
        if (idx >= 0 && !bam) {
            if (ui->comboPlanFileSpecial->currentIndex() == idx)
                ui->comboPlanFileSpecial->setCurrentIndex(
                    ui->comboPlanFileSpecial->findData(UFT_FILE_GENERIC));
            ui->comboPlanFileSpecial->removeItem(idx);
        } else if (idx < 0 && bam) {
            const char *n = uft_copy_file_special_name(UFT_FILE_BAM);
            ui->comboPlanFileSpecial->insertItem(
                UFT_FILE_BAM, QString::fromUtf8(n ? n : "BAM"), UFT_FILE_BAM);
        }
    }

    /* 3. Und jetzt die Bedienelemente.
     *
     * Die Zuordnung Widget -> Parameter steht HIER, an einer Stelle.
     * Sie fuehrt nur, was sich belegen laesst: gemessen (MF-1232) haben
     * von 155 Planparametern 26 ueberhaupt ein Bedienelement, und von
     * denen sind diese eindeutig. Ein Widget zu erfinden, das es nicht
     * gibt, waere die Klasse MF-767.
     *
     * Was der Plan macht:
     *   FORBIDDEN -> ausblenden   (die Ebene zeigt es ausdruecklich nicht)
     *   HIDDEN    -> ausblenden   (fuer diese Ebene nicht vorgesehen)
     *   FORCED    -> sperren      (der Plan bestimmt den Wert)
     *   READONLY  -> sperren      (gemessener Quellwert)
     *   sonst     -> frei
     */
    struct { const char *param; QWidget *w; } bindung[] = {
        { "read.revolutions",   ui->spinRevolutions },
        { "read.require_index", ui->checkUseIndex },
        { "write.precomp",      ui->checkWritePrecomp },
        { "write.verify",       ui->checkXCopyVerify },
        { "rpm",                ui->comboRPM },
        { "encoding",           ui->comboEncoding },
        { "layout.half_tracks", ui->checkHalfTracks },
        { "layout.half_tracks", ui->checkDetectHalfTracks },
        { "hash.enabled",       ui->checkGenerateHash },
        { "geometry.cylinders", ui->spinTracks },
        { "geometry.heads",     ui->spinSides },
        { "gcr.variant",        ui->comboGCRType },
        /* MF-1282: neun weitere, die der Kern seit MF-1232 kennt und die in
         * diesem Formular schon ein Bedienelement haben. Die Zuordnung kommt
         * aus der Karte des GUI-Umbaus (test-gui/gui-gerüst/zuordnung), nicht
         * aus dem Gefuehl; aufgenommen wurde nur, was BEIDES hat — einen
         * Eintrag in `k_param[]` und ein vorhandenes Element. Was der Kern
         * nicht fuehrt (die acht `plan.*`-Achsen, die zehn `a8.*`), steht
         * ausdruecklich NICHT hier: eine Bindung an einen Parameter, den es
         * nicht gibt, waere eine Zusage ohne Gegenstueck (MF-767). */
        { "geometry.sectors",         ui->spinSectors },
        { "geometry.sector_size",     ui->comboSectorSize },
        { "layout.active_head",       ui->comboXCopySides },
        { "source.format",            ui->comboFormat },
        { "flux.revolution_policy",   ui->comboFluxMerge },
        { "flux.sample_clock",        ui->comboSampleRate },
        { "preserve_weak_bits",       ui->checkWeakBits },
        { "evidence.hash_algorithms", ui->checkHashSha512 },
        /* NICHT gebunden, und der Grund ist gemessen:
         * `gcr.preserve_raw_nibbles` verlangt `UFT_CAP_GCR`, und
         * `copyPlanCaps()` setzt diese Flagge NIE — der Reiter kann sie
         * nicht messen, das steht dort ausdruecklich. Eine Bindung wuerde
         * `checkDecodeGCR` dauerhaft unsichtbar machen: ein Bedienelement,
         * das der Bediener nie wiedersieht. Dasselbe gilt fuer alles, was
         * `UFT_CAP_FILESYSTEM` oder `UFT_CAP_CBM_BAM` verlangt.
         *
         * BEFUND, aelter als diese Aenderung: `gcr.variant` -> `comboGCRType`
         * oben in dieser Tafel ist genau dieser Fall und steht seit MF-1234
         * darin — die GCR-Verfahrenswahl ist damit unerreichbar. Der Test
         * `test_format_tab_bindung` nagelt das fest, statt es zu beheben:
         * behoben waere es erst, wenn der Reiter GCR messen kann, und das
         * ist eine Entwurfsfrage (welche Quelle sagt es?), keine Zeile hier. */
    };

    for (const auto &b : bindung) {
        if (!b.w) continue;
        const char *wert = nullptr;
        /* MF-1234: mit den Faehigkeiten des Formats. Was es nicht
         * kann, verschwindet - ein Regler ohne Bedeutung ist keine
         * Einstellung, sondern eine Irrefuehrung. */
        const uft_copy_pstate_t z =
            uft_copy_param_state_caps(&plan, caps, b.param, &wert);
        /* MF-1282: und die Beschriftung geht mit. Vorher blieb sie stehen
         * und zeigte auf nichts — gemessen bei DOSCopy, wo die ganze
         * Geometrie verschwindet und „Tracks:", „Sides:", „Sectors:" und
         * „Size:" uebrig blieben. */
        QWidget *besch = beschriftungVon(b.w);
        const bool sichtbar = !(z == UFT_PSTATE_FORBIDDEN || z == UFT_PSTATE_HIDDEN);
        if (besch) besch->setVisible(sichtbar);
        switch (z) {
            case UFT_PSTATE_FORBIDDEN:
            case UFT_PSTATE_HIDDEN:
                b.w->setVisible(false);
                break;
            case UFT_PSTATE_FORCED:
                b.w->setVisible(true);
                b.w->setEnabled(false);
                b.w->setToolTip(tr("Vom Kopierplan festgelegt: %1")
                                    .arg(QString::fromUtf8(wert ? wert : "")));
                break;
            case UFT_PSTATE_READONLY:
                b.w->setVisible(true);
                b.w->setEnabled(false);
                b.w->setToolTip(tr("Gemessener Quellwert, nicht einstellbar."));
                break;
            default:
                b.w->setVisible(true);
                b.w->setEnabled(true);
                b.w->setToolTip(QString());
                break;
        }
    }

    /* MF-1238: die beiden neuen Anzeigen haengen am selben Plan wie
     * alles andere — also werden sie hier nachgezogen und nicht an
     * jedem einzelnen Aenderungsweg einzeln. */
    zeigeCaps();
    /* `isHidden()` und nicht `isVisible()`: ein Widget in einem nie
     * gezeigten Fenster ist NICHT sichtbar, auch wenn es niemand
     * versteckt hat — mit `isVisible()` haenge die Zusage am
     * Fensterzustand statt am Aufklappen (gemessen im Offscreen-Lauf). */
    if (ui->textPlanJson && !ui->textPlanJson->isHidden())
        ui->textPlanJson->setPlainText(planJson());
}

void FormatTab::onCopyPlanChanged(int index) {
    Q_UNUSED(index);
    applyCopyPlan();
    emit formatSettingsChanged();
}

void FormatTab::onCopyPlanToggled(bool checked) {
    Q_UNUSED(checked);
    applyCopyPlan();
    emit formatSettingsChanged();
}

void FormatTab::onCopyModeChanged(int index) {
    Q_UNUSED(index);
    emit formatSettingsChanged();
}

void FormatTab::onAllTracksToggled(bool checked) {
    updateXCopyTrackRange(!checked);
    emit formatSettingsChanged();
}

void FormatTab::updateXCopyTrackRange(bool enabled) {
    Q_UNUSED(enabled);
    // UI elements removed in simplified layout
}

void FormatTab::updateXCopyModeOptions(const QString& mode) {
    bool isFluxMode = (mode == "Flux");
    Q_UNUSED(isFluxMode);  // Reserved for future flux-specific XCopy options
    
    // Flux settings handled by embedded panel
    
    // Panel handles its own styling
}

// ============================================================================
// NIBBLE Dependencies
// ============================================================================

void FormatTab::onGCRTypeChanged(int index) {
    QString gcrType = ui->comboGCRType->itemText(index);
    updateNibbleOptions(gcrType);
    emit formatSettingsChanged();
}

void FormatTab::updateNibbleOptions(const QString& gcrType) {
    bool enabled = (gcrType != "Off" && !gcrType.isEmpty());
    
    ui->checkDecodeGCR->setEnabled(enabled);
    // Handled by flux panel
    ui->checkHalfTracks->setEnabled(enabled);
    
    QString style = enabled ? "" : "color: gray;";
    ui->checkDecodeGCR->setStyleSheet(style);
    // Handled by flux panel
    ui->checkHalfTracks->setStyleSheet(style);
    
    // Options set based on GCR type
}

// ============================================================================
// WRITE Dependencies
// ============================================================================

void FormatTab::onRetryErrorsToggled(bool checked) {
    updateRetryOptions(checked);
    emit formatSettingsChanged();
}

void FormatTab::updateRetryOptions(bool enabled) {
    Q_UNUSED(enabled);
    // UI elements removed in simplified layout
}

// ============================================================================
// LOGGING Dependencies
// ============================================================================

void FormatTab::onLogToFileToggled(bool checked) {
    updateLogFileOptions(checked);
    emit formatSettingsChanged();
}

void FormatTab::updateLogFileOptions(bool enabled) {
    ui->editLogPath->setEnabled(enabled);
    ui->btnBrowseLog->setEnabled(enabled);
    ui->checkLogTimestamps->setEnabled(enabled);
    ui->checkVerboseLog->setEnabled(enabled);
}

// ============================================================================
// FORENSIC Dependencies
// ============================================================================

void FormatTab::onValidateStructureToggled(bool checked) {
    updateForensicValidation(checked);
    emit formatSettingsChanged();
}

void FormatTab::onReportFormatChanged(int index) {
    Q_UNUSED(index);
    // Report format combo removed in simplified layout
    emit formatSettingsChanged();
}

void FormatTab::updateForensicValidation(bool enabled) {
    Q_UNUSED(enabled);
    // Validation checkbox removed in simplified layout
}

void FormatTab::updateForensicReport(const QString& format) {
    Q_UNUSED(format);
    // Report format removed in simplified layout
}

// ============================================================================
// PLL Dependencies
// ============================================================================

void FormatTab::onAdaptivePLLToggled(bool checked) {
    updatePLLOptions(checked);
    emit formatSettingsChanged();
}

void FormatTab::onPreserveTimingToggled(bool /*checked*/) {
    emit formatSettingsChanged();
}

void FormatTab::updatePLLOptions(bool enabled) {
    Q_UNUSED(enabled);
    // Advanced PLL settings removed in simplified layout
}

void FormatTab::updateFluxOptions(bool /* isFluxFormat */) {
    // Use embedded panels
    // Flux/PLL options handled by UI widgets and Advanced dialogs
}

void FormatTab::updateGCROptions(bool isGCRFormat) {
    ui->comboGCRType->setEnabled(isGCRFormat);
    ui->checkDecodeGCR->setEnabled(isGCRFormat);
}

// ============================================================================
// PROTECTION Dependencies
// ============================================================================

void FormatTab::onDetectAllToggled(bool checked) {
    syncProtectionWidgets(checked);
    emit protectionSettingsChanged();
}

void FormatTab::onPlatformChanged(int /*index*/) {
    emit protectionSettingsChanged();
}

void FormatTab::onProtectionCheckChanged() {
    emit protectionSettingsChanged();
}

void FormatTab::syncProtectionWidgets(bool detectAll) {
    bool enableIndividual = !detectAll;
    
    ui->checkDetectWeakBitsProt->setEnabled(enableIndividual);
    ui->checkDetectLongTracks->setEnabled(enableIndividual);
    ui->checkDetectHalfTracks->setEnabled(enableIndividual);
    ui->checkDetectTiming->setEnabled(enableIndividual);
    ui->checkDetectNoFlux->setEnabled(enableIndividual);
    ui->checkDetectCustomSync->setEnabled(enableIndividual);
    
    if (detectAll) {
        ui->checkDetectWeakBitsProt->setChecked(true);
        ui->checkDetectLongTracks->setChecked(true);
        ui->checkDetectHalfTracks->setChecked(true);
        ui->checkDetectTiming->setChecked(true);
        ui->checkDetectNoFlux->setChecked(true);
        ui->checkDetectCustomSync->setChecked(true);
    }
}

// ============================================================================
// Protection Settings API
// ============================================================================

uint32_t FormatTab::getProtectionFlags() const {
    uint32_t flags = 0;
    
    if (ui->checkDetectAll->isChecked()) {
        flags = UFT_PROT_ANAL_ALL;
    } else {
        if (ui->checkDetectWeakBitsProt->isChecked()) flags |= UFT_PROT_ANAL_WEAK_BITS;
        if (ui->checkDetectLongTracks->isChecked())   flags |= UFT_PROT_ANAL_TIMING;
        if (ui->checkDetectHalfTracks->isChecked())   flags |= UFT_PROT_ANAL_HALF_TRACKS;
        if (ui->checkDetectTiming->isChecked())       flags |= UFT_PROT_ANAL_TIMING;
        if (ui->checkDetectNoFlux->isChecked())       flags |= UFT_PROT_ANAL_WEAK_BITS;
        if (ui->checkDetectCustomSync->isChecked())   flags |= UFT_PROT_ANAL_SIGNATURES;
    }
    
    if (flags == 0) flags = UFT_PROT_ANAL_QUICK;
    
    return flags;
}

uft_platform_t FormatTab::getPlatformHint() const {
    int idx = ui->comboPlatform->currentIndex();
    switch (idx) {
        case 0: return UFT_PLATFORM_UNKNOWN;
        case 1: return UFT_PLATFORM_C64;
        case 2: return UFT_PLATFORM_AMIGA;
        case 3: return UFT_PLATFORM_ATARI_ST;
        case 4: return UFT_PLATFORM_APPLE_II;
        case 5: return UFT_PLATFORM_PC_DOS;
        default: return UFT_PLATFORM_UNKNOWN;
    }
}

bool FormatTab::isPreserveProtection() const {
    return ui->radioPreserve->isChecked();
}

void FormatTab::getProtectionConfig(uft_prot_config_t* config) const {
    if (!config) return;
    
    uft_prot_config_init(config);
    config->flags = getProtectionFlags();
    config->platform_hint = getPlatformHint();
    config->confidence_threshold = 70;
}

// ============================================================================
// Format Settings API
// ============================================================================

QString FormatTab::getSelectedSystem() const {
    return ui->comboSystem->currentText();
}

QString FormatTab::getSelectedFormat() const {
    return ui->comboFormat->currentText();
}

QString FormatTab::getSelectedVersion() const {
    return ui->comboVersion->currentText();
}

// ============================================================================
// Settings Persistence
// ============================================================================

void FormatTab::loadSettings() {
    QSettings settings;
    settings.beginGroup(SETTINGS_GROUP);
    
    int sysIdx = settings.value("system", 0).toInt();
    if (sysIdx < ui->comboSystem->count()) {
        ui->comboSystem->setCurrentIndex(sysIdx);
    }
    
    ui->checkAllTracks->setChecked(settings.value("allTracks", true).toBool());
    ui->checkDetectAll->setChecked(settings.value("detectAll", true).toBool());
    ui->comboPlatform->setCurrentIndex(settings.value("platform", 0).toInt());
    ui->radioPreserve->setChecked(settings.value("preserve", true).toBool());
    ui->radioRemove->setChecked(!settings.value("preserve", true).toBool());
    // PLL settings handled by embedded panel
    ui->checkLogToFile->setChecked(settings.value("logToFile", false).toBool());
    
    
    // Read Options
    
    // Write Options
    
    settings.endGroup();
    
    setupInitialState();
}

void FormatTab::saveSettings() {
    QSettings settings;
    settings.beginGroup(SETTINGS_GROUP);
    
    settings.setValue("system", ui->comboSystem->currentIndex());
    settings.setValue("format", ui->comboFormat->currentIndex());
    settings.setValue("allTracks", ui->checkAllTracks->isChecked());
    settings.setValue("gcrType", ui->comboGCRType->currentIndex());
    settings.setValue("detectAll", ui->checkDetectAll->isChecked());
    settings.setValue("platform", ui->comboPlatform->currentIndex());
    settings.setValue("preserve", ui->radioPreserve->isChecked());
    // PLL settings handled by embedded panel
    settings.setValue("logToFile", ui->checkLogToFile->isChecked());
    
    // Read Options
    
    // Write Options
    
    settings.endGroup();
    settings.sync();
}

// ============================================================================
// Preset Management
// ============================================================================

void FormatTab::setupBuiltinPresets() {
    // Default preset
    Preset defPreset;
    defPreset.name = tr("(Default)");
    defPreset.system = "Commodore 64/128";
    defPreset.format = "D64";
    defPreset.version = "Standard";
    defPreset.encoding = "GCR";
    defPreset.tracks = 35;
    defPreset.heads = 1;
    defPreset.density = "DD";
    defPreset.halfTracks = false;
    defPreset.preserveTiming = true;
    defPreset.adaptivePLL = true;
    defPreset.copyMode = "Sector";
    defPreset.gcrType = "C64";
    defPreset.detectProtection = true;
    m_presets["(Default)"] = defPreset;
    
    // C64 Preservation preset
    Preset c64Pres;
    c64Pres.name = tr("C64 Preservation");
    c64Pres.system = "Commodore 64/128";
    c64Pres.format = "G64";
    c64Pres.version = "G64 v1.2";
    c64Pres.encoding = "GCR";
    c64Pres.tracks = 42;
    c64Pres.heads = 1;
    c64Pres.density = "DD";
    c64Pres.halfTracks = true;
    c64Pres.preserveTiming = true;
    c64Pres.adaptivePLL = true;
    c64Pres.copyMode = "Flux";
    c64Pres.gcrType = "C64";
    c64Pres.detectProtection = true;
    m_presets["C64 Preservation"] = c64Pres;
    
    // Amiga OCS/ECS preset
    Preset amigaOCS;
    amigaOCS.name = tr("Amiga OCS/ECS");
    amigaOCS.system = "Amiga";
    amigaOCS.format = "ADF";
    amigaOCS.version = "OFS";
    amigaOCS.encoding = "MFM";
    amigaOCS.tracks = 80;
    amigaOCS.heads = 2;
    amigaOCS.density = "DD";
    amigaOCS.halfTracks = false;
    amigaOCS.preserveTiming = true;
    amigaOCS.adaptivePLL = true;
    amigaOCS.copyMode = "Sector";
    amigaOCS.gcrType = "Off";
    amigaOCS.detectProtection = true;
    m_presets["Amiga OCS/ECS"] = amigaOCS;
    
    // Amiga Preservation preset
    Preset amigaPres;
    amigaPres.name = tr("Amiga Preservation");
    amigaPres.system = "Amiga";
    amigaPres.format = "IPF";
    amigaPres.version = "IPF v2";
    amigaPres.encoding = "MFM";
    amigaPres.tracks = 84;
    amigaPres.heads = 2;
    amigaPres.density = "DD";
    amigaPres.halfTracks = false;
    amigaPres.preserveTiming = true;
    amigaPres.adaptivePLL = true;
    amigaPres.copyMode = "Flux";
    amigaPres.gcrType = "Off";
    amigaPres.detectProtection = true;
    m_presets["Amiga Preservation"] = amigaPres;
    
    // Atari ST preset
    Preset atariST;
    atariST.name = tr("Atari ST");
    atariST.system = "Atari ST/STE";
    atariST.format = "ST";
    atariST.version = "Standard";
    atariST.encoding = "MFM";
    atariST.tracks = 80;
    atariST.heads = 2;
    atariST.density = "DD";
    atariST.halfTracks = false;
    atariST.preserveTiming = false;
    atariST.adaptivePLL = true;
    atariST.copyMode = "Sector";
    atariST.gcrType = "Off";
    atariST.detectProtection = true;
    m_presets["Atari ST"] = atariST;
    
    // PC DOS 1.44MB preset
    Preset pcHD;
    pcHD.name = tr("PC DOS 1.44MB");
    pcHD.system = "PC/DOS";
    pcHD.format = "IMG";
    pcHD.version = "HD 1.44M";
    pcHD.encoding = "MFM";
    pcHD.tracks = 80;
    pcHD.heads = 2;
    pcHD.density = "HD";
    pcHD.halfTracks = false;
    pcHD.preserveTiming = false;
    pcHD.adaptivePLL = true;
    pcHD.copyMode = "Sector";
    pcHD.gcrType = "Off";
    pcHD.detectProtection = false;
    m_presets["PC DOS 1.44MB"] = pcHD;
    
    // PC DOS 720K preset
    Preset pcDD;
    pcDD.name = tr("PC DOS 720K");
    pcDD.system = "PC/DOS";
    pcDD.format = "IMG";
    pcDD.version = "DD 720K";
    pcDD.encoding = "MFM";
    pcDD.tracks = 80;
    pcDD.heads = 2;
    pcDD.density = "DD";
    pcDD.halfTracks = false;
    pcDD.preserveTiming = false;
    pcDD.adaptivePLL = true;
    pcDD.copyMode = "Sector";
    pcDD.gcrType = "Off";
    pcDD.detectProtection = false;
    m_presets["PC DOS 720K"] = pcDD;
    
    // Apple II preset
    Preset apple2;
    apple2.name = tr("Apple II DOS 3.3");
    apple2.system = "Apple II";
    apple2.format = "DSK";
    apple2.version = "DOS 3.3";
    apple2.encoding = "GCR";
    apple2.tracks = 35;
    apple2.heads = 1;
    apple2.density = "DD";
    apple2.halfTracks = false;
    apple2.preserveTiming = true;
    apple2.adaptivePLL = true;
    apple2.copyMode = "Sector";
    apple2.gcrType = "Apple";
    apple2.detectProtection = true;
    m_presets["Apple II DOS 3.3"] = apple2;
    
    // ZX Spectrum preset
    Preset zxSpec;
    zxSpec.name = tr("ZX Spectrum +3");
    zxSpec.system = "ZX Spectrum";
    zxSpec.format = "DSK";
    zxSpec.version = "Extended";
    zxSpec.encoding = "MFM";
    zxSpec.tracks = 40;
    zxSpec.heads = 1;
    zxSpec.density = "DD";
    zxSpec.halfTracks = false;
    zxSpec.preserveTiming = true;
    zxSpec.adaptivePLL = true;
    zxSpec.copyMode = "Sector";
    zxSpec.gcrType = "Off";
    zxSpec.detectProtection = true;
    m_presets["ZX Spectrum +3"] = zxSpec;
    
    // Flux Analysis preset
    Preset fluxAnalysis;
    fluxAnalysis.name = tr("Flux Analysis");
    fluxAnalysis.system = "Flux/Raw";
    fluxAnalysis.format = "SCP";
    fluxAnalysis.version = "v2.4";
    fluxAnalysis.encoding = "Raw Flux";
    fluxAnalysis.tracks = 84;
    fluxAnalysis.heads = 2;
    fluxAnalysis.density = "Auto";
    fluxAnalysis.halfTracks = true;
    fluxAnalysis.preserveTiming = true;
    fluxAnalysis.adaptivePLL = true;
    fluxAnalysis.copyMode = "Flux";
    fluxAnalysis.gcrType = "Off";
    fluxAnalysis.detectProtection = true;
    m_presets["Flux Analysis"] = fluxAnalysis;
    
    // Update combo
    updatePresetCombo();
}

void FormatTab::updatePresetCombo() {
    ui->comboPreset->blockSignals(true);
    ui->comboPreset->clear();
    
    // Add builtin presets first
    QStringList builtins = {"(Default)", "C64 Preservation", "Amiga OCS/ECS", 
                           "Amiga Preservation", "Atari ST", "PC DOS 1.44MB",
                           "PC DOS 720K", "Apple II DOS 3.3", "ZX Spectrum +3",
                           "Flux Analysis"};
    
    for (const QString& name : builtins) {
        if (m_presets.contains(name)) {
            ui->comboPreset->addItem(name);
        }
    }
    
    // Add user presets (those not in builtins)
    for (auto it = m_presets.begin(); it != m_presets.end(); ++it) {
        if (!builtins.contains(it.key())) {
            ui->comboPreset->addItem("📁 " + it.key());
        }
    }
    
    ui->comboPreset->blockSignals(false);
}

void FormatTab::onPresetChanged(int index) {
    Q_UNUSED(index);
    // Preview only - don't apply until Load clicked
}

void FormatTab::onLoadPreset() {
    QString name = ui->comboPreset->currentText();
    // Remove user prefix if present
    if (name.startsWith("📁 ")) {
        name = name.mid(3);
    }
    
    if (!m_presets.contains(name)) {
        return;
    }
    
    applyPreset(m_presets[name]);
}

void FormatTab::onSavePreset() {
    QString name = ui->comboPreset->currentText();
    // Remove user prefix if present
    if (name.startsWith("📁 ")) {
        name = name.mid(3);
    }
    
    // Don't overwrite builtins - create new
    QStringList builtins = {"(Default)", "C64 Preservation", "Amiga OCS/ECS", 
                           "Amiga Preservation", "Atari ST", "PC DOS 1.44MB",
                           "PC DOS 720K", "Apple II DOS 3.3", "ZX Spectrum +3",
                           "Flux Analysis"};
    
    if (builtins.contains(name)) {
        // Ask for new name
        bool ok;
        QString newName = QInputDialog::getText(this, tr("Save Preset"),
            tr("Enter preset name:"), QLineEdit::Normal,
            tr("My Preset"), &ok);
        
        if (!ok || newName.isEmpty()) {
            return;
        }
        name = newName;
    }
    
    // Get current settings
    Preset preset = getCurrentSettings();
    preset.name = name;
    
    m_presets[name] = preset;
    savePresetsToFile();
    updatePresetCombo();
    
    // Select the saved preset
    int idx = ui->comboPreset->findText("📁 " + name);
    if (idx >= 0) {
        ui->comboPreset->setCurrentIndex(idx);
    }
}

FormatTab::Preset FormatTab::getCurrentSettings() const {
    Preset p;
    p.system = ui->comboSystem->currentText();
    p.format = ui->comboFormat->currentText();
    p.version = ui->comboVersion->currentText();
    p.encoding = ui->comboEncoding->currentText();
    p.tracks = ui->spinTracks->value();
    p.heads = 2;  // Default
    p.density = "DD";  // Default
    p.halfTracks = ui->checkHalfTracks->isChecked();
    p.preserveTiming = true; // From flux panel
    p.adaptivePLL = true; // From embedded panel
    p.gcrType = ui->comboGCRType->currentText();
    p.detectProtection = ui->checkDetectAll->isChecked();
    return p;
}

void FormatTab::applyPreset(const Preset& preset) {
    // Block signals during update
    ui->comboSystem->blockSignals(true);
    ui->comboFormat->blockSignals(true);
    
    // Set system
    int sysIdx = ui->comboSystem->findText(preset.system, Qt::MatchContains);
    if (sysIdx >= 0) {
        ui->comboSystem->setCurrentIndex(sysIdx);
        populateFormatsForSystem(preset.system);
    }
    
    // Set format
    int fmtIdx = ui->comboFormat->findText(preset.format, Qt::MatchContains);
    if (fmtIdx >= 0) {
        ui->comboFormat->setCurrentIndex(fmtIdx);
        populateVersionsForFormat(preset.format);
    }
    
    // Set version
    int verIdx = ui->comboVersion->findText(preset.version, Qt::MatchContains);
    if (verIdx >= 0) {
        ui->comboVersion->setCurrentIndex(verIdx);
    }
    
    // Set encoding
    int encIdx = ui->comboEncoding->findText(preset.encoding, Qt::MatchContains);
    if (encIdx >= 0) {
        ui->comboEncoding->setCurrentIndex(encIdx);
    }
    
    // Set other values
    ui->spinTracks->setValue(preset.tracks);
    
    ui->checkHalfTracks->setChecked(preset.halfTracks);
    // Preserve timing handled by flux panel
    // PLL preset handled by embedded panel
    
    if (false) { // Copy mode removed
    }
    
    int gcrIdx = ui->comboGCRType->findText(preset.gcrType, Qt::MatchContains);
    if (gcrIdx >= 0) {
        ui->comboGCRType->setCurrentIndex(gcrIdx);
    }
    
    ui->checkDetectAll->setChecked(preset.detectProtection);
    
    // Restore signals
    ui->comboSystem->blockSignals(false);
    ui->comboFormat->blockSignals(false);
    
    // Update dependent options
    updateFormatSpecificOptions(preset.format);
    updateNibbleOptions(preset.gcrType);
    updatePLLOptions(preset.adaptivePLL);
    
    emit formatSettingsChanged();
}

QStringList FormatTab::getPresetNames() const {
    return m_presets.keys();
}

void FormatTab::loadPresetsFromFile() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(path);
    m_presetsFilePath = path + "/presets.json";
    
    QFile file(m_presetsFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;  // No user presets yet
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isObject()) {
        return;
    }
    
    QJsonObject root = doc.object();
    for (auto it = root.begin(); it != root.end(); ++it) {
        QJsonObject obj = it.value().toObject();
        Preset p;
        p.name = it.key();
        p.system = obj["system"].toString();
        p.format = obj["format"].toString();
        p.version = obj["version"].toString();
        p.encoding = obj["encoding"].toString();
        p.tracks = obj["tracks"].toInt(80);
        p.heads = obj["heads"].toInt(2);
        p.density = obj["density"].toString("DD");
        p.halfTracks = obj["halfTracks"].toBool(false);
        p.preserveTiming = obj["preserveTiming"].toBool(true);
        p.adaptivePLL = obj["adaptivePLL"].toBool(true);
        p.copyMode = obj["copyMode"].toString("Sector");
        p.gcrType = obj["gcrType"].toString("Off");
        p.detectProtection = obj["detectProtection"].toBool(true);
        
        m_presets[p.name] = p;
    }
}

void FormatTab::savePresetsToFile() {
    QJsonObject root;
    
    // Only save user presets (not builtins)
    QStringList builtins = {"(Default)", "C64 Preservation", "Amiga OCS/ECS", 
                           "Amiga Preservation", "Atari ST", "PC DOS 1.44MB",
                           "PC DOS 720K", "Apple II DOS 3.3", "ZX Spectrum +3",
                           "Flux Analysis"};
    
    for (auto it = m_presets.begin(); it != m_presets.end(); ++it) {
        if (builtins.contains(it.key())) {
            continue;  // Skip builtins
        }
        
        const Preset& p = it.value();
        QJsonObject obj;
        obj["system"] = p.system;
        obj["format"] = p.format;
        obj["version"] = p.version;
        obj["encoding"] = p.encoding;
        obj["tracks"] = p.tracks;
        obj["heads"] = p.heads;
        obj["density"] = p.density;
        obj["halfTracks"] = p.halfTracks;
        obj["preserveTiming"] = p.preserveTiming;
        obj["adaptivePLL"] = p.adaptivePLL;
        obj["copyMode"] = p.copyMode;
        obj["gcrType"] = p.gcrType;
        obj["detectProtection"] = p.detectProtection;
        
        root[p.name] = obj;
    }
    
    QFile file(m_presetsFilePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson());
        file.close();
    }
}

// ============================================================================
// Read Options Slots
// ============================================================================

void FormatTab::onReadSpeedChanged(int index)
{
    Q_UNUSED(index);
    emit readOptionsChanged();
}

void FormatTab::onIgnoreReadErrorsChanged(bool checked)
{
    Q_UNUSED(checked);
    emit readOptionsChanged();
}

void FormatTab::onFastErrorSkipChanged(bool checked)
{
    Q_UNUSED(checked);
    emit readOptionsChanged();
}

void FormatTab::onAdvancedScanningChanged(bool checked)
{
    Q_UNUSED(checked);
    emit readOptionsChanged();
}

void FormatTab::onScanFactorChanged(int value)
{
    Q_UNUSED(value);
    emit readOptionsChanged();
}

void FormatTab::onReadTimingDataChanged(bool checked)
{
    // If reading timing data, sub-channel might be useful too
    Q_UNUSED(checked);
    emit readOptionsChanged();
}

void FormatTab::onDPMAnalysisChanged(bool /* checked */)
{
    // Enable/disable the DPM accuracy combo
    emit readOptionsChanged();
}

void FormatTab::onReadSubChannelChanged(bool checked)
{
    Q_UNUSED(checked);
    emit readOptionsChanged();
}

// ============================================================================
// Write Options Slots
// ============================================================================

void FormatTab::onVerifyAfterWriteChanged(bool checked)
{
    Q_UNUSED(checked);
    emit writeOptionsChanged();
}

void FormatTab::onIgnoreWriteErrorsChanged(bool checked)
{
    // Warning: ignoring write errors can corrupt data
    if (checked) {
        qWarning() << "Warning: Ignoring write errors may result in corrupted output";
    }
    emit writeOptionsChanged();
}

void FormatTab::onWriteTimingDataChanged(bool checked)
{
    Q_UNUSED(checked);
    emit writeOptionsChanged();
}

void FormatTab::onCorrectSubChannelChanged(bool checked)
{
    Q_UNUSED(checked);
    emit writeOptionsChanged();
}

// ============================================================================
// Read/Write Options Getters
// ============================================================================

FormatTab::ReadOptions FormatTab::getReadOptions() const
{
    ReadOptions opts = {};
    
    
    return opts;
}

FormatTab::WriteOptions FormatTab::getWriteOptions() const
{
    WriteOptions opts = {};
    
    
    return opts;
}

// ============================================================================
// XCopy Handlers
// ============================================================================


// ============================================================================
// Logging Handlers
// ============================================================================

void FormatTab::onBrowseLogPath() {
    QString path = QFileDialog::getSaveFileName(this,
        tr("Select Log File"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        tr("Log Files (*.log *.txt);;All Files (*)"));
    
    if (!path.isEmpty()) {
        ui->editLogPath->setText(path);
        emit formatSettingsChanged();
    }
}

// ============================================================================
// ADVANCED DIALOG HANDLERS
// ============================================================================

void FormatTab::onNibbleAdvanced() {
    NibbleAdvancedDialog dlg(this);
    dlg.setParams(m_nibbleAdvParams);
    
    if (dlg.exec() == QDialog::Accepted) {
        m_nibbleAdvParams = dlg.getParams();
        emit formatSettingsChanged();
    }
}

void FormatTab::onGw2DmkOpenClicked()
{
    // Create dialog with GW→DMK Panel
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle(tr("GW→DMK Direct Read (TRS-80)"));
    dlg->setMinimumSize(800, 600);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    
    QVBoxLayout *layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(4, 4, 4, 4);
    
    UftGw2DmkPanel *panel = new UftGw2DmkPanel(dlg);
    layout->addWidget(panel);
    
    // Apply preset from combo if available
    QString preset = ui->comboGw2DmkPreset->currentText();
    if (preset.contains("SSSD")) {
        panel->setPreset(0);  // Model I/III SSSD
    } else if (preset.contains("SSDD")) {
        panel->setPreset(1);  // Model I/III SSDD
    } else if (preset.contains("Model 4")) {
        panel->setPreset(2);  // Model 4 DSDD
    }
    
    dlg->show();
}
