# yawc (Yet Another Wayland Compositor)

`yawc` is yet another wayland compositor, built on the latest available wlroots. It is meant solely for personal usage so there WILL NOT be support for anything that i don't/can't use.

## Quirks

- Window management logic (stacking, tiling, etc.) is loaded as a plugin.
- Tearing is always on
- Global shortcuts supported via `xdg-desktop-portal-hyprland`.
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

## Configuration

Configuration is handled via `yawc.toml` (either `~/.config/yawc.toml` or `/etc/yawc/yawc.toml`).

To configure outputs, tools like `wlr-randr` and/or `kanshi` are necessary.

```toml
window_manager = "/usr/local/libexec/yawc/libdefault_wm.so"

autostart = [
    "kanshi",
    "lxqt-session",
    #"swayidle -w timeout 300 'swaylock -f' timeout 310 'wlopm --off eDP-1' resume 'wlopm --on eDP-1'"
]

[pointer]
enabled = true

accel_profile = "flat"
accel_speed = 0

tap_to_click = true
disable_w_typing = true

[keyboard]
enabled = true
xkb_layout = "us"

#["CHICONY HP Basic USB Keyboard"]
#enabled = false

[keybinds]
"F6" = "screengrab"
"Ctrl+Alt+t" = "qterminal"
```

## Images
![Image](https://github.com/user-attachments/assets/94f3522e-b240-400a-95ad-545a1b2c02ad)

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Thanks
To all the other wlroots projects out there, *specially* tinywl and sway.
Nuklear for the window decoration, Hyprland for specific protocols and the desktop portal.
If i forget any, please let me know.
