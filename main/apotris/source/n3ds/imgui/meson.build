# project('imgui', 'cpp')

imgui_dep = dependency(
    'imgui',
    static : true,
    default_options : ['default_library=static'],
    fallback: ['imgui', 'imgui_dep'],
)

imgui_3ds_sources = files(
    'ftpd/imgui_citro3d.cpp',
    'ftpd/imgui_ctru.cpp',
)

imgui_3ds_vshader_shbin = custom_target(
    input : 'ftpd/vshader.v.pica',
    output : 'imgui_vshader.shbin',
    command : [
        'picasso',
        '-o', '@OUTPUT@',
        '@INPUT@',
    ],
)
imgui_3ds_vshader_shbin_bin2s = custom_target(
    'imgui_vshader_bin2s',
    input : imgui_3ds_vshader_shbin,
    output : ['imgui_vshader_shbin.s', 'imgui_vshader_shbin.h'],
    command : [
        py3, call_bin2s, bin2s, '@INPUT@', '@OUTPUT1@', '@OUTPUT0@', 'gba', xxd
    ],
)
imgui_3ds_generated_files = [
    imgui_3ds_vshader_shbin_bin2s,
]

imgui_3ds_inc = include_directories('ftpd', 'imgui_club')
imgui_3ds = static_library(
    'imgui_3ds',
    imgui_3ds_generated_files + imgui_3ds_sources,
    dependencies : imgui_dep,
    include_directories : imgui_3ds_inc,
    cpp_args : ['-std=c++20']
)

imgui_3ds_dep = declare_dependency(
    include_directories : imgui_3ds_inc,
    link_with : imgui_3ds,
)
