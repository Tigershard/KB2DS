# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Workflow Rules

- **Never commit or push without explicit user approval.** Always stop after making code changes and wait for the user to test before committing. Do not combine coding and committing in the same response unless the user explicitly asks for both in one request.

## Project Overview

**kb-to-ds5** is a Linux Qt6 GUI application that converts keyboard and mouse input into DualSense (PS5 controller) HID events. It creates a virtual `/dev/uhid` device and maps evdev input to DualSense button/axis values.

## Build Commands

```bash
# Debug build
mkdir build && cd build
cmake ..
cmake --build .

# Release build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .

# Install (requires sudo — writes to /usr/local/bin and /etc/udev/rules.d/)
sudo cmake --install .
```

Requirements: CMake 3.20+, C++23 compiler, Qt6 Widgets dev libraries, Linux kernel headers.

There are no automated tests or lint tools configured. Compiler warnings (`-Wall -Wextra`) are the primary static check.

## Runtime Prerequisites

The user must be in the `input` group and the udev rules must be installed:

```bash
sudo cp 99-KB2DS.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
sudo usermod -aG input $USER
# Log out and back in
```

## Architecture

### Threading Model

- **Main thread**: Qt GUI (`MainWindow`, `MappingEditorWidget`)
- **Worker thread**: `InputWorker` (QThread) — grabs evdev devices, polls events, writes HID reports

Config updates from the GUI are mutex-protected and applied to the running worker via a dirty flag (no restart needed).

### Data Flow

```
/dev/input/event* (evdev, grabbed exclusively)
        ↓
   InputWorker
   - applies key→button mappings (Mapping list)
   - accumulates mouse movement → analog stick
   - encodes 64-byte DualSense input report
        ↓
   /dev/uhid (virtual HID device seen by OS as PS5 controller)
```

### Key Data Structures (`src/mapping.hpp`)

**DS5 input report layout** (64 bytes, Report ID 0x01):
- `buf[1–2]`: LX/LY (left stick, 0–255, neutral=128)
- `buf[3–4]`: RX/RY (right stick)
- `buf[5–6]`: L2/R2 analog
- `buf[8]`: face buttons `[7:△][6:○][5:✕][4:□]` | D-pad hat nibble (`0`=N, `2`=E, `4`=S, `6`=W, `8`=none)
- `buf[9]`: `[7:R3][6:L3][5:Options][4:Create][3:R2][2:L2][1:R1][0:L1]`
- `buf[10]`: `[2:Mute][1:Touchpad][0:PS]`

**`Mapping` struct** binds one evdev input to one DS5 output:
- `InputKind`: `Key` (evdev keycode/button) or `MouseAxis` (REL_X/REL_Y)
- `OutputKind`: `Button` (bitmask on a byte), `DpadDir` (hat switch direction), or `AxisFixed` (set axis byte to a value while held)

**`Config`**: holds `QList<Mapping>` + `MouseStickConfig` (sensitivity, which stick).

### Module Responsibilities

| Module | Responsibility |
|--------|---------------|
| `src/mainwindow` | Qt GUI, system tray, start/stop, settings, coordinates everything |
| `src/inputworker` | Background thread: evdev grab → DS5 report synthesis → uhid write |
| `src/mappingeditorwidget` | Table UI for editing mappings; `KeyCaptureDialog` and `OutputPickerDialog` |
| `src/mappingstorage` | JSON load/save to `~/.config/kb-to-ds5/mappings.json`; default mappings |
| `relay-core/uhid_device.hpp` | RAII wrapper around `/dev/uhid` (UHID_CREATE/INPUT2/DESTROY) |
| `relay-core/hidraw_utils.hpp` | Scans `/dev/hidraw*` for physical DS5; reads its HID descriptor |
| `relay-core/ds5_report.hpp` | `apply_bindings()` utility; D-pad combination logic |

### HID Emulation Details

- If a physical DualSense is connected (VID=0x054C, PID=0x0CE6 or 0x0DF2), its HID report descriptor is read and reused for authenticity.
- Falls back to a hardcoded DS5 descriptor supporting Report IDs 0x01 (input), 0x09 (pairing info), 0x20 (firmware version).
- Firmware version is spoofed (`0x0100008b`) to satisfy the kernel `hid-playstation` driver's probe.
- The worker responds to `UHID_GET_REPORT` requests during device initialization.

### D-Pad Logic

Multiple simultaneous directional keys (e.g., Up+Left) accumulate a bitmask and are combined into a single compass hat value (0=N, 1=NE, 2=E … 8=none) in `ds5_report.hpp`.

### Configuration Persistence

- **JSON**: `~/.config/kb-to-ds5/mappings.json` (mappings array + mouse stick config)
- **QSettings**: `background` (bool), `mouse_enabled`, `mouse_right`, `sens_x`, `sens_y`
