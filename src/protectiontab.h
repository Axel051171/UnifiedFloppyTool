#pragma once
#include <QWidget>
namespace Ui { class TabProtection; }
class ProtectionTab : public QWidget {
    Q_OBJECT
public:
    explicit ProtectionTab(QWidget *parent = nullptr);
    ~ProtectionTab();
private:
    Ui::TabProtection *ui;
};
