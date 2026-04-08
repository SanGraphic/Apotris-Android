# Based on https://github.com/devkitPro/libfat/blob/69543f058c10c90cddf3b3243819fa1953e3ea9e/gba/Makefile
project('libfat', 'c',
  version: '1.1.5',
  meson_version: '>=0.54.0',
  default_options: ['warning_level=1', 'c_std=none'])

libgba_dep = dependency('libgba')

cc = meson.get_compiler('c')

has_devoptab_t = cc.has_type('devoptab_t', prefix: '#include <sys/iosupport.h>')

if not has_devoptab_t
  error('compiler doesn\'t suppport devoptab_t')
endif

ver = meson.project_version().split('.')

ver_cfg = {
  'LIBFAT_MAJOR': ver[0],
  'LIBFAT_MINOR': ver[1],
  'LIBFAT_PATCH': ver[2],
}

libfatversion_h = configure_file(
  configuration: ver_cfg,
  input: 'libfatversion.h.in',
  output: 'libfatversion.h')

sources = [
  'source/cache.c',
  'source/directory.c',
  'source/disc.c',
  'source/fatdir.c',
  'source/fatfile.c',
  'source/file_allocation_table.c',
  'source/filetime.c',
  'source/libfat.c',
  'source/lock.c',
  'source/partition.c',
  libfatversion_h
]

includes = ['include']

libfat = static_library('fat', sources,
  include_directories: includes,
  c_args: ['-DGBA'],
  dependencies: libgba_dep)

libfat_dep = declare_dependency(
  include_directories: includes,
  link_with: libfat,
  sources: libfatversion_h)

meson.override_dependency('libfat', libfat_dep)
