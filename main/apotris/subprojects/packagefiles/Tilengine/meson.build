project('Tilengine', 'c', version : '1.0', default_options : ['c_std=gnu99'])

sources = files(
    'src' / 'Animation.c',
    'src' / 'Base64.c',
    'src' / 'Bitmap.c',
    'src' / 'Blitters.c',
    'src' / 'Draw.c',
    'src' / 'Layer.c',
    'src' / 'List.c',
    'src' / 'LoadBitmap.c',
    'src' / 'LoadFile.c',
    'src' / 'LoadPalette.c',
    'src' / 'LoadSequencePack.c',
    'src' / 'LoadSpriteset.c',
    'src' / 'LoadTMX.c',
    'src' / 'LoadTilemap.c',
    'src' / 'LoadTileset.c',
    'src' / 'Math2D.c',
    'src' / 'Object.c',
    'src' / 'ObjectList.c',
    'src' / 'Palette.c',
    'src' / 'ResourcePacker.c',
    'src' / 'Sequence.c',
    'src' / 'SequencePack.c',
    'src' / 'Sprite.c',
    'src' / 'Spriteset.c',
    'src' / 'Tables.c',
    'src' / 'Tilemap.c',
    'src' / 'Tilengine.c',
    'src' / 'Tileset.c',
    'src' / 'World.c',
    'src' / 'aes.c',
    'src' / 'cJSON.c',
    'src' / 'crc32.c',
    'src' / 'md5.c',
    'src' / 'simplexml.c',
)

inc = include_directories('include')

link_args = []
deps = []
if host_machine.system() == 'windows'
    link_args += '-Wl,--export-all-symbols'
else
    cc = meson.get_compiler('c')
    m_dep = cc.find_library('m', required : true)
    deps += m_dep
endif

if host_machine.system() == 'windows'
    c_args = ['-O2', '-DLIB_EXPORTS']
else
    c_args = ['-O2', '-DLIB_EXPORTS', '-Wno-unused-result', '-Wno-format-truncation']
endif

lib = static_library(
    'Tilengine',
    sources,
    dependencies : deps,
    include_directories : inc,
    link_args : link_args,
    c_args : c_args,
    pic : true,
)

# For Android, apotris links this with shared_library(link_whole:) — export the target by name.
tilengine_static_lib = lib

# Android libmain.so + lld: normal link_with can omit .o from the archive; whole-archive fixes TLN_*.
if host_machine.system() == 'android'
    tilengine_dep = declare_dependency(link_whole : lib, include_directories : inc)
else
    tilengine_dep = declare_dependency(link_with : lib, include_directories : inc)
endif
