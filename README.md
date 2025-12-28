# yawc (Yet Another Wayland Compositor)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

`yawc` is a **modular Wayland compositor** built on **wlroots** (0.20+). It was primarily designed for personal usage, with focus on building custom Window Managers without handling low-level Wayland boilerplate.

## Key features / quirks

- **Modular architecture**: Window management logic (stacking, tiling, etc.) is loaded as a plugin.
- **wlroots-based**: Built on the latest `wlroots` (0.20+).
- **Tearing enabled**: Enabled by default for low latency.
- **Global shortcuts**: Handled via `xdg-desktop-portal-hyprland`.
  - List active shortcuts: `yawc-shortcuts`
  - Reload configuration: `yawc-reload`
- **HiDPI X11**: Uses `xwayland-satellite` to prevent blurry XWayland apps.
- **Configuration**: Simple TOML configuration with hot-reload support.
- **Input handling**: Centralized input configuration with per-device overrides.
- **Keybind sequences**: Support for multi-key sequences (e.g., Emacs-style bindings).

## Build instructions

`yawc` uses the Meson build system.

**Dependencies:**
- `wlroots` (0.20+)
- `wayland-server`
- `wayland-protocols`
- `pixman`
- `libinput`
- `xkbcommon`
- `xcb`
- `tomlplusplus`

**Build:**
```bash
meson setup build
meson compile -C build
```

**Install:**
```bash
sudo meson install -C build
```

## Runtime dependencies

**Required:**
- `wlroots` (0.20+)

**Optional but recommended:**
- `xdg-desktop-portal-hyprland` - For global shortcuts, screenshotting and screensharing support
- `xwayland-satellite` - For XWayland support (prevents blurry X11 apps [old folder has the old implementation])
- `wlr-randr` or `kanshi` - For output configuration

## Configuration

Configuration is handled via `yawc.toml` (typically `~/.config/yawc/yawc.toml`).

**Outputs**: Use tools like `wlr-randr` and `kanshi` to configure outputs.

**Inputs**: Input configuration is handled directly in `yawc.toml` as there is currently no standardized Wayland tool for this.

```toml
# Window Manager Plugin (Required)
window_manager = "/usr/lib/yawc/default_wm.so"

# Autostart
autostart = [
    "/usr/lib/polkit-gnome/polkit-gnome-authentication-agent-1",
    "dbus-update-activation-environment --systemd WAYLAND_DISPLAY XDG_CURRENT_DESKTOP"
]

[environment]
XDG_CURRENT_DESKTOP = "YAWC"

[keyboard]
xkb_layout = "us"
# xkb_options = "caps:escape"
repeat_rate = 25
repeat_delay = 600

[pointer]
accel_profile = "adaptive"
nat_scrolling = true
tap_to_click = true

[keybinds]
# Syntax: "Modifier+Key" = "Command"
"Super+Return" = "alacritty"
"Super+d" = "wofi --show drun"
"Super+b" = "firefox"

# Keybind Sequences: Comma-separated for multi-key bindings
# Timeout: 1 second between keys (e.g., "Super+x,Super+c" requires Super+x then Super+c within 1s)
"Super+x,Super+c" = "alacritty"

# Global shortcuts exposed to xdg-desktop-portal
# Format: "Modifier+Key" = "app_id:id"
"Super+F5" = "com.obsproject.Studio:_toggle_recording"

## Tools

```

**Reload configuration**: The compositor listens for `SIGUSR1` to reload the configuration without restarting:
```bash
yawc-reload  # or: kill -USR1 $(pidof yawc)
```

**List global shortcuts**: View all registered global shortcuts:
```bash
yawc-shortcuts
```

## Device specific configuration

Override settings for specific input devices by creating a section with the device name:

```toml
["Logitech G Pro X Superlight"]
type = "pointer"
accel_profile = "flat"
accel_speed = 0.0

["Keychron K2"]
type = "keyboard"
xkb_layout = "de"

```

## Images
![Image](https://github.com/user-attachments/assets/94f3522e-b240-400a-95ad-545a1b2c02ad)

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Thanks
To all the other wlroots projects out there, *specially* tinywl and sway.
