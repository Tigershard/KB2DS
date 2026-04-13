#pragma once

#include "mapping.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <QThread>

namespace relay { class UhidDevice; }

class InputWorker : public QThread {
    Q_OBJECT
public:
    struct InputDeviceInfo {
        QString path;   // e.g. "/dev/input/event5"
        QString name;   // e.g. "Logitech USB Keyboard"
    };

    explicit InputWorker(QObject* parent = nullptr);
    ~InputWorker() override;

    void stop();
    void toggle_pause();                        // thread-safe; toggles pause/resume while running
    void update_config(kb::Config config);      // thread-safe, can be called while running
    void set_selected_devices(const QStringList& paths);  // call before start()

    static QList<InputDeviceInfo> enumerate_devices();

signals:
    void started_ok();
    void pause_changed(bool paused);  // emitted when Pause key or toggle_pause() toggles state
    void error(const QString& message);
    void stats(quint64 reports_sent);
    void log_message(const QString& msg);

protected:
    void run() override;

private:
    // ── Config hot-swap (mutex-protected) ────────────────────────────────────
    kb::Config              config_;
    kb::Config              pending_config_;
    std::mutex              config_mutex_;
    std::atomic<bool>       config_dirty_{false};

    // ── State ─────────────────────────────────────────────────────────────────
    std::atomic<bool>       running_{false};
    std::atomic<bool>       pause_requested_{false};
    bool                    paused_{false};  // only written inside run()
    std::unique_ptr<relay::UhidDevice> virt_dev_;

    // ── Grabbed evdev nodes ───────────────────────────────────────────────────
    struct GrabbedNode { int fd = -1; std::string path; };
    std::vector<GrabbedNode> grabbed_nodes_;
    QStringList              selected_devices_;

    // ── Input state ───────────────────────────────────────────────────────────
    std::unordered_map<int, bool> key_state_;   // evdev code → pressed
    float mouse_acc_x_ = 0.0f;
    float mouse_acc_y_ = 0.0f;

    // Touchpad finger tracking
    float touchpad_pos_x_      = 960.0f;
    float touchpad_pos_y_      = 540.0f;
    bool  touchpad_finger_down_ = false;

    // ── Helpers ───────────────────────────────────────────────────────────────
    bool open_and_grab_inputs();
    void release_inputs();
    bool build_uhid_device();
    void process_evdev_events(int fd);
    void synthesize_and_send();

    // DS5 report descriptor (read from physical device or hardcoded fallback)
    static std::vector<uint8_t> get_report_descriptor();
};
