@echo off
setlocal enabledelayedexpansion

cd /d audio || exit /b 1

for %%f in (*.mod *.it *.xm *.s3m) do (
    ..\tools\countchan.exe "%%f" || (
        echo Error counting channels for %%f
        REM :: exit /b 1
    )
)
cd /d .. || exit /b 1

.\tools\buildSoundbank.exe append || (
    echo Error building soundbank
    exit /b 1
)

.\tools\gbfs.exe tools\audio.gbfs tools\soundbank.bin tools\effect_locations.bin || (
    echo Error creating audio.gbfs
    exit /b 1
)

.\tools\padbin.exe 256 tools\Apotris-base.gba || (
    echo Error padding Apotris-base.gba
    exit /b 1
)

.\tools\cat.exe tools\Apotris-base.gba tools\audio.gbfs > Apotris.gba || (
    echo Error creating Apotris.gba
    exit /b 1
)

@echo off
set "file=Apotris.gba"
FOR %%A IN ("%file%") DO set size=%%~zA

set /a "power=1"
:loop
if %power% LSS %size% (
  set /a "power=%power%*2"
  goto :loop
)

if %power% GTR 33554432 (
  echo:
  echo:
  echo "File size too large (%size%, max is 32MB). Try adding fewer songs."
  echo:
  echo:
  pause
) else (
  echo !power!
  tools\padbin.exe !power! Apotris.gba || (
    echo Error padding Apotris.gba
    exit /b 1
  )
)
