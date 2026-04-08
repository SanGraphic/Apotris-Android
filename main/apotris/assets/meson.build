audio_files = files(
    'GAME_a_thirno.it',
    'GAME_b_oh_my_god.mod',
    'GAME_c_unsuspected_h.mod',
    'GAME_d_warning_infected.mod',
    'GAME_e_schizofrenium_long.mod',
    'GAME_f__detersive_.mod',
    'MENU_a_menu.it',
    'MENU_b_optikal-innovation.mod',
    'MENU_c_schmid-space_taxi.mod',
    'backToBackQuad.wav',
    'backToBackTSpin.wav',
    'clear.wav',
    'cool.wav',
    'decahexatris.wav',
    'dodecatris.wav',
    'double.wav',
    'end.wav',
    'gameover.wav',
    'go.wav',
    'hold.wav',
    'initial.wav',
    'invalid.wav',
    'levelUP.wav',
    'levelUPsound.wav',
    'menuCancel.wav',
    'menuConfirm.wav',
    'menuMove.wav',
    'multiClear.wav',
    'octoris.wav',
    'perfectClear.wav',
    'perfectris.wav',
    'place.wav',
    'quad.wav',
    'ready.wav',
    'regret.wav',
    'rotate.wav',
    'secret.wav',
    'shift.wav',
    'shift2.wav',
    'tSpin.wav',
    'tSpinMini.wav',
    'time.wav',
    'towerScroll.wav',
    'perfectScroll.wav',
    'triple.wav',
    'ultimatris.wav',
    'win.wav',
    'youLose.wav',
    'youWin.wav',
    'zoneStart.wav',
)

fs = import('fs')

list = audio_files
list += files('favicon32.bmp')

foreach file : list
    filename = fs.name(file)
    audio_target = configure_file(
        input : file,
        output: filename,
        copy : true)
endforeach
