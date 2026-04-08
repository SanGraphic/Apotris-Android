# Based on https://github.com/AntonioND/libugba/blob/v0.3.0/source/gba/CMakeLists.txt#L22-L42
# and https://github.com/AntonioND/libugba/blob/v0.3.0/libsysgba/CMakeLists.txt#L12-L21
project('libugba', 'c',
  version: '0.3.0',
  license: 'MIT',
  meson_version: '>=0.54.0',
  default_options: ['warning_level=2', 'c_std=c11'])

sources_sysgba = [
  'libsysgba/source/gba_crt0.s',
  'libsysgba/source/syscalls.c',
]

sources_ugba = [
  'source/background.c',
  'source/bios_wrappers.c',
  'source/console.c',
  'source/debug.c',
  'source/display.c',
  'source/dma.c',
  'source/fp_math.c',
  'source/gba/bios.c',
  'source/gba/interrupts_gba.c',
  'source/gba/irq_handler.s',
  'source/gba/main.c',
  'source/gba/waitcnt.c',
  'source/graphics/font_palette_bin.c',
  'source/graphics/font_tiles_bin.c',
  'source/input.c',
  'source/interrupts.c',
  'source/obj.c',
  'source/sound.c',
  'source/sram.c',
  'source/timer.c',
  'source/version.c',
  'source/vram.c',
]

includes_ugba = ['include']

sysgba = static_library('sysgba', sources_sysgba)

ugba = static_library('ugba', sources_ugba,
  include_directories: includes_ugba,
  c_args: ['-D__GBA__', '-Wno-unused-parameter'])

ld = '-T' + meson.current_source_dir() / 'libsysgba/source/gba_cart.ld'

sysgba_dep = declare_dependency(
  link_with: sysgba,
  link_args: [ld])

ugba_dep = declare_dependency(
  include_directories: includes_ugba,
  link_with: ugba,
  compile_args: ['-D__GBA__'])

meson.override_dependency('libsysgba', sysgba_dep)
meson.override_dependency('libugba', ugba_dep)
