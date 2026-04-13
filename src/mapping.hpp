#pragma once

#include <cstdint>
#include <cstddef>
#include <QString>
#include <QList>

namespace kb {

// ── Input source ──────────────────────────────────────────────────────────────

enum class InputKind : uint8_t {
    Key,        // evdev EV_KEY event: keyboard key or mouse button
    MouseAxis,  // evdev EV_REL: REL_X or REL_Y (for global mouse→stick mapping)
};

// ── Output target ─────────────────────────────────────────────────────────────

enum class OutputKind : uint8_t {
    Button,    // set a bitmask bit in a DS5 report byte
    DpadDir,   // contribute a direction to the D-pad hat accumulator
    AxisFixed, // set an analog axis byte to a fixed value while key is held
};

// DS5 USB input report layout (report ID 0x01):
//   buf[1] = LX  (0=left, 128=center, 255=right)
//   buf[2] = LY  (0=up,   128=center, 255=down)
//   buf[3] = RX
//   buf[4] = RY
//   buf[5] = L2 analog (0=released, 255=full press)
//   buf[6] = R2 analog
//   buf[7] = vendor byte
//   buf[8] = [7:△][6:○][5:✕][4:□][3:0] hat (0=N,2=E,4=S,6=W,8=none)
//   buf[9] = [7:R3][6:L3][5:Options][4:Create][3:R2][2:L2][1:R1][0:L1]
//   buf[10]= [2:Mute][1:Touchpad][0:PS]  (bits 3-7 ignored for standard DS5)

struct Mapping {
    bool    enabled = true;
    QString label;

    // ── Input ────────────────────────────────────────────────────────────────
    InputKind input_kind = InputKind::Key;
    int       input_code = 0;   // evdev KEY_* code (see linux/input-event-codes.h)

    // ── Output ───────────────────────────────────────────────────────────────
    OutputKind output_kind = OutputKind::Button;

    // For OutputKind::Button:
    size_t  btn_byte = 0;   // byte offset in report buffer (0 = report ID)
    uint8_t btn_mask = 0;   // bitmask to OR into that byte

    // For OutputKind::DpadDir:
    //   dpad_bit: 0x01=Up, 0x02=Right, 0x04=Down, 0x08=Left
    uint8_t dpad_bit = 0;

    // For OutputKind::AxisFixed:
    size_t  axis_offset = 0;    // byte offset in report (1=LX, 2=LY, 3=RX, 4=RY, 5=L2, 6=R2)
    uint8_t axis_value  = 0;    // value to write while key is held (0–255; sticks neutral=128)
};

// ── Global mouse-to-right-stick config ───────────────────────────────────────

struct MouseStickConfig {
    bool  enabled         = true;
    bool  use_right_stick = true;   // false = left stick
    float sensitivity_x   = 0.25f;
    float sensitivity_y   = 0.25f;
    int   touchpad_key    = 0;      // evdev keycode; 0 = disabled
};

// ── Full config ───────────────────────────────────────────────────────────────

struct Config {
    QList<Mapping>   mappings;
    MouseStickConfig mouse_stick;
};

// ── Human-readable names for DS5 outputs (used by UI) ─────────────────────────

struct OutputInfo {
    const char* name;
    OutputKind  kind;
    size_t      btn_byte;
    uint8_t     btn_mask;
    uint8_t     dpad_bit;
    size_t      axis_offset;
    uint8_t     axis_value;
};

inline constexpr OutputInfo DS5_OUTPUTS[] = {
    // Face buttons
    {"Square",         OutputKind::Button,    8, 0x10, 0, 0, 0},
    {"Cross",          OutputKind::Button,    8, 0x20, 0, 0, 0},
    {"Circle",         OutputKind::Button,    8, 0x40, 0, 0, 0},
    {"Triangle",       OutputKind::Button,    8, 0x80, 0, 0, 0},
    // Shoulder / trigger digital
    {"L1",             OutputKind::Button,    9, 0x01, 0, 0, 0},
    {"R1",             OutputKind::Button,    9, 0x02, 0, 0, 0},
    {"L2 (digital)",   OutputKind::Button,    9, 0x04, 0, 0, 0},
    {"R2 (digital)",   OutputKind::Button,    9, 0x08, 0, 0, 0},
    // Trigger analog (full press)
    {"L2 (analog)",    OutputKind::AxisFixed, 0, 0,    0, 5, 255},
    {"R2 (analog)",    OutputKind::AxisFixed, 0, 0,    0, 6, 255},
    // Misc buttons
    {"Create",         OutputKind::Button,    9, 0x10, 0, 0, 0},
    {"Options",        OutputKind::Button,    9, 0x20, 0, 0, 0},
    {"L3",             OutputKind::Button,    9, 0x40, 0, 0, 0},
    {"R3",             OutputKind::Button,    9, 0x80, 0, 0, 0},
    {"PS",             OutputKind::Button,   10, 0x01, 0, 0, 0},
    {"Touchpad",       OutputKind::Button,   10, 0x02, 0, 0, 0},
    {"Mute",           OutputKind::Button,   10, 0x04, 0, 0, 0},
    // D-pad
    {"DPad Up",        OutputKind::DpadDir,   0, 0, 0x01, 0, 0},
    {"DPad Right",     OutputKind::DpadDir,   0, 0, 0x02, 0, 0},
    {"DPad Down",      OutputKind::DpadDir,   0, 0, 0x04, 0, 0},
    {"DPad Left",      OutputKind::DpadDir,   0, 0, 0x08, 0, 0},
    // Left stick axes
    {"Left Stick Left",  OutputKind::AxisFixed, 0, 0, 0, 1,   0},
    {"Left Stick Right", OutputKind::AxisFixed, 0, 0, 0, 1, 255},
    {"Left Stick Up",    OutputKind::AxisFixed, 0, 0, 0, 2,   0},
    {"Left Stick Down",  OutputKind::AxisFixed, 0, 0, 0, 2, 255},
    // Right stick axes
    {"Right Stick Left",  OutputKind::AxisFixed, 0, 0, 0, 3,   0},
    {"Right Stick Right", OutputKind::AxisFixed, 0, 0, 0, 3, 255},
    {"Right Stick Up",    OutputKind::AxisFixed, 0, 0, 0, 4,   0},
    {"Right Stick Down",  OutputKind::AxisFixed, 0, 0, 0, 4, 255},
};
inline constexpr int DS5_OUTPUT_COUNT = static_cast<int>(sizeof(DS5_OUTPUTS) / sizeof(DS5_OUTPUTS[0]));

} // namespace kb
