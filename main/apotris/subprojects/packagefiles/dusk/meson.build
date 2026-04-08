# Based on https://github.com/bmchtech/dusk/blob/131f14ea65799a8c4ff2ff43d0c9978b3d7fda2a/src/dusk/Makefile#L20-L56
project('dusk', 'c',
  version: '0.1.7',
  license: 'GPL-3.0-only',
  meson_version: '>=0.54.0',
  default_options: ['warning_level=1', 'c_std=none'])

c_args = [
  '-Werror=implicit-function-declaration',
]

add_project_arguments(c_args, language: 'c')

sources = [
  'src/dusk/src/contrib/initMapRegisters.c',
  'src/dusk/src/contrib/libgbfs.c',
  'src/dusk/src/contrib/loadMapFromROM.c',
  'src/dusk/src/contrib/loadObject.c',
  'src/dusk/src/contrib/loadObjectID.c',
  'src/dusk/src/contrib/loadPosition.c',
  'src/dusk/src/contrib/loadString.c',
  'src/dusk/src/contrib/mgba.c',
  'src/dusk/src/contrib/popValue.c',
  'src/dusk/src/contrib/setMapOnScreen.c',
  'src/dusk/src/contrib/shiftMap.c',
  'src/dusk/src/contrib/shiftMapLayer.c',
  'src/dusk/src/contrib/shiftMapObjects.c',
  'src/dusk/src/load.c',
  'src/dusk/src/sprites.c',
  'src/dusk/src/sys.c',
]

includes = [
  'src/dusk/include',
  'src/dusk/include/contrib',
]

libtonc = dependency('libtonc')

dusk = static_library('dusk', sources,
  include_directories: includes,
  dependencies: [libtonc],
  c_args: ['-DDUSK_VERSION="@0@"'.format(meson.project_version())])

dusk_dep = declare_dependency(
  link_with: dusk,
  include_directories: includes)

meson.override_dependency('dusk', dusk_dep)
