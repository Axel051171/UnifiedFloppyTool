/**
 * @file uft_protection_panel.h
 * @brief Protection Panel - Copy Protection Detection and Analysis
 */

#ifndef UFT_PROTECTION_PANEL_H
#define UFT_PROTECTION_PANEL_H

#include <QWidget>
#include <QGroupBox>
#include <QListWidget>
#include <QTableWidget>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>

class UftProtectionPanel : public QWidget
{
    Q_OBJECT

public:
    explicit UftProtectionPanel(QWidget *parent = nullptr);

    struct ProtectionParams {
        /* Detection */
        bool detect_all;
        bool detect_weak_bits;
        bool detect_long_tracks;
        bool detect_short_tracks;
        bool detect_no_flux;
        bool detect_timing_variance;
        bool detect_half_tracks;
        bool detect_custom_encoding;
        
        /* Platform-Specific */
        bool detect_amiga_protections;
        bool detect_c64_protections;
        bool detect_apple_protections;
        bool detect_atari_protections;
        bool detect_pc_protections;
        
        /* Output */
        bool preserve_protection;
        bool remove_protection;
        bool create_unprotected_copy;
    };

    ProtectionParams getParams() const;
    void setParams(const ProtectionParams &params);

signals:
    void paramsChanged();
    void protectionDetected(const QString &name, const QString &details);

public slots:
    void scanProtection();
    void analyzeSelected();

private:
    void setupUi();
    void createDetectionGroup();
    void createPlatformGroup();
    void createOutputGroup();
    void createProtectionList();
    void createDetailsView();
    void populateProtectionList();

    /* Detection */
    QGroupBox *m_detectionGroup;
    QCheckBox *m_detectAll;
    QCheckBox *m_detectWeakBits;
    QCheckBox *m_detectLongTracks;
    QCheckBox *m_detectShortTracks;
    QCheckBox *m_detectNoFlux;
    QCheckBox *m_detectTimingVariance;
    QCheckBox *m_detectHalfTracks;
    QCheckBox *m_detectCustomEncoding;
    
    /* Platform */
    QGroupBox *m_platformGroup;
    QCheckBox *m_detectAmiga;
    QCheckBox *m_detectC64;
    QCheckBox *m_detectApple;
    QCheckBox *m_detectAtari;
    QCheckBox *m_detectPC;
    
    /* Output */
    QGroupBox *m_outputGroup;
    QCheckBox *m_preserveProtection;
    QCheckBox *m_removeProtection;
    QCheckBox *m_createUnprotected;
    
    /* Known Protections List */
    QListWidget *m_protectionList;
    
    /* Results */
    QTableWidget *m_resultsTable;
    QPlainTextEdit *m_detailsView;
    QLabel *m_statusLabel;
    QPushButton *m_scanButton;
};

/* ============================================================================
 * Known Copy Protection Systems
 * ============================================================================ */

struct ProtectionSystem {
    const char *name;
    const char *platform;
    const char *description;
    const char *signature;
};

static const ProtectionSystem KNOWN_PROTECTIONS[] = {
    /* Amiga */
    { "Rob Northen Copylock",    "Amiga", "Track length/timing protection", "Track 0 timing variance" },
    { "CAPS/SPS",                "Amiga", "Softpres format protection", "IPF signature" },
    { "Timelord",                "Amiga", "Track timing protection", "Variable track lengths" },
    { "Hexalock",                "Amiga", "Track encryption", "Encrypted track data" },
    { "FBI Protection",          "Amiga", "FBI games protection", "Custom track format" },
    { "Gremlin Protection",      "Amiga", "Gremlin games protection", "Half tracks" },
    { "Rainbow Arts",            "Amiga", "Rainbow Arts protection", "Modified sync marks" },
    
    /* Commodore 64 */
    { "V-Max!",                  "C64", "Vorpal protection", "Long tracks, custom sync" },
    { "Rapidlok",                "C64", "Rapidlok protection", "Half tracks, SYNC patterns" },
    { "GMA",                     "C64", "Green Moon Alliance", "Weak bits, timing" },
    { "Fat Track",               "C64", "Fat track protection", "Extended track length" },
    { "Pirateslayer",            "C64", "Pirateslayer protection", "Custom GCR patterns" },
    { "Freeload",                "C64", "Freeload protection", "Custom loader, timing" },
    { "TDP",                     "C64", "The Disk Protector", "Half tracks, density" },
    
    /* Apple II */
    { "Spiradisc",               "Apple II", "Spiraling sectors", "Non-standard interleave" },
    { "Locksmith",               "Apple II", "Locksmith protection", "Half tracks" },
    { "EA Protection",           "Apple II", "Electronic Arts", "Modified address marks" },
    { "Softguard",               "Apple II", "Softguard protection", "Encrypted nibbles" },
    { "ProLock",                 "Apple II", "ProLock protection", "Timing/density" },
    
    /* Atari */
    { "Happy Track",             "Atari", "Happy enhancement", "Extra track data" },
    { "Super Archiver",          "Atari", "Super Archiver format", "Weak sectors" },
    { "APE VAPI",                "Atari", "VAPI protection data", "Timing info in ATX" },
    
    /* PC */
    { "Prolok",                  "PC", "Vault Prolok", "Weak sectors, CRC errors" },
    { "EasyLok",                 "PC", "EasyLok protection", "Custom sector IDs" },
    { "Superlok",                "PC", "Superlok protection", "Long/short sectors" },
    { "Copylock",                "PC", "DOS Copylock", "Weak bits, timing" },
    { "SafeDisc",                "PC", "SafeDisc protection", "Weak sectors" },
    { "SecuROM",                 "PC", "SecuROM protection", "Data position measurement" },
    
    { nullptr, nullptr, nullptr, nullptr }
};

#endif /* UFT_PROTECTION_PANEL_H */
