#include "mappingstorage.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include <linux/input-event-codes.h>

namespace MappingStorage {

static QString config_path()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
           + "/mappings.json";
}

// ── Default mappings (used when no config file exists) ────────────────────────

static kb::Config make_defaults()
{
    kb::Config cfg;
    cfg.mouse_stick.enabled         = true;
    cfg.mouse_stick.use_right_stick = true;
    cfg.mouse_stick.sensitivity_x   = 0.25f;
    cfg.mouse_stick.sensitivity_y   = 0.25f;

    using K = kb::InputKind;
    using O = kb::OutputKind;

    auto btn = [](int key, const char* label, size_t byte, uint8_t mask) {
        kb::Mapping m;
        m.enabled = true;
        m.label = QString::fromUtf8(label);
        m.input_kind = K::Key;
        m.input_code = key;
        m.output_kind = O::Button;
        m.btn_byte = byte;
        m.btn_mask = mask;
        return m;
    };
    auto axis = [](int key, const char* label, size_t off, uint8_t val) {
        kb::Mapping m;
        m.enabled = true;
        m.label = QString::fromUtf8(label);
        m.input_kind = K::Key;
        m.input_code = key;
        m.output_kind = O::AxisFixed;
        m.axis_offset = off;
        m.axis_value  = val;
        return m;
    };
    auto dpad = [](int key, const char* label, uint8_t bit) {
        kb::Mapping m;
        m.enabled = true;
        m.label = QString::fromUtf8(label);
        m.input_kind = K::Key;
        m.input_code = key;
        m.output_kind = O::DpadDir;
        m.dpad_bit = bit;
        return m;
    };

    // WASD → left stick
    cfg.mappings << axis(KEY_W, "W → Left Stick Up",    2,   0);
    cfg.mappings << axis(KEY_S, "S → Left Stick Down",  2, 255);
    cfg.mappings << axis(KEY_A, "A → Left Stick Left",  1,   0);
    cfg.mappings << axis(KEY_D, "D → Left Stick Right", 1, 255);

    // Face buttons
    cfg.mappings << btn(KEY_SPACE,  "Space → Cross",    8, 0x20);
    cfg.mappings << btn(KEY_E,      "E → Square",       8, 0x10);
    cfg.mappings << btn(KEY_R,      "R → Triangle",     8, 0x80);
    cfg.mappings << btn(KEY_Q,      "Q → Circle",       8, 0x40);

    // Shoulder buttons
    cfg.mappings << btn(KEY_LEFTSHIFT,  "LShift → R1",  9, 0x02);
    cfg.mappings << btn(KEY_LEFTCTRL,   "LCtrl → L1",   9, 0x01);

    // Trigger analog (mouse buttons)
    {
        kb::Mapping m;
        m.enabled = true;
        m.label = "Mouse Left → R2";
        m.input_kind = K::Key;
        m.input_code = BTN_LEFT;
        m.output_kind = O::AxisFixed;
        m.axis_offset = 6;
        m.axis_value  = 255;
        cfg.mappings << m;
    }
    {
        kb::Mapping m;
        m.enabled = true;
        m.label = "Mouse Right → L2";
        m.input_kind = K::Key;
        m.input_code = BTN_RIGHT;
        m.output_kind = O::AxisFixed;
        m.axis_offset = 5;
        m.axis_value  = 255;
        cfg.mappings << m;
    }

    // D-pad
    cfg.mappings << dpad(KEY_UP,    "Up → DPad Up",      0x01);
    cfg.mappings << dpad(KEY_DOWN,  "Down → DPad Down",  0x04);
    cfg.mappings << dpad(KEY_LEFT,  "Left → DPad Left",  0x08);
    cfg.mappings << dpad(KEY_RIGHT, "Right → DPad Right",0x02);

    // Misc
    cfg.mappings << btn(KEY_F,   "F → Options",   9, 0x20);
    cfg.mappings << btn(KEY_G,   "G → Create",    9, 0x10);
    cfg.mappings << btn(KEY_ESC, "Esc → PS",     10, 0x01);
    cfg.mappings << btn(KEY_F1,  "F1 → L3",       9, 0x40);
    cfg.mappings << btn(KEY_F2,  "F2 → R3",       9, 0x80);

    return cfg;
}

// ── JSON serialisation helpers ─────────────────────────────────────────────────

static QString kindStr(kb::InputKind k) {
    return k == kb::InputKind::Key ? "key" : "mouse_axis";
}
static QString outputKindStr(kb::OutputKind k) {
    switch (k) {
        case kb::OutputKind::Button:    return "button";
        case kb::OutputKind::DpadDir:   return "dpad";
        case kb::OutputKind::AxisFixed: return "axis_fixed";
    }
    return "button";
}
static kb::InputKind parseInputKind(const QString& s) {
    return s == "mouse_axis" ? kb::InputKind::MouseAxis : kb::InputKind::Key;
}
static kb::OutputKind parseOutputKind(const QString& s) {
    if (s == "dpad")       return kb::OutputKind::DpadDir;
    if (s == "axis_fixed") return kb::OutputKind::AxisFixed;
    return kb::OutputKind::Button;
}

void save(const kb::Config& config)
{
    const QString path = config_path();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonArray arr;
    for (const auto& m : config.mappings) {
        QJsonObject o;
        o["enabled"]      = m.enabled;
        o["label"]        = m.label;
        o["input_kind"]   = kindStr(m.input_kind);
        o["input_code"]   = m.input_code;
        o["output_kind"]  = outputKindStr(m.output_kind);
        o["btn_byte"]     = static_cast<int>(m.btn_byte);
        o["btn_mask"]     = m.btn_mask;
        o["dpad_bit"]     = m.dpad_bit;
        o["axis_offset"]  = static_cast<int>(m.axis_offset);
        o["axis_value"]   = m.axis_value;
        arr.append(o);
    }

    QJsonObject ms;
    ms["enabled"]         = config.mouse_stick.enabled;
    ms["use_right_stick"] = config.mouse_stick.use_right_stick;
    ms["sensitivity_x"]   = static_cast<double>(config.mouse_stick.sensitivity_x);
    ms["sensitivity_y"]   = static_cast<double>(config.mouse_stick.sensitivity_y);
    ms["touchpad_key"]    = config.mouse_stick.touchpad_key;

    QJsonObject root;
    root["mappings"]    = arr;
    root["mouse_stick"] = ms;

    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(root).toJson());
}

kb::Config load()
{
    QFile f(config_path());
    if (!f.open(QIODevice::ReadOnly))
        return make_defaults();

    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return make_defaults();

    const auto root = doc.object();

    kb::Config cfg;
    for (const auto& v : root["mappings"].toArray()) {
        const QJsonObject o = v.toObject();
        kb::Mapping m;
        m.enabled     = o["enabled"].toBool(true);
        m.label       = o["label"].toString();
        m.input_kind  = parseInputKind(o["input_kind"].toString());
        m.input_code  = o["input_code"].toInt();
        m.output_kind = parseOutputKind(o["output_kind"].toString());
        m.btn_byte    = static_cast<size_t>(o["btn_byte"].toInt());
        m.btn_mask    = static_cast<uint8_t>(o["btn_mask"].toInt());
        m.dpad_bit    = static_cast<uint8_t>(o["dpad_bit"].toInt());
        m.axis_offset = static_cast<size_t>(o["axis_offset"].toInt());
        m.axis_value  = static_cast<uint8_t>(o["axis_value"].toInt());
        cfg.mappings << m;
    }

    if (cfg.mappings.isEmpty())
        return make_defaults();

    const auto ms = root["mouse_stick"].toObject();
    cfg.mouse_stick.enabled         = ms["enabled"].toBool(true);
    cfg.mouse_stick.use_right_stick = ms["use_right_stick"].toBool(true);
    cfg.mouse_stick.sensitivity_x   = static_cast<float>(ms["sensitivity_x"].toDouble(0.25));
    cfg.mouse_stick.sensitivity_y   = static_cast<float>(ms["sensitivity_y"].toDouble(0.25));
    cfg.mouse_stick.touchpad_key    = ms["touchpad_key"].toInt(0);

    return cfg;
}

// ── Key name table ─────────────────────────────────────────────────────────────

static const struct { int code; const char* name; } KEY_NAMES[] = {
    {KEY_ESC,           "Esc"},
    {KEY_1,             "1"}, {KEY_2, "2"}, {KEY_3, "3"}, {KEY_4, "4"},
    {KEY_5,             "5"}, {KEY_6, "6"}, {KEY_7, "7"}, {KEY_8, "8"},
    {KEY_9,             "9"}, {KEY_0, "0"},
    {KEY_MINUS,         "-"}, {KEY_EQUAL, "="},
    {KEY_BACKSPACE,     "Backspace"},
    {KEY_TAB,           "Tab"},
    {KEY_Q,             "Q"}, {KEY_W, "W"}, {KEY_E, "E"}, {KEY_R, "R"},
    {KEY_T,             "T"}, {KEY_Y, "Y"}, {KEY_U, "U"}, {KEY_I, "I"},
    {KEY_O,             "O"}, {KEY_P, "P"},
    {KEY_LEFTBRACE,     "["}, {KEY_RIGHTBRACE, "]"},
    {KEY_ENTER,         "Enter"},
    {KEY_LEFTCTRL,      "LCtrl"},
    {KEY_A,             "A"}, {KEY_S, "S"}, {KEY_D, "D"}, {KEY_F, "F"},
    {KEY_G,             "G"}, {KEY_H, "H"}, {KEY_J, "J"}, {KEY_K, "K"},
    {KEY_L,             "L"},
    {KEY_SEMICOLON,     ";"}, {KEY_APOSTROPHE, "'"},
    {KEY_GRAVE,         "`"},
    {KEY_LEFTSHIFT,     "LShift"},
    {KEY_BACKSLASH,     "\\"},
    {KEY_Z,             "Z"}, {KEY_X, "X"}, {KEY_C, "C"}, {KEY_V, "V"},
    {KEY_B,             "B"}, {KEY_N, "N"}, {KEY_M, "M"},
    {KEY_COMMA,         ","}, {KEY_DOT, "."}, {KEY_SLASH, "/"},
    {KEY_RIGHTSHIFT,    "RShift"},
    {KEY_KPASTERISK,    "KP *"},
    {KEY_LEFTALT,       "LAlt"},
    {KEY_SPACE,         "Space"},
    {KEY_CAPSLOCK,      "CapsLock"},
    {KEY_F1,  "F1"},  {KEY_F2,  "F2"},  {KEY_F3,  "F3"},  {KEY_F4,  "F4"},
    {KEY_F5,  "F5"},  {KEY_F6,  "F6"},  {KEY_F7,  "F7"},  {KEY_F8,  "F8"},
    {KEY_F9,  "F9"},  {KEY_F10, "F10"}, {KEY_F11, "F11"}, {KEY_F12, "F12"},
    {KEY_NUMLOCK,       "NumLock"},
    {KEY_SCROLLLOCK,    "ScrollLock"},
    {KEY_RIGHTCTRL,     "RCtrl"},
    {KEY_RIGHTALT,      "RAlt"},
    {KEY_HOME,          "Home"},
    {KEY_UP,            "Up"}, {KEY_PAGEUP, "PgUp"},
    {KEY_LEFT,          "Left"},
    {KEY_RIGHT,         "Right"},
    {KEY_END,           "End"},
    {KEY_DOWN,          "Down"}, {KEY_PAGEDOWN, "PgDn"},
    {KEY_INSERT,        "Insert"},
    {KEY_DELETE,        "Delete"},
    {KEY_LEFTMETA,      "LMeta"},
    {KEY_RIGHTMETA,     "RMeta"},
    // Mouse buttons
    {BTN_LEFT,          "Mouse Left"},
    {BTN_RIGHT,         "Mouse Right"},
    {BTN_MIDDLE,        "Mouse Middle"},
    {BTN_SIDE,          "Mouse Side"},
    {BTN_EXTRA,         "Mouse Extra"},
};

QString keyName(int code)
{
    for (const auto& e : KEY_NAMES) {
        if (e.code == code)
            return QString::fromLatin1(e.name);
    }
    return QString("KEY_%1").arg(code);
}

int keyCode(const QString& name)
{
    for (const auto& e : KEY_NAMES) {
        if (name == QString::fromLatin1(e.name))
            return e.code;
    }
    return -1;
}

} // namespace MappingStorage
