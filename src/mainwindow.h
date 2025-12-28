/**
 * @file mainwindow.h
 * @brief Main Window - Uses Qt Designer .ui file
 */

#pragma once

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class VisualDiskWindow;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void loadTabWidgets();
    
    Ui::MainWindow *ui;
    VisualDiskWindow *m_visualDiskWindow;
};
