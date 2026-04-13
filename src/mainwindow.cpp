#include "mainwindow.hpp"
#include "controllermapwidget.hpp"
#include "inputworker.hpp"
#include "mappingeditorwidget.hpp"
#include "mappingstorage.hpp"

#include <linux/input-event-codes.h>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QSystemTrayIcon>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

// ── Constructor / destructor ──────────────────────────────────────────────────

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , settings_("kb-to-ds5", "kb-to-ds5")
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowTitle("kb-to-ds5");
    setMinimumSize(480, 350);
    resize(1080, 720);

    qApp->installEventFilter(this);

    worker_ = new InputWorker(this);

    setup_ui();
    setup_tray();
    load_settings();
    update_state(false);

    connect(worker_, &InputWorker::started_ok,   this, &MainWindow::on_worker_started);
    connect(worker_, &InputWorker::pause_changed, this, [this](bool paused) {
        pause_btn_->setText(paused ? "▶  Resume" : "⏸  Pause");
        status_label_->setText(paused
            ? "Paused — keyboard/mouse released"
            : "Running — keyboard grabbed, virtual DualSense active");
    });
    connect(worker_, &InputWorker::error,        this, &MainWindow::on_worker_error);
    connect(worker_, &InputWorker::stats,        this, &MainWindow::on_worker_stats);
    connect(worker_, &InputWorker::log_message,  this, &MainWindow::on_worker_log);
}

MainWindow::~MainWindow() {
    qApp->removeEventFilter(this);
    if (worker_) {
        worker_->stop();
        if (!worker_->wait(3000))
            worker_->terminate();
    }
}

// ── UI setup ──────────────────────────────────────────────────────────────────

void MainWindow::setup_ui()
{
    auto* central = new QWidget(this);
    setCentralWidget(central);
    central->setObjectName("CW");
    central->setAttribute(Qt::WA_StyledBackground);
    central->setStyleSheet(
        "QWidget#CW {"
        "  background: #12121f;"
        "  border: 1px solid #2a2a45;"
        "  border-radius: 10px;"
        "}");

    auto* outer = new QVBoxLayout(central);
    outer->setSpacing(0);
    outer->setContentsMargins(0, 0, 0, 0);

    // ── Custom title bar ───────────────────────────────────────────────────────
    custom_title_bar_ = new QWidget(central);
    custom_title_bar_->setFixedHeight(42);
    custom_title_bar_->setObjectName("TB");
    custom_title_bar_->setAttribute(Qt::WA_StyledBackground);
    custom_title_bar_->setStyleSheet(
        "QWidget#TB {"
        "  background: #0d0d1e;"
        "  border-top-left-radius: 10px;"
        "  border-top-right-radius: 10px;"
        "  border-bottom: 1px solid #2a2a45;"
        "}");
    custom_title_bar_->installEventFilter(this);

    auto* tb = new QHBoxLayout(custom_title_bar_);
    tb->setContentsMargins(14, 0, 6, 0);
    tb->setSpacing(0);

    title_label_ = new QLabel("  Keyboard → DualSense", custom_title_bar_);
    title_label_->setStyleSheet(
        "font-size: 13px; font-weight: bold; color: #e0e0f0; background: transparent;");
    tb->addWidget(title_label_);
    tb->addStretch();

    auto* min_btn = new QPushButton("—", custom_title_bar_);
    min_btn->setFixedSize(34, 28);
    min_btn->setStyleSheet(
        "QPushButton { background: transparent; color: #7878aa; border: none;"
        "font-size: 14px; border-radius: 4px; }"
        "QPushButton:hover { background: #2a2a45; color: #e0e0f0; }");
    tb->addWidget(min_btn);

    auto* close_btn = new QPushButton("✕", custom_title_bar_);
    close_btn->setFixedSize(34, 28);
    close_btn->setStyleSheet(
        "QPushButton { background: transparent; color: #7878aa; border: none;"
        "font-size: 11px; border-radius: 4px; }"
        "QPushButton:hover { background: #c0392b; color: #ffffff; }");
    tb->addWidget(close_btn);
    tb->addSpacing(2);

    outer->addWidget(custom_title_bar_);

    // ── Content area ──────────────────────────────────────────────────────────
    auto* content = new QWidget(central);
    auto* layout  = new QVBoxLayout(content);
    layout->setSpacing(10);
    layout->setContentsMargins(16, 12, 16, 16);

    // Status bar (always visible)
    status_group_       = new QGroupBox("Status", content);
    auto* status_layout = new QVBoxLayout(status_group_);
    status_layout->setContentsMargins(10, 6, 10, 6);
    status_layout->setSpacing(2);

    status_label_ = new QLabel("Stopped", content);
    status_label_->setStyleSheet("font-size: 13px; color: #e0e0f0; background: transparent;");
    status_layout->addWidget(status_label_);

    stats_label_ = new QLabel("Reports: 0", content);
    stats_label_->setStyleSheet("color: #7878aa; font-size: 11px; background: transparent;");
    status_layout->addWidget(stats_label_);

    layout->addWidget(status_group_);

    // Start / Pause / Stop buttons (always visible)
    auto* btn_row = new QHBoxLayout();
    start_btn_ = new QPushButton("▶  Start",  content);
    pause_btn_ = new QPushButton("⏸  Pause",  content);
    stop_btn_  = new QPushButton("⏹  Stop",   content);

    start_btn_->setMinimumHeight(36);
    pause_btn_->setMinimumHeight(36);
    stop_btn_->setMinimumHeight(36);
    pause_btn_->setEnabled(false);
    stop_btn_->setEnabled(false);

    start_btn_->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "stop:0 #00c9a7, stop:1 #00a896); color: #0a0a1a; border: none;"
        "border-radius: 8px; font-weight: bold; font-size: 13px; }"
        "QPushButton:hover { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "stop:0 #00e0bb, stop:1 #00c9a7); }"
        "QPushButton:disabled { background: #1e1e35; color: #4a4a6a; }");
    pause_btn_->setStyleSheet(
        "QPushButton { background: #1a1a2e; color: #c8a020; border: 1px solid #4a3a10;"
        "border-radius: 8px; font-weight: bold; font-size: 13px; }"
        "QPushButton:hover { background: #c8a020; color: #0a0a1a; border-color: #c8a020; }"
        "QPushButton:disabled { background: #1e1e35; color: #4a4a6a; border-color: #2a2a45; }");
    stop_btn_->setStyleSheet(
        "QPushButton { background: #1e1220; color: #e05252; border: 1px solid #5a2a2a;"
        "border-radius: 8px; font-weight: bold; font-size: 13px; }"
        "QPushButton:hover { background: #e05252; color: #fff; border-color: #e05252; }"
        "QPushButton:disabled { background: #1e1e35; color: #4a4a6a; border-color: #2a2a45; }");

    btn_row->addWidget(start_btn_);
    btn_row->addWidget(pause_btn_);
    btn_row->addWidget(stop_btn_);
    layout->addLayout(btn_row);

    // ── Main tab widget ────────────────────────────────────────────────────────
    const QString tab_style =
        "QTabWidget::pane { border: 1px solid #2a2a45; background: #12121f;"
        "  border-radius: 0 4px 4px 4px; }"
        "QTabBar::tab { background: #1a1a2e; color: #7878aa; padding: 6px 16px;"
        "  border: 1px solid #2a2a45; border-bottom: none;"
        "  border-radius: 4px 4px 0 0; margin-right: 2px; }"
        "QTabBar::tab:selected { background: #12121f; color: #e0e0f0; }"
        "QTabBar::tab:hover    { color: #e0e0f0; }";

    main_tabs_ = new QTabWidget(content);
    main_tabs_->setStyleSheet(tab_style);

    // ── Tab 1: Input Devices ──────────────────────────────────────────────────
    auto* devices_tab    = new QWidget();
    auto* devices_layout = new QVBoxLayout(devices_tab);
    devices_layout->setContentsMargins(10, 10, 10, 10);
    devices_layout->setSpacing(8);

    devices_group_ = new QGroupBox("Input Devices", devices_tab);
    auto* dg_layout = new QVBoxLayout(devices_group_);

    device_list_ = new QListWidget(devices_tab);
    device_list_->setStyleSheet(
        "QListWidget { background: #0d0d1e; color: #e0e0f0; border: 1px solid #2a2a45;"
        "border-radius: 4px; font-size: 12px; }"
        "QListWidget::item:selected { background: #1e1e35; }"
        "QListWidget::indicator:checked { color: #00c9a7; }");
    dg_layout->addWidget(device_list_);

    refresh_btn_ = new QPushButton("Refresh Devices", devices_tab);
    refresh_btn_->setStyleSheet(
        "QPushButton { background: #1e1e35; color: #a0a0c0; border: 1px solid #2a2a45;"
        "border-radius: 6px; padding: 4px 10px; font-size: 12px; }"
        "QPushButton:hover { background: #2a2a45; color: #e0e0f0; }");
    dg_layout->addWidget(refresh_btn_);

    devices_layout->addWidget(devices_group_);
    main_tabs_->addTab(devices_tab, "Input Devices");

    // ── Tab 2: Settings ───────────────────────────────────────────────────────
    auto* settings_tab    = new QWidget();
    auto* settings_layout = new QVBoxLayout(settings_tab);
    settings_layout->setContentsMargins(10, 10, 10, 10);
    settings_layout->setSpacing(8);

    settings_group_ = new QGroupBox("Settings", settings_tab);
    auto* sg_layout = new QVBoxLayout(settings_group_);

    background_cb_ = new QCheckBox("Run in background when window is closed", settings_tab);
    sg_layout->addWidget(background_cb_);

    auto* ms_header = new QHBoxLayout();
    mouse_enable_cb_ = new QCheckBox("Mouse → Right Stick", settings_tab);
    right_stick_cb_  = new QCheckBox("Use left stick instead", settings_tab);
    ms_header->addWidget(mouse_enable_cb_);
    ms_header->addWidget(right_stick_cb_);
    ms_header->addStretch();
    sg_layout->addLayout(ms_header);

    auto* sens_row = new QHBoxLayout();
    sens_row->addWidget(new QLabel("Sensitivity X:", settings_tab));
    sens_x_spin_ = new QDoubleSpinBox(settings_tab);
    sens_x_spin_->setRange(0.01, 5.0);
    sens_x_spin_->setSingleStep(0.05);
    sens_x_spin_->setDecimals(2);
    sens_x_spin_->setValue(0.25);
    sens_x_spin_->setFixedWidth(70);
    sens_row->addWidget(sens_x_spin_);
    sens_row->addSpacing(12);
    sens_row->addWidget(new QLabel("Y:", settings_tab));
    sens_y_spin_ = new QDoubleSpinBox(settings_tab);
    sens_y_spin_->setRange(0.01, 5.0);
    sens_y_spin_->setSingleStep(0.05);
    sens_y_spin_->setDecimals(2);
    sens_y_spin_->setValue(0.25);
    sens_y_spin_->setFixedWidth(70);
    sens_row->addWidget(sens_y_spin_);
    sens_row->addStretch();
    sg_layout->addLayout(sens_row);

    // Touchpad mode key
    auto* tp_row = new QHBoxLayout();
    tp_row->addWidget(new QLabel("Touchpad Mode Key:", settings_tab));
    touchpad_key_label_ = new QLabel("None", settings_tab);
    touchpad_key_label_->setStyleSheet("color: #00c9a7; font-size: 12px; min-width: 80px;");
    tp_row->addWidget(touchpad_key_label_);
    touchpad_key_btn_ = new QPushButton("Set Key", settings_tab);
    touchpad_key_btn_->setFixedWidth(80);
    touchpad_key_btn_->setStyleSheet(
        "QPushButton { background: #1e1e35; color: #a0a0c0; border: 1px solid #2a2a45;"
        "border-radius: 6px; padding: 3px 8px; font-size: 12px; }"
        "QPushButton:hover { background: #2a2a45; color: #e0e0f0; }");
    tp_row->addWidget(touchpad_key_btn_);
    touchpad_key_clear_ = new QPushButton("Clear", settings_tab);
    touchpad_key_clear_->setFixedWidth(60);
    touchpad_key_clear_->setStyleSheet(
        "QPushButton { background: #1e1220; color: #e05252; border: 1px solid #5a2a2a;"
        "border-radius: 6px; padding: 3px 8px; font-size: 12px; }"
        "QPushButton:hover { background: #e05252; color: #fff; }");
    tp_row->addWidget(touchpad_key_clear_);
    tp_row->addStretch();
    sg_layout->addLayout(tp_row);

    settings_layout->addWidget(settings_group_);
    settings_layout->addStretch();
    main_tabs_->addTab(settings_tab, "Settings");

    // ── Tab 3: Mapping Table ──────────────────────────────────────────────────
    editor_ = new MappingEditorWidget();
    main_tabs_->addTab(editor_, "Mapping Table");

    // ── Tab 4: Controller Map ─────────────────────────────────────────────────
    controller_map_ = new ControllerMapWidget();
    main_tabs_->addTab(controller_map_, "Controller Map");

    layout->addWidget(main_tabs_, /*stretch=*/1);

    outer->addWidget(content);

    // ── Signals ───────────────────────────────────────────────────────────────
    connect(start_btn_, &QPushButton::clicked, this, &MainWindow::on_start_clicked);
    connect(pause_btn_, &QPushButton::clicked, this, [this]() { worker_->toggle_pause(); });
    connect(stop_btn_,  &QPushButton::clicked, this, &MainWindow::on_stop_clicked);
    connect(close_btn,  &QPushButton::clicked, this, &MainWindow::close);
    connect(min_btn,    &QPushButton::clicked, this, &MainWindow::showMinimized);

    connect(refresh_btn_, &QPushButton::clicked, this, &MainWindow::refresh_devices);
    connect(device_list_, &QListWidget::itemChanged,
            this, [this](QListWidgetItem*) { save_device_selection(); });

    connect(editor_, &MappingEditorWidget::config_changed,
            this, &MainWindow::on_mapping_changed);

    connect(mouse_enable_cb_, &QCheckBox::toggled, this, &MainWindow::on_sensitivity_changed);
    connect(right_stick_cb_,  &QCheckBox::toggled, this, &MainWindow::on_sensitivity_changed);
    connect(sens_x_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::on_sensitivity_changed);
    connect(sens_y_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::on_sensitivity_changed);

    connect(touchpad_key_btn_, &QPushButton::clicked, this, [this]() {
        KeyCaptureDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted && dlg.captured_evdev_code() >= 0) {
            mouse_stick_.touchpad_key = dlg.captured_evdev_code();
            touchpad_key_label_->setText(dlg.captured_name());
            save_settings();
            if (worker_->isRunning()) push_config_to_worker();
        }
    });

    connect(touchpad_key_clear_, &QPushButton::clicked, this, [this]() {
        mouse_stick_.touchpad_key = 0;
        touchpad_key_label_->setText("None");
        save_settings();
        if (worker_->isRunning()) push_config_to_worker();
    });
}

void MainWindow::setup_tray()
{
    const QIcon app_icon(":/icon.png");
    const QIcon icon = app_icon.isNull()
        ? QIcon::fromTheme("input-gaming", QIcon::fromTheme("joystick"))
        : app_icon;
    setWindowIcon(icon);
    tray_icon_ = new QSystemTrayIcon(icon, this);
    tray_menu_ = new QMenu(this);

    tray_show_ = tray_menu_->addAction("Show window");
    connect(tray_show_, &QAction::triggered, this, [this]() {
        showNormal(); activateWindow();
    });
    tray_menu_->addSeparator();
    tray_quit_ = tray_menu_->addAction("Quit");
    connect(tray_quit_, &QAction::triggered, this, []() { QApplication::quit(); });

    tray_icon_->setContextMenu(tray_menu_);
    if (QSystemTrayIcon::isSystemTrayAvailable())
        tray_icon_->show();
    else
        background_cb_->setEnabled(false);

    connect(tray_icon_, &QSystemTrayIcon::activated,
            this, &MainWindow::on_tray_activated);
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void MainWindow::on_start_clicked()
{
    if (worker_->isRunning()) return;

    const QStringList devices = selected_device_paths();
    if (devices.isEmpty()) {
        QMessageBox::warning(this, "No Devices Selected",
            "Please select at least one input device from the Input Devices list.");
        return;
    }

    // Load saved config into editor if not yet loaded
    kb::Config cfg = editor_->config();
    if (cfg.mappings.isEmpty()) {
        cfg = MappingStorage::load();
        editor_->set_config(cfg);
    }

    cfg.mouse_stick = mouse_stick_;
    worker_->set_selected_devices(devices);
    worker_->update_config(cfg);
    worker_->start();

    status_label_->setText("Starting...");
    start_btn_->setEnabled(false);
}

void MainWindow::on_stop_clicked()
{
    worker_->stop();
    if (!worker_->wait(3000))
        worker_->terminate();
    update_state(false);
}

void MainWindow::on_worker_started()
{
    update_state(true);
    tray_icon_->showMessage("kb-to-ds5",
                            "Virtual DualSense is active. Keyboard is grabbed.",
                            QSystemTrayIcon::Information, 2000);
}

void MainWindow::on_worker_error(const QString& msg)
{
    update_state(false);
    QMessageBox::critical(this, "Error", msg);
}

void MainWindow::on_worker_stats(quint64 reports)
{
    stats_label_->setText(QString("Reports sent: %1").arg(reports));
}

void MainWindow::on_worker_log(const QString& msg)
{
    status_label_->setText(msg);
}

void MainWindow::on_mapping_changed()
{
    auto cfg = editor_->config();
    cfg.mouse_stick = mouse_stick_;
    MappingStorage::save(cfg);
    controller_map_->set_mappings(cfg.mappings);
    if (worker_->isRunning())
        worker_->update_config(cfg);
}

void MainWindow::on_sensitivity_changed()
{
    mouse_stick_.enabled         = mouse_enable_cb_->isChecked();
    mouse_stick_.use_right_stick = !right_stick_cb_->isChecked();
    mouse_stick_.sensitivity_x   = static_cast<float>(sens_x_spin_->value());
    mouse_stick_.sensitivity_y   = static_cast<float>(sens_y_spin_->value());
    save_settings();
    controller_map_->set_mouse_stick(mouse_stick_);
    if (worker_->isRunning()) push_config_to_worker();
}

void MainWindow::on_tray_activated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick) {
        showNormal(); activateWindow();
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void MainWindow::update_state(bool running)
{
    is_running_ = running;
    start_btn_->setEnabled(!running);
    pause_btn_->setEnabled(running);
    pause_btn_->setText("⏸  Pause");
    stop_btn_->setEnabled(running);
    devices_group_->setEnabled(!running);
    editor_->set_running(running);

    if (running) {
        status_label_->setText("Running — keyboard grabbed, virtual DualSense active");
        tray_icon_->setToolTip("kb-to-ds5 — running");
    } else {
        status_label_->setText("Stopped");
        stats_label_->setText("Reports: 0");
        tray_icon_->setToolTip("kb-to-ds5 — stopped");
    }
}

void MainWindow::refresh_devices()
{
    const QStringList saved = settings_.value("selected_devices").toStringList();

    // Block itemChanged signals while rebuilding to avoid spurious saves
    device_list_->blockSignals(true);
    device_list_->clear();

    const auto devices = InputWorker::enumerate_devices();
    for (const auto& dev : devices) {
        const QString node = dev.path.section('/', -1);  // e.g. "event5"
        auto* item = new QListWidgetItem(node + " — " + dev.name);
        item->setData(Qt::UserRole, dev.path);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(saved.contains(dev.path) ? Qt::Checked : Qt::Unchecked);
        device_list_->addItem(item);
    }

    device_list_->blockSignals(false);
}

void MainWindow::save_device_selection()
{
    settings_.setValue("selected_devices", selected_device_paths());
}

QStringList MainWindow::selected_device_paths() const
{
    QStringList paths;
    for (int i = 0; i < device_list_->count(); ++i) {
        auto* item = device_list_->item(i);
        if (item->checkState() == Qt::Checked)
            paths << item->data(Qt::UserRole).toString();
    }
    return paths;
}

void MainWindow::push_config_to_worker()
{
    auto cfg = editor_->config();
    cfg.mouse_stick = mouse_stick_;
    worker_->update_config(cfg);
}

void MainWindow::save_settings()
{
    settings_.setValue("background",    background_cb_->isChecked());
    settings_.setValue("mouse_enabled", mouse_stick_.enabled);
    settings_.setValue("mouse_right",   mouse_stick_.use_right_stick);
    settings_.setValue("sens_x",        mouse_stick_.sensitivity_x);
    settings_.setValue("sens_y",        mouse_stick_.sensitivity_y);
    settings_.setValue("touchpad_key",  mouse_stick_.touchpad_key);
}

void MainWindow::load_settings()
{
    background_cb_->setChecked(settings_.value("background", false).toBool());

    mouse_stick_.enabled         = settings_.value("mouse_enabled", true).toBool();
    mouse_stick_.use_right_stick = settings_.value("mouse_right",   true).toBool();
    mouse_stick_.sensitivity_x   = settings_.value("sens_x",        0.25f).toFloat();
    mouse_stick_.sensitivity_y   = settings_.value("sens_y",        0.25f).toFloat();
    mouse_stick_.touchpad_key    = settings_.value("touchpad_key",  0).toInt();

    mouse_enable_cb_->setChecked(mouse_stick_.enabled);
    right_stick_cb_->setChecked(!mouse_stick_.use_right_stick);
    sens_x_spin_->setValue(mouse_stick_.sensitivity_x);
    sens_y_spin_->setValue(mouse_stick_.sensitivity_y);
    if (mouse_stick_.touchpad_key != 0)
        touchpad_key_label_->setText(MappingStorage::keyName(mouse_stick_.touchpad_key));

    // Load mappings into editor and controller map
    const auto cfg = MappingStorage::load();
    editor_->set_config(cfg);
    controller_map_->set_mappings(cfg.mappings);
    controller_map_->set_mouse_stick(mouse_stick_);

    // Populate device list (restores saved selection via QSettings)
    refresh_devices();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (background_cb_->isChecked() && QSystemTrayIcon::isSystemTrayAvailable()) {
        hide();
        event->ignore();
    } else {
        if (worker_) {
            worker_->stop();
            worker_->wait(3000);
        }
        tray_icon_->hide();
        event->accept();
        QApplication::quit();
    }
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
    // ── Title-bar drag ────────────────────────────────────────────────────────
    if (obj == custom_title_bar_ && event->type() == QEvent::MouseButtonPress) {
        auto* e = static_cast<QMouseEvent*>(event);
        if (e->button() == Qt::LeftButton && windowHandle())
            windowHandle()->startSystemMove();
    }

    // ── Edge / corner resize ──────────────────────────────────────────────────
    static constexpr int EDGE = 6;  // hit-zone width in pixels
    const auto etype = event->type();
    if (etype == QEvent::MouseMove || etype == QEvent::MouseButtonPress) {
        auto* e   = static_cast<QMouseEvent*>(event);
        const QPoint gpos = e->globalPosition().toPoint();
        const QRect  wr   = geometry();

        if (wr.contains(gpos)) {
            const int x = gpos.x() - wr.left();
            const int y = gpos.y() - wr.top();

            Qt::Edges edges;
            if (x <= EDGE)              edges |= Qt::LeftEdge;
            if (x >= wr.width() - EDGE) edges |= Qt::RightEdge;
            if (y <= EDGE)              edges |= Qt::TopEdge;
            if (y >= wr.height() - EDGE) edges |= Qt::BottomEdge;

            if (etype == QEvent::MouseMove) {
                if      (edges == (Qt::LeftEdge  | Qt::TopEdge) ||
                         edges == (Qt::RightEdge | Qt::BottomEdge))
                    setCursor(Qt::SizeFDiagCursor);
                else if (edges == (Qt::RightEdge | Qt::TopEdge) ||
                         edges == (Qt::LeftEdge  | Qt::BottomEdge))
                    setCursor(Qt::SizeBDiagCursor);
                else if (edges & (Qt::LeftEdge | Qt::RightEdge))
                    setCursor(Qt::SizeHorCursor);
                else if (edges & (Qt::TopEdge | Qt::BottomEdge))
                    setCursor(Qt::SizeVerCursor);
                else
                    unsetCursor();
            } else if (edges && e->button() == Qt::LeftButton && windowHandle()) {
                windowHandle()->startSystemResize(edges);
                return true;
            }
        } else if (etype == QEvent::MouseMove) {
            unsetCursor();
        }
    }

    return QMainWindow::eventFilter(obj, event);
}
