/**
 * @file UftWidgetBinder.h
 * @brief Automatic Bidirectional Widget-Model Binding
 * 
 * P1-004: GUI Parameter Bidirektional
 * 
 * Connects Qt widgets to UftParameterModel with automatic two-way sync.
 * 
 * Usage:
 *   UftWidgetBinder binder(&model);
 *   binder.bind(ui->cylindersSpin, "cylinders");
 *   binder.bind(ui->formatCombo, "format");
 *   // Now changes in widget update model and vice versa
 */

#ifndef UFT_WIDGET_BINDER_H
#define UFT_WIDGET_BINDER_H

#include <QObject>
#include <QWidget>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QSlider>
#include <QHash>
#include <QPointer>

class UftParameterModel;

/**
 * @brief Binding record for tracking widget-parameter connections
 */
struct UftBinding {
    QPointer<QWidget> widget;
    QString paramName;
    QString widgetSignal;
    bool updating = false;  /* Prevent recursion */
};

/**
 * @brief Automatic two-way binding between widgets and parameter model
 */
class UftWidgetBinder : public QObject
{
    Q_OBJECT

public:
    explicit UftWidgetBinder(UftParameterModel *model, QObject *parent = nullptr);
    ~UftWidgetBinder();
    
    /**
     * @brief Bind a widget to a parameter
     * 
     * Automatically detects widget type and connects appropriate signals.
     * 
     * @param widget Widget to bind
     * @param paramName Parameter name in model
     * @return true if binding successful
     */
    bool bind(QWidget *widget, const QString &paramName);
    
    /**
     * @brief Unbind a widget
     */
    void unbind(QWidget *widget);
    
    /**
     * @brief Unbind all widgets
     */
    void unbindAll();
    
    /**
     * @brief Sync all widgets from model
     */
    void syncAllFromModel();
    
    /**
     * @brief Check if widget is bound
     */
    bool isBound(QWidget *widget) const;
    
    /**
     * @brief Get parameter name for widget
     */
    QString parameterFor(QWidget *widget) const;

signals:
    void bindingCreated(QWidget *widget, const QString &paramName);
    void bindingRemoved(QWidget *widget);
    void syncError(const QString &error);

private slots:
    /* Widget change handlers */
    void onSpinBoxChanged(int value);
    void onDoubleSpinBoxChanged(double value);
    void onComboBoxChanged(int index);
    void onLineEditChanged(const QString &text);
    void onCheckBoxChanged(bool checked);
    void onSliderChanged(int value);
    
    /* Model change handler */
    void onModelParameterChanged(const QString &name, const QVariant &oldVal, const QVariant &newVal);

private:
    bool bindSpinBox(QSpinBox *spin, const QString &paramName);
    bool bindDoubleSpinBox(QDoubleSpinBox *spin, const QString &paramName);
    bool bindComboBox(QComboBox *combo, const QString &paramName);
    bool bindLineEdit(QLineEdit *edit, const QString &paramName);
    bool bindCheckBox(QCheckBox *check, const QString &paramName);
    bool bindSlider(QSlider *slider, const QString &paramName);
    
    void updateWidgetFromModel(const QString &paramName);
    UftBinding* findBinding(QWidget *widget);
    UftBinding* findBindingByParam(const QString &paramName);
    
    UftParameterModel *m_model;
    QList<UftBinding> m_bindings;
    bool m_blockSignals = false;
};

#endif /* UFT_WIDGET_BINDER_H */
