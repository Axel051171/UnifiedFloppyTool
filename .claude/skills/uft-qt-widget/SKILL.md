---
name: uft-qt-widget
description: |
  Use when adding a new Qt6 widget/tab/dialog to UFT's GUI. Trigger
  phrases: "neues Qt-widget", "neuer tab", "add Qt panel", "widget
  für X analyse", "GUI-dialog für Y", "OTDR-style panel". Enforces
  MASTER_PLAN Regel 2 (keine neuen GUI-Elemente ohne Backend) and
  the UftOtdrPanel style conventions.
---

# UFT Qt6 Widget Template

Use this skill when adding a widget, tab, or dialog. UFT's canonical
style reference is `src/gui/uft_otdr_panel.cpp` — the DeepRead/OTDR
analysis panel. Every GUI element MUST wire to a working backend OR be
disabled with an explanatory tooltip (Regel 2).

## Step 1: Decide the shape

| Want | Template | Register where |
|---|---|---|
| Tab in main window | `src/xcopytab.cpp` | `src/mainwindow.cpp` tab init |
| Analysis panel | `src/gui/uft_otdr_panel.cpp` | tabs or docked |
| Modal dialog | `src/advanceddialogs.cpp` | direct caller in actions |
| Dock widget | `src/widgets/fluxvisualizerwidget.cpp` | main window layout |
| Reusable widget | `src/widgets/trackgridwidget.cpp` | embedded by others |

## Step 2: File skeleton

Three files per widget (canonical pattern):

```
src/gui/uft_<name>_panel.h      — class declaration
src/gui/uft_<name>_panel.cpp    — implementation
ui/<name>.ui                    — Qt Designer form (optional but preferred)
```

Header skeleton:

```cpp
/**
 * @file uft_<name>_panel.h
 * @brief <one-line description of the panel's purpose>
 */
#pragma once

#include <QWidget>
#include <memory>

namespace Ui { class UftFooPanel; }

class UftFooPanel : public QWidget {
    Q_OBJECT
public:
    explicit UftFooPanel(QWidget *parent = nullptr);
    ~UftFooPanel() override;

    /* Public API — backend-driven */
    void loadAnalysisResult(const uft_foo_result_t &result);
    void clear();

signals:
    void exportRequested(const QString &path);
    void analysisTriggered();

private slots:
    void onRefreshClicked();
    void onParameterChanged();

private:
    std::unique_ptr<Ui::UftFooPanel> ui;
    /* private state — pImpl if large */
};
```

## Step 3: Regel 2 — backend or setEnabled(false)

MASTER_PLAN.md enforces this explicitly:

> Jeder neue Qt-Tab / Button / Dialog muss eine wirkende Backend-
> Funktion triggern, oder `setEnabled(false)` mit Tooltip "Feature X:
> planned for release Y" haben.

In the constructor, IF the backend is not yet connected:

```cpp
UftFooPanel::UftFooPanel(QWidget *parent)
    : QWidget(parent), ui(std::make_unique<Ui::UftFooPanel>())
{
    ui->setupUi(this);

    /* MF-012 pattern — if backend is phantom, guard it */
    if (/* backend not wired yet */) {
        ui->btnStart->setEnabled(false);
        ui->btnStart->setToolTip(
            tr("FOO analysis: planned for v4.2.0.\n"
               "See docs/MASTER_PLAN.md M2."));
    }
}
```

A CI test (`tests/test_gui_backend_wiring.cpp`, MASTER_PLAN Regel 2)
checks that every `QPushButton::clicked` connect points to a non-empty
slot. Empty-body slots or `/* TODO */` fail the test.

## Step 4: Register in build system

`.pro` file:
```
SOURCES += src/gui/uft_<name>_panel.cpp
HEADERS += src/gui/uft_<name>_panel.h
FORMS   += ui/<name>.ui              # if using Designer
```

Make sure `Q_OBJECT` is present — otherwise moc won't run.

`CMakeLists.txt` (if adding to the Qt build tree):
```cmake
qt_add_executable(UnifiedFloppyTool
    ...
    src/gui/uft_<name>_panel.cpp
    src/gui/uft_<name>_panel.h
)
```

CMake with `CMAKE_AUTOMOC ON` handles moc automatically.

## Step 5: OTDR-panel visual conventions

Look at `src/gui/uft_otdr_panel.cpp` — it is the canonical style:

- **Left pane**: parameter controls (QGroupBox with QFormLayout).
- **Right pane**: result display (chart or tree or heatmap).
- **Bottom bar**: status label + progress bar + Export button.
- **Colors**: use `palette()` not hardcoded `#RRGGBB`. Dark-mode safe.
- **Icons**: `QIcon::fromTheme()` with a .png fallback in `resources/`.
- **Charts**: QtCharts if plotting, not custom QPainter (unless heatmap).

## Step 6: Thread safety for backend calls

UFT rule: **NEVER** block the UI thread on I/O or decoder operations.
Any action that takes > 16 ms needs a worker:

```cpp
void UftFooPanel::onStartClicked() {
    auto *worker = new FooWorker(m_inputPath);
    auto *thread = new QThread(this);
    worker->moveToThread(thread);
    connect(thread, &QThread::started, worker, &FooWorker::process);
    connect(worker, &FooWorker::progress, this, &UftFooPanel::onProgress);
    connect(worker, &FooWorker::finished, thread, &QThread::quit);
    connect(worker, &FooWorker::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}
```

Template: see `CopyWorker` in `src/xcopytab.cpp`.

## Step 7: Tests

Two test layers:

1. **Widget smoke test** (`tests/test_<name>_panel.cpp`) — constructs
   the widget, calls the public API with a mock result, verifies no
   crash. Uses `QTest::qWaitForWindowExposed` for paint cycles.
2. **Backend integration** — optional, via Qt Test framework with
   `QSignalSpy` to capture emitted signals.

```cpp
void TestUftFooPanel::test_load_result_displays_values() {
    UftFooPanel panel;
    uft_foo_result_t result = make_mock_result();
    panel.loadAnalysisResult(result);
    QVERIFY(panel.findChild<QLabel *>("valueLabel")->text() == "42");
}
```

## Step 8: Accessibility baseline

Non-negotiable for museum deployment (Principle 7):

- **Keyboard navigable** — all widgets reachable via Tab, no mouse-only
  actions.
- **Accessible names** — set `accessibleName()` on custom widgets.
- **Status announcements** — long operations emit `statusMessage()`
  signal that gets wired to the main window status bar.
- **Minimum font size** — 9pt, respect system scaling.

## Anti-patterns

- **Don't use QWidget::setStyleSheet with hardcoded colors** — kills
  dark-mode support. Use palette roles.
- **Don't hardcode paths** — use `QStandardPaths` for user data.
- **Don't block the UI thread on a QMessageBox inside a worker signal
  handler** — queue it on the main event loop.
- **Don't create a panel without connecting signals to actual code** —
  Regel 2 violation (phantom feature).
- **Don't use `QTextStream::readLine()` for parsing binary data** —
  locale / encoding issues.

## Related

- `src/gui/uft_otdr_panel.cpp` — canonical reference
- `docs/MASTER_PLAN.md` Regel 2 (no GUI without backend)
- `docs/DESIGN_PRINCIPLES.md` Principle 7 (honesty — setEnabled over
  silent no-op)
- `src/xcopytab.cpp` — MF-012 tooltip-guard example
