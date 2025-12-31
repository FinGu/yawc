project('xwayland-satellite', 'rust', version: '0.1')

cargo = find_program('cargo', required: true)
sh = find_program('sh', required: true)

xws_target = custom_target(
  'cargo-build-xwayland-satellite',
  output: 'xwayland-satellite',
  build_by_default: true,
  install: true,
  install_dir: get_option('bindir'),
  console: true,
  command: [
    sh, '-c',
    # 1. Build release
    # 2. Create the destination directory explicitly (Fixes 'No such file' on cp)
    # 3. Copy directly from the standard release path
    '@0@ build --release --manifest-path "@1@/Cargo.toml" --target-dir "@2@/target" && mkdir -p "$(dirname "@3@")" && cp "@2@/target/release/xwayland-satellite" "@3@"'
    .format(
      cargo.full_path(),           # @0@
      meson.project_source_root(), # @1@
      meson.current_build_dir(),   # @2@
      '@OUTPUT@'                   # @3@
    )
  ]
)
