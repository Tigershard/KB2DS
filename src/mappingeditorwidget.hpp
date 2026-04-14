#pragma once

#include "mapping.hpp"

#include <QDialog>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QTableWidget;

// ── Key capture dialog ────────────────────────────────────────────────────────

class KeyCaptureDialog : public QDialog {
    Q_OBJECT
public:
    explicit KeyCaptureDialog(QWidget* parent = nullptr);

    int captured_evdev_code() const { return evdev_code_; }
    QString captured_name()  const { return key_name_; }

    // Call these for mouse button mappings (bypasses key capture)
    void set_mouse_button(int btn_code, const QString& name);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    QLabel* hint_label_    = nullptr;
    int     evdev_code_    = -1;
    QString key_name_;

    static int qt_key_to_evdev(int qt_key, Qt::KeyboardModifiers mods);
};

// ── Output picker dialog ──────────────────────────────────────────────────────

class OutputPickerDialog : public QDialog {
    Q_OBJECT
public:
    explicit OutputPickerDialog(QWidget* parent = nullptr);

    // Returns index into kb::DS5_OUTPUTS[], or -1 if cancelled
    int selected_output_index() const { return selected_; }

private:
    int selected_ = -1;
};

// ── Mapping editor widget ─────────────────────────────────────────────────────

class MappingEditorWidget : public QWidget {
    Q_OBJECT
public:
    explicit MappingEditorWidget(QWidget* parent = nullptr);

    void set_config(const kb::Config& config);
    kb::Config config() const;

    void set_running(bool running);
    void retranslate(bool ru);

    // Call after the profile has been persisted so the Save button goes grey
    void mark_saved();
    // Call when an external change (e.g. mouse sensitivity) should enable Save
    void mark_dirty();
    // Rebuild the table (e.g. to pick up new theme colours for arrow items)
    void refresh_display();

signals:
    void config_changed();
    void save_requested();   // emitted when the Save button is clicked

private slots:
    void on_add_clicked();
    void on_delete_clicked();
    void on_cell_changed(int row, int col);
    void on_cell_double_clicked(int row, int col);
    void on_context_menu(const QPoint& pos);

private:
    void rebuild_table();
    void add_table_row(int row, const kb::Mapping& m);
    void remap_input(int row);
    void remap_output(int row);

    QTableWidget* table_      = nullptr;
    QPushButton*  add_btn_    = nullptr;
    QPushButton*  delete_btn_ = nullptr;
    QPushButton*  save_btn_   = nullptr;

    QList<kb::Mapping> mappings_;
    bool               updating_ = false;   // suppress recursive signals
    bool               dirty_    = false;   // unsaved changes exist
};
