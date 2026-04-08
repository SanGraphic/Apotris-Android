shader_files = files(
    'crt-lottes.glsl',
    'crt-pi.glsl',
    'retro-v2.glsl',
    'sameboy-lcd.glsl',
    'zfast_lcd.glsl',
)

fs = import('fs')

foreach file : shader_files
    filename = fs.name(file)
    shader_target = configure_file(
        input : file,
        output: filename,
        copy : true)
endforeach
