/**
 * @file workflowtab.cpp
 * @brief Workflow Tab Widget Implementation
 */

#include "workflowtab.h"
#include "ui_tab_workflow.h"

WorkflowTab::WorkflowTab(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TabWorkflow)
{
    ui->setupUi(this);  // DIES lädt tab_workflow.ui!
}

WorkflowTab::~WorkflowTab()
{
    delete ui;
}
