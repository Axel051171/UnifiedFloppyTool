---
name: uft-qt-widget
description: |
  Use when adding a NEW Qt6 widget, tab, or dialog to UFT's GUI. Trigger
  phrases: "neues Qt-widget", "neuer tab", "add Qt panel", "widget für X
  analyse", "GUI-dialog für Y", "analysis panel für Z", "OTDR-style panel".
  Enforces MASTER_PLAN Regel 2 (no GUI without working backend). DO NOT
  use for: modifying existing widget (→ structured-reviewer), pure styling
  changes (→ quick-fix), Qt Designer UI-file-only tweaks, plotting with
  QCustomPlot/QtCharts (those have their own patterns).
---

# UFT Qt6 Widget

Use this skill when adding a new widget, tab, dialog, or dock to UFT's GUI.
The canonical style reference is `src/gui/uft_otdr_panel.cpp`.

## When to use this skill

- Adding a new analysis panel (OTDR-like, sector inspector, protection browser)
- Adding a new tab to the main window
- Adding a modal dialog (settings, preferences, import wizard)
- Adding a dock widget (file browser, visualizer sidebar)
- Adding a reusable sub-widget (shared across panels)

**Do NOT use this skill for:**
- Modifying an existing widget — use `structured-reviewer`
- Styling/theming changes only — use `quick-fix`
- Qt Designer `.ui` tweaks without C++ changes
- QCustomPlot/QtCharts plotting work — separate patterns

## Widget type → template mapping

| Widget type | Template reference | Location |
|-------------|--------------------|----------|
| Main-window tab | `src/xcopytab.cpp` | `src/` |
| Analysis panel | `src/gui/uft_otdr_panel.cpp` | `src/gui/` |
| Modal dialog | `src/advanceddialogs.cpp` | `src/` |
| Dock widget | `src/widgets/fluxvisualizerwidget.cpp` | `src/widgets/` |
| Reusable sub-widget | `src/widgets/trackgridwidget.cpp` | `src/widgets/` |

## Workflow

### Step 1: Scaffold 3 files

```bash
python3 .claude/skills/uft-qt-widget/scripts/scaffold_widget.py \
    --name foo_panel \
    --class UftFooPanel \
    --kind analysis_panel
```

Creates:
- `src/gui/uft_foo_panel.h`
- `src/gui/uft_foo_panel.cpp`
- `ui/foo.ui` (stub Designer file, optional)

### Step 2: Respect Regel 2 — backend or setEnabled(false)

Master-Plan mandate: every GUI element must either trigger real backend
work OR be `setEnabled(false)` with an explanatory tooltip. No phantom
features.

```cpp
UftFooPanel::UftFooPanel(QWidget *parent)
    : QWidget(parent), ui(std::make_unique<Ui::UftFooPanel>()) {
    ui->setupUi(this);

    /* MF-012 pattern — guard if backend not wired */
    if (!backendAvailable()) {
        ui->btnAnalyze->setEnabled(false);
        ui->btnAnalyze->setToolTip(
            tr("Foo analysis: planned for v4.2.0.\n"
               "See docs/MASTER_PLAN.md M2."));
    }
}
```

A CI test (`tests/test_gui_backend_wiring.cpp`) verifies every
`QPushButton::clicked` connect points to a non-empty slot. Empty-body
slots or `/* TODO */` fail the test.

### Step 3: Long operations on a worker thread

UFT rule: **any action taking >16 ms** must be on a worker. Use the
`CopyWorker` pattern from `src/xcopytab.cpp`:

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

### Step 4: OTDR visual conventions

Look at `src/gui/uft_otdr_panel.cpp`:

- **Left pane:** parameter controls (QGroupBox + QFormLayout)
- **Right pane:** result display (chart, tree, heatmap)
- **Bottom bar:** status label + progress bar + Export button
- **Colors:** use `palette()` roles, not hardcoded `#RRGGBB`
- **Icons:** `QIcon::fromTheme()` with fallback in `resources/`
- **Charts:** QtCharts if plotting; custom `QPainter` only for heatmaps

### Step 5: Accessibility baseline (Principle 7)

Non-negotiable for museum deployment:

- **Keyboard navigable** — every widget reachable via Tab
- **Accessible names** — `accessibleName()` on custom widgets
- **Status announcements** — emit `statusMessage()` signal wired to main
  window status bar
- **Minimum font size** 9pt, respect system scaling

## Verification

```bash
# 1. Compile check (including moc)
qmake && make -j$(nproc) 2>&1 | grep "uft_<n>" | grep -E "error|warning"
# expect: empty output

# 2. Widget smoke test
cd tests && make test_<n>_panel && ./test_<n>_panel

# 3. Backend-wiring CI check
make test_gui_backend_wiring && ./test_gui_backend_wiring

# 4. Manual GUI check
./uft 2>&1 &   # launch and verify widget appears correctly
```

## Common pitfalls

### `QWidget::setStyleSheet` with hardcoded colors

Kills dark-mode support. Use `palette()` roles:

```cpp
/* WRONG */
setStyleSheet("background: #ffffff; color: #000000;");

/* RIGHT */
QPalette p = palette();
p.setColor(QPalette::Base, p.color(QPalette::Window));
setPalette(p);
```

### Blocking the UI thread on a QMessageBox inside a worker signal

```cpp
/* WRONG — deadlocks if signal arrives before main event loop */
connect(worker, &Worker::error, [](QString msg) {
    QMessageBox::critical(this, "Error", msg);
});

/* RIGHT — queue on main event loop */
connect(worker, &Worker::error, this, [this](QString msg) {
    QMetaObject::invokeMethod(this, [this, msg]() {
        QMessageBox::critical(this, "Error", msg);
    }, Qt::QueuedConnection);
});
```

### Forgotten `Q_OBJECT` macro

Without `Q_OBJECT`, moc doesn't run, signals/slots silently fail at runtime.
Always check the generated `moc_<n>.cpp` exists after build.

### Hardcoded paths to resources

Use `QStandardPaths` for user data, `:/resources/` URI for bundled files.
`/home/user/...` breaks for everyone but you.

## Related

- `src/gui/uft_otdr_panel.cpp` — canonical analysis-panel reference
- `src/xcopytab.cpp` — canonical tab + worker pattern
- `docs/MASTER_PLAN.md` Regel 2 (no GUI without backend)
- `docs/DESIGN_PRINCIPLES.md` Principle 7 (honesty)
- `.claude/skills/uft-hal-backend/` — if the panel triggers hardware I/O
