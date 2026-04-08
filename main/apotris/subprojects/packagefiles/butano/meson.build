# Based on https://github.com/GValiente/butano/blob/15.4.1/butano/butano.mak#L76-L123
project('butano', 'c', 'cpp',
  version: '15.7.0',
  license: 'Zlib',
  meson_version: '>=0.54.0',
  default_options: [
    'warning_level=3',
    'c_std=none',
    'cpp_std=c++20',
    'cpp_eh=none',
    'cpp_rtti=false'])

c_args = [
  '-Wpedantic',
  '-Wshadow',
  '-Wundef',
  '-Wunused-parameter',
  '-Wmisleading-indentation',
  '-Wduplicated-cond',
  '-Wduplicated-branches',
  '-Wlogical-op',
  '-Wnull-dereference',
  '-Wswitch-default',
  '-Wstack-usage=16384',
]

cpp_args = [
  '-Wuseless-cast',
  '-Wnon-virtual-dtor',
  '-Woverloaded-virtual',
  '-fno-threadsafe-statics',
  '-fuse-cxa-atexit',
]

add_project_arguments(c_args, language: ['c', 'cpp'])
add_project_arguments(cpp_args, language: 'cpp')

sources = [
  'butano/hw/3rd_party/gbt-player/src/gbt_player.c',
  'butano/hw/3rd_party/libtonc/src/tonc_color.c',
  'butano/hw/3rd_party/libtonc/src/tonc_sbmp16.c',
  'butano/hw/3rd_party/libtonc/src/tonc_schr4c.c',
  'butano/hw/3rd_party/libtonc/src/tonc_schr4r.c',
  'butano/hw/3rd_party/libtonc/src/tonc_surface.c',
  'butano/hw/3rd_party/libtonc/src/tte/bmp16_drawg_b1cs.c',
  'butano/hw/3rd_party/libtonc/src/tte/tte_init_bmp.c',
  'butano/hw/3rd_party/libtonc/src/tte/tte_main.c',
  'butano/hw/3rd_party/libugba/src/interrupts_gba.c',
  'butano/hw/3rd_party/vgm-player/src/vgm.cpp',
  'butano/hw/src/bn_hw_audio.cpp',
  'butano/hw/src/bn_hw_cstdlib.cpp',
  'butano/hw/src/bn_hw_cstring.cpp',
  'butano/hw/src/bn_hw_game_pak.cpp',
  'butano/hw/src/bn_hw_irq.cpp',
  'butano/hw/src/bn_hw_link.cpp',
  'butano/hw/src/bn_hw_log.cpp',
  'butano/hw/src/bn_hw_memory.cpp',
  'butano/hw/src/bn_hw_palettes.cpp',
  'butano/hw/src/bn_hw_show.cpp',
  'butano/hw/src/bn_hw_sprite_tiles.cpp',
  'butano/hw/src/bn_hw_sram.cpp',
  'butano/hw/src/bn_hw_system_font.cpp',
  'butano/hw/src/bn_hw_text.cpp',
  'butano/src/bn_affine_bg_animate_actions.cpp',
  'butano/src/bn_audio_manager.cpp',
  'butano/src/bn_best_fit_allocator.cpp',
  'butano/src/bn_bg_blocks_manager.cpp',
  'butano/src/bn_bgs_manager.cpp',
  'butano/src/bn_cameras_manager.cpp',
  'butano/src/bn_color_effect.cpp',
  'butano/src/bn_core.cpp',
  'butano/src/bn_display_manager.cpp',
  'butano/src/bn_format.cpp',
  'butano/src/bn_generic_pool.cpp',
  'butano/src/bn_gpio_manager.cpp',
  'butano/src/bn_hblank_effects_manager.cpp',
  'butano/src/bn_hdma_manager.cpp',
  'butano/src/bn_keypad_manager.cpp',
  'butano/src/bn_link_manager.cpp',
  'butano/src/bn_log.cpp',
  'butano/src/bn_math.cpp',
  'butano/src/bn_memory_manager.cpp',
  'butano/src/bn_palettes_manager.cpp',
  'butano/src/bn_profiler.cpp',
  'butano/src/bn_reciprocal_lut.cpp',
  'butano/src/bn_regular_bg_animate_actions.cpp',
  'butano/src/bn_sin_lut.cpp',
  'butano/src/bn_sprite_affine_mats_manager.cpp',
  'butano/src/bn_sprite_animate_actions.cpp',
  'butano/src/bn_sprite_text_generator.cpp',
  'butano/src/bn_sprite_tiles_manager.cpp',
  'butano/src/bn_sprites_manager.cpp',
  'butano/src/bn_sram.cpp',
  'butano/src/bn_sstream.cpp',
  'butano/src/bn_timer.cpp',
]

sources_asm = [
  'butano/hw/3rd_party/agbabi/src/atan2.s',
  'butano/hw/3rd_party/agbabi/src/lmul.s',
  'butano/hw/3rd_party/agbabi/src/memcpy.s',
  'butano/hw/3rd_party/agbabi/src/memmove.s',
  'butano/hw/3rd_party/agbabi/src/memset.s',
  'butano/hw/3rd_party/agbabi/src/rmemcpy.s',
  'butano/hw/3rd_party/cult-of-gba-bios/src/huffman.s',
  'butano/hw/3rd_party/cult-of-gba-bios/src/lz77.s',
  'butano/hw/3rd_party/cult-of-gba-bios/src/running_length.s',
  'butano/hw/3rd_party/gba-modern/src/clz.s',
  'butano/hw/3rd_party/gba-modern/src/sdiv32.s',
  'butano/hw/3rd_party/gba-modern/src/sqrt32.s',
  'butano/hw/3rd_party/gba-modern/src/udiv32.s',
  'butano/hw/3rd_party/libtonc/asm/clr_blend_fast.s',
  'butano/hw/3rd_party/libtonc/asm/clr_fade_fast.s',
  'butano/hw/3rd_party/libtonc/asm/tonc_bios.s',
  'butano/hw/3rd_party/libtonc/asm/tonc_memset.s',
  'butano/hw/3rd_party/libtonc/asm/tonc_nocash.s',
  'butano/hw/3rd_party/libtonc/src/font/verdana9.s',
  'butano/hw/3rd_party/libugba/src/irq_handler.s',
  'butano/hw/3rd_party/posprintf/src/posprintf.s',
  'butano/hw/src/bn_hw_bg_blocks.s',
  'butano/hw/src/bn_hw_core.s',
  'butano/hw/src/bn_hw_hblank_effects.s',
  'butano/hw/src/bn_hw_palettes_asm.s',
]

sources_iwram = [
  'butano/hw/src/bn_hw_memory.bn_iwram.cpp',
  'butano/hw/src/bn_hw_palettes.bn_iwram.cpp',
  'butano/hw/src/bn_hw_sprite_tiles.bn_iwram.cpp',
  'butano/src/bn_sprites_manager.bn_iwram.cpp',
]

sources_ewram = [
  'butano/hw/src/bn_hw_game_pak.bn_ewram.cpp',
  'butano/hw/src/bn_hw_sram.bn_ewram.cpp',
]

sources_nolto = [
  'butano/hw/src/bn_hw_common.bn_noflto.cpp',
]

includes = [
  'butano/include',
  'butano/hw/3rd_party/libtonc/include',
  'butano/hw/3rd_party/libugba/include',
]

deps = [
  dependency('maxmod'),
]

butano_asm = static_library('butano-asm', sources_asm,
  include_directories: includes,
  dependencies: deps,
  c_args: ['-xassembler-with-cpp'])

butano_iwram = static_library('butano-iwram', sources_iwram,
  include_directories: includes,
  dependencies: deps,
  c_args: ['-marm', '-fno-lto'])

butano_ewram = static_library('butano-ewram', sources_ewram,
  include_directories: includes,
  dependencies: deps,
  c_args: ['-fno-lto'])

butano_nolto = static_library('butano-nolto', sources_nolto,
  include_directories: includes,
  dependencies: deps,
  c_args: ['-fno-lto'])

butano = static_library('butano', sources,
  include_directories: includes,
  dependencies: deps,
  objects: [
    butano_asm.extract_all_objects(recursive: false),
    butano_iwram.extract_all_objects(recursive: false),
    butano_ewram.extract_all_objects(recursive: false),
    butano_nolto.extract_all_objects(recursive: false),
  ])

butano_dep = declare_dependency(
  include_directories: includes,
  link_with: butano)

meson.override_dependency('butano', butano_dep)
