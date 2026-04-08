project('gbt-player', 'c',
  version: '4.4.1',
  license: 'MIT',
  meson_version: '>=0.54.0',
  default_options: ['warning_level=1', 'c_std=none'])

gbt_player = static_library('gbt-player', 'gba/gbt_player/gbt_player.c')

gbt_player_dep = declare_dependency(
  include_directories: ['gba/gbt_player'],
  link_with: gbt_player)

meson.override_dependency('gbt-player', gbt_player_dep)

meson.override_find_program('mod2gbt', files('gba/mod2gbt/mod2gbt.py'))
meson.override_find_program('s3m2gbt', files('gba/s3m2gbt/s3m2gbt.py'))
meson.override_find_program('s3msplit', files('gba/s3msplit/s3msplit.py'))
