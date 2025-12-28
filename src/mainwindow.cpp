#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "visualdisk.h"

// Tab widget classes
#include "workflowtab.h"
#include "hardwaretab.h"
#include "formattab.h"
// FluxTab removed - now Sub-Tab in FormatTab!
// AdvancedTab removed - now Sub-Tab in FormatTab!
#include "protectiontab.h"
#include "catalogtab.h"
#include "toolstab.h"

#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_visualDiskWindow(nullptr)
{
    ui->setupUi(this);
    loadTabWidgets();
}

MainWindow::~MainWindow()
{
    delete ui;
    if (m_visualDiskWindow) {
        delete m_visualDiskWindow;
    }
}

void MainWindow::loadTabWidgets()
{
    // Tab 1: Workflow
    WorkflowTab* workflowTab = new WorkflowTab();
    QVBoxLayout* layout1 = new QVBoxLayout(ui->tab_workflow);
    layout1->setContentsMargins(0, 0, 0, 0);
    layout1->addWidget(workflowTab);
    
    // Tab 2: Hardware
    HardwareTab* hardwareTab = new HardwareTab();
    QVBoxLayout* layout2 = new QVBoxLayout(ui->tab_hardware);
    layout2->setContentsMargins(0, 0, 0, 0);
    layout2->addWidget(hardwareTab);
    
    // Tab 3: Format (now includes Flux + Format/Geometry + Advanced!)
    FormatTab* formatTab = new FormatTab();
    QVBoxLayout* layout3 = new QVBoxLayout(ui->tab_format);
    layout3->setContentsMargins(0, 0, 0, 0);
    layout3->addWidget(formatTab);
    
    // Tab 4: Protection
    ProtectionTab* protectionTab = new ProtectionTab();
    QVBoxLayout* layout4 = new QVBoxLayout(ui->tab_protection);
    layout4->setContentsMargins(0, 0, 0, 0);
    layout4->addWidget(protectionTab);
    
    // Tab 5: Catalog (was Tab 7!)
    CatalogTab* catalogTab = new CatalogTab();
    QVBoxLayout* layout5 = new QVBoxLayout(ui->tab_catalog);
    layout5->setContentsMargins(0, 0, 0, 0);
    layout5->addWidget(catalogTab);
    
    // Tab 6: Tools (was Tab 8!)
    ToolsTab* toolsTab = new ToolsTab();
    QVBoxLayout* layout6 = new QVBoxLayout(ui->tab_tools);
    layout6->setContentsMargins(0, 0, 0, 0);
    layout6->addWidget(toolsTab);
}
