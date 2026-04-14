#include "mappingeditorwidget.hpp"
#include "apptheme.hpp"
#include "mappingstorage.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <linux/input-event-codes.h>

// ── Qt key → evdev code lookup ────────────────────────────────────────────────

static const struct { int qt_key; int evdev; } QT_TO_EVDEV[] = {
    {Qt::Key_Escape,      KEY_ESC},
    {Qt::Key_Tab,         KEY_TAB},
    {Qt::Key_Backspace,   KEY_BACKSPACE},
    {Qt::Key_Return,      KEY_ENTER},
    {Qt::Key_Enter,       KEY_KPENTER},
    {Qt::Key_Delete,      KEY_DELETE},
    {Qt::Key_Insert,      KEY_INSERT},
    {Qt::Key_Home,        KEY_HOME},
    {Qt::Key_End,         KEY_END},
    {Qt::Key_PageUp,      KEY_PAGEUP},
    {Qt::Key_PageDown,    KEY_PAGEDOWN},
    {Qt::Key_Left,        KEY_LEFT},
    {Qt::Key_Right,       KEY_RIGHT},
    {Qt::Key_Up,          KEY_UP},
    {Qt::Key_Down,        KEY_DOWN},
    {Qt::Key_Space,       KEY_SPACE},
    {Qt::Key_CapsLock,    KEY_CAPSLOCK},
    {Qt::Key_NumLock,     KEY_NUMLOCK},
    {Qt::Key_ScrollLock,  KEY_SCROLLLOCK},
    {Qt::Key_F1,  KEY_F1},  {Qt::Key_F2,  KEY_F2},
    {Qt::Key_F3,  KEY_F3},  {Qt::Key_F4,  KEY_F4},
    {Qt::Key_F5,  KEY_F5},  {Qt::Key_F6,  KEY_F6},
    {Qt::Key_F7,  KEY_F7},  {Qt::Key_F8,  KEY_F8},
    {Qt::Key_F9,  KEY_F9},  {Qt::Key_F10, KEY_F10},
    {Qt::Key_F11, KEY_F11}, {Qt::Key_F12, KEY_F12},
    {Qt::Key_Shift,        KEY_LEFTSHIFT},
    {Qt::Key_Control,      KEY_LEFTCTRL},
    {Qt::Key_Alt,          KEY_LEFTALT},
    {Qt::Key_Meta,         KEY_LEFTMETA},
    {Qt::Key_A, KEY_A}, {Qt::Key_B, KEY_B}, {Qt::Key_C, KEY_C},
    {Qt::Key_D, KEY_D}, {Qt::Key_E, KEY_E}, {Qt::Key_F, KEY_F},
    {Qt::Key_G, KEY_G}, {Qt::Key_H, KEY_H}, {Qt::Key_I, KEY_I},
    {Qt::Key_J, KEY_J}, {Qt::Key_K, KEY_K}, {Qt::Key_L, KEY_L},
    {Qt::Key_M, KEY_M}, {Qt::Key_N, KEY_N}, {Qt::Key_O, KEY_O},
    {Qt::Key_P, KEY_P}, {Qt::Key_Q, KEY_Q}, {Qt::Key_R, KEY_R},
    {Qt::Key_S, KEY_S}, {Qt::Key_T, KEY_T}, {Qt::Key_U, KEY_U},
    {Qt::Key_V, KEY_V}, {Qt::Key_W, KEY_W}, {Qt::Key_X, KEY_X},
    {Qt::Key_Y, KEY_Y}, {Qt::Key_Z, KEY_Z},
    {Qt::Key_0, KEY_0}, {Qt::Key_1, KEY_1}, {Qt::Key_2, KEY_2},
    {Qt::Key_3, KEY_3}, {Qt::Key_4, KEY_4}, {Qt::Key_5, KEY_5},
    {Qt::Key_6, KEY_6}, {Qt::Key_7, KEY_7}, {Qt::Key_8, KEY_8},
    {Qt::Key_9, KEY_9},
    {Qt::Key_Minus,       KEY_MINUS},
    {Qt::Key_Equal,       KEY_EQUAL},
    {Qt::Key_BracketLeft, KEY_LEFTBRACE},
    {Qt::Key_BracketRight,KEY_RIGHTBRACE},
    {Qt::Key_Semicolon,   KEY_SEMICOLON},
    {Qt::Key_Apostrophe,  KEY_APOSTROPHE},
    {Qt::Key_QuoteLeft,   KEY_GRAVE},
    {Qt::Key_Backslash,   KEY_BACKSLASH},
    {Qt::Key_Comma,       KEY_COMMA},
    {Qt::Key_Period,      KEY_DOT},
    {Qt::Key_Slash,       KEY_SLASH},
};

int KeyCaptureDialog::qt_key_to_evdev(int qt_key, Qt::KeyboardModifiers mods)
{
    // Handle shift-modified keys by stripping to base key
    (void)mods;
    for (const auto& e : QT_TO_EVDEV) {
        if (e.qt_key == qt_key) return e.evdev;
    }
    return -1;
}

// ── KeyCaptureDialog ──────────────────────────────────────────────────────────

KeyCaptureDialog::KeyCaptureDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Capture Input");
    setModal(true);
    setFixedSize(340, 180);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(16, 16, 16, 16);

    hint_label_ = new QLabel("Press a key or mouse button...", this);
    hint_label_->setAlignment(Qt::AlignCenter);
    hint_label_->setStyleSheet("font-size: 13px;");
    layout->addWidget(hint_label_);

    layout->addStretch();

    auto* cancel_btn = new QPushButton("Cancel", this);
    cancel_btn->setObjectName("DangerBtn");
    layout->addWidget(cancel_btn, 0, Qt::AlignRight);

    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);

    setFocusPolicy(Qt::StrongFocus);
    setFocus();
}

void KeyCaptureDialog::set_mouse_button(int btn_code, const QString& name)
{
    evdev_code_ = btn_code;
    key_name_   = name;
    accept();
}

void KeyCaptureDialog::keyPressEvent(QKeyEvent* event)
{
    const int code = qt_key_to_evdev(event->key(), event->modifiers());
    if (code >= 0) {
        evdev_code_ = code;
        key_name_   = MappingStorage::keyName(code);
        accept();
    }
    // ignore unknown keys
}

void KeyCaptureDialog::mousePressEvent(QMouseEvent* event)
{
    int btn_code = -1;
    QString name;
    switch (event->button()) {
        case Qt::LeftButton:   btn_code = BTN_LEFT;   name = "Mouse Left";   break;
        case Qt::RightButton:  btn_code = BTN_RIGHT;  name = "Mouse Right";  break;
        case Qt::MiddleButton: btn_code = BTN_MIDDLE; name = "Mouse Middle"; break;
        case Qt::BackButton:   btn_code = BTN_SIDE;   name = "Mouse Side";   break;
        case Qt::ForwardButton:btn_code = BTN_EXTRA;  name = "Mouse Extra";  break;
        default: break;
    }
    if (btn_code >= 0)
        set_mouse_button(btn_code, name);
}

// ── OutputPickerDialog ────────────────────────────────────────────────────────

OutputPickerDialog::OutputPickerDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Select DS5 Output");
    setModal(true);
    setMinimumWidth(300);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);
    layout->setContentsMargins(16, 16, 16, 16);

    auto* label = new QLabel("Choose the DualSense output:", this);
    label->setStyleSheet("font-size: 12px;");
    layout->addWidget(label);

    auto* combo = new QComboBox(this);
    for (int i = 0; i < kb::DS5_OUTPUT_COUNT; ++i)
        combo->addItem(QString::fromLatin1(kb::DS5_OUTPUTS[i].name), i);

    layout->addWidget(combo);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setObjectName("OkBtn");
    buttons->button(QDialogButtonBox::Cancel)->setObjectName("DangerBtn");
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, [this, combo]() {
        selected_ = combo->currentData().toInt();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

// ── MappingEditorWidget ───────────────────────────────────────────────────────

MappingEditorWidget::MappingEditorWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* group  = new QGroupBox("Key Mappings", this);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(group);

    auto* inner = new QVBoxLayout(group);
    inner->setSpacing(6);

    table_ = new QTableWidget(0, 4, this);
    table_->setHorizontalHeaderLabels({"", "DS5 Output", "→", "Input Key"});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    table_->setColumnWidth(0, 28);
    table_->setColumnWidth(2, 20);
    table_->verticalHeader()->hide();
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    inner->addWidget(table_);

    auto* btn_row = new QHBoxLayout();
    add_btn_    = new QPushButton("+ Add Mapping",   this);
    delete_btn_ = new QPushButton("Delete Selected", this);
    save_btn_   = new QPushButton("Save Profile",    this);

    add_btn_->setObjectName("AddMappingBtn");
    delete_btn_->setObjectName("DangerBtn");
    save_btn_->setObjectName("SaveMappingBtn");
    save_btn_->setEnabled(false);

    btn_row->addWidget(add_btn_);
    btn_row->addWidget(delete_btn_);
    btn_row->addStretch();
    btn_row->addWidget(save_btn_);
    inner->addLayout(btn_row);

    table_->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(add_btn_,    &QPushButton::clicked, this, &MappingEditorWidget::on_add_clicked);
    connect(delete_btn_, &QPushButton::clicked, this, &MappingEditorWidget::on_delete_clicked);
    connect(save_btn_,   &QPushButton::clicked, this, &MappingEditorWidget::save_requested);
    connect(table_, &QTableWidget::cellChanged,       this, &MappingEditorWidget::on_cell_changed);
    connect(table_, &QTableWidget::cellDoubleClicked, this, &MappingEditorWidget::on_cell_double_clicked);
    connect(table_, &QTableWidget::customContextMenuRequested, this, &MappingEditorWidget::on_context_menu);

    // Enable Save button whenever an unsaved change is made (while not running)
    connect(this, &MappingEditorWidget::config_changed, this, [this]() {
        dirty_ = true;
        save_btn_->setEnabled(add_btn_->isEnabled());  // disabled while running
    });
}

void MappingEditorWidget::set_config(const kb::Config& config)
{
    mappings_ = config.mappings;
    rebuild_table();
    mark_saved();
}

kb::Config MappingEditorWidget::config() const
{
    kb::Config cfg;
    cfg.mappings = mappings_;
    return cfg;
}

void MappingEditorWidget::set_running(bool running)
{
    add_btn_->setEnabled(!running);
    delete_btn_->setEnabled(!running);
    save_btn_->setEnabled(!running && dirty_);
}

void MappingEditorWidget::mark_saved()
{
    dirty_ = false;
    save_btn_->setEnabled(false);
}

void MappingEditorWidget::mark_dirty()
{
    dirty_ = true;
    save_btn_->setEnabled(add_btn_->isEnabled());  // disabled while running
}

void MappingEditorWidget::refresh_display()
{
    rebuild_table();
}

void MappingEditorWidget::retranslate(bool /*ru*/)
{
    // Localisation hook; English only for now
}

void MappingEditorWidget::rebuild_table()
{
    updating_ = true;
    table_->setRowCount(0);
    table_->setRowCount(mappings_.size());
    for (int i = 0; i < mappings_.size(); ++i)
        add_table_row(i, mappings_[i]);
    updating_ = false;
}

void MappingEditorWidget::add_table_row(int row, const kb::Mapping& m)
{
    // Col 0: enable checkbox
    auto* cb = new QCheckBox(this);
    cb->setChecked(m.enabled);
    cb->setStyleSheet("margin-left: 6px;");
    table_->setCellWidget(row, 0, cb);
    connect(cb, &QCheckBox::toggled, this, [this, row](bool checked) {
        if (row < mappings_.size()) {
            mappings_[row].enabled = checked;
            emit config_changed();
        }
    });

    // Col 1: DS5 output name — match against known outputs first for a clean name,
    // fall back to the stored label (stripping any "Key → " prefix for old entries)
    QString out_name;
    for (int i = 0; i < kb::DS5_OUTPUT_COUNT; ++i) {
        const auto& o = kb::DS5_OUTPUTS[i];
        if (m.output_kind == o.kind &&
            m.btn_byte    == o.btn_byte &&
            m.btn_mask    == o.btn_mask &&
            m.dpad_bit    == o.dpad_bit &&
            m.axis_offset == o.axis_offset &&
            m.axis_value  == o.axis_value)
        {
            out_name = QString::fromLatin1(o.name);
            break;
        }
    }
    if (out_name.isEmpty()) {
        out_name = m.label;
        const int arrow_pos = out_name.indexOf(" \u2192 ");
        if (arrow_pos >= 0)
            out_name = out_name.mid(arrow_pos + 3);
    }
    auto* out_item = new QTableWidgetItem(out_name);
    out_item->setTextAlignment(Qt::AlignCenter);
    table_->setItem(row, 1, out_item);

    // Col 2: arrow (colour follows current theme accent)
    auto* arrow = new QTableWidgetItem("→");
    arrow->setTextAlignment(Qt::AlignCenter);
    arrow->setForeground(QColor(Themes::current().accent));
    table_->setItem(row, 2, arrow);

    // Col 3: input key name
    const QString key_name = (m.input_kind == kb::InputKind::Key)
        ? MappingStorage::keyName(m.input_code)
        : QString("Mouse Axis %1").arg(m.input_code);
    auto* key_item = new QTableWidgetItem(key_name);
    key_item->setTextAlignment(Qt::AlignCenter);
    table_->setItem(row, 3, key_item);

    table_->setRowHeight(row, 28);
}

void MappingEditorWidget::on_add_clicked()
{
    // Step 1: pick DS5 output
    OutputPickerDialog od(this);
    if (od.exec() != QDialog::Accepted || od.selected_output_index() < 0) return;

    // Step 2: capture key
    KeyCaptureDialog kd(this);
    if (kd.exec() != QDialog::Accepted || kd.captured_evdev_code() < 0) return;

    const int idx = od.selected_output_index();
    const auto& out = kb::DS5_OUTPUTS[idx];

    kb::Mapping m;
    m.enabled     = true;
    m.input_kind  = kb::InputKind::Key;
    m.input_code  = kd.captured_evdev_code();
    m.output_kind = out.kind;
    m.btn_byte    = out.btn_byte;
    m.btn_mask    = out.btn_mask;
    m.dpad_bit    = out.dpad_bit;
    m.axis_offset = out.axis_offset;
    m.axis_value  = out.axis_value;
    m.label       = QString::fromLatin1(out.name);

    mappings_.append(m);
    rebuild_table();
    emit config_changed();
}

void MappingEditorWidget::on_delete_clicked()
{
    const int row = table_->currentRow();
    if (row < 0 || row >= mappings_.size()) return;

    mappings_.removeAt(row);
    rebuild_table();
    emit config_changed();
}

void MappingEditorWidget::on_cell_changed(int /*row*/, int /*col*/)
{
    if (updating_) return;
    emit config_changed();
}

void MappingEditorWidget::on_cell_double_clicked(int row, int col)
{
    if (row < 0 || row >= mappings_.size()) return;
    if (col == 1) remap_output(row);
    else if (col == 3) remap_input(row);
}

void MappingEditorWidget::on_context_menu(const QPoint& pos)
{
    const int row = table_->rowAt(pos.y());
    if (row < 0 || row >= mappings_.size()) return;

    QMenu menu(this);
    auto* act_input  = menu.addAction("Change Input Key");
    auto* act_output = menu.addAction("Change DS5 Output");
    menu.addSeparator();
    auto* act_delete = menu.addAction("Delete");
    act_delete->setEnabled(!add_btn_->isEnabled() == false);  // disabled while running

    const bool running = !add_btn_->isEnabled();
    act_input->setEnabled(!running);
    act_output->setEnabled(!running);
    act_delete->setEnabled(!running);

    auto* chosen = menu.exec(table_->viewport()->mapToGlobal(pos));
    if      (chosen == act_input)  remap_input(row);
    else if (chosen == act_output) remap_output(row);
    else if (chosen == act_delete) {
        mappings_.removeAt(row);
        rebuild_table();
        emit config_changed();
    }
}

void MappingEditorWidget::remap_input(int row)
{
    KeyCaptureDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted || dlg.captured_evdev_code() < 0) return;

    mappings_[row].input_kind = kb::InputKind::Key;
    mappings_[row].input_code = dlg.captured_evdev_code();
    rebuild_table();
    emit config_changed();
}

void MappingEditorWidget::remap_output(int row)
{
    OutputPickerDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted || dlg.selected_output_index() < 0) return;

    const auto& out = kb::DS5_OUTPUTS[dlg.selected_output_index()];
    mappings_[row].output_kind = out.kind;
    mappings_[row].btn_byte    = out.btn_byte;
    mappings_[row].btn_mask    = out.btn_mask;
    mappings_[row].dpad_bit    = out.dpad_bit;
    mappings_[row].axis_offset = out.axis_offset;
    mappings_[row].axis_value  = out.axis_value;
    mappings_[row].label       = QString::fromLatin1(out.name);
    rebuild_table();
    emit config_changed();
}
