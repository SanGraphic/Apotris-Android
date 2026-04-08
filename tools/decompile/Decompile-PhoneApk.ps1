# Re-run apktool + jadx on the pulled phone APK (outputs are gitignored).
# Requires: JDK on PATH, apktool.jar beside this script (or APOTRIS_APKTOOL_JAR).
# Optional: jadx in tools/decompile/jadx-app/bin/jadx.bat (or set APOTRIS_JADX_BAT).

param(
    [string]$Apk = (Join-Path $PSScriptRoot "..\phone-apk-extract\com.example.apotris-from-phone.apk")
)

$ErrorActionPreference = "Stop"
$here = $PSScriptRoot
$apktoolJar = if ($env:APOTRIS_APKTOOL_JAR) { $env:APOTRIS_APKTOOL_JAR } else { Join-Path $here "apktool.jar" }
$outApktool = Join-Path $here "apktool-phone"
$outJadx = Join-Path $here "jadx-phone"
$jadxBat = if ($env:APOTRIS_JADX_BAT) { $env:APOTRIS_JADX_BAT } else { Join-Path $here "jadx-app\bin\jadx.bat" }

if (-not (Test-Path $Apk)) { throw "APK not found: $Apk" }
if (-not (Test-Path $apktoolJar)) { throw "apktool.jar missing: $apktoolJar" }

Write-Host "==> apktool decode -> $outApktool"
& java -jar $apktoolJar d $Apk -o $outApktool -f

if (Test-Path $jadxBat) {
    Write-Host "==> jadx -> $outJadx"
    & $jadxBat -d $outJadx --show-bad-code $Apk
} else {
    Write-Host "Skip jadx (missing $jadxBat). Extract jadx release zip to tools/decompile/jadx-app/"
}

Write-Host "Done."
