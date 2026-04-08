# Based on
project('gba-link-connection', 'cpp',
  version: '3.0.0',
  license: 'MIT',
  meson_version: '>=0.54.0',
  default_options: ['warning_level=0', 'cpp_std=none'])

gba_link_connection_dep = declare_dependency(
  include_directories: ['lib'])

meson.override_dependency('gba-link-connection', gba_link_connection_dep)
