#include "inputworker.hpp"
#include "hidraw_utils.hpp"
#include "uhid_device.hpp"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace fs = std::filesystem;

// ── VID:PID ────────────────────────────────────────────────────────────────────
static constexpr uint16_t DS5_VID  = 0x054C;
static constexpr uint16_t DS5_PID  = 0x0CE6;

// ── Firmware constants (hardcoded to plausible DS5 values) ─────────────────────
static constexpr uint8_t  DS_REPORT_FIRMWARE     = 0x20;
static constexpr uint8_t  DS_REPORT_PAIRING      = 0x09;
static constexpr uint8_t  DS_REPORT_CALIBRATION  = 0x05;
static constexpr uint32_t DS5_HW_VER             = 0x01000216;
static constexpr uint32_t DS5_FW_VER             = 0x0100008b;

// Calibration report (41 bytes) — values from a real DS5 device.
// hid-playstation probe fails with -EINVAL if gyro_speed_{plus,minus} == 0,
// so we must supply non-zero values here.
//
// Byte layout (all int16 LE, buf[0] = report ID):
//   [1:2]  gyro pitch bias      [3:4]  gyro yaw bias     [5:6]  gyro roll bias
//   [7:8]  gyro pitch plus      [9:10] gyro pitch minus
//   [11:12] gyro yaw plus       [13:14] gyro yaw minus
//   [15:16] gyro roll plus      [17:18] gyro roll minus
//   [19:20] gyro speed plus     [21:22] gyro speed minus   ← must be non-zero
//   [23:24] acc x plus          [25:26] acc x minus
//   [27:28] acc y plus          [29:30] acc y minus
//   [31:32] acc z plus          [33:34] acc z minus
static std::vector<uint8_t> make_calibration_report()
{
    std::vector<uint8_t> buf(41, 0x00);
    buf[0] = DS_REPORT_CALIBRATION;

    // Gyro bias: 0 for all axes
    // buf[1:6] already 0x00

    // Gyro range: ±8191 (0x1FFF / 0xE001)
    for (int base : {7, 11, 15}) {   // pitch, yaw, roll plus
        buf[base]     = 0xFF; buf[base + 1] = 0x1F;  // +8191 LE
        buf[base + 2] = 0x01; buf[base + 3] = 0xE0;  // -8191 LE
    }

    // Gyro speed: 1172 (0x0494) — MUST be non-zero or hid-playstation aborts probe
    buf[19] = 0x94; buf[20] = 0x04;   // speed plus
    buf[21] = 0x94; buf[22] = 0x04;   // speed minus

    // Accelerometer range: ±8192 (0x2000 / 0xE000)
    for (int base : {23, 27, 31}) {   // x, y, z plus
        buf[base]     = 0x00; buf[base + 1] = 0x20;  // +8192 LE
        buf[base + 2] = 0x00; buf[base + 3] = 0xE0;  // -8192 LE
    }

    return buf;
}

static std::vector<uint8_t> make_firmware_report()
{
    std::vector<uint8_t> buf(64, 0x00);
    buf[0]  = DS_REPORT_FIRMWARE;
    buf[24] = (DS5_HW_VER >>  0) & 0xFF;
    buf[25] = (DS5_HW_VER >>  8) & 0xFF;
    buf[26] = (DS5_HW_VER >> 16) & 0xFF;
    buf[27] = (DS5_HW_VER >> 24) & 0xFF;
    buf[28] = (DS5_FW_VER >>  0) & 0xFF;
    buf[29] = (DS5_FW_VER >>  8) & 0xFF;
    buf[30] = (DS5_FW_VER >> 16) & 0xFF;
    buf[31] = (DS5_FW_VER >> 24) & 0xFF;
    return buf;
}

// ── Hardcoded DualSense report descriptor (fallback) ──────────────────────────
// Matches the byte layout defined in ds5_report.hpp:
//   buf[0]  = Report ID (0x01)
//   buf[1]  = LX, buf[2] = LY, buf[3] = RX, buf[4] = RY
//   buf[5]  = L2 analog, buf[6] = R2 analog
//   buf[7]  = vendor byte
//   buf[8]  = [7:△][6:○][5:✕][4:□] | [3:0] D-pad hat
//   buf[9]  = [7:R3][6:L3][5:Options][4:Create][3:R2][2:L2][1:R1][0:L1]
//   buf[10] = [2:Mute][1:Touchpad][0:PS]
//   buf[11..63] = vendor bytes
static const std::vector<uint8_t> DS5_FALLBACK_RDESC = {
    0x05, 0x01,             // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05,             // Usage (Game Pad)
    0xA1, 0x01,             // Collection (Application)
      // ── Report ID 0x01: Input ──────────────────────────────
      0x85, 0x01,           //   Report ID (1)
      // 6 axes: LX LY RX RY L2 R2  (bytes 1-6)
      0x09, 0x30, 0x09, 0x31, 0x09, 0x32,
      0x09, 0x35, 0x09, 0x33, 0x09, 0x34,
      0x15, 0x00,           //   Logical Minimum (0)
      0x26, 0xFF, 0x00,     //   Logical Maximum (255)
      0x75, 0x08,           //   Report Size (8 bits)
      0x95, 0x06,           //   Report Count (6)
      0x81, 0x02,           //   Input (Data,Var,Abs)
      // Vendor byte (byte 7)
      0x06, 0x00, 0xFF,     //   Usage Page (Vendor 0xFF00)
      0x09, 0x20,           //   Usage (0x20)
      0x75, 0x08,           //   Report Size (8)
      0x95, 0x01,           //   Report Count (1)
      0x81, 0x02,           //   Input (Data,Var,Abs)
      // D-pad hat (byte 8, lower nibble = bits 0-3)
      0x05, 0x01,           //   Usage Page (Generic Desktop)
      0x09, 0x39,           //   Usage (Hat switch)
      0x15, 0x00,           //   Logical Minimum (0)
      0x25, 0x07,           //   Logical Maximum (7)
      0x35, 0x00,           //   Physical Minimum (0)
      0x46, 0x3B, 0x01,     //   Physical Maximum (315)
      0x65, 0x14,           //   Unit (English Rotation)
      0x75, 0x04,           //   Report Size (4 bits)
      0x95, 0x01,           //   Report Count (1)
      0x81, 0x42,           //   Input (Data,Var,Abs,Null)
      0x65, 0x00,           //   Unit (None)
      // Buttons 1-4: Square Cross Circle Triangle (byte 8, upper nibble = bits 4-7)
      0x05, 0x09,           //   Usage Page (Button)
      0x19, 0x01,           //   Usage Minimum (Button 1)
      0x29, 0x04,           //   Usage Maximum (Button 4)
      0x15, 0x00,           //   Logical Minimum (0)
      0x25, 0x01,           //   Logical Maximum (1)
      0x75, 0x01,           //   Report Size (1 bit)
      0x95, 0x04,           //   Report Count (4)
      0x81, 0x02,           //   Input (Data,Var,Abs)
      // Buttons 5-12: L1 R1 L2 R2 Create Options L3 R3 (byte 9, bits 0-7)
      0x19, 0x05,           //   Usage Minimum (Button 5)
      0x29, 0x0C,           //   Usage Maximum (Button 12)
      0x75, 0x01,           //   Report Size (1 bit)
      0x95, 0x08,           //   Report Count (8)
      0x81, 0x02,           //   Input (Data,Var,Abs)
      // Buttons 13-14: PS Touchpad (byte 10, bits 0-1)
      0x19, 0x0D,           //   Usage Minimum (Button 13)
      0x29, 0x0E,           //   Usage Maximum (Button 14)
      0x95, 0x02,           //   Report Count (2)
      0x81, 0x02,           //   Input (Data,Var,Abs)
      // Vendor bits (byte 10, bits 2-7): Mute + reserved
      0x06, 0x00, 0xFF,     //   Usage Page (Vendor 0xFF00)
      0x09, 0x21,           //   Usage (0x21)
      0x75, 0x01,           //   Report Size (1 bit)
      0x95, 0x06,           //   Report Count (6)
      0x81, 0x02,           //   Input (Data,Var,Abs)
      // Remaining vendor bytes (bytes 11-63 = 53 bytes)
      0x09, 0x22,           //   Usage (0x22)
      0x75, 0x08,           //   Report Size (8 bits)
      0x95, 0x35,           //   Report Count (53)
      0x81, 0x02,           //   Input (Data,Var,Abs)
      // ── Feature Report 0x05: Calibration (gyro + accel) ───
      0x85, 0x05,           //   Report ID (5)
      0x09, 0x25,           //   Usage (vendor 0x25)
      0x75, 0x08,           //   Report Size (8 bits)
      0x95, 0x28,           //   Report Count (40 bytes)
      0xB1, 0x02,           //   Feature (Data,Var,Abs)
      // ── Feature Report 0x09: Pairing info ──────────────────
      0x85, 0x09,           //   Report ID (9)
      0x09, 0x23,           //   Usage (0x23)
      0x75, 0x08,           //   Report Size (8)
      0x95, 0x3F,           //   Report Count (63)
      0xB1, 0x02,           //   Feature (Data,Var,Abs)
      // ── Feature Report 0x20: Firmware version ──────────────
      0x85, 0x20,           //   Report ID (0x20)
      0x09, 0x24,           //   Usage (0x24)
      0x75, 0x08,           //   Report Size (8)
      0x95, 0x3F,           //   Report Count (63)
      0xB1, 0x02,           //   Feature (Data,Var,Abs)
    0xC0                    // End Collection
};

// ── Report descriptor: try physical device first, then fall back ───────────────

std::vector<uint8_t> InputWorker::get_report_descriptor()
{
    auto info = relay::find_hidraw_device(DS5_VID, DS5_PID);
    if (info) {
        fprintf(stderr, "[KB2DS] Found physical DualSense at %s\n",
                info->path.c_str());
        const int fd = ::open(info->path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd >= 0) {
            relay::Fd guard(fd);
            try {
                auto rd = relay::read_report_descriptor(fd);
                if (!rd.empty()) {
                    fprintf(stderr, "[KB2DS] Using descriptor from %s (%zu bytes)\n",
                            info->path.c_str(), rd.size());
                    return rd;
                }
                fprintf(stderr, "[KB2DS] Descriptor from %s was empty — using built-in\n",
                        info->path.c_str());
            } catch (const std::exception& e) {
                fprintf(stderr, "[KB2DS] Failed to read descriptor from %s: %s — using built-in\n",
                        info->path.c_str(), e.what());
            }
        } else {
            fprintf(stderr, "[KB2DS] Cannot open %s: %s — using built-in\n",
                    info->path.c_str(), strerror(errno));
        }
    } else {
        fprintf(stderr, "[KB2DS] No physical DualSense found — using built-in descriptor\n");
    }
    fprintf(stderr, "[KB2DS] Built-in descriptor size: %zu bytes\n",
            DS5_FALLBACK_RDESC.size());
    return DS5_FALLBACK_RDESC;
}

// ── Node classification: is this a keyboard or mouse? ─────────────────────────

static bool is_keyboard_or_mouse(int fd)
{
    // Must support EV_KEY
    unsigned long ev_bits[(EV_MAX / (sizeof(unsigned long) * 8)) + 1] = {};
    if (::ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) < 0) return false;
    if (!(ev_bits[EV_KEY / (sizeof(unsigned long) * 8)] &
          (1UL << (EV_KEY % (sizeof(unsigned long) * 8))))) return false;

    // Must NOT have absolute axes (gamepads/tablets have ABS_X; skip them)
    unsigned long abs_bits[(ABS_MAX / (sizeof(unsigned long) * 8)) + 1] = {};
    if (::ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)), abs_bits) < 0) return true;
    const bool has_abs_x = abs_bits[ABS_X / (sizeof(unsigned long) * 8)] &
                           (1UL << (ABS_X % (sizeof(unsigned long) * 8)));
    return !has_abs_x;
}

// ── InputWorker ───────────────────────────────────────────────────────────────

InputWorker::InputWorker(QObject* parent) : QThread(parent) {}

InputWorker::~InputWorker() {
    stop();
    wait();
}

void InputWorker::stop() {
    running_ = false;
}

void InputWorker::toggle_pause() {
    pause_requested_.store(true, std::memory_order_release);
}

void InputWorker::set_selected_devices(const QStringList& paths)
{
    selected_devices_ = paths;
}

QList<InputWorker::InputDeviceInfo> InputWorker::enumerate_devices()
{
    QList<InputDeviceInfo> result;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator("/sys/class/input", ec)) {
        const auto node = entry.path().filename().string();
        if (node.compare(0, 5, "event") != 0) continue;

        const std::string dev = "/dev/input/" + node;
        const int fd = ::open(dev.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;

        if (!is_keyboard_or_mouse(fd)) { ::close(fd); continue; }

        char dname[256] = {};
        ::ioctl(fd, EVIOCGNAME(sizeof(dname)), dname);
        ::close(fd);

        result.push_back({QString::fromStdString(dev), QString::fromUtf8(dname)});
    }
    return result;
}

void InputWorker::update_config(kb::Config config)
{
    std::lock_guard lk(config_mutex_);
    pending_config_ = std::move(config);
    config_dirty_.store(true, std::memory_order_release);
}

bool InputWorker::open_and_grab_inputs()
{
    grabbed_nodes_.clear();

    for (const QString& qpath : selected_devices_) {
        const std::string path = qpath.toStdString();
        const auto slash = path.rfind('/');
        const std::string node = (slash != std::string::npos) ? path.substr(slash + 1) : path;

        const int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            fprintf(stderr, "[KB2DS] Cannot open %s: %s\n", path.c_str(), strerror(errno));
            continue;
        }

        char dname[256] = {};
        ::ioctl(fd, EVIOCGNAME(sizeof(dname)), dname);

        if (::ioctl(fd, EVIOCGRAB, 1) < 0) {
            fprintf(stderr, "[KB2DS] EVIOCGRAB failed for %s (%s): %s\n",
                    node.c_str(), dname, strerror(errno));
            ::close(fd);
            continue;
        }

        fprintf(stderr, "[KB2DS] Grabbed %s — %s\n", node.c_str(), dname);
        grabbed_nodes_.push_back({fd, path});
    }
    return !grabbed_nodes_.empty();
}

void InputWorker::release_inputs()
{
    for (auto& n : grabbed_nodes_) {
        if (n.fd >= 0) {
            ::ioctl(n.fd, EVIOCGRAB, 0);
            ::close(n.fd);
            fprintf(stderr, "[KB2DS] Released %s\n", n.path.c_str());
        }
    }
    grabbed_nodes_.clear();
}

bool InputWorker::build_uhid_device()
{
    relay::UhidDevice::Config cfg;
    cfg.name              = "DualSense Wireless Controller";
    cfg.vendor            = DS5_VID;
    cfg.product           = DS5_PID;
    cfg.report_descriptor = get_report_descriptor();
    fprintf(stderr, "[KB2DS] Sending UHID_CREATE2 with descriptor: %zu bytes\n",
            cfg.report_descriptor.size());
    if (cfg.report_descriptor.empty()) {
        emit error("Report descriptor is empty — cannot create virtual DualSense.");
        return false;
    }
    cfg.on_get_report = [](uint8_t report_id) -> std::vector<uint8_t> {
        fprintf(stderr, "[KB2DS] GET_REPORT 0x%02X requested\n", report_id);
        if (report_id == DS_REPORT_CALIBRATION) {
            fprintf(stderr, "[KB2DS] → responding with calibration (41 bytes)\n");
            return make_calibration_report();
        }
        if (report_id == DS_REPORT_FIRMWARE) {
            fprintf(stderr, "[KB2DS] → responding with firmware info (64 bytes)\n");
            return make_firmware_report();
        }
        if (report_id == DS_REPORT_PAIRING) {
            fprintf(stderr, "[KB2DS] → responding with pairing info\n");
            std::vector<uint8_t> buf(20, 0x00);
            buf[0] = DS_REPORT_PAIRING;
            buf[6] = 0x01;  // MAC: 00:00:00:00:00:01
            return buf;
        }
        fprintf(stderr, "[KB2DS] → unknown report 0x%02X — returning EIO\n", report_id);
        return {};
    };

    try {
        virt_dev_ = std::make_unique<relay::UhidDevice>(std::move(cfg));
    } catch (const std::exception& e) {
        emit error(QString("Failed to create virtual DualSense: %1").arg(e.what()));
        return false;
    }
    return true;
}

void InputWorker::process_evdev_events(int fd)
{
    input_event ev;
    while (::read(fd, &ev, sizeof(ev)) == sizeof(ev)) {
        if (ev.type == EV_KEY) {
            key_state_[ev.code] = (ev.value != 0);  // 0=up, 1=down, 2=repeat
            if (ev.value != 2 && !paused_) {  // ignore repeats and paused state
                QSet<int> active;
                for (const auto& [code, pressed] : key_state_)
                    if (pressed) active.insert(code);
                emit keys_changed(active);
            }
        } else if (ev.type == EV_REL) {
            if (ev.code == REL_X)      mouse_acc_x_ += static_cast<float>(ev.value);
            else if (ev.code == REL_Y) mouse_acc_y_ += static_cast<float>(ev.value);
        }
    }
}

void InputWorker::synthesize_and_send()
{
    // Build a fresh neutral 64-byte DS5 input report
    uint8_t buf[64] = {};
    buf[0] = 0x01;    // Report ID
    buf[1] = 128;     // LX neutral
    buf[2] = 128;     // LY neutral
    buf[3] = 128;     // RX neutral
    buf[4] = 128;     // RY neutral
    buf[5] = 0;       // L2 released
    buf[6] = 0;       // R2 released
    buf[7] = 0;       // vendor byte
    buf[8] = 0x08;    // D-pad hat = none (8), face buttons = 0

    // D-pad accumulator: bit0=Up, bit1=Right, bit2=Down, bit3=Left
    uint8_t dpad_flags = 0;
    bool    has_dpad   = false;

    // Axis accumulator: track signed deltas from WASD-style mappings
    // Multiple keys on the same axis → last writer wins (simple but works well)

    for (const auto& m : config_.mappings) {
        if (!m.enabled) continue;
        if (m.input_kind != kb::InputKind::Key) continue;

        auto it = key_state_.find(m.input_code);
        if (it == key_state_.end() || !it->second) continue;

        switch (m.output_kind) {
            case kb::OutputKind::Button:
                if (m.btn_byte < 64)
                    buf[m.btn_byte] |= m.btn_mask;
                break;

            case kb::OutputKind::DpadDir:
                dpad_flags |= m.dpad_bit;
                has_dpad    = true;
                break;

            case kb::OutputKind::AxisFixed:
                if (m.axis_offset > 0 && m.axis_offset < 64)
                    buf[m.axis_offset] = m.axis_value;
                break;
        }
    }

    // Apply accumulated D-pad result
    if (has_dpad) {
        static constexpr uint8_t DPAD_COMBINE[16] = {
            8, 0, 2, 1, 4, 8, 3, 2, 6, 7, 8, 0, 5, 6, 4, 8
        };
        buf[8] = static_cast<uint8_t>((buf[8] & 0xF0u) | DPAD_COMBINE[dpad_flags & 0x0F]);
    }

    // Always mark both touchpad contacts inactive by default.
    // DS5 touch point layout (4 bytes each, starting at buf[33]):
    //   byte 0: bit7=0 active / bit7=1 inactive, bits6:0 = contact_id
    //   byte 1: x[7:0]
    //   byte 2: x[11:8] in bits[3:0], y[3:0] in bits[7:4]
    //   byte 3: y[11:4]
    buf[33] = 0x80;  // contact 0: inactive
    buf[37] = 0x81;  // contact 1: inactive

    // Check if touchpad mode key is held
    const int tp_key = config_.mouse_stick.touchpad_key;
    const bool tp_key_held = tp_key != 0 &&
        key_state_.count(tp_key) && key_state_.at(tp_key);

    if (tp_key_held) {
        // On first frame, start finger at touchpad centre
        if (!touchpad_finger_down_) {
            touchpad_pos_x_ = 960.0f;
            touchpad_pos_y_ = 540.0f;
            touchpad_finger_down_ = true;
        }
        // Mouse deltas → absolute touchpad position (sensitivity 4× the stick scale)
        constexpr float TP_SCALE = 4.0f;
        touchpad_pos_x_ = std::clamp(touchpad_pos_x_ + mouse_acc_x_ * TP_SCALE, 0.0f, 1919.0f);
        touchpad_pos_y_ = std::clamp(touchpad_pos_y_ + mouse_acc_y_ * TP_SCALE, 0.0f, 1079.0f);
        mouse_acc_x_ = 0.0f;
        mouse_acc_y_ = 0.0f;

        const int x = static_cast<int>(touchpad_pos_x_);
        const int y = static_cast<int>(touchpad_pos_y_);
        buf[33] = 0x00;  // contact 0: active, contact_id=0
        buf[34] = static_cast<uint8_t>(x & 0xFF);
        buf[35] = static_cast<uint8_t>(((x >> 8) & 0x0F) | ((y & 0x0F) << 4));
        buf[36] = static_cast<uint8_t>((y >> 4) & 0xFF);
    } else {
        if (touchpad_finger_down_) {
            touchpad_finger_down_ = false;
            // buf[33] already 0x80 (inactive) — finger lifted
        }

        // Mouse → stick mapping (only when not in touchpad mode)
        if (config_.mouse_stick.enabled &&
            (mouse_acc_x_ != 0.0f || mouse_acc_y_ != 0.0f))
        {
            const size_t ox = config_.mouse_stick.use_right_stick ? 3 : 1;
            const size_t oy = config_.mouse_stick.use_right_stick ? 4 : 2;

            const float raw_x = 128.0f + mouse_acc_x_ * config_.mouse_stick.sensitivity_x;
            const float raw_y = 128.0f + mouse_acc_y_ * config_.mouse_stick.sensitivity_y;

            buf[ox] = static_cast<uint8_t>(std::clamp(raw_x, 0.0f, 255.0f));
            buf[oy] = static_cast<uint8_t>(std::clamp(raw_y, 0.0f, 255.0f));

            // Exponential decay so stick returns to neutral after movement stops
            constexpr float DECAY = 0.8f;
            mouse_acc_x_ *= DECAY;
            mouse_acc_y_ *= DECAY;
            if (std::abs(mouse_acc_x_) < 0.5f) mouse_acc_x_ = 0.0f;
            if (std::abs(mouse_acc_y_) < 0.5f) mouse_acc_y_ = 0.0f;
        }
    }

    virt_dev_->send_input_report(buf, sizeof(buf));
}

void InputWorker::run()
{
    running_ = true;
    paused_  = false;
    pause_requested_.store(false, std::memory_order_relaxed);
    key_state_.clear();
    mouse_acc_x_ = 0.0f;
    mouse_acc_y_ = 0.0f;
    touchpad_pos_x_ = 960.0f;
    touchpad_pos_y_ = 540.0f;
    touchpad_finger_down_ = false;

    // ── 1. Grab keyboard / mouse evdev nodes ──────────────────────────────────
    if (!open_and_grab_inputs()) {
        emit error("No keyboard or mouse evdev nodes could be grabbed.\n\n"
                   "Make sure the user is in the 'input' group:\n"
                   "  sudo usermod -aG input $USER\n"
                   "and install the udev rule:\n"
                   "  sudo cp 99-KB2DS.rules /etc/udev/rules.d/\n"
                   "  sudo udevadm control --reload-rules");
        running_ = false;
        return;
    }

    // ── 2. Create virtual DualSense ───────────────────────────────────────────
    if (!build_uhid_device()) {
        release_inputs();
        running_ = false;
        return;
    }

    emit started_ok();
    emit log_message(QString("Grabbed %1 input device(s). Virtual DualSense active.")
                     .arg(static_cast<int>(grabbed_nodes_.size())));

    // ── 3. Build poll fd array (evdev nodes first, uhid last; never rebuilt) ────
    std::vector<pollfd> pfds;
    pfds.reserve(grabbed_nodes_.size() + 1);
    for (const auto& n : grabbed_nodes_)
        pfds.push_back({n.fd, POLLIN, 0});
    pfds.push_back({virt_dev_->fd(), POLLIN, 0});

    constexpr int REPORT_INTERVAL_MS = 8;  // ~125 Hz
    quint64 reports = 0;

    // ── 4. Main loop ──────────────────────────────────────────────────────────
    while (running_) {
        // Hot-swap config from UI thread
        if (config_dirty_.load(std::memory_order_acquire)) {
            std::lock_guard lk(config_mutex_);
            config_ = std::move(pending_config_);
            config_dirty_.store(false, std::memory_order_relaxed);
        }

        // Handle pause / resume toggle (from Pause key or GUI button)
        if (pause_requested_.exchange(false, std::memory_order_acq_rel)) {
            if (!paused_) {
                // Release exclusive grab but keep fds open so Pause key is still readable
                for (auto& n : grabbed_nodes_)
                    ::ioctl(n.fd, EVIOCGRAB, 0);
                key_state_.clear();
                mouse_acc_x_ = 0.0f;
                mouse_acc_y_ = 0.0f;
                paused_ = true;
                emit keys_changed({});
                emit pause_changed(true);
                emit log_message("Paused — keyboard/mouse released");
            } else {
                // Re-acquire exclusive grab
                for (auto& n : grabbed_nodes_)
                    ::ioctl(n.fd, EVIOCGRAB, 1);
                paused_ = false;
                emit pause_changed(false);
                emit log_message(QString("Resumed — grabbed %1 device(s).")
                                 .arg(static_cast<int>(grabbed_nodes_.size())));
            }
        }

        for (auto& pfd : pfds) pfd.revents = 0;
        const int ret = ::poll(pfds.data(), static_cast<nfds_t>(pfds.size()),
                               REPORT_INTERVAL_MS);
        if (ret < 0) {
            if (errno == EINTR) continue;
            fprintf(stderr, "[KB2DS] poll error: %s\n", strerror(errno));
            break;
        }

        // Always drain evdev buffers (grab released or not) so the Pause key
        // is detectable in both states and the kernel queue never overflows
        for (int i = 0; i < static_cast<int>(pfds.size()) - 1; ++i) {
            if (pfds[i].revents & POLLIN)
                process_evdev_events(pfds[i].fd);
        }

        // Pause key toggles pause regardless of current state
        {
            auto it = key_state_.find(KEY_PAUSE);
            if (it != key_state_.end() && it->second) {
                it->second = false;  // consume to prevent re-triggering on hold
                pause_requested_.store(true, std::memory_order_release);
            }
        }

        // When paused, discard all input so a neutral report is sent
        if (paused_) {
            key_state_.clear();
            mouse_acc_x_ = 0.0f;
            mouse_acc_y_ = 0.0f;
        }

        // Process uhid events (always — even when paused, to serve GET_REPORT)
        if (pfds.back().revents & POLLIN)
            virt_dev_->process_event();

        synthesize_and_send();
        ++reports;
        if (reports % 1000 == 0)
            emit stats(reports);
    }

    // ── 5. Cleanup ────────────────────────────────────────────────────────────
    emit keys_changed({});
    release_inputs();
    virt_dev_.reset();
    running_ = false;
}
