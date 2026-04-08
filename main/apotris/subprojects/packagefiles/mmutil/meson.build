# Based on https://github.com/devkitPro/mmutil/blob/v1.10.1/Makefile.am
project('mmutil', 'c',
  version: '1.10.1',
  meson_version: '>=0.50.0',
  default_options: ['warning_level=0', 'c_std=none'])

sources = [
  'source/adpcm.c',
  'source/files.c',
  'source/gba.c',
  'source/it.c',
  'source/main.c',
  'source/mas.c',
  'source/mod.c',
  'source/msl.c',
  'source/nds.c',
  'source/s3m.c',
  'source/samplefix.c',
  'source/simple.c',
  'source/wav.c',
  'source/xm.c',
]

includes = ['source']

version_string = meson.project_version()
pkgver = 'PACKAGE_VERSION="@0@"'.format(version_string)

args = ['-D' + pkgver, '-DVERSION_STRING="@0@"'.format(version_string)]

cc = meson.get_compiler('c', native: true)

if cc.get_id() != 'msvc'
  args += ['-Wno-multichar']
endif

m_dep = cc.find_library('m', required: false)

mmutil = executable('mmutil', sources,
  include_directories: includes,
  c_args: args,
  dependencies: m_dep,
  native: true)

meson.override_find_program('mmutil',  mmutil)
