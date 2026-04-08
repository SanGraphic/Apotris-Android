project('heartlib', 'c',
  version: '0.100.0',
  license: 'GPL-3.0-or-later',
  meson_version: '>=0.54.0',
  default_options: ['warning_level=1', 'c_std=none'])

sources = [
  'src/aplib.c',
  'src/asm_misc.s',
  'src/bios.s',
  'src/debug.c',
  'src/drawer.c',
  'src/font.c',
  'src/fx.c',
  'src/gbfs.c',
  'src/input.c',
  'src/intdispatch.s',
  'src/interrupt.c',
  'src/jpg.c',
  'src/memcpy.s',
  'src/misc.c',
  'src/oam.c',
  'src/pcx.c',
  'src/pucrunch.s',
  'src/rng.c',
  'src/rtc.s',
  'src/rumble.c',
  'src/text.c',
  'src/video.c',
]

includes = ['include']

heartlib = static_library('heart', sources,
  include_directories: includes)

heartlib_dep = declare_dependency(
  include_directories: includes,
  link_with: heartlib)

meson.override_dependency('heartlib', heartlib_dep)
