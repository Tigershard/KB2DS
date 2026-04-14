#include "mainwindow.hpp"
#include "controllermapwidget.hpp"
#include "igdbclient.hpp"
#include "inputworker.hpp"
#include "mappingeditorwidget.hpp"
#include "mappingstorage.hpp"
#include "saveprofiledialog.hpp"

#include <linux/input-event-codes.h>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QProcess>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
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

    worker_      = new InputWorker(this);
    igdb_client_ = new IgdbClient(this);

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

    // Status bar + profile display (always visible, side by side)
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

    // Profile display — right of the status group (clickable: opens profile picker)
    profile_box_ = new QWidget(content);
    profile_box_->setFixedWidth(110);
    profile_box_->setObjectName("ProfileBox");
    profile_box_->setAttribute(Qt::WA_StyledBackground);
    profile_box_->setStyleSheet(
        "QWidget#ProfileBox { background: #0d0d1e; border: 1px solid #2a2a45; border-radius: 6px; }"
        "QWidget#ProfileBox:hover { border-color: #00c9a7; background: #0f0f26; }");
    profile_box_->setCursor(Qt::PointingHandCursor);
    profile_box_->installEventFilter(this);

    auto* pb_layout = new QVBoxLayout(profile_box_);
    pb_layout->setContentsMargins(4, 4, 4, 4);
    pb_layout->setSpacing(0);

    profile_display_label_ = new QLabel(profile_box_);
    profile_display_label_->setAlignment(Qt::AlignCenter);
    profile_display_label_->setWordWrap(true);
    profile_display_label_->setStyleSheet(
        "color: #7878aa; font-size: 10px; background: transparent;");
    profile_display_label_->setText("No Profile");
    pb_layout->addWidget(profile_display_label_);

    auto* top_row = new QHBoxLayout();
    top_row->setSpacing(8);
    top_row->addWidget(status_group_, 1);
    top_row->addWidget(profile_box_);
    layout->addLayout(top_row);

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

    settings_layout->addWidget(settings_group_);

    // ── IGDB game art ──────────────────────────────────────────────────────────
    igdb_group_ = new QGroupBox("IGDB Game Art", settings_tab);
    auto* igdb_layout = new QVBoxLayout(igdb_group_);

    auto* igdb_hint = new QLabel(
        "Attach PS5 cover art to profiles. Requires a free Twitch developer account.<br>"
        "<a href='https://dev.twitch.tv/console/apps/create' style='color:#00c9a7;'>"
        "Register for free IGDB API keys \u2192</a>",
        settings_tab);
    igdb_hint->setStyleSheet("color: #7878aa; font-size: 11px;");
    igdb_hint->setTextFormat(Qt::RichText);
    igdb_hint->setTextInteractionFlags(Qt::TextBrowserInteraction);
    connect(igdb_hint, &QLabel::linkActivated, this, [this](const QString& url) {
        static const QStringList browsers = {
            "firefox", "firefox-bin", "chromium", "chromium-browser",
            "google-chrome", "brave", "vivaldi", "opera"
        };
        for (const QString& b : browsers) {
            if (QProcess::startDetached(b, {url})) return;
        }
        QMessageBox::information(this, "Open URL",
            "Could not launch a browser automatically.\nPlease visit:\n" + url);
    });
    igdb_layout->addWidget(igdb_hint);

    auto* igdb_id_row = new QHBoxLayout();
    igdb_id_row->addWidget(new QLabel("Client ID:", settings_tab));
    igdb_id_edit_ = new QLineEdit(settings_tab);
    igdb_id_edit_->setPlaceholderText("Twitch client ID");
    igdb_id_edit_->setStyleSheet(
        "QLineEdit { background: #0d0d1e; color: #e0e0f0; border: 1px solid #2a2a45;"
        "border-radius: 4px; padding: 3px 8px; font-size: 12px; }");
    igdb_id_row->addWidget(igdb_id_edit_, 1);
    igdb_layout->addLayout(igdb_id_row);

    auto* igdb_secret_row = new QHBoxLayout();
    igdb_secret_row->addWidget(new QLabel("Client Secret:", settings_tab));
    igdb_secret_edit_ = new QLineEdit(settings_tab);
    igdb_secret_edit_->setPlaceholderText("Twitch client secret");
    igdb_secret_edit_->setEchoMode(QLineEdit::Password);
    igdb_secret_edit_->setStyleSheet(
        "QLineEdit { background: #0d0d1e; color: #e0e0f0; border: 1px solid #2a2a45;"
        "border-radius: 4px; padding: 3px 8px; font-size: 12px; }");
    igdb_secret_row->addWidget(igdb_secret_edit_, 1);
    igdb_layout->addLayout(igdb_secret_row);

    settings_layout->addWidget(igdb_group_);
    settings_layout->addStretch();
    main_tabs_->addTab(settings_tab, "Settings");

    // ── Tab 3: Mapping Table ──────────────────────────────────────────────────
    auto* mapping_tab    = new QWidget();
    auto* mapping_layout = new QVBoxLayout(mapping_tab);
    mapping_layout->setContentsMargins(8, 8, 8, 4);
    mapping_layout->setSpacing(6);

    auto* profile_row = new QHBoxLayout();
    profile_row->addWidget(new QLabel("Profile:", mapping_tab));
    profile_combo_ = new QComboBox(mapping_tab);
    profile_combo_->setMinimumWidth(140);
    profile_combo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    profile_combo_->setStyleSheet(
        "QComboBox { background: #0d0d1e; color: #e0e0f0; border: 1px solid #2a2a45;"
        "border-radius: 4px; padding: 3px 8px; font-size: 12px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: #0d0d1e; color: #e0e0f0;"
        "  border: 1px solid #2a2a45; selection-background-color: #1e1e35; }");
    profile_row->addWidget(profile_combo_, 1);

    profile_load_btn_ = new QPushButton("Load", mapping_tab);
    profile_load_btn_->setFixedWidth(64);
    profile_load_btn_->setStyleSheet(
        "QPushButton { background: #1e1e35; color: #00c9a7; border: 1px solid #2a2a45;"
        "border-radius: 6px; padding: 3px 8px; font-size: 12px; }"
        "QPushButton:hover { background: #00c9a7; color: #0a0a1a; }"
        "QPushButton:disabled { background: #1e1e35; color: #4a4a6a; border-color: #2a2a45; }");
    profile_row->addWidget(profile_load_btn_);

    profile_save_btn_ = new QPushButton("Save As...", mapping_tab);
    profile_save_btn_->setFixedWidth(84);
    profile_save_btn_->setStyleSheet(
        "QPushButton { background: #1e1e35; color: #a0a0c0; border: 1px solid #2a2a45;"
        "border-radius: 6px; padding: 3px 8px; font-size: 12px; }"
        "QPushButton:hover { background: #2a2a45; color: #e0e0f0; }");
    profile_row->addWidget(profile_save_btn_);

    profile_delete_btn_ = new QPushButton("Delete", mapping_tab);
    profile_delete_btn_->setFixedWidth(64);
    profile_delete_btn_->setStyleSheet(
        "QPushButton { background: #1e1220; color: #e05252; border: 1px solid #5a2a2a;"
        "border-radius: 6px; padding: 3px 8px; font-size: 12px; }"
        "QPushButton:hover { background: #e05252; color: #fff; }"
        "QPushButton:disabled { background: #1e1e35; color: #4a4a6a; border-color: #2a2a45; }");
    profile_row->addWidget(profile_delete_btn_);

    mapping_layout->addLayout(profile_row);

    // ── Mouse Sensitivity (per-profile) ───────────────────────────────────────
    auto* mouse_group  = new QGroupBox("Mouse Sensitivity", mapping_tab);
    auto* mouse_layout = new QVBoxLayout(mouse_group);
    mouse_layout->setSpacing(6);

    auto* ms_header = new QHBoxLayout();
    mouse_enable_cb_ = new QCheckBox("Mouse → Stick", mapping_tab);
    right_stick_cb_  = new QCheckBox("Use left stick instead", mapping_tab);
    ms_header->addWidget(mouse_enable_cb_);
    ms_header->addWidget(right_stick_cb_);
    ms_header->addStretch();
    mouse_layout->addLayout(ms_header);

    auto* sens_row = new QHBoxLayout();
    sens_row->addWidget(new QLabel("Sensitivity X:", mapping_tab));
    sens_x_spin_ = new QDoubleSpinBox(mapping_tab);
    sens_x_spin_->setRange(0.01, 10.0);
    sens_x_spin_->setSingleStep(0.05);
    sens_x_spin_->setDecimals(2);
    sens_x_spin_->setValue(0.25);
    sens_x_spin_->setFixedWidth(70);
    sens_row->addWidget(sens_x_spin_);
    sens_row->addSpacing(12);
    sens_row->addWidget(new QLabel("Y:", mapping_tab));
    sens_y_spin_ = new QDoubleSpinBox(mapping_tab);
    sens_y_spin_->setRange(0.01, 10.0);
    sens_y_spin_->setSingleStep(0.05);
    sens_y_spin_->setDecimals(2);
    sens_y_spin_->setValue(0.25);
    sens_y_spin_->setFixedWidth(70);
    sens_row->addWidget(sens_y_spin_);
    sens_row->addStretch();
    mouse_layout->addLayout(sens_row);

    auto* tp_row = new QHBoxLayout();
    tp_row->addWidget(new QLabel("Touchpad Mode Key:", mapping_tab));
    touchpad_key_label_ = new QLabel("None", mapping_tab);
    touchpad_key_label_->setStyleSheet("color: #00c9a7; font-size: 12px; min-width: 80px;");
    tp_row->addWidget(touchpad_key_label_);
    touchpad_key_btn_ = new QPushButton("Set Key", mapping_tab);
    touchpad_key_btn_->setFixedWidth(80);
    touchpad_key_btn_->setStyleSheet(
        "QPushButton { background: #1e1e35; color: #a0a0c0; border: 1px solid #2a2a45;"
        "border-radius: 6px; padding: 3px 8px; font-size: 12px; }"
        "QPushButton:hover { background: #2a2a45; color: #e0e0f0; }");
    tp_row->addWidget(touchpad_key_btn_);
    touchpad_key_clear_ = new QPushButton("Clear", mapping_tab);
    touchpad_key_clear_->setFixedWidth(60);
    touchpad_key_clear_->setStyleSheet(
        "QPushButton { background: #1e1220; color: #e05252; border: 1px solid #5a2a2a;"
        "border-radius: 6px; padding: 3px 8px; font-size: 12px; }"
        "QPushButton:hover { background: #e05252; color: #fff; }");
    tp_row->addWidget(touchpad_key_clear_);
    tp_row->addStretch();
    mouse_layout->addLayout(tp_row);

    mapping_layout->addWidget(mouse_group);

    editor_ = new MappingEditorWidget();
    mapping_layout->addWidget(editor_, 1);
    main_tabs_->addTab(mapping_tab, "Mapping Table");

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

    connect(profile_load_btn_,  &QPushButton::clicked, this, &MainWindow::on_profile_load);
    connect(profile_save_btn_,  &QPushButton::clicked, this, &MainWindow::on_profile_save_as);
    connect(profile_delete_btn_,&QPushButton::clicked, this, &MainWindow::on_profile_delete);
    connect(editor_, &MappingEditorWidget::save_requested, this, &MainWindow::on_profile_save_current);

    connect(profile_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                const QString name = profile_combo_->currentText();
                if (!name.isEmpty())
                    load_profile_by_name(name);
            });

    // Save IGDB credentials whenever either field loses focus
    auto save_igdb = [this]() {
        settings_.setValue("igdb_client_id",     igdb_id_edit_->text().trimmed());
        settings_.setValue("igdb_client_secret", igdb_secret_edit_->text().trimmed());
        igdb_client_->set_credentials(igdb_id_edit_->text().trimmed(),
                                      igdb_secret_edit_->text().trimmed());
    };
    connect(igdb_id_edit_,     &QLineEdit::editingFinished, this, save_igdb);
    connect(igdb_secret_edit_, &QLineEdit::editingFinished, this, save_igdb);

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
            editor_->mark_dirty();
            if (worker_->isRunning()) push_config_to_worker();
        }
    });

    connect(touchpad_key_clear_, &QPushButton::clicked, this, [this]() {
        mouse_stick_.touchpad_key = 0;
        touchpad_key_label_->setText("None");
        save_settings();
        editor_->mark_dirty();
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
    editor_->mark_dirty();
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
    settings_.setValue("igdb_client_id",     igdb_id_edit_->text().trimmed());
    settings_.setValue("igdb_client_secret", igdb_secret_edit_->text().trimmed());
}

void MainWindow::load_settings()
{
    background_cb_->setChecked(settings_.value("background", false).toBool());

    mouse_stick_.enabled         = settings_.value("mouse_enabled", true).toBool();
    mouse_stick_.use_right_stick = settings_.value("mouse_right",   true).toBool();
    mouse_stick_.sensitivity_x   = settings_.value("sens_x",        0.25f).toFloat();
    mouse_stick_.sensitivity_y   = settings_.value("sens_y",        0.25f).toFloat();
    mouse_stick_.touchpad_key    = settings_.value("touchpad_key",  0).toInt();

    // Block signals while populating widgets so that valueChanged / toggled
    // don't fire on_sensitivity_changed() mid-load and call save_settings()
    // with still-empty IGDB fields, overwriting the real saved credentials.
    mouse_enable_cb_->blockSignals(true);
    right_stick_cb_->blockSignals(true);
    sens_x_spin_->blockSignals(true);
    sens_y_spin_->blockSignals(true);

    mouse_enable_cb_->setChecked(mouse_stick_.enabled);
    right_stick_cb_->setChecked(!mouse_stick_.use_right_stick);
    sens_x_spin_->setValue(mouse_stick_.sensitivity_x);
    sens_y_spin_->setValue(mouse_stick_.sensitivity_y);

    mouse_enable_cb_->blockSignals(false);
    right_stick_cb_->blockSignals(false);
    sens_x_spin_->blockSignals(false);
    sens_y_spin_->blockSignals(false);

    if (mouse_stick_.touchpad_key != 0)
        touchpad_key_label_->setText(MappingStorage::keyName(mouse_stick_.touchpad_key));

    // IGDB credentials (loaded before any signal-triggered save can run)
    const QString igdb_id     = settings_.value("igdb_client_id").toString();
    const QString igdb_secret = settings_.value("igdb_client_secret").toString();
    igdb_id_edit_->setText(igdb_id);
    igdb_secret_edit_->setText(igdb_secret);
    igdb_client_->set_credentials(igdb_id, igdb_secret);

    // Load mappings into editor and controller map
    const auto cfg = MappingStorage::load();
    editor_->set_config(cfg);
    controller_map_->set_mappings(cfg.mappings);
    controller_map_->set_mouse_stick(mouse_stick_);

    // Populate profile list and restore last selection (block auto-load signal)
    refresh_profile_list();
    const QString last_profile = settings_.value("last_profile").toString();
    if (!last_profile.isEmpty()) {
        profile_combo_->blockSignals(true);
        const int idx = profile_combo_->findText(last_profile);
        if (idx >= 0) profile_combo_->setCurrentIndex(idx);
        profile_combo_->blockSignals(false);
    }
    update_profile_display(last_profile);

    // Populate device list (restores saved selection via QSettings)
    refresh_devices();
}

void MainWindow::update_profile_display(const QString& name)
{
    if (name.isEmpty()) {
        profile_display_label_->setPixmap({});
        profile_display_label_->setText("No Profile");
        return;
    }
    const QString cover = MappingStorage::profileCoverPath(name);
    if (QFile::exists(cover)) {
        const int w = profile_display_label_->width()  - 2;
        const int h = profile_display_label_->height() - 2;
        const QPixmap pm = QPixmap(cover).scaled(
            w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        profile_display_label_->setPixmap(pm);
        profile_display_label_->setToolTip(name);
    } else {
        profile_display_label_->setPixmap({});
        profile_display_label_->setText(name);
        profile_display_label_->setToolTip({});
    }
}

void MainWindow::refresh_profile_list()
{
    profile_combo_->blockSignals(true);
    const QString current = profile_combo_->currentText();
    profile_combo_->clear();
    profile_combo_->setIconSize({28, 40});
    for (const QString& name : MappingStorage::listProfiles()) {
        const QString cover = MappingStorage::profileCoverPath(name);
        if (QFile::exists(cover)) {
            const QPixmap pm = QPixmap(cover).scaled(
                28, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            profile_combo_->addItem(QIcon(pm), name);
        } else {
            profile_combo_->addItem(name);
        }
    }
    const int idx = profile_combo_->findText(current);
    if (idx >= 0) profile_combo_->setCurrentIndex(idx);
    profile_combo_->blockSignals(false);

    const bool has = profile_combo_->count() > 0;
    profile_load_btn_->setEnabled(has);
    profile_delete_btn_->setEnabled(has);
}

void MainWindow::show_profile_menu()
{
    const QStringList profiles = MappingStorage::listProfiles();
    if (profiles.isEmpty()) return;

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background: #1a1a2e; color: #e0e0f0; border: 1px solid #2a2a45;"
        "  border-radius: 6px; padding: 4px; }"
        "QMenu::item { padding: 6px 16px 6px 8px; border-radius: 4px; }"
        "QMenu::item:selected { background: #2a2a45; color: #00c9a7; }"
        "QMenu::item:checked { color: #00c9a7; font-weight: bold; }");

    const QString current = profile_combo_->currentText();
    for (const QString& name : profiles) {
        QAction* action = menu.addAction(name);
        const QString cover = MappingStorage::profileCoverPath(name);
        if (QFile::exists(cover)) {
            const QPixmap pm = QPixmap(cover).scaled(
                20, 28, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            action->setIcon(QIcon(pm));
        }
        action->setCheckable(true);
        action->setChecked(name == current);
    }

    // Show the menu just below the profile box
    const QPoint pos = profile_box_->mapToGlobal(
        QPoint(0, profile_box_->height() + 2));
    QAction* chosen = menu.exec(pos);
    if (chosen)
        load_profile_by_name(chosen->text());
}

void MainWindow::load_profile_by_name(const QString& name)
{
    if (name.isEmpty()) return;

    const auto cfg = MappingStorage::loadProfile(name);
    editor_->set_config(cfg);

    mouse_stick_ = cfg.mouse_stick;
    mouse_enable_cb_->blockSignals(true);
    right_stick_cb_->blockSignals(true);
    sens_x_spin_->blockSignals(true);
    sens_y_spin_->blockSignals(true);
    mouse_enable_cb_->setChecked(mouse_stick_.enabled);
    right_stick_cb_->setChecked(!mouse_stick_.use_right_stick);
    sens_x_spin_->setValue(mouse_stick_.sensitivity_x);
    sens_y_spin_->setValue(mouse_stick_.sensitivity_y);
    touchpad_key_label_->setText(
        mouse_stick_.touchpad_key != 0
            ? MappingStorage::keyName(mouse_stick_.touchpad_key)
            : "None");
    mouse_enable_cb_->blockSignals(false);
    right_stick_cb_->blockSignals(false);
    sens_x_spin_->blockSignals(false);
    sens_y_spin_->blockSignals(false);

    MappingStorage::save(cfg);
    save_settings();
    controller_map_->set_mappings(cfg.mappings);
    controller_map_->set_mouse_stick(mouse_stick_);
    if (worker_->isRunning()) push_config_to_worker();
    settings_.setValue("last_profile", name);

    // Keep the combo box in sync regardless of which code path triggered this
    profile_combo_->blockSignals(true);
    const int combo_idx = profile_combo_->findText(name);
    if (combo_idx >= 0) profile_combo_->setCurrentIndex(combo_idx);
    profile_combo_->blockSignals(false);

    update_profile_display(name);
}

void MainWindow::on_profile_load()
{
    // Browse for any profile JSON file (e.g. shared profiles downloaded from GitHub)
    const QString path = QFileDialog::getOpenFileName(
        this, "Load Profile",
        QString(),
        "Profile files (*.json)");
    if (path.isEmpty()) return;

    const auto cfg = MappingStorage::loadFromPath(path);
    const QString name = QFileInfo(path).completeBaseName();

    editor_->set_config(cfg);

    mouse_stick_ = cfg.mouse_stick;
    mouse_enable_cb_->blockSignals(true);
    right_stick_cb_->blockSignals(true);
    sens_x_spin_->blockSignals(true);
    sens_y_spin_->blockSignals(true);
    mouse_enable_cb_->setChecked(mouse_stick_.enabled);
    right_stick_cb_->setChecked(!mouse_stick_.use_right_stick);
    sens_x_spin_->setValue(mouse_stick_.sensitivity_x);
    sens_y_spin_->setValue(mouse_stick_.sensitivity_y);
    touchpad_key_label_->setText(
        mouse_stick_.touchpad_key != 0
            ? MappingStorage::keyName(mouse_stick_.touchpad_key)
            : "None");
    mouse_enable_cb_->blockSignals(false);
    right_stick_cb_->blockSignals(false);
    sens_x_spin_->blockSignals(false);
    sens_y_spin_->blockSignals(false);

    MappingStorage::save(cfg);
    save_settings();
    controller_map_->set_mappings(cfg.mappings);
    controller_map_->set_mouse_stick(mouse_stick_);
    if (worker_->isRunning()) push_config_to_worker();
    update_profile_display(name);
}

void MainWindow::on_profile_save_current()
{
    const QString name = profile_combo_->currentText();
    if (name.isEmpty()) {
        // No profile selected — fall back to Save As
        on_profile_save_as();
        return;
    }

    auto cfg = editor_->config();
    cfg.mouse_stick = mouse_stick_;
    MappingStorage::saveProfile(cfg, name);
    settings_.setValue("last_profile", name);
    editor_->mark_saved();
}

void MainWindow::on_profile_save_as()
{
    SaveProfileDialog dlg(igdb_client_, profile_combo_->currentText(), this);
    if (dlg.exec() != QDialog::Accepted || dlg.profile_name().isEmpty()) return;

    const QString name = dlg.profile_name();
    auto cfg = editor_->config();
    cfg.mouse_stick = mouse_stick_;
    MappingStorage::saveProfile(cfg, name);

    const QPixmap cover = dlg.selected_cover();
    if (!cover.isNull())
        cover.save(MappingStorage::profileCoverPath(name), "PNG");

    refresh_profile_list();
    const int idx = profile_combo_->findText(name);
    if (idx >= 0) profile_combo_->setCurrentIndex(idx);
    settings_.setValue("last_profile", name);
    update_profile_display(name);
    editor_->mark_saved();
}

void MainWindow::on_profile_delete()
{
    const QString name = profile_combo_->currentText();
    if (name.isEmpty()) return;

    const auto reply = QMessageBox::question(this, "Delete Profile",
        QString("Delete profile \"%1\"?").arg(name),
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    MappingStorage::deleteProfile(name);
    settings_.remove("last_profile");
    refresh_profile_list();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (background_cb_->isChecked() && QSystemTrayIcon::isSystemTrayAvailable()) {
        hide();
        event->ignore();
    } else {
        save_settings();
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
    // ── Profile box click ─────────────────────────────────────────────────────
    if (obj == profile_box_ && event->type() == QEvent::MouseButtonPress) {
        auto* e = static_cast<QMouseEvent*>(event);
        if (e->button() == Qt::LeftButton) {
            show_profile_menu();
            return true;
        }
    }

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
