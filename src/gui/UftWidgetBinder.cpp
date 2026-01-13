/**
 * @file UftWidgetBinder.cpp
 * @brief Widget Binder Implementation
 */

#include "UftWidgetBinder.h"
#include "UftParameterModel.h"
#include <QDebug>

UftWidgetBinder::UftWidgetBinder(UftParameterModel *model, QObject *parent)
    : QObject(parent)
    , m_model(model)
{
    /* Connect to model's parameter changed signal */
    connect(m_model, &UftParameterModel::parameterChanged,
            this, &UftWidgetBinder::onModelParameterChanged);
}

UftWidgetBinder::~UftWidgetBinder()
{
    unbindAll();
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Public Binding API
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool UftWidgetBinder::bind(QWidget *widget, const QString &paramName)
{
    if (!widget || paramName.isEmpty()) return false;
    
    /* Check if already bound */
    if (isBound(widget)) {
        unbind(widget);
    }
    
    bool success = false;
    
    /* Detect widget type and bind appropriately */
    if (auto *spin = qobject_cast<QSpinBox*>(widget)) {
        success = bindSpinBox(spin, paramName);
    }
    else if (auto *dspin = qobject_cast<QDoubleSpinBox*>(widget)) {
        success = bindDoubleSpinBox(dspin, paramName);
    }
    else if (auto *combo = qobject_cast<QComboBox*>(widget)) {
        success = bindComboBox(combo, paramName);
    }
    else if (auto *edit = qobject_cast<QLineEdit*>(widget)) {
        success = bindLineEdit(edit, paramName);
    }
    else if (auto *check = qobject_cast<QCheckBox*>(widget)) {
        success = bindCheckBox(check, paramName);
    }
    else if (auto *slider = qobject_cast<QSlider*>(widget)) {
        success = bindSlider(slider, paramName);
    }
    else {
        qWarning() << "Unsupported widget type for binding:" << widget->metaObject()->className();
        return false;
    }
    
    if (success) {
        emit bindingCreated(widget, paramName);
        /* Initial sync from model */
        updateWidgetFromModel(paramName);
    }
    
    return success;
}

void UftWidgetBinder::unbind(QWidget *widget)
{
    for (int i = m_bindings.size() - 1; i >= 0; --i) {
        if (m_bindings[i].widget == widget) {
            emit bindingRemoved(widget);
            m_bindings.removeAt(i);
            break;
        }
    }
}

void UftWidgetBinder::unbindAll()
{
    for (auto &binding : m_bindings) {
        if (binding.widget) {
            emit bindingRemoved(binding.widget);
        }
    }
    m_bindings.clear();
}

void UftWidgetBinder::syncAllFromModel()
{
    m_blockSignals = true;
    for (const auto &binding : m_bindings) {
        updateWidgetFromModel(binding.paramName);
    }
    m_blockSignals = false;
}

bool UftWidgetBinder::isBound(QWidget *widget) const
{
    for (const auto &binding : m_bindings) {
        if (binding.widget == widget) return true;
    }
    return false;
}

QString UftWidgetBinder::parameterFor(QWidget *widget) const
{
    for (const auto &binding : m_bindings) {
        if (binding.widget == widget) return binding.paramName;
    }
    return QString();
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Widget-specific Binding
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool UftWidgetBinder::bindSpinBox(QSpinBox *spin, const QString &paramName)
{
    UftBinding binding;
    binding.widget = spin;
    binding.paramName = paramName;
    binding.widgetSignal = "valueChanged";
    m_bindings.append(binding);
    
    connect(spin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &UftWidgetBinder::onSpinBoxChanged);
    
    return true;
}

bool UftWidgetBinder::bindDoubleSpinBox(QDoubleSpinBox *spin, const QString &paramName)
{
    UftBinding binding;
    binding.widget = spin;
    binding.paramName = paramName;
    binding.widgetSignal = "valueChanged";
    m_bindings.append(binding);
    
    connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &UftWidgetBinder::onDoubleSpinBoxChanged);
    
    return true;
}

bool UftWidgetBinder::bindComboBox(QComboBox *combo, const QString &paramName)
{
    UftBinding binding;
    binding.widget = combo;
    binding.paramName = paramName;
    binding.widgetSignal = "currentIndexChanged";
    m_bindings.append(binding);
    
    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UftWidgetBinder::onComboBoxChanged);
    
    return true;
}

bool UftWidgetBinder::bindLineEdit(QLineEdit *edit, const QString &paramName)
{
    UftBinding binding;
    binding.widget = edit;
    binding.paramName = paramName;
    binding.widgetSignal = "textChanged";
    m_bindings.append(binding);
    
    connect(edit, &QLineEdit::textChanged,
            this, &UftWidgetBinder::onLineEditChanged);
    
    return true;
}

bool UftWidgetBinder::bindCheckBox(QCheckBox *check, const QString &paramName)
{
    UftBinding binding;
    binding.widget = check;
    binding.paramName = paramName;
    binding.widgetSignal = "toggled";
    m_bindings.append(binding);
    
    connect(check, &QCheckBox::toggled,
            this, &UftWidgetBinder::onCheckBoxChanged);
    
    return true;
}

bool UftWidgetBinder::bindSlider(QSlider *slider, const QString &paramName)
{
    UftBinding binding;
    binding.widget = slider;
    binding.paramName = paramName;
    binding.widgetSignal = "valueChanged";
    m_bindings.append(binding);
    
    connect(slider, &QSlider::valueChanged,
            this, &UftWidgetBinder::onSliderChanged);
    
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Widget Change Handlers (Widget → Model)
 * ═══════════════════════════════════════════════════════════════════════════════ */

void UftWidgetBinder::onSpinBoxChanged(int value)
{
    if (m_blockSignals) return;
    
    auto *spin = qobject_cast<QSpinBox*>(sender());
    if (!spin) return;
    
    if (auto *binding = findBinding(spin)) {
        if (binding->updating) return;
        binding->updating = true;
        m_model->setValue(binding->paramName, value);
        binding->updating = false;
    }
}

void UftWidgetBinder::onDoubleSpinBoxChanged(double value)
{
    if (m_blockSignals) return;
    
    auto *spin = qobject_cast<QDoubleSpinBox*>(sender());
    if (!spin) return;
    
    if (auto *binding = findBinding(spin)) {
        if (binding->updating) return;
        binding->updating = true;
        m_model->setValue(binding->paramName, value);
        binding->updating = false;
    }
}

void UftWidgetBinder::onComboBoxChanged(int index)
{
    if (m_blockSignals) return;
    
    auto *combo = qobject_cast<QComboBox*>(sender());
    if (!combo) return;
    
    if (auto *binding = findBinding(combo)) {
        if (binding->updating) return;
        binding->updating = true;
        /* Store the text value, not index */
        m_model->setValue(binding->paramName, combo->currentText());
        binding->updating = false;
    }
}

void UftWidgetBinder::onLineEditChanged(const QString &text)
{
    if (m_blockSignals) return;
    
    auto *edit = qobject_cast<QLineEdit*>(sender());
    if (!edit) return;
    
    if (auto *binding = findBinding(edit)) {
        if (binding->updating) return;
        binding->updating = true;
        m_model->setValue(binding->paramName, text);
        binding->updating = false;
    }
}

void UftWidgetBinder::onCheckBoxChanged(bool checked)
{
    if (m_blockSignals) return;
    
    auto *check = qobject_cast<QCheckBox*>(sender());
    if (!check) return;
    
    if (auto *binding = findBinding(check)) {
        if (binding->updating) return;
        binding->updating = true;
        m_model->setValue(binding->paramName, checked);
        binding->updating = false;
    }
}

void UftWidgetBinder::onSliderChanged(int value)
{
    if (m_blockSignals) return;
    
    auto *slider = qobject_cast<QSlider*>(sender());
    if (!slider) return;
    
    if (auto *binding = findBinding(slider)) {
        if (binding->updating) return;
        binding->updating = true;
        m_model->setValue(binding->paramName, value);
        binding->updating = false;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Model Change Handler (Model → Widget)
 * ═══════════════════════════════════════════════════════════════════════════════ */

void UftWidgetBinder::onModelParameterChanged(const QString &name, 
                                               const QVariant &/*oldVal*/, 
                                               const QVariant &newVal)
{
    auto *binding = findBindingByParam(name);
    if (!binding || !binding->widget) return;
    
    if (binding->updating) return;
    binding->updating = true;
    
    QWidget *w = binding->widget;
    
    /* Update widget based on type */
    if (auto *spin = qobject_cast<QSpinBox*>(w)) {
        spin->setValue(newVal.toInt());
    }
    else if (auto *dspin = qobject_cast<QDoubleSpinBox*>(w)) {
        dspin->setValue(newVal.toDouble());
    }
    else if (auto *combo = qobject_cast<QComboBox*>(w)) {
        int idx = combo->findText(newVal.toString());
        if (idx >= 0) combo->setCurrentIndex(idx);
    }
    else if (auto *edit = qobject_cast<QLineEdit*>(w)) {
        edit->setText(newVal.toString());
    }
    else if (auto *check = qobject_cast<QCheckBox*>(w)) {
        check->setChecked(newVal.toBool());
    }
    else if (auto *slider = qobject_cast<QSlider*>(w)) {
        slider->setValue(newVal.toInt());
    }
    
    binding->updating = false;
}

void UftWidgetBinder::updateWidgetFromModel(const QString &paramName)
{
    QVariant value = m_model->getValue(paramName);
    if (!value.isValid()) return;
    
    /* Trigger the model change handler */
    onModelParameterChanged(paramName, QVariant(), value);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Helpers
 * ═══════════════════════════════════════════════════════════════════════════════ */

UftBinding* UftWidgetBinder::findBinding(QWidget *widget)
{
    for (auto &binding : m_bindings) {
        if (binding.widget == widget) return &binding;
    }
    return nullptr;
}

UftBinding* UftWidgetBinder::findBindingByParam(const QString &paramName)
{
    for (auto &binding : m_bindings) {
        if (binding.paramName == paramName) return &binding;
    }
    return nullptr;
}
