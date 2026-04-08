# Based on https://github.com/gbadev-org/libtonc/blob/cc862ceb8b7151f1125b6ce629925be6e54827c3/Makefile#L17-L44
project('libtonc', 'c',
  version: '1.4.3',
  license: 'MIT',
  meson_version: '>=0.59.0',
  default_options: ['warning_level=1', 'c_std=none'])

sources_asm = [
  'asm/clr_blend_fast.s',
  'asm/clr_fade_fast.s',
  'asm/div_lut.s',
  'asm/sin_lut.s',
  'asm/tonc_bios.s',
  'asm/tonc_bios_ex.s',
  'asm/tonc_isr_master.s',
  'asm/tonc_isr_nest.s',
  'asm/tonc_memcpy.s',
  'asm/tonc_memset.s',
  'asm/tonc_nocash.s',
  'src/font/sys8.s',
  'src/font/verdana10.s',
  'src/font/verdana9.s',
  'src/font/verdana9_b4.s',
  'src/font/verdana9b.s',
  'src/font/verdana9i.s',
  'src/pre1.3/toncfont.s',
  'src/tte/bmp8_drawg_b1cts_fast.s',
  'src/tte/chr4c_drawg_b1cts_fast.s',
  'src/tte/chr4c_drawg_b4cts_fast.s',
  'src/tte/chr4r_drawg_b1cts_fast.s',
  'src/tte/tte_types.s',
]

sources_c = [
  'src/pre1.3/tonc_bitmap.c',
  'src/pre1.3/tonc_text_bm.c',
  'src/pre1.3/tonc_text.c',
  'src/pre1.3/tonc_text_map.c',
  'src/pre1.3/tonc_text_oam.c',
  'src/tonc_bg_affine.c',
  'src/tonc_bg.c',
  'src/tonc_bmp16.c',
  'src/tonc_bmp8.c',
  'src/tonc_color.c',
  'src/tonc_core.c',
  'src/tonc_input.c',
  'src/tonc_irq.c',
  'src/tonc_math.c',
  'src/tonc_oam.c',
  'src/tonc_obj_affine.c',
  'src/tonc_sbmp16.c',
  'src/tonc_sbmp8.c',
  'src/tonc_schr4c.c',
  'src/tonc_schr4r.c',
  'src/tonc_surface.c',
  'src/tonc_video.c',
  'src/tte/ase_drawg.c',
  'src/tte/bmp16_drawg_b1cs.c',
  'src/tte/bmp16_drawg.c',
  'src/tte/bmp8_drawg_b1cs.c',
  'src/tte/bmp8_drawg.c',
  'src/tte/chr4c_drawg_b1cts.c',
  'src/tte/chr4c_drawg_b4cts.c',
  'src/tte/chr4r_drawg_b1cts.c',
  'src/tte/obj_drawg.c',
  'src/tte/se_drawg.c',
  'src/tte/tte_init_ase.c',
  'src/tte/tte_init_bmp.c',
  'src/tte/tte_init_chr4c.c',
  'src/tte/tte_init_chr4r.c',
  'src/tte/tte_init_obj.c',
  'src/tte/tte_init_se.c',
  'src/tte/tte_main.c',
]

includes = ['include']

compile_args = ['-Wno-incompatible-pointer-types']

cc = meson.get_compiler('c')

# Check if we have the relevant newlib patches
has_devoptab_t = cc.has_type('devoptab_t', prefix: '#include <sys/iosupport.h>')

console = get_option('dkp_console').require(has_devoptab_t)

if console.allowed()
  sources_c += ['src/tte/tte_iohook.c']

  compile_args += ['-DLIBTONC_HAS_CONSOLE']
endif

libtonc_asm = static_library('tonc_asm', sources_asm,
  include_directories: includes,
  c_args: ['-xassembler-with-cpp', '-Wa,--warn'])

libtonc = static_library('tonc', sources_c,
  include_directories: includes,
  c_args: compile_args,
  objects: libtonc_asm.extract_all_objects(recursive: false))

libtonc_dep = declare_dependency(
  compile_args: compile_args,
  include_directories: includes,
  link_with: libtonc)

meson.override_dependency('libtonc', libtonc_dep)
