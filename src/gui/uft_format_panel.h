/**
 * @file uft_format_panel.h
 * @brief Format Settings Panel - Geometry, Encoding, Filesystem
 */

#ifndef UFT_FORMAT_PANEL_H
#define UFT_FORMAT_PANEL_H

#include <QWidget>
#include <QTreeWidget>
#include <QGroupBox>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>
#include <QStackedWidget>

class UftFormatPanel : public QWidget
{
    Q_OBJECT

public:
    explicit UftFormatPanel(QWidget *parent = nullptr);

    /* Format Parameters */
    struct FormatParams {
        /* Profile */
        QString profile_name;
        QString system;             /* Amiga, C64, Apple, Atari, PC... */
        
        /* Geometry */
        int     tracks;
        int     sides;
        int     sectors_per_track;
        int     sector_size;
        int     total_sectors;
        int     track_distance;     /* 40 or 80 tracks */
        double  rpm;
        
        /* Encoding */
        QString encoding;           /* MFM, FM, GCR, Apple GCR */
        int     bitrate;            /* kbps */
        QString data_rate;          /* SD, DD, HD, ED */
        int     gap3_length;
        int     pregap_length;
        int     interleave;
        int     skew;
        int     sector_id_start;
        
        /* Filesystem */
        QString filesystem;         /* FAT12, OFS, FFS, CBM DOS, ... */
        QString version;            /* Format version */
        bool    bootable;
        
        /* Output */
        QString output_format;
        QString extension;
    };

    FormatParams getParams() const;
    void setParams(const FormatParams &params);
    void setProfile(const QString &profile);

signals:
    void paramsChanged();
    void profileSelected(const QString &profile);

private:
    void setupUi();
    void createProfileTree();
    void createGeometryGroup();
    void createEncodingGroup();
    void createFilesystemGroup();
    void createOutputGroup();
    void populateProfiles();

    /* Profile Tree */
    QTreeWidget *m_profileTree;
    
    /* Geometry */
    QGroupBox *m_geometryGroup;
    QSpinBox *m_tracks;
    QComboBox *m_sides;
    QSpinBox *m_sectorsPerTrack;
    QComboBox *m_sectorSize;
    QLabel *m_totalSectors;
    QComboBox *m_trackDistance;
    QDoubleSpinBox *m_rpm;
    
    /* Encoding */
    QGroupBox *m_encodingGroup;
    QComboBox *m_encoding;
    QSpinBox *m_bitrate;
    QComboBox *m_dataRate;
    QSpinBox *m_gap3;
    QSpinBox *m_pregap;
    QSpinBox *m_interleave;
    QSpinBox *m_skew;
    QSpinBox *m_sectorIdStart;
    
    /* Filesystem */
    QGroupBox *m_filesystemGroup;
    QComboBox *m_filesystem;
    QComboBox *m_version;
    QCheckBox *m_bootable;
    QLineEdit *m_diskName;
    
    /* Output */
    QGroupBox *m_outputGroup;
    QComboBox *m_outputFormat;
    QLineEdit *m_extension;
    QCheckBox *m_useDefaults;
};

/* ============================================================================
 * Predefined Profiles
 * ============================================================================ */

struct FormatProfile {
    const char *name;
    const char *system;
    int tracks;
    int sides;
    int sectors;
    int sector_size;
    const char *encoding;
    int bitrate;
    const char *filesystem;
};

static const FormatProfile PRESET_PROFILES[] = {
    /* Amiga */
    { "ADF DD (880K)",      "Amiga",     80, 2, 11, 512, "MFM", 250, "OFS/FFS" },
    { "ADF HD (1760K)",     "Amiga",     80, 2, 22, 512, "MFM", 500, "OFS/FFS" },
    
    /* Commodore */
    { "D64 35 Track",       "Commodore", 35, 1, 21, 256, "GCR", 250, "CBM DOS" },
    { "D64 40 Track",       "Commodore", 40, 1, 21, 256, "GCR", 250, "CBM DOS" },
    { "D71 70 Track",       "Commodore", 70, 2, 21, 256, "GCR", 250, "CBM DOS" },
    { "D81 80 Track",       "Commodore", 80, 2, 10, 512, "MFM", 250, "CBM DOS" },
    
    /* Apple II */
    { "DOS 3.3 (140K)",     "Apple II",  35, 1, 16, 256, "GCR", 250, "DOS 3.3" },
    { "ProDOS (140K)",      "Apple II",  35, 1, 16, 256, "GCR", 250, "ProDOS" },
    { "ProDOS 800K",        "Apple II",  80, 2, 12, 512, "GCR", 250, "ProDOS" },
    
    /* Atari 8-Bit */
    { "ATR SD (90K)",       "Atari 8",   40, 1, 18, 128, "FM",  125, "Atari DOS" },
    { "ATR ED (130K)",      "Atari 8",   40, 1, 26, 128, "FM",  125, "Atari DOS" },
    { "ATR DD (180K)",      "Atari 8",   40, 1, 18, 256, "MFM", 250, "Atari DOS" },
    
    /* Atari ST */
    { "ST SS (360K)",       "Atari ST",  80, 1,  9, 512, "MFM", 250, "FAT12" },
    { "ST DS (720K)",       "Atari ST",  80, 2,  9, 512, "MFM", 250, "FAT12" },
    { "ST HD (1440K)",      "Atari ST",  80, 2, 18, 512, "MFM", 500, "FAT12" },
    
    /* IBM PC */
    { "PC 360K",            "IBM PC",    40, 2,  9, 512, "MFM", 250, "FAT12" },
    { "PC 720K",            "IBM PC",    80, 2,  9, 512, "MFM", 250, "FAT12" },
    { "PC 1.2M",            "IBM PC",    80, 2, 15, 512, "MFM", 500, "FAT12" },
    { "PC 1.44M",           "IBM PC",    80, 2, 18, 512, "MFM", 500, "FAT12" },
    { "PC 2.88M",           "IBM PC",    80, 2, 36, 512, "MFM", 1000, "FAT12" },
    
    /* ZX Spectrum */
    { "TRD DS (640K)",      "Spectrum",  80, 2, 16, 256, "MFM", 250, "TR-DOS" },
    
    /* BBC Micro */
    { "SSD 40T (100K)",     "BBC",       40, 1, 10, 256, "FM",  125, "DFS" },
    { "SSD 80T (200K)",     "BBC",       80, 1, 10, 256, "FM",  125, "DFS" },
    { "DSD 80T (400K)",     "BBC",       80, 2, 10, 256, "FM",  125, "DFS" },
    
    /* NEC PC-98 */
    { "D88 2D (320K)",      "PC-98",     40, 2, 16, 256, "MFM", 250, "PC-98" },
    { "D88 2DD (640K)",     "PC-98",     80, 2, 16, 256, "MFM", 250, "PC-98" },
    { "D88 2HD (1.2M)",     "PC-98",     77, 2,  8, 1024, "MFM", 500, "PC-98" },
    
    /* TRS-80 */
    { "TRS-80 SSSD",        "TRS-80",    35, 1, 10, 256, "FM",  125, "TRSDOS" },
    { "TRS-80 DSDD",        "TRS-80",    40, 2, 18, 256, "MFM", 250, "TRSDOS" },
    
    /* Flux */
    { "KryoFlux Stream",    "Flux",      84, 2,  0,   0, "RAW",   0, "None" },
    { "SCP SuperCard Pro",  "Flux",      84, 2,  0,   0, "RAW",   0, "None" },
    { "HFE v1",             "Flux",      80, 2,  0,   0, "RAW",   0, "None" },
    { "HFE v3",             "Flux",      80, 2,  0,   0, "RAW",   0, "None" },
    
    { nullptr, nullptr, 0, 0, 0, 0, nullptr, 0, nullptr }
};

#endif /* UFT_FORMAT_PANEL_H */
