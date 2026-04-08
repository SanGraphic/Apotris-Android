# Based on
project('gba-rumble', 'c',
  version: '1.0.1',
  license: 'MIT',
  meson_version: '>=0.54.0',
  default_options: ['warning_level=0', 'cpp_std=none'])


gba_rumble = static_library('gba-rumble', ['gba-rumble.c', 'gba-rumble-cart.c'])

gba_rumble_dep = declare_dependency(include_directories: ['.'], link_with: gba_rumble)

meson.override_dependency('gba-rumble', gba_rumble_dep)
