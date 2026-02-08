/**
 * @file uft_hardware_panel.h
 * @brief Hardware Panel - Controller Selection and Configuration
 */

#ifndef UFT_HARDWARE_PANEL_H
#define UFT_HARDWARE_PANEL_H

#include <QWidget>
#include <QGroupBox>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QStackedWidget>
#include <QLineEdit>

class UftHardwarePanel : public QWidget
{
    Q_OBJECT

public:
    explicit UftHardwarePanel(QWidget *parent = nullptr);

    enum ControllerType {
        CONTROLLER_NONE = 0,
        CONTROLLER_GREASEWEAZLE,
        CONTROLLER_KRYOFLUX,
        CONTROLLER_FLUXENGINE,
        CONTROLLER_SUPERCARD_PRO,
        CONTROLLER_CATWEASEL,
        CONTROLLER_APPLESAUCE,
        CONTROLLER_FC5025,
        CONTROLLER_PAULINE,
        CONTROLLER_RAWREAD,
        CONTROLLER_COUNT
    };

    struct HardwareParams {
        /* Controller */
        ControllerType controller;
        QString device_path;
        QString firmware_version;
        
        /* Drive */
        int drive_select;           /* 0=A, 1=B, 2=External */
        int drive_type;             /* 0=3.5", 1=5.25", 2=8" */
        bool double_step;
        int step_delay_us;
        int settle_time_ms;
        int head_load_time_ms;
        
        /* Motor */
        int motor_on_delay_ms;
        int motor_off_delay_ms;
        double rpm_target;
        bool rpm_compensation;
        
        /* Index */
        bool use_index;
        double index_offset_us;
        int index_timeout_ms;
        
        /* Advanced */
        int sample_rate_mhz;
        bool filter_enabled;
        int filter_frequency;
        bool tpi_40;                /* 40 TPI drive */
    };

    HardwareParams getParams() const;
    void setParams(const HardwareParams &params);

signals:
    void paramsChanged();
    void controllerConnected(const QString &name);
    void controllerDisconnected();
    void driveDetected(int type);

public slots:
    void detectHardware();
    void connectController();
    void disconnectController();
    void calibrateDrive();
    void testDrive();

private:
    void setupUi();
    void createControllerGroup();
    void createDriveGroup();
    void createMotorGroup();
    void createIndexGroup();
    void createAdvancedGroup();
    void createStatusGroup();
    void createControllerSettings();

    /* Controller */
    QGroupBox *m_controllerGroup;
    QComboBox *m_controllerType;
    QComboBox *m_devicePath;
    QPushButton *m_detectButton;
    QPushButton *m_connectButton;
    QLabel *m_firmwareLabel;
    QLabel *m_statusLabel;
    
    /* Drive */
    QGroupBox *m_driveGroup;
    QComboBox *m_driveSelect;
    QComboBox *m_driveType;
    QCheckBox *m_doubleStep;
    QSpinBox *m_stepDelay;
    QSpinBox *m_settleTime;
    QSpinBox *m_headLoadTime;
    
    /* Motor */
    QGroupBox *m_motorGroup;
    QSpinBox *m_motorOnDelay;
    QSpinBox *m_motorOffDelay;
    QDoubleSpinBox *m_rpmTarget;
    QCheckBox *m_rpmCompensation;
    
    /* Index */
    QGroupBox *m_indexGroup;
    QCheckBox *m_useIndex;
    QDoubleSpinBox *m_indexOffset;
    QSpinBox *m_indexTimeout;
    
    /* Advanced */
    QGroupBox *m_advancedGroup;
    QComboBox *m_sampleRate;
    QCheckBox *m_filterEnabled;
    QSpinBox *m_filterFrequency;
    QCheckBox *m_tpi40;
    
    /* Controller-Specific Settings */
    QStackedWidget *m_controllerSettings;
    QWidget *m_greaseweazleSettings;
    QWidget *m_kryofluxSettings;
    QWidget *m_fluxengineSettings;
    QWidget *m_scpSettings;
    
    /* Status */
    QListWidget *m_logList;
    QPushButton *m_calibrateButton;
    QPushButton *m_testButton;
};

/* ============================================================================
 * Controller Info
 * ============================================================================ */

struct ControllerInfo {
    const char *name;
    const char *manufacturer;
    const char *usb_vid_pid;
    bool supports_flux_read;
    bool supports_flux_write;
    bool supports_index;
    int max_sample_rate;
};

static const ControllerInfo CONTROLLERS[] = {
    { "KryoFlux",           "KryoFlux",        "03eb:6124", true,  true,  true,  48 },
    { "SuperCard Pro",      "Jim Drew",        "04D8:F8EB", true,  true,  true,  50 },
    { "Catweasel MK4",      "Individual Comp", "PCI",       true,  true,  true,  28 },
    { "Applesauce",         "John Googolplex", "Various",   true,  true,  true,  32 },
    { "FC5025",             "Device Side",     "16C0:06D6", false, false, true,   0 },
    { "Pauline",            "UFT",   "Various",   true,  true,  true,  50 },
    { nullptr, nullptr, nullptr, false, false, false, 0 }
};

#endif /* UFT_HARDWARE_PANEL_H */
