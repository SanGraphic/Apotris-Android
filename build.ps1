#!/usr/bin/env pwsh
# =============================================================================
# Apotris Android — Full Build Script
# Compiles libmain.so (C++) + builds signed release APK via Uber APK Signer
#
# Usage:
#   .\build.ps1
#   .\build.ps1 -SkipNative    # skip C++ rebuild, only rebuild APK
#   .\build.ps1 -Install       # install to connected device after signing
#
# First run: set KEYSTORE_PATH, KEYSTORE_PASS, KEY_ALIAS, KEY_PASS below,
# or set them as environment variables before running.
# =============================================================================

param(
    [switch]$SkipNative,
    [switch]$Install
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ---------------------------------------------------------------------------
# Configuration — override via env vars or edit here
# ---------------------------------------------------------------------------
$KEYSTORE_PATH = if ($env:APOTRIS_KEYSTORE) { $env:APOTRIS_KEYSTORE } else { "apotris-release.jks" }
$KEYSTORE_PASS = if ($env:APOTRIS_KEYSTORE_PASS) { $env:APOTRIS_KEYSTORE_PASS } else { "changeme" }
$KEY_ALIAS     = if ($env:APOTRIS_KEY_ALIAS) { $env:APOTRIS_KEY_ALIAS } else { "apotris" }
$KEY_PASS      = if ($env:APOTRIS_KEY_PASS) { $env:APOTRIS_KEY_PASS } else { "changeme" }

$ROOT          = $PSScriptRoot
$BUILD_DIR     = "$ROOT\main\apotris\build-android"
$PROJECT_DIR   = "$ROOT\main\apotris\build-android\project"
$JNILIBS_DIR   = "$PROJECT_DIR\app\src\main\jniLibs\arm64-v8a"
$APK_UNSIGNED  = "$PROJECT_DIR\app\build\outputs\apk\release\app-release-unsigned.apk"
$APK_SIGNED    = "$ROOT\apotris-arm64-v8a-$(Get-Date -Format 'yyyy-MM-dd').apk"
$UBER_SIGNER   = "$ROOT\uber-apk-signer.jar"

# NDK versions folder for llvm-strip (contains 26.1.xxx, 29.xxx, etc.)
# Prefer Android Studio default: %LOCALAPPDATA%\Android\Sdk\ndk — same as C:\Users\<you>\AppData\Local\Android\Sdk\ndk
# Override: APOTRIS_NDK_VERSIONS; exact NDK root (one version): ANDROID_NDK_HOME
# If AppData NDK is missing, fall back to ANDROID_SDK_ROOT\ndk or ANDROID_HOME\ndk
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

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
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
    $pick = Get-ChildItem $NDK_VERSIONS_DIR -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending | Select-Object -First 1
    if (-not $pick) { Fail "No NDK versions under $NDK_VERSIONS_DIR" }
    return $pick.FullName
}

function Get-ApotrisNdkPrebuiltHostName([string]$NdkRoot) {
    $prebuiltRoot = Join-Path $NdkRoot "toolchains\llvm\prebuilt"
    if (-not (Test-Path $prebuiltRoot)) { Fail "Invalid NDK (no toolchains/llvm/prebuilt): $NdkRoot" }
    $d = Get-ChildItem $prebuiltRoot -Directory | Select-Object -First 1
    if (-not $d) { Fail "NDK prebuilt host folder empty: $prebuiltRoot" }
    return $d.Name
}

function Write-ApotrisMesonCrossIni([string]$NdkRoot, [string]$OutPath) {
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
        "c = toolchain + '/aarch64-linux-android' + api + '-clang.cmd'",
        "cpp = toolchain + '/aarch64-linux-android' + api + '-clang++.cmd'",
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
        "cpu_family = 'aarch64'",
        "cpu = 'aarch64'",
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

function Ensure-AndroidGradleProject {
    $sdl = Join-Path $ROOT "main\apotris\subprojects\SDL2-2.28.5\android-project"
    if (-not (Test-Path (Join-Path $sdl "gradlew.bat"))) {
        Fail "SDL2 android-project missing at $sdl (run meson setup once so the SDL2 subproject is present)."
    }
    $appDir = Join-Path $PROJECT_DIR "app"
    $mainSrc = Join-Path $appDir "src\main"
    $manifestPath = Join-Path $mainSrc "AndroidManifest.xml"

    function Copy-SdlAndroidSources {
        New-Item -ItemType Directory -Force $mainSrc | Out-Null
        Copy-Item (Join-Path $sdl "app\src\main\AndroidManifest.xml") $mainSrc -Force
        Copy-Item (Join-Path $sdl "app\src\main\java") (Join-Path $mainSrc "java") -Recurse -Force
        Copy-Item (Join-Path $sdl "app\src\main\res") (Join-Path $mainSrc "res") -Recurse -Force
    }

    if (-not (Test-Path (Join-Path $PROJECT_DIR "gradlew.bat"))) {
        Step "Bootstrapping Gradle project from SDL2 (prebuilt libmain.so, arm64-v8a only)"
        New-Item -ItemType Directory -Force $PROJECT_DIR | Out-Null
        Copy-Item (Join-Path $sdl "gradlew.bat") $PROJECT_DIR -Force
        Copy-Item (Join-Path $sdl "gradlew") $PROJECT_DIR -Force
        Copy-Item (Join-Path $sdl "gradle") (Join-Path $PROJECT_DIR "gradle") -Recurse -Force
        Copy-Item (Join-Path $sdl "build.gradle") $PROJECT_DIR -Force
        Copy-Item (Join-Path $sdl "settings.gradle") $PROJECT_DIR -Force
        New-Item -ItemType Directory -Force $appDir | Out-Null
        Copy-Item (Join-Path $sdl "app\proguard-rules.pro") $appDir -Force
        Copy-SdlAndroidSources
        $nestedBad = Join-Path $appDir "src\src"
        if (Test-Path $nestedBad) { Remove-Item $nestedBad -Recurse -Force }
    } elseif (-not (Test-Path $manifestPath)) {
        Step "Repairing app/src/main (manifest or SDL Java missing)"
        Copy-SdlAndroidSources
        $nestedBad = Join-Path $appDir "src\src"
        if (Test-Path $nestedBad) { Remove-Item $nestedBad -Recurse -Force }
    }

    Step "Applying Apotris android-port (launcher name, icon sync, static SDL / libmain only)"
    Apply-ApotrisAndroidPort $mainSrc
    Sync-ApotrisGameAssets $mainSrc
    Sync-ApotrisLauncherIcons $mainSrc
    Ensure-ApotrisRootGradle $PROJECT_DIR

    # Line array: avoids PS 5.1 parsing issues with @'...'@ here-strings next to Gradle single-quoted strings.
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
        '        versionName ''4.1.1b''',
        '        ndk {',
        '            abiFilters ''arm64-v8a''',
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

    $gradlePropsPath = Join-Path $PROJECT_DIR "gradle.properties"
    $propLines = New-Object System.Collections.ArrayList
    if (Test-Path $gradlePropsPath) {
        Get-Content -LiteralPath $gradlePropsPath | ForEach-Object { [void]$propLines.Add($_) }
    }
    function Ensure-GradlePropertyLine([string]$Name, [string]$Value) {
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
    Ensure-GradlePropertyLine 'android.useAndroidX' 'true'
    Ensure-GradlePropertyLine 'android.enableJetifier' 'true'
    $propLines | Set-Content -LiteralPath $gradlePropsPath -Encoding ASCII

    Ok "Gradle project at $PROJECT_DIR"
}

function Write-AndroidLocalProperties {
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
            $sdkFromNdk = Split-Path (Split-Path $ndkTry)
            if (Test-Path $sdkFromNdk) { $sdkRoot = Normalize-DirPath $sdkFromNdk }
        }
    }

    if (-not (Test-Path $sdkRoot)) {
        Fail ("Android SDK not found. Set ANDROID_HOME / ANDROID_SDK_ROOT, or install SDK; tried parent of NDK under $NDK_VERSIONS_DIR.")
    }

    $sdkUnix = $sdkRoot.Replace('\', '/')
    "sdk.dir=$sdkUnix" | Set-Content -Path (Join-Path $PROJECT_DIR "local.properties") -Encoding ASCII
    Ok "local.properties sdk.dir=$sdkUnix"
}

# ---------------------------------------------------------------------------
# 1. Verify tools
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
# java -version writes to stderr; avoid PS 5.1 treating it as a terminating error
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
        $ndkDirs = @(Get-ChildItem $NDK_VERSIONS_DIR -Directory -ErrorAction SilentlyContinue)
        if ($ndkDirs.Count -eq 0) {
            Fail ("No NDK version directories under " + $NDK_VERSIONS_DIR + ". Install an NDK release from the SDK Manager.")
        }
        $picked = ($ndkDirs | Sort-Object Name -Descending | Select-Object -First 1).FullName
        Ok ("NDK root: " + $NDK_VERSIONS_DIR + " (using " + (Split-Path $picked -Leaf) + ")")
    }
}

# ---------------------------------------------------------------------------
# 2. Generate keystore if it doesn't exist
# ---------------------------------------------------------------------------
if (-not (Test-Path $KEYSTORE_PATH)) {
    Step "Generating release keystore at $KEYSTORE_PATH"
    $keytoolCmd = Get-Command keytool -ErrorAction SilentlyContinue
    $keytool = if ($keytoolCmd) { $keytoolCmd.Source } else { $null }
    if (-not $keytool) { Fail "keytool not found - install JDK" }
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
# 3. Compile C++ native library (libmain.so)
# ---------------------------------------------------------------------------
if (-not $SkipNative) {
    $ndkRoot = Get-ApotrisNdkRoot
    Step "Writing Meson cross file for NDK: $(Split-Path $ndkRoot -Leaf)"
    $crossIni = Join-Path $ROOT "main\apotris\meson\android-arm64-auto.ini"
    Write-ApotrisMesonCrossIni $ndkRoot $crossIni
    Ok "Cross file: $crossIni"

    Step "Meson setup (uses NDK under $NDK_VERSIONS_DIR)"
    Push-Location $ROOT
    $crossRel = "main/apotris/meson/android-arm64-auto.ini"
    $mesonCommon = @(
        "--cross-file", $crossRel,
        "-Db_lto=false",
        "--buildtype=release",
        # Feature option on SDL2 wrap; ensures SDL_hidapi PLATFORM_* are not required on Android+lld.
        "-Dsdl2:use_hidapi=disabled"
    )
    # Meson skips wrap patch_directory when subprojects/Tilengine already exists (e.g. cached tree).
    # Force our overlay (link_whole + pic) so libmain.so links all TLN_* from static Tilengine.
    $tilengineDir = Join-Path $ROOT "main\apotris\subprojects\Tilengine"
    $tilengineOverlay = Join-Path $ROOT "main\apotris\subprojects\packagefiles\Tilengine\meson.build"
    if ((Test-Path $tilengineDir) -and (Test-Path $tilengineOverlay)) {
        Copy-Item -LiteralPath $tilengineOverlay -Destination (Join-Path $tilengineDir "meson.build") -Force
        Ok "Applied Tilengine meson.build overlay (patch_directory skip workaround)"
    }
    # Meson expects: setup [options] <sourcedir> <builddir> (--reconfigure updates existing builddir)
    if (Test-Path "main/apotris/build-android/meson-private/coredata.dat") {
        python -m mesonbuild.mesonmain setup "main/apotris" "main/apotris/build-android" --reconfigure @mesonCommon
    } else {
        python -m mesonbuild.mesonmain setup "main/apotris" "main/apotris/build-android" @mesonCommon
    }
    if ($LASTEXITCODE -ne 0) { Pop-Location; Fail "meson setup failed" }

    # If Meson cloned Tilengine but skipped patch_directory, upstream meson.build has no link_whole (lld drops TLN_*).
    $tilMesonDst = Join-Path $ROOT "main\apotris\subprojects\Tilengine\meson.build"
    if ((Test-Path $tilMesonDst) -and (Test-Path $tilengineOverlay)) {
        $tilContent = Get-Content -LiteralPath $tilMesonDst -Raw
        if ($tilContent -notmatch 'link_whole\s*:') {
            Copy-Item -LiteralPath $tilengineOverlay -Destination $tilMesonDst -Force
            Ok "Tilengine overlay missing link_whole; applied and reconfiguring"
            python -m mesonbuild.mesonmain setup "main/apotris" "main/apotris/build-android" --reconfigure @mesonCommon
            if ($LASTEXITCODE -ne 0) { Pop-Location; Fail "meson reconfigure after Tilengine overlay failed" }
        }
    }

    Step "Compiling libmain.so (meson compile activates MSVC for host tools like padbin)"
    # Raw `ninja` fails on Windows cross-builds: build.ninja invokes `cl`/`link` for native
    # executables (e.g. padbin) and those are not on PATH outside a VS environment.
    python -m mesonbuild.mesonmain compile -C main/apotris/build-android main
    if ($LASTEXITCODE -ne 0) { Pop-Location; Fail "native library build failed" }
    Pop-Location
    Ok "libmain.so compiled"

    Step "Copying libmain.so to jniLibs"
    New-Item -ItemType Directory -Force $JNILIBS_DIR | Out-Null
    Copy-Item "$BUILD_DIR\libmain.so" "$JNILIBS_DIR\libmain.so" -Force
    Ok "Copied libmain.so (SDL2 + game; single .so - same model as working com.example.apotris APK)"

    Step "Copying libc++_shared.so from NDK (if present)"
    $prebuiltHost = Get-ChildItem "$ndkRoot\toolchains\llvm\prebuilt" -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($prebuiltHost) {
        $cxxShared = Join-Path $prebuiltHost.FullName "sysroot\usr\lib\aarch64-linux-android\libc++_shared.so"
        if (Test-Path $cxxShared) {
            Copy-Item $cxxShared $JNILIBS_DIR -Force
            Ok "Copied libc++_shared.so (C++ runtime; packaged next to libmain.so like release APKs)"
        } else {
            Write-Host "    No libc++_shared.so at expected NDK path, skipping" -ForegroundColor Yellow
        }
    }

    Step "Stripping debug symbols from libmain.so"
    $ndkRootForStrip = $null
    if ($env:ANDROID_NDK_HOME) {
        $ndkRootForStrip = Normalize-DirPath $env:ANDROID_NDK_HOME
    } else {
        $ndkPick = Get-ChildItem $NDK_VERSIONS_DIR -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending | Select-Object -First 1
        if ($ndkPick) { $ndkRootForStrip = $ndkPick.FullName }
    }
    $strip = if ($ndkRootForStrip) {
        Get-ChildItem -Recurse "$ndkRootForStrip\toolchains\llvm\prebuilt" -Filter "llvm-strip.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    } else { $null }
    if ($strip -and (Test-Path "$JNILIBS_DIR\libmain.so")) {
        & $strip.FullName --strip-unneeded "$JNILIBS_DIR\libmain.so"
        $sz = [math]::Round((Get-Item "$JNILIBS_DIR\libmain.so").Length / 1MB, 1)
        Ok "Stripped libmain.so (${sz} MB)"
    } elseif (-not $strip) {
        Write-Host "    llvm-strip not found, skipping" -ForegroundColor Yellow
    }
} else {
    Write-Host "    Skipping native build (-SkipNative)" -ForegroundColor Yellow
}

# ---------------------------------------------------------------------------
# 4. Build release APK with Gradle
# ---------------------------------------------------------------------------
Ensure-AndroidGradleProject
Write-AndroidLocalProperties

Step "Building release APK with Gradle"
$keystoreAbsPath = (Resolve-Path $KEYSTORE_PATH).Path
Push-Location $PROJECT_DIR

$gradlewBat = Join-Path $PROJECT_DIR "gradlew.bat"
$gradlewUnix = Join-Path $PROJECT_DIR "gradlew"
if (Test-Path $gradlewBat) {
    $gradleLauncher = $gradlewBat
} elseif (Test-Path $gradlewUnix) {
    $gradleLauncher = $gradlewUnix
} else {
    Pop-Location
    Fail ("Gradle wrapper missing at " + $PROJECT_DIR)
}

# Pass signing config via command line so we don't store credentials in gradle files
$gradleArgs = @(
    "assembleRelease",
    "-Pandroid.injected.signing.store.file=$keystoreAbsPath",
    "-Pandroid.injected.signing.store.password=$KEYSTORE_PASS",
    "-Pandroid.injected.signing.key.alias=$KEY_ALIAS",
    "-Pandroid.injected.signing.key.password=$KEY_PASS"
)

& $gradleLauncher @gradleArgs
if ($LASTEXITCODE -ne 0) { Pop-Location; Fail "Gradle build failed" }
Pop-Location
Ok "Release APK built"

# ---------------------------------------------------------------------------
# 5. Sign with Uber APK Signer (v2 + v3 signatures)
# ---------------------------------------------------------------------------
Step "Signing with Uber APK Signer"

$unsignedApk = Get-ChildItem "$PROJECT_DIR\app\build\outputs\apk\release" -Filter "*.apk" |
    Select-Object -First 1
if (-not $unsignedApk) { Fail "No APK found in release output directory" }

$signOutDir = "$ROOT\signed_output"
New-Item -ItemType Directory -Force $signOutDir | Out-Null

java -jar $UBER_SIGNER `
    --apks $unsignedApk.FullName `
    --ks $KEYSTORE_PATH `
    --ksPass $KEYSTORE_PASS `
    --ksAlias $KEY_ALIAS `
    --ksKeyPass $KEY_PASS `
    --out $signOutDir `
    --allowResign

if ($LASTEXITCODE -ne 0) { Fail "Uber APK Signer failed" }

$signedOutput = Get-ChildItem $signOutDir -Filter "*.apk" |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $signedOutput) { Fail "Signed APK not found in $signOutDir" }

Move-Item $signedOutput.FullName $APK_SIGNED -Force
Remove-Item $signOutDir -Recurse -Force

Ok "Signed APK: $APK_SIGNED"

# ---------------------------------------------------------------------------
# 6. Optionally install to device
# ---------------------------------------------------------------------------
if ($Install) {
    Step "Installing to device"
    Require "adb" "Install Android SDK Platform Tools"
    adb install -r $APK_SIGNED
    if ($LASTEXITCODE -ne 0) { Fail "adb install failed" }
    Ok "Installed successfully"
}

# ---------------------------------------------------------------------------
# Done
# ---------------------------------------------------------------------------
Write-Host "`n==> Build complete!" -ForegroundColor Green
Write-Host "    Signed APK: $APK_SIGNED" -ForegroundColor White
if (Test-Path $APK_SIGNED) {
    $size = [math]::Round((Get-Item $APK_SIGNED).Length / 1MB, 1)
    Write-Host "    Size: ${size} MB" -ForegroundColor White
}
