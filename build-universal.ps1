#!/usr/bin/env pwsh
# =============================================================================
# Apotris Android — Universal (multi-ABI) build — STANDALONE
#
# Does NOT modify build.ps1, build-android/, or android-arm64-auto.ini.
# Uses separate trees:
#   main/apotris/build-android-universal/     — Meson dirs per ABI + Gradle project
#   main/apotris/meson/universal-generated/   — generated cross files (gitignored)
#
# Usage:
#   .\build-universal.ps1
#   .\build-universal.ps1 -SkipNative
#   .\build-universal.ps1 -Install
#   .\build-universal.ps1 -Abis arm64-v8a,armeabi-v7a   # subset
#
# Requires: same as build.ps1 (Python, meson, ninja, NDK, JDK, Android SDK).
# SDL2 subproject under main/apotris/subprojects/ is created on first Meson setup.
# =============================================================================

param(
    [switch]$SkipNative,
    [switch]$Install,
    [string]$Abis = 'arm64-v8a,armeabi-v7a,x86,x86_64'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$KEYSTORE_PATH = if ($env:APOTRIS_KEYSTORE) { $env:APOTRIS_KEYSTORE } else { "apotris-release.jks" }
$KEYSTORE_PASS = if ($env:APOTRIS_KEYSTORE_PASS) { $env:APOTRIS_KEYSTORE_PASS } else { "changeme" }
$KEY_ALIAS     = if ($env:APOTRIS_KEY_ALIAS) { $env:APOTRIS_KEY_ALIAS } else { "apotris" }
$KEY_PASS      = if ($env:APOTRIS_KEY_PASS) { $env:APOTRIS_KEY_PASS } else { "changeme" }

$ROOT                = $PSScriptRoot
$UNIVERSAL_BASE      = Join-Path $ROOT "main\apotris\build-android-universal"
$PROJECT_DIR_UNIV    = Join-Path $UNIVERSAL_BASE "project"
$CROSS_OUT_DIR       = Join-Path $ROOT "main\apotris\meson\universal-generated"
$APK_SIGNED          = Join-Path $ROOT ("apotris-universal-$(Get-Date -Format 'yyyy-MM-dd').apk")
$UBER_SIGNER         = Join-Path $ROOT "uber-apk-signer.jar"
$SIGNED_STAGING      = Join-Path $ROOT "signed_output_universal"

function Script:Normalize-DirPath([string]$p) {
    if (-not $p) { return $p }
    return ($p -replace '[\\/]+$', '')
}
$LOCAL_STUDIO_SDK = Join-Path $env:LOCALAPPDATA "Android\Sdk"
$LOCAL_STUDIO_NDK = Join-Path $LOCAL_STUDIO_SDK "ndk"
$ANDROID_SDK_FALLBACK = Normalize-DirPath $(if ($env:ANDROID_SDK_ROOT) { $env:ANDROID_SDK_ROOT }
    elseif ($env:ANDROID_HOME) { $env:ANDROID_HOME }
    else { $LOCAL_STUDIO_SDK })
$NDK_VERSIONS_DIR = Normalize-DirPath $(if ($env:APOTRIS_NDK_VERSIONS) { $env:APOTRIS_NDK_VERSIONS }
    elseif (Test-Path $LOCAL_STUDIO_NDK) { $LOCAL_STUDIO_NDK }
    else { Join-Path $ANDROID_SDK_FALLBACK "ndk" })

# ABI -> Meson build subdir, cross filename, Clang triple (NDK wrapper name), Meson cpu, sysroot lib folder for libc++_shared
$script:UNIVERSAL_ABI_TABLE = @(
    @{
        Id          = 'arm64-v8a'
        MesonSubdir = 'meson-arm64'
        CrossFile   = 'android-uni-arm64.ini'
        ClangTriple = 'aarch64-linux-android'
        CpuFamily   = 'aarch64'
        Cpu         = 'aarch64'
        LibSubdir   = 'aarch64-linux-android'
    },
    @{
        Id          = 'armeabi-v7a'
        MesonSubdir = 'meson-armv7'
        CrossFile   = 'android-uni-armv7.ini'
        ClangTriple = 'armv7a-linux-androideabi'
        CpuFamily   = 'arm'
        Cpu         = 'armv7a'
        LibSubdir   = 'arm-linux-androideabi'
    },
    @{
        Id          = 'x86'
        MesonSubdir = 'meson-x86'
        CrossFile   = 'android-uni-x86.ini'
        ClangTriple = 'i686-linux-android'
        CpuFamily   = 'x86'
        Cpu         = 'i686'
        LibSubdir   = 'i686-linux-android'
    },
    @{
        Id          = 'x86_64'
        MesonSubdir = 'meson-x64'
        CrossFile   = 'android-uni-x64.ini'
        ClangTriple = 'x86_64-linux-android'
        CpuFamily   = 'x86_64'
        Cpu         = 'x86_64'
        LibSubdir   = 'x86_64-linux-android'
    }
)

function Step($msg) { Write-Host "`n==> $msg" -ForegroundColor Cyan }
function Ok($msg)   { Write-Host "    OK: $msg" -ForegroundColor Green }
function Fail($msg) { Write-Host "    FAIL: $msg" -ForegroundColor Red; exit 1 }

function Require($cmd, $hint) {
    if (-not (Get-Command $cmd -ErrorAction SilentlyContinue)) {
        Fail "$cmd not found. $hint"
    }
}

function Get-ApotrisNdkRoot {
    if ($env:ANDROID_NDK_HOME) {
        $h = Normalize-DirPath $env:ANDROID_NDK_HOME
        if (-not (Test-Path $h)) { Fail "ANDROID_NDK_HOME not found: $h" }
        return $h
    }
    if (-not (Test-Path $NDK_VERSIONS_DIR)) {
        Fail "NDK folder missing: $NDK_VERSIONS_DIR"
    }
    $dirs = @(Get-ChildItem $NDK_VERSIONS_DIR -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending)
    if ($dirs.Count -eq 0) { Fail "No NDK versions under $NDK_VERSIONS_DIR" }
    # Prefer NDK 26.x: bundled SDL 2.28.5 uses ALooper_pollAll, removed/unavailable in NDK 29+ sysroots.
    $pref26 = $dirs | Where-Object { $_.Name -match '^26\.' } | Select-Object -First 1
    if ($pref26) { return $pref26.FullName }
    return $dirs[0].FullName
}

function Get-ApotrisNdkPrebuiltHostName([string]$NdkRoot) {
    $prebuiltRoot = Join-Path $NdkRoot "toolchains\llvm\prebuilt"
    if (-not (Test-Path $prebuiltRoot)) { Fail "Invalid NDK (no toolchains/llvm/prebuilt): $NdkRoot" }
    $d = Get-ChildItem $prebuiltRoot -Directory | Select-Object -First 1
    if (-not $d) { Fail "NDK prebuilt host folder empty: $prebuiltRoot" }
    return $d.Name
}

function Write-UniversalMesonCrossIni {
    param(
        [string]$NdkRoot,
        [string]$OutPath,
        [string]$ClangTriple,
        [string]$CpuFamily,
        [string]$Cpu
    )
    $ndkFwd = $NdkRoot.Replace('\', '/')
    $hostTag = Get-ApotrisNdkPrebuiltHostName $NdkRoot
    $lines = @(
        "[constants]",
        "ndk_path = '$ndkFwd'",
        "prebuilt = '$hostTag'",
        "toolchain = ndk_path + '/toolchains/llvm/prebuilt/' + prebuilt + '/bin'",
        "sysroot = ndk_path + '/toolchains/llvm/prebuilt/' + prebuilt + '/sysroot'",
        "api = '34'",
        "",
        "[binaries]",
        "c = toolchain + '/$ClangTriple' + api + '-clang.cmd'",
        "cpp = toolchain + '/$ClangTriple' + api + '-clang++.cmd'",
        "ar = toolchain + '/llvm-ar.exe'",
        "strip = toolchain + '/llvm-strip.exe'",
        "ranlib = toolchain + '/llvm-ranlib.exe'",
        "",
        "[built-in options]",
        "c_args = ['-DVAR_ARRAYS', '-D_FILE_OFFSET_BITS=64', '--sysroot=' + sysroot]",
        "cpp_args = ['-DVAR_ARRAYS', '-D_FILE_OFFSET_BITS=64', '--sysroot=' + sysroot]",
        "c_link_args = ['--sysroot=' + sysroot]",
        "cpp_link_args = ['--sysroot=' + sysroot]",
        "",
        "[host_machine]",
        "system = 'android'",
        "cpu_family = '$CpuFamily'",
        "cpu = '$Cpu'",
        "endian = 'little'"
    )
    $enc = New-Object System.Text.UTF8Encoding $false
    [System.IO.File]::WriteAllLines($OutPath, $lines, $enc)
}

function Apply-ApotrisAndroidPort {
    param([string]$MainSrcRoot)
    $port = Join-Path $ROOT "android-port"
    if (-not (Test-Path (Join-Path $port "AndroidManifest.xml"))) {
        Fail "Repository android-port/ is missing (expected Apotris manifest + ApotrisActivity)."
    }
    New-Item -ItemType Directory -Force $MainSrcRoot | Out-Null
    Copy-Item (Join-Path $port "AndroidManifest.xml") (Join-Path $MainSrcRoot "AndroidManifest.xml") -Force
    $portJava = Join-Path $port "java"
    if (Test-Path $portJava) {
        Copy-Item (Join-Path $portJava "*") (Join-Path $MainSrcRoot "java") -Recurse -Force
    }
    $portKotlin = Join-Path $port "kotlin"
    $mainKotlin = Join-Path $MainSrcRoot "kotlin"
    if (Test-Path $portKotlin) {
        New-Item -ItemType Directory -Force $mainKotlin | Out-Null
        Copy-Item (Join-Path $portKotlin "*") $mainKotlin -Recurse -Force
    }
    $portRes = Join-Path $port "res"
    $mainRes = Join-Path $MainSrcRoot "res"
    if (Test-Path $portRes) {
        New-Item -ItemType Directory -Force $mainRes | Out-Null
        Copy-Item (Join-Path $portRes "*") $mainRes -Recurse -Force
    } else {
        $valuesTo = Join-Path $mainRes "values"
        New-Item -ItemType Directory -Force $valuesTo | Out-Null
        Copy-Item (Join-Path $port "res\values\strings.xml") (Join-Path $valuesTo "strings.xml") -Force
    }
}

function Ensure-ApotrisRootGradle {
    param([string]$ProjectDir)
    $path = Join-Path $ProjectDir "build.gradle"
    if (-not (Test-Path $path)) { return }
    $text = Get-Content -LiteralPath $path -Raw
    $kotlinCp = "classpath 'org.jetbrains.kotlin:kotlin-gradle-plugin:1.9.22'"
    if ($text -notmatch 'kotlin-gradle-plugin') {
        $text = $text -replace "(classpath 'com\.android\.tools\.build:gradle:[^']+'\s*\r?\n)", "`$1        $kotlinCp`n"
    }
    $jit = "maven { url 'https://jitpack.io' }"
    if ($text -notmatch 'jitpack\.io') {
        $text = $text -replace "(buildscript\s*\{\s*repositories\s*\{[^\}]+)(mavenCentral\(\))", "`$1`$2`n        $jit"
        $text = $text -replace "(allprojects\s*\{\s*repositories\s*\{[^\}]+)(mavenCentral\(\))", "`$1`$2`n        $jit"
    }
    $text = $text.TrimEnd("`r", "`n") + [Environment]::NewLine
    Set-Content -LiteralPath $path -Value $text -Encoding ASCII -NoNewline
}

function Sync-ApotrisLauncherIcons {
    param([string]$MainSrcRoot)
    $icon = Join-Path $ROOT "newlogo.png"
    if (-not (Test-Path $icon)) { $icon = Join-Path $ROOT "gameicon.png" }
    if (-not (Test-Path $icon)) { $icon = Join-Path $ROOT "apotris.png" }
    if (-not (Test-Path $icon)) { return }
    $res = Join-Path $MainSrcRoot "res"
    if (-not (Test-Path $res)) { return }
    Get-ChildItem $res -Directory -Filter "mipmap-*" -ErrorAction SilentlyContinue | ForEach-Object {
        Copy-Item $icon (Join-Path $_.FullName "ic_launcher.png") -Force
        Copy-Item $icon (Join-Path $_.FullName "ic_launcher_round.png") -Force
    }
}

function Sync-ApotrisGameAssets {
    param([string]$MainSrcRoot)
    $src = Join-Path $ROOT "main\apotris\assets"
    if (-not (Test-Path $src)) { return }
    $dst = Join-Path $MainSrcRoot "assets\apotris_game_assets\assets"
    New-Item -ItemType Directory -Force $dst | Out-Null
    Copy-Item (Join-Path $src "*") $dst -Recurse -Force
}

function Ensure-UniversalAndroidGradleProject {
    param([string[]]$AbiList)

    $sdl = Join-Path $ROOT "main\apotris\subprojects\SDL2-2.28.5\android-project"
    if (-not (Test-Path (Join-Path $sdl "gradlew.bat"))) {
        Fail "SDL2 android-project missing at $sdl (run .\build.ps1 once or meson setup on main/apotris so the SDL2 subproject is present)."
    }
    $appDir = Join-Path $PROJECT_DIR_UNIV "app"
    $mainSrc = Join-Path $appDir "src\main"
    $manifestPath = Join-Path $mainSrc "AndroidManifest.xml"

    function Copy-SdlAndroidSourcesUniversal {
        New-Item -ItemType Directory -Force $mainSrc | Out-Null
        Copy-Item (Join-Path $sdl "app\src\main\AndroidManifest.xml") $mainSrc -Force
        Copy-Item (Join-Path $sdl "app\src\main\java") (Join-Path $mainSrc "java") -Recurse -Force
        Copy-Item (Join-Path $sdl "app\src\main\res") (Join-Path $mainSrc "res") -Recurse -Force
    }

    if (-not (Test-Path (Join-Path $PROJECT_DIR_UNIV "gradlew.bat"))) {
        Step "Bootstrapping universal Gradle project from SDL2 (separate from build-android/project)"
        New-Item -ItemType Directory -Force $PROJECT_DIR_UNIV | Out-Null
        Copy-Item (Join-Path $sdl "gradlew.bat") $PROJECT_DIR_UNIV -Force
        Copy-Item (Join-Path $sdl "gradlew") $PROJECT_DIR_UNIV -Force
        Copy-Item (Join-Path $sdl "gradle") (Join-Path $PROJECT_DIR_UNIV "gradle") -Recurse -Force
        Copy-Item (Join-Path $sdl "build.gradle") $PROJECT_DIR_UNIV -Force
        Copy-Item (Join-Path $sdl "settings.gradle") $PROJECT_DIR_UNIV -Force
        New-Item -ItemType Directory -Force $appDir | Out-Null
        Copy-Item (Join-Path $sdl "app\proguard-rules.pro") $appDir -Force
        Copy-SdlAndroidSourcesUniversal
        $nestedBad = Join-Path $appDir "src\src"
        if (Test-Path $nestedBad) { Remove-Item $nestedBad -Recurse -Force }
    } elseif (-not (Test-Path $manifestPath)) {
        Step "Repairing universal app/src/main (manifest or SDL Java missing)"
        Copy-SdlAndroidSourcesUniversal
        $nestedBad = Join-Path $appDir "src\src"
        if (Test-Path $nestedBad) { Remove-Item $nestedBad -Recurse -Force }
    }

    Step "Applying Apotris android-port to universal project"
    Apply-ApotrisAndroidPort $mainSrc
    Sync-ApotrisGameAssets $mainSrc
    Sync-ApotrisLauncherIcons $mainSrc
    Ensure-ApotrisRootGradle $PROJECT_DIR_UNIV

    $abiGradle = ($AbiList | ForEach-Object { "'" + $_ + "'" }) -join ', '
    $appGradle = @(
        'plugins {',
        '    id ''com.android.application''',
        '    id ''org.jetbrains.kotlin.android''',
        '}',
        'android {',
        '    namespace ''com.apotris.android''',
        '    compileSdk 34',
        '    defaultConfig {',
        '        applicationId ''com.apotris.android''',
        '        minSdk 21',
        '        targetSdk 34',
        '        versionCode 1',
        '        versionName ''4.1.1b-universal''',
        '        ndk {',
        "            abiFilters $abiGradle",
        '        }',
        '    }',
        '    buildTypes {',
        '        release {',
        '            minifyEnabled false',
        '        }',
        '    }',
        '    compileOptions {',
        '        sourceCompatibility JavaVersion.VERSION_1_8',
        '        targetCompatibility JavaVersion.VERSION_1_8',
        '    }',
        '    kotlinOptions {',
        '        jvmTarget = ''1.8''',
        '    }',
        '    lint {',
        '        abortOnError false',
        '    }',
        '}',
        'dependencies {',
        '    implementation ''androidx.core:core-splashscreen:1.0.1''',
        '    implementation ''androidx.core:core-ktx:1.12.0''',
        '    implementation ''org.jetbrains.kotlinx:kotlinx-coroutines-android:1.7.3''',
        '    implementation ''com.github.Swordfish90:RadialGamePad:2.0.0''',
        '}'
    ) -join [Environment]::NewLine
    Set-Content -Path (Join-Path $appDir "build.gradle") -Value $appGradle -Encoding ASCII

    $gradlePropsPath = Join-Path $PROJECT_DIR_UNIV "gradle.properties"
    $propLines = New-Object System.Collections.ArrayList
    if (Test-Path $gradlePropsPath) {
        Get-Content -LiteralPath $gradlePropsPath | ForEach-Object { [void]$propLines.Add($_) }
    }
    function Ensure-GradlePropertyLineUniv([string]$Name, [string]$Value) {
        $pattern = '^\s*' + [regex]::Escape($Name) + '\s*='
        $replacement = ($Name + '=' + $Value)
        for ($i = 0; $i -lt $propLines.Count; $i++) {
            if ($propLines[$i] -match $pattern) {
                $propLines[$i] = $replacement
                return
            }
        }
        if ($propLines.Count -gt 0 -and $propLines[$propLines.Count - 1].Trim().Length -gt 0) {
            [void]$propLines.Add('')
        }
        [void]$propLines.Add($replacement)
    }
    Ensure-GradlePropertyLineUniv 'android.useAndroidX' 'true'
    Ensure-GradlePropertyLineUniv 'android.enableJetifier' 'true'
    $propLines | Set-Content -LiteralPath $gradlePropsPath -Encoding ASCII

    Ok "Universal Gradle project at $PROJECT_DIR_UNIV"
}

function Write-UniversalAndroidLocalProperties {
    $sdkRoot = if ($env:ANDROID_SDK_ROOT) { Normalize-DirPath $env:ANDROID_SDK_ROOT }
        elseif ($env:ANDROID_HOME) { Normalize-DirPath $env:ANDROID_HOME }
        else { $LOCAL_STUDIO_SDK }

    if (-not (Test-Path $sdkRoot)) {
        $ndkTry = $null
        if ($env:ANDROID_NDK_HOME) {
            $ndkTry = Normalize-DirPath $env:ANDROID_NDK_HOME
        } elseif (Test-Path $NDK_VERSIONS_DIR) {
            $pick = Get-ChildItem $NDK_VERSIONS_DIR -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending | Select-Object -First 1
            if ($pick) { $ndkTry = $pick.FullName }
        }
        if ($ndkTry -and (Test-Path $ndkTry)) {
            # e.g. .../Sdk/ndk/26.1.x -> .../Sdk
            $sdkFromNdk = Split-Path (Split-Path $ndkTry)
            if (Test-Path $sdkFromNdk) { $sdkRoot = Normalize-DirPath $sdkFromNdk }
        }
    }

    if (-not (Test-Path $sdkRoot)) {
        Fail ("Android SDK not found. Tried: ANDROID_SDK_ROOT, ANDROID_HOME, $LOCAL_STUDIO_SDK, and parent of NDK. Set ANDROID_HOME or install the SDK.")
    }

    $sdkUnix = $sdkRoot.Replace('\', '/')
    "sdk.dir=$sdkUnix" | Set-Content -Path (Join-Path $PROJECT_DIR_UNIV "local.properties") -Encoding ASCII
    Ok "local.properties sdk.dir=$sdkUnix"
}

# --- Parse requested ABIs ---
$requestedAbis = @($Abis -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ })
$activeConfigs = @()
foreach ($row in $UNIVERSAL_ABI_TABLE) {
    if ($requestedAbis -contains $row.Id) {
        $activeConfigs += $row
    }
}
if ($activeConfigs.Count -eq 0) {
    Fail "No matching ABIs. Use -Abis with one or more of: arm64-v8a, armeabi-v7a, x86, x86_64"
}
$abiIds = $activeConfigs | ForEach-Object { $_.Id }

Step "Universal build: ABIs = $($abiIds -join ', ')"

# ---------------------------------------------------------------------------
# Tools
# ---------------------------------------------------------------------------
Step "Checking tools"

Require "python" "Install Python from https://python.org"
$ninja = python -m ninja --version 2>&1
if ($LASTEXITCODE -ne 0) { Fail "python -m ninja not available. Run: pip install ninja" }
Ok "ninja $ninja"

$mesonCheck = python -m mesonbuild.mesonmain --version 2>&1
if ($LASTEXITCODE -ne 0) { Fail "Meson not available. Run: pip install meson" }
Ok "meson $mesonCheck"

if (-not (Test-Path $UBER_SIGNER)) {
    Step "Downloading Uber APK Signer"
    $uberUrl = "https://github.com/patrickfav/uber-apk-signer/releases/download/v1.3.0/uber-apk-signer-1.3.0.jar"
    Invoke-WebRequest -Uri $uberUrl -OutFile $UBER_SIGNER -UseBasicParsing
    Ok "Downloaded uber-apk-signer.jar"
}

Require "java" "Install JDK from https://adoptium.net"
$javaVerLine = (cmd /c "java -version 2>&1" | Select-Object -First 1)
Ok "java $javaVerLine"

if (-not $SkipNative) {
    if ($env:ANDROID_NDK_HOME) {
        $ndkHome = Normalize-DirPath $env:ANDROID_NDK_HOME
        if (-not (Test-Path $ndkHome)) { Fail "ANDROID_NDK_HOME is set but not found: $ndkHome" }
        Ok "NDK (ANDROID_NDK_HOME): $ndkHome"
    } else {
        if (-not (Test-Path $NDK_VERSIONS_DIR)) {
            Fail ("Android NDK folder not found at " + $NDK_VERSIONS_DIR + ". Install the NDK via Android Studio SDK Manager, or set ANDROID_SDK_ROOT / APOTRIS_NDK_VERSIONS.")
        }
        $ndkPick = Get-ApotrisNdkRoot
        Ok ("NDK: " + (Split-Path $ndkPick -Leaf) + " (universal build prefers 26.x over 29+ for bundled SDL)")
    }
}

# ---------------------------------------------------------------------------
# Keystore
# ---------------------------------------------------------------------------
if (-not (Test-Path $KEYSTORE_PATH)) {
    Step "Generating release keystore at $KEYSTORE_PATH"
    $keytoolCmd = Get-Command keytool -ErrorAction SilentlyContinue
    if (-not $keytoolCmd) { Fail "keytool not found - install JDK" }
    & keytool -genkeypair `
        -keystore $KEYSTORE_PATH `
        -alias $KEY_ALIAS `
        -keyalg RSA -keysize 2048 -validity 10000 `
        -storepass $KEYSTORE_PASS -keypass $KEY_PASS `
        -dname "CN=Apotris, O=Apotris, C=US" 2>&1 | Out-Null
    Ok "Keystore created: $KEYSTORE_PATH"
} else {
    Ok "Keystore exists: $KEYSTORE_PATH"
}

# ---------------------------------------------------------------------------
# Native: one Meson build dir per ABI
# ---------------------------------------------------------------------------
New-Item -ItemType Directory -Force $CROSS_OUT_DIR | Out-Null

if (-not $SkipNative) {
    $ndkRoot = Get-ApotrisNdkRoot
    $prebuiltHost = Get-ChildItem "$ndkRoot\toolchains\llvm\prebuilt" -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $prebuiltHost) { Fail "NDK prebuilt host folder missing under $ndkRoot" }

    $tilengineDir = Join-Path $ROOT "main\apotris\subprojects\Tilengine"
    $tilengineOverlay = Join-Path $ROOT "main\apotris\subprojects\packagefiles\Tilengine\meson.build"

    foreach ($cfg in $activeConfigs) {
        $crossPath = Join-Path $CROSS_OUT_DIR $cfg.CrossFile
        $buildDirAbs = Join-Path $UNIVERSAL_BASE $cfg.MesonSubdir
        $buildDirRel = "main/apotris/build-android-universal/$($cfg.MesonSubdir)"
        $jniAbi = Join-Path $PROJECT_DIR_UNIV "app\src\main\jniLibs\$($cfg.Id)"

        Step "Meson cross file for $($cfg.Id) -> $($cfg.CrossFile)"
        Write-UniversalMesonCrossIni $ndkRoot $crossPath $cfg.ClangTriple $cfg.CpuFamily $cfg.Cpu
        Ok "Cross file: $crossPath"

        $crossRel = "main/apotris/meson/universal-generated/$($cfg.CrossFile)"
        $mesonCommon = @(
            "--cross-file", $crossRel,
            "-Db_lto=false",
            "--buildtype=release",
            "-Dsdl2:use_hidapi=disabled"
        )

        Step "Meson setup + compile: $($cfg.Id) -> $buildDirRel"
        Push-Location $ROOT
        if ((Test-Path $tilengineDir) -and (Test-Path $tilengineOverlay)) {
            Copy-Item -LiteralPath $tilengineOverlay -Destination (Join-Path $tilengineDir "meson.build") -Force
        }
        $coreData = Join-Path $buildDirAbs "meson-private\coredata.dat"
        if (Test-Path $coreData) {
            python -m mesonbuild.mesonmain setup "main/apotris" $buildDirRel --reconfigure @mesonCommon
        } else {
            python -m mesonbuild.mesonmain setup "main/apotris" $buildDirRel @mesonCommon
        }
        if ($LASTEXITCODE -ne 0) { Pop-Location; Fail "meson setup failed for $($cfg.Id)" }

        $tilMesonDst = Join-Path $tilengineDir "meson.build"
        if ((Test-Path $tilMesonDst) -and (Test-Path $tilengineOverlay)) {
            $tilContent = Get-Content -LiteralPath $tilMesonDst -Raw
            if ($tilContent -notmatch 'link_whole\s*:') {
                Copy-Item -LiteralPath $tilengineOverlay -Destination $tilMesonDst -Force
                python -m mesonbuild.mesonmain setup "main/apotris" $buildDirRel --reconfigure @mesonCommon
                if ($LASTEXITCODE -ne 0) { Pop-Location; Fail "meson reconfigure after Tilengine overlay failed for $($cfg.Id)" }
            }
        }

        python -m mesonbuild.mesonmain compile -C $buildDirRel main
        if ($LASTEXITCODE -ne 0) { Pop-Location; Fail "meson compile failed for $($cfg.Id)" }
        Pop-Location
        Ok "libmain.so built for $($cfg.Id)"

        Step "jniLibs/$($cfg.Id): copy libmain.so + libc++_shared.so"
        New-Item -ItemType Directory -Force $jniAbi | Out-Null
        $so = Join-Path $buildDirAbs "libmain.so"
        if (-not (Test-Path $so)) { Fail "libmain.so missing at $so" }
        Copy-Item $so (Join-Path $jniAbi "libmain.so") -Force

        $cxxShared = Join-Path $prebuiltHost.FullName "sysroot\usr\lib\$($cfg.LibSubdir)\libc++_shared.so"
        if (Test-Path $cxxShared) {
            Copy-Item $cxxShared $jniAbi -Force
            Ok "Copied libc++_shared.so"
        } else {
            Write-Host "    WARN: libc++_shared.so not at $cxxShared" -ForegroundColor Yellow
        }

        $strip = Get-ChildItem -Recurse "$ndkRoot\toolchains\llvm\prebuilt" -Filter "llvm-strip.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        $mainSo = Join-Path $jniAbi "libmain.so"
        if ($strip -and (Test-Path $mainSo)) {
            & $strip.FullName --strip-unneeded $mainSo
            $sz = [math]::Round((Get-Item $mainSo).Length / 1MB, 1)
            Ok "Stripped libmain.so (${sz} MB)"
        }
    }
} else {
    Write-Host "    Skipping native build (-SkipNative); expecting jniLibs already populated" -ForegroundColor Yellow
}

# ---------------------------------------------------------------------------
# Gradle + sign
# ---------------------------------------------------------------------------
Ensure-UniversalAndroidGradleProject -AbiList $abiIds
Write-UniversalAndroidLocalProperties

Step "Building universal release APK with Gradle"
$keystoreAbsPath = (Resolve-Path $KEYSTORE_PATH).Path
Push-Location $PROJECT_DIR_UNIV

$gradlewBat = Join-Path $PROJECT_DIR_UNIV "gradlew.bat"
if (-not (Test-Path $gradlewBat)) {
    Pop-Location
    Fail "Gradle wrapper missing at $PROJECT_DIR_UNIV"
}

$gradleArgs = @(
    "assembleRelease",
    "-Pandroid.injected.signing.store.file=$keystoreAbsPath",
    "-Pandroid.injected.signing.store.password=$KEYSTORE_PASS",
    "-Pandroid.injected.signing.key.alias=$KEY_ALIAS",
    "-Pandroid.injected.signing.key.password=$KEY_PASS"
)

& $gradlewBat @gradleArgs
if ($LASTEXITCODE -ne 0) { Pop-Location; Fail "Gradle build failed" }
Pop-Location
Ok "Release APK built"

Step "Signing with Uber APK Signer"

$unsignedApk = Get-ChildItem "$PROJECT_DIR_UNIV\app\build\outputs\apk\release" -Filter "*.apk" |
    Select-Object -First 1
if (-not $unsignedApk) { Fail "No APK found in universal release output directory" }

New-Item -ItemType Directory -Force $SIGNED_STAGING | Out-Null

java -jar $UBER_SIGNER `
    --apks $unsignedApk.FullName `
    --ks $KEYSTORE_PATH `
    --ksPass $KEYSTORE_PASS `
    --ksAlias $KEY_ALIAS `
    --ksKeyPass $KEY_PASS `
    --out $SIGNED_STAGING `
    --allowResign

if ($LASTEXITCODE -ne 0) { Fail "Uber APK Signer failed" }

$signedOutput = Get-ChildItem $SIGNED_STAGING -Filter "*.apk" |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $signedOutput) { Fail "Signed APK not found in $SIGNED_STAGING" }

Move-Item $signedOutput.FullName $APK_SIGNED -Force
Remove-Item $SIGNED_STAGING -Recurse -Force

Ok "Signed universal APK: $APK_SIGNED"

if ($Install) {
    Step "Installing to device"
    Require "adb" "Install Android SDK Platform Tools"
    adb install -r $APK_SIGNED
    if ($LASTEXITCODE -ne 0) { Fail "adb install failed" }
    Ok "Installed successfully"
}

Write-Host "`n==> Universal build complete!" -ForegroundColor Green
Write-Host "    Signed APK: $APK_SIGNED" -ForegroundColor White
if (Test-Path $APK_SIGNED) {
    $size = [math]::Round((Get-Item $APK_SIGNED).Length / 1MB, 1)
    Write-Host "    Size: ${size} MB" -ForegroundColor White
}
