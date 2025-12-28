srcs_wm = files('main.c', 'nuklear_wm_gl.c')

inc = include_directories('..')

cc = meson.get_compiler('c')
math = cc.find_library('m', required: false)

lib_main = shared_module('default_wm',
  srcs_wm,
  include_directories: inc,
  link_args: ['-Wl,--unresolved-symbols=ignore-in-object-files'],
  dependencies: [math],
  install: true,
  install_dir: get_option('libexecdir') / 'yawc'
)
