#pragma once
#include <QWidget>
namespace Ui { class TabFormat; }
class FormatTab : public QWidget {
    Q_OBJECT
public:
    explicit FormatTab(QWidget *parent = nullptr);
    ~FormatTab();
private:
    Ui::TabFormat *ui;
};
