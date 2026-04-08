project('Apotris',
        'c', 'cpp',
        version : '4.1.1b',
        license : 'AGPL-3.0', meson_version : '>=0.60.0',
        default_options : ['buildtype=debugoptimized', 'cpp_std=c++17', 'default_library=static', 'warning_level=0', 'b_lto=false'])


# The name of your project.
name = 'Apotris'

# Define the platform-specific directories
src_dir = 'source'
inc_dir = 'include'
data_dir = 'data'
audio_dir = 'assets'
shaders_dir = 'assets/shaders'

# Include directories
includes = [
    'include',
    data_dir,
]

# Bring in all source files
sources = files(
    src_dir / 'audio_files.cpp',
    src_dir / 'blockEngine.cpp',
    src_dir / 'bot.cpp',
    src_dir / 'descriptions.cpp',
    src_dir / 'detectEmulators.c',
    src_dir / 'flashSaves.cpp',
    src_dir / 'game.cpp',
    src_dir / 'gbp_logo.cpp',
    src_dir / 'interrupt.cpp',
    src_dir / 'liba_gba.cpp',
    src_dir / 'liba_mm.cpp',
    src_dir / 'liba_pc.cpp',
    src_dir / 'liba_switch.cpp',
    src_dir / 'liba_portmaster.cpp',
    src_dir / 'liba_web.cpp',
    src_dir / 'liba_window.cpp',
    src_dir / 'liba_sdl_audio.cpp',
    src_dir / 'main.cpp',
    src_dir / 'menuSystem.cpp',
    src_dir / 'menus.cpp',
    src_dir / 'multiplayer.cpp',
    src_dir / 'multiplayerClasses.cpp',
    src_dir / 'music.cpp',
    src_dir / 'prng.cpp',
    src_dir / 'qrcodegen.c',
    src_dir / 'rumblePatterns.cpp',
    src_dir / 'saving.cpp',
    src_dir / 'scene.cpp',
    src_dir / 'screenAudio.cpp',
    src_dir / 'screenControls.cpp',
    src_dir / 'screenCredits.cpp',
    src_dir / 'screenGraphics.cpp',
    src_dir / 'screenHandling.cpp',
    src_dir / 'screenLinks.cpp',
    src_dir / 'screenPlayOptions.cpp',
    src_dir / 'screenSkinEditor.cpp',
    src_dir / 'screenColorEditor.cpp',
    src_dir / 'screenTitle.cpp',
    src_dir / 'sprites.cpp',
    src_dir / 'tetromino.cpp',
    src_dir / 'text.cpp',
    src_dir / 'nanotime.c',
    src_dir / 'shader.cpp',
    src_dir / 'gl.c',
    src_dir / 'gles2.c',
    src_dir / 'xoshiro128plusplus.c',
)

if host_machine.system() == 'android'
    sources += files(src_dir / 'liba_android_jni.cpp')
endif

# Generate version header
configure_file(
    input : 'include/version.h.in',
    output : 'version.h',
    configuration : {
        'version': meson.project_version(),
    },
)

# Bring in binary/data files
subdir(data_dir)

# Common build file references

icon_file = files('sprites/favicon32.bmp')
effect_locations_file = 'effect_locations.bin'

# Set up recommended compiler and linker flags for GBA before loading any other projects
if host_machine.system() == 'gba'
    subdir('meson/toolchain')
    add_project_arguments('-DLINK_WIRELESS_USE_SEND_RECEIVE_LATCH=1', language : 'cpp')
    add_project_arguments('-Wno-incompatible-pointer-types', language : 'c')
    if get_option('emu_detect')
        add_project_arguments('-DEMU_DETECT', language : 'cpp')
    endif
endif

if host_machine.system() == 'windows'
    add_global_arguments('-DWIN32', language : ['c', 'cpp'])
    add_global_arguments('-mwin32', language : ['c', 'cpp'])
elif host_machine.system() == 'emscripten'
    add_global_arguments('-pthread', language : ['cpp', 'c'])
    add_global_arguments('-fno-stack-protector', language : ['cpp', 'c'])
    add_global_arguments('-U_FORTIFY_SOURCE', language : ['cpp', 'c'])
elif host_machine.system() == 'darwin'
    add_global_arguments('-mmacosx-version-min=10.15', language : ['cpp', 'c'])
endif

build_type = get_option('buildtype')

if host_machine.system() == 'n3ds'

    # Avoid issue with breaking stdlib synchronization
    # (https://github.com/devkitPro/gcc/issues/5)
    add_global_arguments([
        '-D_GLIBCXX_GTHREAD_USE_WEAK=1',
    ], language: ['cpp'])

    if build_type == 'release' or build_type == 'minsize'
        add_global_arguments(['-ffunction-sections','-fdata-sections'], language : ['c', 'cpp'])
        add_global_link_arguments(['-Wl,--gc-sections'], language : ['c', 'cpp'])
    endif

    enable_debug_ui = get_option('debug_ui')
    if enable_debug_ui
        add_global_arguments([
            '-DImTextureID=void*',
            '-DIMGUI_DISABLE_OBSOLETE_FUNCTIONS=1',
            '-DIMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS=1',
        ], language: ['cpp'])
        add_project_arguments('-DDEBUG_UI', language : ['c', 'cpp'])
    else
        add_project_arguments(['-DNO_DIAGNOSE', '-DNDEBUG'], language : ['c', 'cpp'])
    endif

    enable_profiling = get_option('profiling')
    if enable_profiling
        add_global_arguments([
            '-DTRACY_ENABLE',
            '-DTRACY_DELAYED_INIT',
            '-DTRACY_MANUAL_LIFETIME',
            '-DTRACY_NO_FRAME_IMAGE',
            '-DTRACY_SYMBOL_OFFLINE_RESOLVE',
        ], language: ['cpp'])
    endif

endif

# Don't use mGBA logging in release build types, only debug and friends
if build_type.startswith('release') or build_type.startswith('minsize')
  add_project_arguments('-DNO_DIAGNOSE', language : ['c', 'cpp'])
endif

# Various helper libraries, subprojects, and scripts common to all builds
fs = import('fs')
tools = subproject('general-tools')
padbin = tools.get_variable('padbin_executable')
bin2s = files('tools/bin2s.py')
call_bin2s = files('tools/callBin2S.py')
if host_machine.system() == 'horizon'
    elf2nro = find_program('elf2nro')
    nacptool = find_program('nacptool')
    call_build_rom = files('tools/switchMakeNRO.py')
endif

# Required to invoke python scripts in a Windows-native build environment
pymod = import('python')
py3 = pymod.find_installation('python3', required: true)

# Windows uses a pre-compiled xxd
if build_machine.system() == 'windows'
    xxd = files('tools/xxd.exe')
else
    xxd = find_program('xxd', required : true)
endif


# Custom warning level flags
add_project_arguments('-Wall', language : ['c', 'cpp'])
add_project_arguments('-Wno-array-bounds', language : ['c', 'cpp'])
add_project_arguments('-Wno-shadow', language : ['c', 'cpp'])
add_project_arguments('-Wno-unused', language : ['c', 'cpp'])

subdir('assets')
subdir('assets/shaders')

if host_machine.system() != 'gba' and host_machine.system() != 'n3ds'
    #############################
    # Windows, macOS, and Linux #
    #############################

    # Bring in required audio assets to copy to build output

    # All the dependencies should be static to work around various issues or platform specific behavior
    opus = dependency('opus', static : true, fallback : ['opus', 'opus_dep'])
    ogg = dependency('ogg', static : true, fallback : ['ogg', 'libogg_dep'])
    if host_machine.system() == 'emscripten' or host_machine.system() == 'horizon'
        # Other required dependencies
        openmpt = dependency('openmpt', static : true, default_options : ['default_library=static'], fallback : ['openmpt', 'openmpt_dep'])
        tilengine = dependency('Tilengine', static : true, default_options : ['default_library=static'], fallback : ['Tilengine', 'tilengine_dep'])
        sdl2_dep = dependency('sdl2', static : true, default_options : ['default_library=static', 'test=false'], fallback : ['sdl2', 'sdl2_dep'])
        if host_machine.system() == 'horizon'
            sdl_mixer = dependency('SDL2_mixer', static : true, default_options : ['default_library=static'], fallback : ['sdl2_mixer', 'sdl2_mixer_dep'])
        else
            sdl_mixer = dependency('sdl2_mixer', static : true, default_options : ['default_library=static'], fallback : ['sdl2_mixer', 'sdl2_mixer_dep'])
        endif
        soloud = dependency('SoLoud', static : true, default_options : ['default_library=static'], fallback : ['SoLoud', 'soloud_dep'])
    else
        # Other required dependencies
        openmpt = dependency('openmpt', static : true, default_options : ['default_library=static'], fallback : ['openmpt', 'openmpt_dep'])
        tilengine = dependency('Tilengine', static : true, default_options : ['default_library=static'], fallback : ['Tilengine', 'tilengine_dep'])
        if host_machine.system() == 'darwin'
            sdl2_dep = subproject('sdl2', default_options : ['use_audio_jack=disabled', 'default_library=static', 'test=false']).get_variable('sdl2_dep')
            sdl_mixer = dependency('sdl2_mixer', static : true, default_options : ['default_library=static'], fallback : ['sdl2_mixer', 'sdl2_mixer_dep'])
        else
            if not get_option('portmaster')
                sdl2_dep = dependency('sdl2', static : true, default_options : ['default_library=static', 'test=false'], fallback : ['sdl2', 'sdl2_dep'])
                sdl_mixer = dependency('sdl2_mixer', static : true, default_options : ['default_library=static'], fallback : ['sdl2_mixer', 'sdl2_mixer_dep'])
            else
                sdl2_dep = dependency('sdl2', static : false, default_options : ['default_library=shared', 'test=false'], fallback : ['sdl2', 'sdl2_dep'])
                sdl_mixer = dependency('sdl2_mixer', static : false, default_options : ['default_library=shared'], fallback : ['sdl2_mixer', 'sdl2_mixer_dep'])
            endif
        endif
        soloud = dependency('SoLoud', static : true, default_options : ['default_library=static'], fallback : ['SoLoud', 'soloud_dep'])
    endif

    # Android libmain.so: lld only pulls .o from static .a on demand; SDL's C++ hid backend and
    # Tilengine objects can be skipped, leaving PLATFORM_hid_* / TLN_* undefined. Meson >= 1.2
    # can force whole-archive semantics per dependency.
    tilengine_link = tilengine
    sdl2_link = sdl2_dep
    if host_machine.system() == 'android' and meson.version().version_compare('>=1.2.0')
        tilengine_link = tilengine.as_link_whole()
        sdl2_link = sdl2_dep.as_link_whole()
    endif

    dependencies = [
        dependency('threads'),
        ogg,
        openmpt,
        sdl_mixer,
        opus,
        tilengine_link,
        sdl2_link,
        soloud,
    ]

    # Define non-GBA specific preprocessor guards
    if host_machine.system() == 'horizon'
        add_project_arguments('-DSWITCH', language : ['cpp', 'c'])
    elif get_option('portmaster')
        add_project_arguments('-DPORTMASTER', language : ['cpp', 'c'])
        add_project_arguments('-DGL', language : ['c', 'cpp'])

        dependencies += dependency('glesv2')
    elif host_machine.system() == 'android'
        add_project_arguments('-DPC', language : ['cpp', 'c'])
        add_project_arguments('-DANDROID', language : ['cpp', 'c'])
        add_project_arguments('-DGL', language : ['c', 'cpp'])

        dependencies += dependency('glesv2', required : false)
        if not dependencies[-1].found()
            dependencies += declare_dependency(link_args : ['-lGLESv2', '-lEGL'])
        endif
    elif host_machine.system() != 'emscripten'
        add_project_arguments('-DPC', language : ['cpp', 'c'])
        add_project_arguments('-DGL', language : ['c', 'cpp'])

        dependencies += dependency('gl')
    else
        add_project_arguments('-DWEB', language : ['cpp', 'c'])
    endif

    add_project_arguments('-DTE', language : ['c', 'cpp'])

    generated_files = []
    # Assemble bin2s targets
    foreach binfile : binary_files
        filename = fs.name(binfile).split('.')[0]
        output_o = '@0@.bin.o'.format(fs.name(filename))
        output_h = '@0@_bin.h'.format(fs.name(filename))

        if host_machine.system() == 'darwin'
            output_s = '@0@_bin.s'.format(fs.name(filename))
            bin2s_target = custom_target(
                'bin2s_' + fs.name(binfile),
                output : [output_s, output_h],
                command : [
                    py3, call_bin2s, bin2s, binfile, output_h, output_s, 'apple', xxd
                ],
                depend_files : binfile,
            )
        else
            output_cpp = '@0@_bin.cpp'.format(fs.name(filename))
            bin2s_target = custom_target(
                'bin2s_' + fs.name(binfile),
                output : [output_cpp, output_h],
                command : [
                    py3, call_bin2s, bin2s, binfile, output_h, output_cpp, 'gcc', xxd
                ],
                depend_files : binfile,
            )
        endif
        generated_files += bin2s_target

    endforeach

    if meson.get_compiler('c').get_id() != 'clang'
        link_args = [
            '-static-libgcc',
            '-static-libstdc++',
        ]
    else
        link_args = []
    endif

    name_suffix = ''

    if host_machine.system() == 'emscripten'
        link_args += '-sAUDIO_WORKLET=1' # Avoid deprecated features in Chrome, Safari, etc.
        link_args += '-sWASM_WORKERS=1' # New threading model
        link_args += '-sSTACK_SIZE=131072' # musl default; gcc default crashes
        link_args += '--emrun'
        link_args += '-sUSE_SDL=2'
        link_args += '-sUSE_SDL_MIXER=2'
        link_args += '-sASYNCIFY' # Necessary to feed JS event pump
        link_args += '-lidbfs.js' # Game doesn't boot without it
        link_args += '-sALLOW_MEMORY_GROWTH=0' # Necessary for mobile webkit
        link_args += '-sINITIAL_MEMORY=67108864' # Reserve 64 MB of heap (default of 16MB works everywhere but let's be safe)
        link_args += '-sMIN_SAFARI_VERSION=150000' # Necessary for pthreads; not relevant on other browsers
        link_args += '--preload-file'
        link_args += 'assets@/assets'
        name_suffix = 'html'
    elif host_machine.system() == 'horizon'
        link_args += '-specs=/opt/devkitpro/libnx/switch.specs'
        name_suffix = 'elf'
    elif host_machine.system() == 'windows'
        link_args += '-Wl,-Bstatic,--whole-archive'
        link_args += '-lwinpthread'
        link_args += '-Wl,--no-whole-archive'
    elif host_machine.system() == 'darwin'
        link_args += '-mmacosx-version-min=10.15'
    endif

    # The final Apotris compile target
    if host_machine.system() == 'emscripten'
        binary = executable(
            name,
            generated_files + sources,
            include_directories : includes,
            dependencies : dependencies,
            name_suffix : name_suffix,
            link_args : link_args,
            win_subsystem: 'windows',
        )
    elif host_machine.system() == 'android'
        # Android requires a shared library named libmain.so loaded by SDL's Java glue
        binary = shared_library(
            'main',
            generated_files + sources,
            include_directories : includes,
            dependencies : dependencies,
            link_args : link_args + ['-llog', '-landroid', '-lGLESv2', '-lEGL', '-lOpenSLES'],
        )
    elif host_machine.system() == 'horizon'
        icon_file = files('dist' / 'Apotris-switch.jpg')
        binary = executable(
            name,
            generated_files + sources,
            include_directories : includes,
            dependencies : dependencies,
            link_args : link_args,
        )
        nro = custom_target(
            'Build romfs and final NRO for Apotris on Nintendo Switch',
            output : 'Apotris.nro',
            command : [
                py3, call_build_rom, elf2nro, nacptool, icon_file, binary, meson.project_version()
            ],
            depends : binary,
            build_by_default : true,
        )
    else
        binary = executable(
            name,
            generated_files + sources,
            include_directories : includes,
            dependencies : dependencies,
            link_args : link_args,
            win_subsystem : 'windows',
            install : true
        )
        if host_machine.system() == 'darwin'
            install_data('dist' / 'Apotris.icns', install_dir : 'Contents/Resources')
            install_data('dist' / 'Apotris.bash', install_dir : 'Contents/MacOS')
            install_data('dist' / 'Info.plist', install_dir : 'Contents')
            install_subdir(audio_dir, install_dir : 'Contents/Resources')
            install_subdir('license', install_dir : 'Contents/Resources')
        else
            install_subdir(audio_dir, install_dir : '.')
            install_subdir(shaders_dir, install_dir : '.')
            install_subdir('license', install_dir : '.')
        endif
    endif
elif host_machine.system() == 'n3ds'
    #####################
    # Nintendo 3DS port #
    #####################

    sources += files(
        src_dir / 'liba_n3ds.cpp',
        src_dir / 'n3ds/audio.cpp',
        src_dir / 'n3ds/renderer.cpp',
    )

    # Enable all 3DS-specific code
    add_project_arguments('-DN3DS', language : ['c', 'cpp'])

    dependencies = []

    # System libraries
    compiler = meson.get_compiler('c')
    if build_type == 'debug'
        dependencies += compiler.find_library('citro3dd')
    else
        dependencies += compiler.find_library('citro3d')
    endif

    if enable_debug_ui
        tilengine = dependency('Tilengine', static : true, default_options : ['default_library=static'], fallback : ['Tilengine', 'tilengine_dep'])
        dependencies += tilengine

        subdir('source/n3ds/imgui')
        dependencies += imgui_3ds_dep
        dependencies += imgui_dep
    endif

    libxmp = dependency('libxmp', static : true)
    dependencies += libxmp

    libvorbisidec = dependency('vorbisidec', static : true)
    dependencies += libvorbisidec

    libmpg123 = dependency('libmpg123', static : true)
    dependencies += libmpg123

    libopusfile = dependency('opusfile', static : true)
    dependencies += libopusfile

    if enable_profiling
        tracy = dependency('tracy', static : true, default_options : ['default_library=static']) #, fallback : ['Tilengine', 'tilengine_dep'])
        dependencies += tracy
    endif

    generated_files = []

    # Assemble bin2s targets
    foreach binfile : binary_files
        filename = fs.name(binfile).split('.')[0]
        output_o = '@0@.bin.o'.format(fs.name(filename))
        output_h = '@0@_bin.h'.format(fs.name(filename))
        output_s = '@0@_bin.s'.format(fs.name(filename))

        bin2s_target = custom_target(
            'bin2s_' + fs.name(binfile),
            output : [output_s, output_h],
            command : [
                py3, call_bin2s, bin2s, binfile, output_h, output_s, 'gba', xxd
            ],
            depend_files : binfile,
        )

        generated_files += bin2s_target
    endforeach

    picasso_wrapper = files('tools/picasso_wrapper.py')

    shader_shbin = custom_target(
        input : [
            src_dir / 'n3ds/shader_sprite.v.pica',
            src_dir / 'n3ds/shader_bg.v.pica',
            src_dir / 'n3ds/shader_bg.g.pica',
            src_dir / 'n3ds/shader_gradient.v.pica',
            src_dir / 'n3ds/shader_bg_mosaic_tex.v.pica',
            src_dir / 'n3ds/shader_simple_tex.v.pica',
        ],
        output : [
            'n3ds_shader.shbin',
            'n3ds_shader.dvle.h',
        ],
        command : [
            py3, picasso_wrapper,
            '-o', '@OUTPUT0@',
            '@INPUT@',
        ],
    )
    shader_shbin_bin2s = custom_target(
        'n3ds_shader_bin2s',
        input : shader_shbin[0],
        output : ['n3ds_shader.shbin.s', 'n3ds_shader.shbin.h'],
        command : [
            py3, call_bin2s, bin2s, '@INPUT@', '@OUTPUT1@', '@OUTPUT0@', 'gba', xxd
        ],
    )

    generated_files += [
        shader_shbin_bin2s,
    ]

    link_args = []

    elf = executable(
        name,
        generated_files + sources,
        include_directories : includes,
        dependencies : dependencies,
        link_args : link_args,
        name_suffix : 'elf',
    )

    smdh_name = name
    if enable_debug_ui
        smdh_name += ' (debug)'
    endif
    if enable_profiling
        smdh_name += ' (profiling)'
    endif

    smdh_longname = 'Apotris multiplatform block stacking game'
    smdh_author = 'akouzoukos, 3DS port by Alvin'

    smdh = custom_target(
        input : [
            src_dir / 'n3ds/icon_48.png',
            # src_dir / 'n3ds/icon_24.png',
        ],
        output : name + '.smdh',
        command : [
            'smdhtool',
            '--create',
            smdh_name,
            smdh_longname,
            smdh_author,
            '@INPUT0@',
            '@OUTPUT@',
            # '@INPUT1@'
        ],
    )

    rom_3dsx = custom_target(
        output : name + '.3dsx',
        input : [elf, smdh],
        command : [
            '3dsxtool',
            '@INPUT0@',
            '@OUTPUT@',
            '--smdh=@INPUT1@',
            #'--romfs=./romfs',
        ],
        build_by_default : true,
    )

    alias_target('3dsx', rom_3dsx)

    cia_banner = custom_target(
        output : 'cia.bnr',
        input : [
            src_dir / 'n3ds/banner_image.png',
            src_dir / '../assets/levelUP.wav',
        ],
        command : [
            'bannertool',
            'makebanner',
            '--image', '@INPUT0@',
            '--audio', '@INPUT1@',
            '--output', '@OUTPUT@',
        ],
    )

    cia_smdh = custom_target(
        output : 'cia.smdh',
        input : [
            src_dir / 'n3ds/icon_48.png',
        ],
        command : [
            'bannertool',
            'makesmdh',
            '--shorttitle', smdh_name,
            '--longtitle', smdh_longname,
            '--publisher', smdh_author,
            '--icon', '@INPUT0@',
            '--output', '@OUTPUT@',
            '--flags', 'visible,recordusage,savedata',
        ],
    )

    version_array = meson.project_version().split('.')

    rom_cia = custom_target(
        output : name + '.cia',
        input : [
            src_dir / 'n3ds/cia.rsf',
            elf,
            cia_smdh,
            cia_banner,
        ],
        command : [
            'makerom',
            '-rsf', '@INPUT0@',
            '-f', 'cia',
            '-o', '@OUTPUT@',
            '-elf', '@INPUT1@',
            '-icon', '@INPUT2@',
            '-banner', '@INPUT3@',
            '-major', version_array[0],
            '-minor', version_array[1],
            '-micro', version_array[2],
        ],
    )

    alias_target('cia', rom_cia)

else
    ##########################################
    # GameBoy-Advance-specific configuration #
    ##########################################
    
    # Only use devkitarm rom runtime if present			
    dka_rt = dependency('dka-rom-rt', required: false)			
    if dka_rt.found()			
        rt_dep = dka_rt			
    else			
        rt_dep = dependency('minrt')			
    endif

    dependencies = [
        rt_dep,
        dependency('libseven'),
        dependency('libutil'),
        dependency('libsavgba'),
        dependency('libtonc'),
        dependency('agbabi'),
        dependency('posprintf'),
        dependency('gbfs'),
        dependency('maxmod'),
        dependency('gba-link-connection'),
        dependency('gba-rumble'),
    ]

    #dependencies_multi = [
    #    dependency('minrt-multiboot'),
    #    dependency('libseven'),
    #    dependency('libtonc'),
    #    dependency('agbabi'),
    #    dependency('posprintf'),
    #    dependency('gbfs'),
    #    dependency('maxmod'),
    #    dependency('gba-link-connection'),
    #]

    # Suppress LinkWireless header warnings
    add_project_arguments('-Wno-format', language : 'cpp')
    add_project_arguments('-Wno-format', language : 'c')

    # Enable all GBA-specific code
    add_project_arguments('-DGBA', language : 'cpp')
    add_project_arguments('-DGBA', language : 'c')

    # Disable mGBA printf on release builds
    if get_option('buildtype').startswith('release')
        add_project_arguments('-DNO_DIAGNOSE', language : 'cpp')
        add_project_arguments('-DNO_DIAGNOSE', language : 'c')
    endif

    # Get build-machine native tools
    gba_tools = subproject('gba-tools')
    gbfs = gba_tools.get_variable('gbfs_executable')
    gbafix = find_program('gbafix-py', required : true)
    makerom = find_program('makerom', required : true)
    makebase = files('tools/makeBase.py')
    subproject('mmutil')
    mmutil = find_program('mmutil')

    # References to python helper scripts
    buildSoundbank = files('tools/buildSoundbank.py')
    gbabuild = files('tools/gbaBuild.py')
    pocket_patch = files('tools/pocket.py')
    multibootbuild = files('tools/multibootBuild.py')

    soundbank_files = audio_files
    soundbank_bin = 'soundbank.bin'
    soundbank_header = 'soundbank.h'
    sound_effects = files('tools/sound_effects.txt')

    generated_files = []
    soundbank_target = custom_target(
        'soundbank',
        output : [soundbank_bin, soundbank_header, effect_locations_file, 'audio.gbfs'],
        command : [
            py3, buildSoundbank, 'default', mmutil, gbfs, sound_effects,soundbank_files,
        ],
        depend_files : soundbank_files,
    )
    generated_files += soundbank_target

    # Assemble bin2s targets
    foreach binfile : binary_files
        filename = fs.name(binfile).split('.')[0]
        output_o = '@0@.bin.o'.format(fs.name(filename))
        output_h = '@0@_bin.h'.format(fs.name(filename))
        output_s = '@0@_bin.s'.format(fs.name(filename))

        bin2s_target = custom_target(
            'bin2s_' + fs.name(binfile),
            output : [output_s, output_h],
            command : [
                py3, call_bin2s, bin2s, binfile, output_h, output_s, 'gba', xxd
            ],
            depend_files : binfile,
        )

        generated_files += bin2s_target
    endforeach

    # # Use gbfs to create archive to append to ROM with sound data
    # audio_target = custom_target(
    #     'audio and effect bank',
    #     output : 'audio.gbfs',
    #     command : [
    #         gbfs, 'audio.gbfs', soundbank_bin, effect_locations_file,
    #     ],
    #     depend_files : [soundbank_bin, effect_locations_file],
    #     depends : soundbank_target,
    # )
    # generated_files += audio_target

    # Required for minrt
    link_args = ['-specs=nosys.specs']

    # The base multiboot elf binary
    #multi = executable(
    #    name + '-multi',
    #    sources + generated_files,
    #    link_args : link_args,
    #    cpp_args : ['-DMULTIBOOT', '-Os'],
    #    c_args : ['-Os'],
    #    include_directories : includes,
    #    dependencies : dependencies_multi,
    #    name_suffix : 'elf')

    # The base multiboot ROM with no audio, unpadded
    #multi_base = custom_target(
    #    name + '-multi-base',
    #    input : multi,
    #    output : name + '-multi-base.gba',
    #    command : [makerom, '@INPUT@', '@OUTPUT@'])

    # The base multiboot ROM with no audio, padded and fixed
    #multi_rom = custom_target(
    #    name + '-multi',
    #    input : multi_base,
    #    output : name + '-multi.gba',
    #    command : [multibootbuild, padbin, '0x10', '@INPUT@', '@OUTPUT@', gbafix])

    #output_o = 'Multiboot.o'
    #output_h = 'Multiboot.h'
    #output_s = 'Multiboot.s'

    #embedded_multiboot = custom_target(
    #    'bin2s_multiboot',
    #    output : [output_s, output_h],
    #    command : [
    #        py3, call_bin2s, bin2s, multi_rom, output_h, output_s, 'gba', xxd
    #    ],
    #    depends : multi_rom,
    #)

    #generated_files += embedded_multiboot

    # The base elf binary
    elf = executable(
        name,
        sources + generated_files, #+ files('source/sendMultiboot.cpp'),
        link_args : link_args,
        include_directories : includes,
        dependencies : dependencies,
        name_suffix : 'elf')

    # The base GBA ROM with no audio
    rom_base = custom_target(
        name + '-base',
        input : elf,
        output : name + '-base.gba',
        command : [py3, makebase, makerom, '@INPUT@', '@OUTPUT@', gbafix])

    # The final GBA ROM with audio
    rom = custom_target(
        'final ROM with audio',
        output : name + '.gba',
        input : rom_base,
        command : [py3, gbabuild, padbin, '256', '@INPUT@', 'audio.gbfs', '0x400000', '@OUTPUT@', gbafix],
        build_by_default : true)

    # Analogue Pocket rumble-patched ROM (masquerades as DRILL DOZER)
    pocket = custom_target(
        'patched ROM for Analogue Pocket rumble support',
        output : name + '-pocket.gba',
        input : rom,
        command : [py3, pocket_patch, gbafix, '@INPUT@', '@OUTPUT@'],
        build_by_default : false)

endif
