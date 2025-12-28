project('yawc', ['c', 'cpp'],
  version: '0.1.0',
  license: 'MIT',
  meson_version: '>=1.3.0',
  default_options: [
    'cpp_std=c++23'
  ]
)

srcs = files('main.cpp', 'server.cpp', 'toplevel.cpp', 'config.cpp', 'utils.cpp', 'window_ops.cpp', 'scene_descriptor.cpp', 'handlers/xdg_shell.cpp', 'handlers/layer_shell.cpp', 'handlers/cursor.cpp', 'handlers/cursor_constraint.cpp', 'handlers/seat.cpp', 'handlers/drag.cpp', 'handlers/keyboard.cpp', 'handlers/idle.cpp', 'handlers/output.cpp', 'handlers/decoration.cpp', 'handlers/xwayland.cpp', 'handlers/screenshare.cpp', 'handlers/lock.cpp', 'wm_api.cpp', 'wm_defs.cpp', 'shm_alloc/shm.cpp', 'shm_alloc/pixel_format.cpp', 'extra/hyprland-global-shortcuts-v1.c', 'handlers/shortcut.cpp', 'keybinds.cpp', 'wm.cpp')

subdir('protocols')
subdir('default-wm')

pixman = dependency('pixman-1')

wlroots_dep = dependency('wlroots-0.20', required: true)

wayland_server_dep = dependency('wayland-server', required: true)
wayland_protocols_dep = dependency('wayland-protocols', required: true)
x11_xkbcommon = dependency('xkbcommon', required: true)
x11_xcb = dependency('xcb', required: true)
libinput = dependency('libinput', required: true)
toml = dependency('tomlplusplus', required: true)

libseat_dep = dependency('libseat', required: false)

deps = [wlroots_dep, wayland_server_dep, wayland_protocols_dep, x11_xkbcommon, x11_xcb, pixman, libinput, toml]
if libseat_dep.found()
  deps += libseat_dep
endif

install_data('tools/yawc-default-wm', install_dir: get_option('bindir'))
install_data('tools/yawc-shortcuts', install_dir: get_option('bindir'))
install_data('tools/yawc-reload', install_dir: get_option('bindir'))
install_data('tools/yawc.toml', install_dir: get_option('sysconfdir') / 'yawc')

executable('yawc',
  srcs,
  dependencies: deps,
  install: true,
  cpp_args: ['-DWLR_USE_UNSTABLE', '-DWM_API_EXPORT'],
  link_args: ['-rdynamic']
)
