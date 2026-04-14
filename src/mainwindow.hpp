#pragma once

#include "mapping.hpp"

#include <QMainWindow>
#include <QSettings>
#include <QSystemTrayIcon>

class ControllerMapWidget;
class InputWorker;
class MappingEditorWidget;
class IgdbClient;
class QCheckBox;
class QComboBox;
class QRadioButton;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
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
    void on_profile_load();
    void on_profile_save_as();
    void on_profile_save_current();
    void on_profile_delete();
    void on_theme_changed();

private:
    void setup_ui();
    void setup_tray();
    void update_state(bool running);
    void save_settings();
    void load_settings();
    void push_config_to_worker();
    void refresh_devices();
    void save_device_selection();
    void refresh_profile_list();
    void update_profile_display(const QString& name);
    void load_profile_by_name(const QString& name);
    void show_profile_menu();
    void apply_theme();
    QStringList selected_device_paths() const;

    // ── UI elements ───────────────────────────────────────────────────────────
    QWidget*             custom_title_bar_    = nullptr;
    QWidget*             profile_box_         = nullptr;
    QLabel*              title_label_      = nullptr;
    QLabel*              status_label_         = nullptr;
    QLabel*              stats_label_          = nullptr;
    QGroupBox*           status_group_         = nullptr;
    QLabel*              profile_display_label_= nullptr;
    QPushButton*         start_btn_        = nullptr;
    QPushButton*         pause_btn_        = nullptr;
    QPushButton*         stop_btn_         = nullptr;
    QGroupBox*           devices_group_    = nullptr;
    QListWidget*         device_list_      = nullptr;
    QPushButton*         refresh_btn_      = nullptr;
    QGroupBox*           settings_group_   = nullptr;
    QCheckBox*           background_cb_    = nullptr;
    QCheckBox*           mouse_enable_cb_  = nullptr;
    QRadioButton*        right_stick_rb_   = nullptr;
    QRadioButton*        left_stick_rb_    = nullptr;
    QDoubleSpinBox*      sens_x_spin_      = nullptr;
    QDoubleSpinBox*      sens_y_spin_      = nullptr;
    QLabel*              touchpad_key_label_ = nullptr;
    QPushButton*         touchpad_key_btn_   = nullptr;
    QPushButton*         touchpad_key_clear_ = nullptr;
    QComboBox*           profile_combo_      = nullptr;
    QPushButton*         profile_load_btn_   = nullptr;
    QPushButton*         profile_save_btn_   = nullptr;
    QPushButton*         profile_delete_btn_ = nullptr;
    QLabel*              igdb_hint_          = nullptr;
    QComboBox*           theme_combo_        = nullptr;
    MappingEditorWidget* editor_           = nullptr;
    ControllerMapWidget* controller_map_   = nullptr;
    QTabWidget*          main_tabs_        = nullptr;

    // ── Tray ──────────────────────────────────────────────────────────────────
    QSystemTrayIcon* tray_icon_       = nullptr;
    QMenu*           tray_menu_       = nullptr;
    QAction*         tray_show_       = nullptr;
    QAction*         tray_quit_       = nullptr;

    // ── IGDB ──────────────────────────────────────────────────────────────────
    QGroupBox*  igdb_group_      = nullptr;
    QLineEdit*  igdb_id_edit_    = nullptr;
    QLineEdit*  igdb_secret_edit_= nullptr;

    // ── State ─────────────────────────────────────────────────────────────────
    InputWorker* worker_      = nullptr;
    IgdbClient*  igdb_client_ = nullptr;
    QSettings    settings_;
    bool         is_running_  = false;

    // Mouse-stick config (owned here, synced to worker and editor)
    kb::MouseStickConfig mouse_stick_;
};
