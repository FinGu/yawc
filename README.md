# yawc (Yet Another Wayland Compositor)

`yawc` is a yet another wayland compositor, built on the latest available wlroots. It is meant solely for me so there WILL NOT be support for anything that i don't/can't use.

## Quirks

- Window management logic (stacking, tiling, etc.) is loaded as a plugin.
- Tearing is always on
- Global shortcuts handled via `xdg-desktop-portal-hyprland`.
  - List active shortcuts: `yawc-shortcuts`
  - Reload configuration: `yawc-reload`
- No xwayland support outside xwayland-satellite ( which is automatically run by the compositor ).
- TOML configuration with hot-reload support.
- Input configuration with per-device overrides.
- Support for keybinds.

## Build instructions

`yawc` uses the Meson build system.

**Dependencies:**
- `wlroots` 
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

## Recommended runtime dependencies
- `xdg-desktop-portal-hyprland` - For global shortcuts, screenshotting and screensharing support
- `xwayland-satellite` - For XWayland support (prevents blurry X11 apps [old folder has the old implementation])
- `wlr-randr` or `kanshi` - For output configuration

## Configuration

> [!NOTE]
> The compositor listens for `SIGUSR1` to reload the configuration without restarting:
> ```bash
> yawc-reload  # or: kill -USR1 $(pidof yawc)
> ```

Configuration is handled via `yawc.toml` (either `~/.config/yawc.toml` or `/etc/yawc/yawc.toml`).
To configure outputs, tools like `wlr-randr` and/or `kanshi` are necessary.

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
Nuklear for the window decoration, Hyprland for specific protocols and the desktop portal.
If i forget any, please let me know.
