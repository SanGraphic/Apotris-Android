# Based on https://github.com/Optiroc/SuperFamiconv/blob/a5027f3c4f1e3e16f868971d343b85a9bf424f96/CMakeLists.txt
project('superfamiconv', 'c', 'cpp',
  version: '0.9.2',
  license: 'MIT',
  meson_version: '>=0.50.0',
  default_options: ['warning_level=2', 'c_std=c99', 'cpp_std=c++14'])

sources = [
  'include/fmt/format.cpp',
  'include/fmt/posix.cpp',
  'include/LodePNG/lodepng.cpp',
  'src/superfamiconv.cpp',
  'src/sfc_palette.cpp',
  'src/sfc_tiles.cpp',
  'src/sfc_map.cpp',
  'src/Image.cpp',
  'src/Map.cpp',
  'src/Palette.cpp',
  'src/Tiles.cpp',
]

includes = ['include']

cpp_args = []
options = []

cpp = meson.get_compiler('cpp', native: true)

# if(MSVC)
if cpp.get_id() == 'msvc'
  sources  += 'include/getoptw/getoptw.c'
  cpp_args += '-DNOMINMAX'
  options  += 'b_vscrt=static_from_buildtype'
else
  # if(WIN32)
  if host_machine.system() == 'windows'
    cpp_args += ['-static', '-static-libgcc', '-static-libstdc++']
    options  += ['b_lto=false']
  endif
endif

superfamiconv = executable('superfamiconv', sources,
  include_directories: includes,
  cpp_args: cpp_args,
  override_options: options,
  native: true)

meson.override_find_program('superfamiconv', superfamiconv)
