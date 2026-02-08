/**
 * @file UftParameterIntegration_example.cpp
 * @brief Example: How to integrate UftParameterModel in MainWindow
 * 
 * This file shows how to use the bidirectional parameter binding.
 * Copy relevant parts to your actual MainWindow implementation.
 */

#if 0  /* Example code - not compiled */

#include "UftParameterModel.h"
#include "UftWidgetBinder.h"
#include <QMainWindow>

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    MainWindow(QWidget *parent = nullptr) : QMainWindow(parent)
    {
        setupUi(this);  /* Assuming ui_mainwindow.h exists */
        
        /* Step 1: Create parameter model */
        m_paramModel = new UftParameterModel(this);
        
        /* Step 2: Create widget binder */
        m_binder = new UftWidgetBinder(m_paramModel, this);
        
        /* Step 3: Bind widgets to parameters */
        setupParameterBindings();
        
        /* Step 4: Connect additional signals */
        connect(m_paramModel, &UftParameterModel::modifiedChanged,
                this, &MainWindow::onParametersModified);
        
        connect(m_paramModel, &UftParameterModel::validChanged,
                this, &MainWindow::onParametersValidChanged);
    }
    
private:
    void setupParameterBindings()
    {
        /* Format parameters */
        m_binder->bind(ui->cylindersSpin, "cylinders");
        m_binder->bind(ui->headsSpin, "heads");
        m_binder->bind(ui->sectorsSpin, "sectors");
        m_binder->bind(ui->formatCombo, "format");
        m_binder->bind(ui->encodingCombo, "encoding");
        
        /* Hardware parameters */
        m_binder->bind(ui->hardwareCombo, "hardware");
        m_binder->bind(ui->deviceEdit, "devicePath");
        m_binder->bind(ui->driveCombo, "driveNumber");
        
        /* Recovery parameters */
        m_binder->bind(ui->retriesSpin, "retries");
        m_binder->bind(ui->revolutionsSpin, "revolutions");
        m_binder->bind(ui->weakBitsCheck, "weakBits");
        
        /* PLL parameters */
        m_binder->bind(ui->pllPhaseGainSpin, "pllPhaseGain");
        m_binder->bind(ui->pllFreqGainSpin, "pllFreqGain");
        m_binder->bind(ui->pllPresetCombo, "pllPreset");
        
        /* Write parameters */
        m_binder->bind(ui->verifyWriteCheck, "verifyAfterWrite");
        m_binder->bind(ui->writeRetriesSpin, "writeRetries");
        
        /* Path parameters */
        m_binder->bind(ui->inputEdit, "inputPath");
        m_binder->bind(ui->outputEdit, "outputPath");
    }
    
private slots:
    void onParametersModified(bool modified)
    {
        /* Update window title */
        QString title = "UnifiedFloppyTool";
        if (modified) title += " *";
        setWindowTitle(title);
        
        /* Enable/disable save button */
        ui->actionSave->setEnabled(modified);
    }
    
    void onParametersValidChanged(bool valid)
    {
        /* Enable/disable execute button */
        ui->executeButton->setEnabled(valid);
    }
    
    void onActionSave()
    {
        QString path = QFileDialog::getSaveFileName(
            this, "Save Parameters", QString(), "UFT Config (*.uft);;JSON (*.json)");
        if (!path.isEmpty()) {
            m_paramModel->saveToFile(path);
        }
    }
    
    void onActionLoad()
    {
        QString path = QFileDialog::getOpenFileName(
            this, "Load Parameters", QString(), "UFT Config (*.uft);;JSON (*.json)");
        if (!path.isEmpty()) {
            m_paramModel->loadFromFile(path);
        }
    }
    
    void onActionReset()
    {
        m_paramModel->reset();
        m_binder->syncAllFromModel();
    }
    
    void onActionUndo()
    {
        m_paramModel->undo();
    }
    
    void onActionRedo()
    {
        m_paramModel->redo();
    }
    
    void onExecute()
    {
        /* Sync to backend before execution */
        m_paramModel->syncToBackend();
        
        /* Now run the actual operation using C backend */
        /* uft_read_disk(m_backendParams); etc. */
    }
    
    void onFormatChanged(const QString &format)
    {
        /* Auto-adjust geometry based on format */
        if (format == "adf") {
            m_paramModel->setCylinders(80);
            m_paramModel->setHeads(2);
            m_paramModel->setSectors(11);
        } else if (format == "d64") {
            m_paramModel->setCylinders(35);
            m_paramModel->setHeads(1);
            /* D64 has variable sectors */
        } else if (format == "scp" || format == "hfe") {
            /* Flux formats - keep current geometry */
        }
        /* ... more formats ... */
    }
    
private:
    UftParameterModel *m_paramModel;
    UftWidgetBinder *m_binder;
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * QML Integration (Alternative)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/*
 * If using QML instead of Widgets:
 * 
 * // main.cpp
 * UftParameterModel *model = new UftParameterModel(&app);
 * engine.rootContext()->setContextProperty("paramModel", model);
 * 
 * // Main.qml
 * SpinBox {
 *     value: paramModel.cylinders
 *     onValueModified: paramModel.cylinders = value
 * }
 * 
 * ComboBox {
 *     currentIndex: formatModel.indexOf(paramModel.format)
 *     onActivated: paramModel.format = formatModel.get(index).name
 * }
 * 
 * TextField {
 *     text: paramModel.inputPath
 *     onTextChanged: paramModel.inputPath = text
 * }
 */

#endif /* Example code */
