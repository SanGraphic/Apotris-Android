# Based on https://github.com/devkitPro/grit/blob/v0.9.2/Makefile.am
project('grit', 'cpp',
  version: '0.9.2',
  license: 'MIT',
  meson_version: '>=0.50.0',
  default_options: ['warning_level=0', 'cpp_std=none'])

pkgver = 'PACKAGE_VERSION="@0@"'.format(meson.project_version())

add_project_arguments(['-D' + pkgver], language: 'cpp', native: true)

sources_libcldib = [
  'cldib/cldib_adjust.cpp',
  'cldib/cldib_conv.cpp',
  'cldib/cldib_core.cpp',
  'cldib/cldib_tmap.cpp',
  'cldib/cldib_tools.cpp',
  'cldib/cldib_wu.cpp',
]

sources_libgrit = [
  'libgrit/cprs.cpp',
  'libgrit/cprs_huff.cpp',
  'libgrit/cprs_lz.cpp',
  'libgrit/cprs_rle.cpp',
  'libgrit/grit_core.cpp',
  'libgrit/grit_misc.cpp',
  'libgrit/grit_prep.cpp',
  'libgrit/grit_shared.cpp',
  'libgrit/grit_xp.cpp',
  'libgrit/logger.cpp',
  'libgrit/pathfun.cpp',
]

sources_grit = [
  'extlib/fi.cpp',
  'srcgrit/cli.cpp',
  'srcgrit/grit_main.cpp',
]

cc = meson.get_compiler('cpp', native: true)

m_dep = cc.find_library('m', required: false)
fi_dep = cc.find_library('freeimage')

libcldib = static_library('cldib', sources_libcldib,
  include_directories: ['cldib'],
  native: true)

libgrit = static_library('grit', sources_libgrit,
  include_directories: ['cldib', 'libgrit'],
  native: true)

grit = executable('grit', sources_grit,
  include_directories: ['cldib', 'libgrit', 'extlib'],
  link_with: [libcldib, libgrit],
  dependencies: [m_dep, fi_dep],
  native: true)

meson.override_find_program('grit', grit)
