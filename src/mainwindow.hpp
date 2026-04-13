#pragma once

#include "mapping.hpp"

#include <QMainWindow>
#include <QSettings>
#include <QSystemTrayIcon>

class ControllerMapWidget;
class InputWorker;
class MappingEditorWidget;
class QCheckBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QListWidget;
class QMenu;
class QPushButton;
class QTabWidget;
class QTimer;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void on_start_clicked();
    void on_stop_clicked();
    void on_worker_started();
    void on_worker_error(const QString& msg);
    void on_worker_stats(quint64 reports);
    void on_worker_log(const QString& msg);
    void on_mapping_changed();
    void on_sensitivity_changed();
    void on_tray_activated(QSystemTrayIcon::ActivationReason reason);

private:
    void setup_ui();
    void setup_tray();
    void update_state(bool running);
    void save_settings();
    void load_settings();
    void push_config_to_worker();
    void refresh_devices();
    void save_device_selection();
    QStringList selected_device_paths() const;

    // ── UI elements ───────────────────────────────────────────────────────────
    QWidget*             custom_title_bar_ = nullptr;
    QLabel*              title_label_      = nullptr;
    QLabel*              status_label_     = nullptr;
    QLabel*              stats_label_      = nullptr;
    QGroupBox*           status_group_     = nullptr;
    QPushButton*         start_btn_        = nullptr;
    QPushButton*         pause_btn_        = nullptr;
    QPushButton*         stop_btn_         = nullptr;
    QGroupBox*           devices_group_    = nullptr;
    QListWidget*         device_list_      = nullptr;
    QPushButton*         refresh_btn_      = nullptr;
    QGroupBox*           settings_group_   = nullptr;
    QCheckBox*           background_cb_    = nullptr;
    QCheckBox*           mouse_enable_cb_  = nullptr;
    QCheckBox*           right_stick_cb_   = nullptr;
    QDoubleSpinBox*      sens_x_spin_      = nullptr;
    QDoubleSpinBox*      sens_y_spin_      = nullptr;
    QLabel*              touchpad_key_label_ = nullptr;
    QPushButton*         touchpad_key_btn_   = nullptr;
    QPushButton*         touchpad_key_clear_ = nullptr;
    MappingEditorWidget* editor_           = nullptr;
    ControllerMapWidget* controller_map_   = nullptr;
    QTabWidget*          main_tabs_        = nullptr;

    // ── Tray ──────────────────────────────────────────────────────────────────
    QSystemTrayIcon* tray_icon_       = nullptr;
    QMenu*           tray_menu_       = nullptr;
    QAction*         tray_show_       = nullptr;
    QAction*         tray_quit_       = nullptr;

    // ── State ─────────────────────────────────────────────────────────────────
    InputWorker* worker_     = nullptr;
    QSettings    settings_;
    bool         is_running_ = false;

    // Mouse-stick config (owned here, synced to worker and editor)
    kb::MouseStickConfig mouse_stick_;
};
