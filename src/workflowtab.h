/**
 * @file workflowtab.h
 * @brief Workflow Tab Widget
 */

#pragma once

#include <QWidget>

namespace Ui {
    class TabWorkflow;
}

class WorkflowTab : public QWidget
{
    Q_OBJECT

public:
    explicit WorkflowTab(QWidget *parent = nullptr);
    ~WorkflowTab();

private:
    Ui::TabWorkflow *ui;
};
