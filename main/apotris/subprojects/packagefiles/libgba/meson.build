# Based on https://github.com/gbadev-org/libgba/blob/0d46fc9f8644126bf869cf33637b32b20cd99c28/Makefile#L12-L30
project('libgba', 'c',
  version: '0.5.2',
  license: 'LGPL-2.0-or-later',
  meson_version: '>=0.59.0',
  default_options: ['warning_level=1', 'c_std=none'])

sources = [
  'src/AffineSet.c',
  'src/ArcTan.s',
  'src/BoyScout/BoyScout.c',
  'src/Compression.c',
  'src/CpuSet.c',
  'src/Div.s',
  'src/DivArm.s',
  'src/InterruptDispatcher.s',
  'src/IntrWait.c',
  'src/MultiBoot.s',
  'src/Reset.s',
  'src/Sound.s',
  'src/Sqrt.s',
  'src/disc_io/disc.c',
  'src/disc_io/dldi.c',
  'src/disc_io/dldi_stub.s',
  'src/disc_io/io_cf_common.c',
  'src/disc_io/io_m3_common.c',
  'src/disc_io/io_m3cf.c',
  'src/disc_io/io_m3sd.c',
  'src/disc_io/io_mpcf.c',
  'src/disc_io/io_sc_common.c',
  'src/disc_io/io_sccf.c',
  'src/disc_io/io_scsd.c',
  'src/disc_io/io_scsd_s.s',
  'src/disc_io/io_sd_common.c',
  'src/fade.c',
  'src/input.c',
  'src/interrupt.c',
  'src/mappy_print.c',
  'src/mb2print.c',
  'src/mbv2.c',
  'src/xcomms.c',
  'src/xcomms_print.c',
]

includes = ['include']

compile_args = []

cc = meson.get_compiler('c')

# Check if we have the relevant newlib patches
has_devoptab_t = cc.has_type('devoptab_t', prefix: '#include <sys/iosupport.h>')

console = get_option('dkp_console').require(has_devoptab_t)

if console.allowed()
  sources += ['src/amiga_fnt.s', 'src/console.c']
  compile_args += ['-DLIBGBA_HAS_CONSOLE']
endif

libgba = static_library('gba', sources,
  include_directories: includes + ['src'],
  c_args: ['-mno-asm-syntax-unified', '-Wno-switch', '-Wno-multichar', '-Wa,--warn'])

libgba_dep = declare_dependency(
  compile_args: compile_args,
  include_directories: includes,
  link_with: libgba)

meson.override_dependency('libgba', libgba_dep)
