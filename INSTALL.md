# Installation Guide

## Windows

### From Release Package

1. Download `UnifiedFloppyTool-v4.0.0-windows-x64.tar.gz`
2. Extract to a folder of your choice (e.g., `C:\Program Files\UnifiedFloppyTool`)
3. Run `UnifiedFloppyTool.exe`

### Hardware Setup

For Greaseweazle and similar USB devices, drivers should install automatically.
If not, install the WinUSB driver using [Zadig](https://zadig.akeo.ie/).

---

## macOS

### From Release Package

1. Download `UnifiedFloppyTool-v4.0.0-macos14-arm64.tar.gz`
2. Extract the archive
3. Move `UnifiedFloppyTool.app` to `/Applications`
4. On first launch, right-click and select "Open" to bypass Gatekeeper

### Requirements

- macOS 14 (Sonoma) or later
- Apple Silicon (M1/M2/M3) or Intel Mac

### Hardware Setup

USB devices should work out of the box. If you encounter permission issues:

```bash
# Allow USB access (may require restart)
sudo spctl --master-disable
```

---

## Linux

### From .deb Package (Ubuntu/Debian)

```bash
sudo dpkg -i unifiedfloppytool_4.0.0_amd64.deb
sudo apt-get install -f  # Install dependencies if needed
```

### From tar.gz

```bash
tar -xzf UnifiedFloppyTool-v4.0.0-linux-x64.tar.gz
cd UnifiedFloppyTool-linux
./UnifiedFloppyTool
```

### Hardware Setup

Install udev rules for USB device access:

```bash
# Create udev rules file
sudo tee /etc/udev/rules.d/99-floppy-devices.rules << 'EOF'
# Greaseweazle
SUBSYSTEM=="usb", ATTR{idVendor}=="1209", ATTR{idProduct}=="4d69", MODE="0666", GROUP="plugdev"

# KryoFlux
SUBSYSTEM=="usb", ATTR{idVendor}=="03eb", ATTR{idProduct}=="6124", MODE="0666", GROUP="plugdev"

# SuperCard Pro
SUBSYSTEM=="usb", ATTR{idVendor}=="0483", ATTR{idProduct}=="5740", MODE="0666", GROUP="plugdev"
EOF

# Reload udev rules
sudo udevadm control --reload-rules
sudo udevadm trigger

# Add yourself to plugdev group
sudo usermod -aG plugdev $USER
```

Log out and back in for group changes to take effect.

---

## Building from Source

### Prerequisites

- Qt 6.5 or later (with SerialPort module)
- C++17 compatible compiler
- libusb-1.0 (Linux)

### Build Steps

```bash
git clone https://github.com/youruser/UnifiedFloppyTool.git
cd UnifiedFloppyTool
qmake UnifiedFloppyTool.pro CONFIG+=release
make -j$(nproc)
```

### macOS Specific

```bash
qmake UnifiedFloppyTool.pro CONFIG+=release QMAKE_MACOSX_DEPLOYMENT_TARGET=14.0
make -j$(sysctl -n hw.ncpu)
macdeployqt UnifiedFloppyTool.app
```

---

## Troubleshooting

### "Device not found"

1. Check USB cable connection
2. Verify udev rules (Linux) or drivers (Windows)
3. Try a different USB port
4. Check device manager/lsusb for device detection

### "Permission denied"

- Linux: Ensure udev rules are installed and you're in the `plugdev` group
- macOS: Grant USB access in System Preferences > Security & Privacy
- Windows: Run as Administrator or fix driver installation

### Qt Runtime Errors

Ensure Qt 6.5+ runtime libraries are installed or bundled with the application.

---

## Support

- GitHub Issues: [Report a bug](https://github.com/youruser/UnifiedFloppyTool/issues)
- Documentation: See `docs/` folder
- Wiki: Check the GitHub Wiki for detailed guides
