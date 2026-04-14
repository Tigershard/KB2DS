# KB2DS

A Linux GUI application that lets you play PS5 Remote Play games using a keyboard and mouse by emulating a DualSense controller.

It reads your keyboard and mouse input via evdev, translates it using a fully customizable mapping table, and writes a virtual DualSense (PS5 controller) HID device to `/dev/uhid`. The OS sees it as a real PS5 controller, so PS5 Remote Play and any other application expecting a DualSense will work without any additional configuration.

## Requirements

- Linux
- CMake 3.20+
- C++23 compiler (GCC 13+ or Clang 17+)
- Qt6 Widgets development libraries
- Linux kernel headers

### Installing build dependencies

**Arch / CachyOS:**
```bash
sudo pacman -S cmake qt6-base gcc
```

**Ubuntu / Debian:**
```bash
sudo apt install cmake qt6-base-dev g++
```

**Fedora:**
```bash
sudo dnf install cmake qt6-qtbase-devel gcc-c++
```

## Building

```bash
git clone https://github.com/Tigershard/KB2DS.git
cd KB2DS
mkdir build && cd build
cmake ..
cmake --build .
```

The binary is at `build/KB2DS`.

### Optional: system install

```bash
sudo cmake --install .
```

This installs the binary to `/usr/local/bin` and the udev rules to `/etc/udev/rules.d/`.

## Setup (one-time)

You need to give your user permission to grab input devices and create virtual HID devices. If you did not use `cmake --install`, do this manually:

```bash
# Install udev rules
sudo cp 99-KB2DS.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger

# Add yourself to the input group
sudo usermod -aG input $USER
```

**Log out and back in** for the group change to take effect.

## Usage

1. Start PS5 Remote Play and connect to your console.
2. Launch `KB2DS` (or find it in your application menu if installed).
3. Select the keyboard and mouse devices to grab from the device list.
4. Click **Start**. Your keyboard and mouse are now captured and translated to DualSense input.
5. Press the **Pause** key on your keyboard to temporarily release your devices and regain normal mouse/keyboard control.

## Mappings

All mappings are fully editable in the **Mappings** tab. Any key or mouse button can be bound to any button, analog trigger, or d-pad direction.

## Mouse sensitivity

Mouse movement is mapped to the right analog stick by default. Sensitivity can be tuned independently for X and Y axes (range 0.01–10.0) in the **Settings** tab. A value around 0.5–1.0 works well for most games; increase it if the stick feels sluggish.

You can also switch mouse movement to the left stick, or bind a key to enter touchpad mode (moves the DualSense touchpad cursor instead of a stick).

## Physical DualSense detected at startup

If a physical DualSense controller (USB) is connected when KB2DS starts, it reads the controller's HID report descriptor and uses it for the virtual device instead of the built-in fallback descriptor. This improves compatibility with games or the kernel driver that inspect the descriptor closely.

The physical controller's inputs are **not** forwarded — only its descriptor is borrowed. All input to the virtual device still comes exclusively from your keyboard and mouse mappings.

## Acknowledgements

The UHID device creation and HID descriptor work in this project was informed by [dualsensectl](https://github.com/nowrep/dualsensectl) by nowrep, which served as the baseline for understanding the DualSense HID protocol on Linux.

## License

GPL-3.0. See [LICENSE](LICENSE).
