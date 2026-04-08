# Based on https://github.com/blocksds/maxmod
project('maxmod', 'c',
  version: '1.0.12',
  default_options : ['c_std=gnu11', 'cpp_std=gnu++11'],
  meson_version: '>=1.1.0')

source_gba = 'source' / 'gba'
source_core = 'source' / 'core'

sources_asm = [
  source_gba / 'mixer_asm.s',
]
sources_c = [
  source_gba  / 'main_gba.c',
  source_gba  / 'mixer.c',
  source_core / 'effect.c',
  source_core / 'mas.c',
  source_core / 'mas_arm.c',
]

build_type = get_option('buildtype')
if build_type.startswith('release')
  add_project_arguments('-Os', language : ['c', 'cpp'])
endif

includes = ['source', 'include']

maxmod_asm = static_library('mm_asm', sources_asm,
  include_directories: includes,
  c_args: ['-xassembler-with-cpp', '-D__GBA__'])

maxmod = static_library('mm', sources_c,
  include_directories: includes,
  c_args: ['-D__GBA__'],
  objects: maxmod_asm.extract_all_objects(recursive: false))

maxmod_dep = declare_dependency(
  include_directories: ['include'],
  link_with: maxmod)

meson.override_dependency('maxmod', maxmod_dep)
