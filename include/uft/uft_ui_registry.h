/**
 * @file uft_ui_registry.h
 * @brief UI Widget Registry for GUI-Backend Mapping (W-P2-004)
 * 
 * Central registry mapping UI widgets to backend parameters.
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#ifndef UFT_UI_REGISTRY_H
#define UFT_UI_REGISTRY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * UI FILE REGISTRY
 *===========================================================================*/

/**
 * @brief UI file categories
 */
typedef enum {
    UFT_UI_CAT_MAIN,          /**< Main window */
    UFT_UI_CAT_TAB,           /**< Tab pages */
    UFT_UI_CAT_DIALOG,        /**< Dialogs */
    UFT_UI_CAT_WIDGET         /**< Custom widgets */
} uft_ui_category_t;

/**
 * @brief UI file entry
 */
typedef struct {
    const char *filename;     /**< UI file name */
    const char *description;  /**< Description */
    uft_ui_category_t category;
    const char *class_name;   /**< Associated C++ class */
    int widget_count;         /**< Approximate widget count */
} uft_ui_file_t;

/**
 * @brief Registered UI files
 */
static const uft_ui_file_t UFT_UI_FILES[] = {
    /* Main */
    {"mainwindow.ui", "Main Application Window", UFT_UI_CAT_MAIN, "MainWindow", 50},
    {"uft_main_window.ui", "UFT Main Window (v2)", UFT_UI_CAT_MAIN, "UftMainWindow", 45},
    
    /* Tabs */
    {"tab_format.ui", "Format Selection Tab", UFT_UI_CAT_TAB, "TabFormat", 25},
    {"tab_hardware.ui", "Hardware Settings Tab", UFT_UI_CAT_TAB, "TabHardware", 20},
    {"tab_status.ui", "Status/Progress Tab", UFT_UI_CAT_TAB, "TabStatus", 15},
    {"tab_forensic.ui", "Forensic Analysis Tab", UFT_UI_CAT_TAB, "TabForensic", 30},
    {"tab_tools.ui", "Tools Tab", UFT_UI_CAT_TAB, "TabTools", 20},
    {"tab_workflow.ui", "Workflow Tab", UFT_UI_CAT_TAB, "TabWorkflow", 25},
    {"tab_diagnostics.ui", "Diagnostics Tab", UFT_UI_CAT_TAB, "TabDiagnostics", 20},
    {"tab_explorer.ui", "File Explorer Tab", UFT_UI_CAT_TAB, "TabExplorer", 15},
    {"tab_nibble.ui", "Nibble Editor Tab", UFT_UI_CAT_TAB, "TabNibble", 25},
    {"tab_xcopy.ui", "XCopy Tab", UFT_UI_CAT_TAB, "TabXCopy", 20},
    {"tab_protection.ui", "Copy Protection Tab", UFT_UI_CAT_TAB, "TabProtection", 20},
    
    /* Dialogs */
    {"visualdiskdialog.ui", "Visual Disk Dialog", UFT_UI_CAT_DIALOG, "VisualDiskDialog", 10},
    {"rawformatdialog.ui", "Raw Format Dialog", UFT_UI_CAT_DIALOG, "RawFormatDialog", 15},
    {"dialog_validation.ui", "Validation Dialog", UFT_UI_CAT_DIALOG, "ValidationDialog", 10},
    {"diskanalyzer_window.ui", "Disk Analyzer Window", UFT_UI_CAT_DIALOG, "DiskAnalyzerWindow", 30},
    
    /* Widgets */
    {"visualdisk.ui", "Visual Disk Widget", UFT_UI_CAT_WIDGET, "VisualDiskWidget", 5},
    
    /* Sentinel */
    {NULL, NULL, 0, NULL, 0}
};

#define UFT_UI_FILE_COUNT 18

/*===========================================================================
 * WIDGET MAPPING
 *===========================================================================*/

/**
 * @brief Widget type
 */
typedef enum {
    UFT_WIDGET_CHECKBOX,
    UFT_WIDGET_SPINBOX,
    UFT_WIDGET_COMBOBOX,
    UFT_WIDGET_LINEEDIT,
    UFT_WIDGET_SLIDER,
    UFT_WIDGET_BUTTON,
    UFT_WIDGET_LABEL,
    UFT_WIDGET_OTHER
} uft_widget_type_t;

/**
 * @brief Widget mapping entry
 */
typedef struct {
    const char *widget_name;  /**< Qt widget object name */
    const char *param_name;   /**< Backend parameter name */
    const char *ui_file;      /**< Containing UI file */
    uft_widget_type_t type;   /**< Widget type */
    const char *signal;       /**< Qt signal to connect */
} uft_widget_map_t;

/**
 * @brief Core widget mappings
 */
static const uft_widget_map_t UFT_WIDGET_MAP[] = {
    /* Format Tab */
    {"formatComboBox", "format", "tab_format.ui", UFT_WIDGET_COMBOBOX, "currentIndexChanged"},
    {"sidesSpinBox", "sides", "tab_format.ui", UFT_WIDGET_SPINBOX, "valueChanged"},
    {"tracksSpinBox", "tracks", "tab_format.ui", UFT_WIDGET_SPINBOX, "valueChanged"},
    {"encodingComboBox", "encoding", "tab_format.ui", UFT_WIDGET_COMBOBOX, "currentIndexChanged"},
    
    /* Hardware Tab */
    {"hardwareComboBox", "hardware", "tab_hardware.ui", UFT_WIDGET_COMBOBOX, "currentIndexChanged"},
    {"deviceEdit", "device", "tab_hardware.ui", UFT_WIDGET_LINEEDIT, "textChanged"},
    
    /* Status Tab */
    {"retriesSpinBox", "retries", "tab_status.ui", UFT_WIDGET_SPINBOX, "valueChanged"},
    {"revolutionsSpinBox", "revolutions", "tab_status.ui", UFT_WIDGET_SPINBOX, "valueChanged"},
    {"verifyCheckBox", "verify", "tab_status.ui", UFT_WIDGET_CHECKBOX, "toggled"},
    
    /* Main Window */
    {"inputFileEdit", "input", "mainwindow.ui", UFT_WIDGET_LINEEDIT, "textChanged"},
    {"outputFileEdit", "output", "mainwindow.ui", UFT_WIDGET_LINEEDIT, "textChanged"},
    {"verboseCheckBox", "verbose", "mainwindow.ui", UFT_WIDGET_CHECKBOX, "toggled"},
    {"quietCheckBox", "quiet", "mainwindow.ui", UFT_WIDGET_CHECKBOX, "toggled"},
    
    /* Forensic Tab */
    {"pllAdjustSpinBox", "pll_adjust", "tab_forensic.ui", UFT_WIDGET_SPINBOX, "valueChanged"},
    {"pllPhaseSpinBox", "pll_phase", "tab_forensic.ui", UFT_WIDGET_SPINBOX, "valueChanged"},
    {"mergeRevsCheckBox", "merge_revs", "tab_forensic.ui", UFT_WIDGET_CHECKBOX, "toggled"},
    
    /* Debug */
    {"debugCheckBox", "debug", "mainwindow.ui", UFT_WIDGET_CHECKBOX, "toggled"},
    {"logFileEdit", "log_file", "mainwindow.ui", UFT_WIDGET_LINEEDIT, "textChanged"},
    
    /* Sentinel */
    {NULL, NULL, NULL, 0, NULL}
};

#define UFT_WIDGET_MAP_COUNT 18

/*===========================================================================
 * API FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get UI file info by filename
 */
const uft_ui_file_t* uft_ui_get_file(const char *filename);

/**
 * @brief Get UI files by category
 */
int uft_ui_get_files_by_category(uft_ui_category_t cat, const uft_ui_file_t **files, int max);

/**
 * @brief Get widget mapping by widget name
 */
const uft_widget_map_t* uft_ui_get_widget_map(const char *widget_name);

/**
 * @brief Get widget mapping by parameter name
 */
const uft_widget_map_t* uft_ui_get_widget_by_param(const char *param_name);

/**
 * @brief Get all widgets in UI file
 */
int uft_ui_get_widgets_in_file(const char *filename, const uft_widget_map_t **widgets, int max);

/**
 * @brief Print UI registry summary
 */
void uft_ui_print_registry(void);

/**
 * @brief Validate widget mappings against param definitions
 */
int uft_ui_validate_mappings(char **errors, int max_errors);

#ifdef __cplusplus
}
#endif

#endif /* UFT_UI_REGISTRY_H */
