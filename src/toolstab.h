#pragma once
#include <QWidget>
namespace Ui { class TabTools; }
class ToolsTab : public QWidget {
    Q_OBJECT
public:
    explicit ToolsTab(QWidget *parent = nullptr);
    ~ToolsTab();
private:
    Ui::TabTools *ui;
};
