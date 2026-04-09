<#
.SYNOPSIS
  Captures per-app rendering stats (gfxinfo) and battery model lines for the app's UID, plus optional Perfetto trace.

.DESCRIPTION
  Android does not expose a single public watts-for-package API on retail builds. This script uses:
  - dumpsys gfxinfo: frame counts, jank, percentile frame times (FPS-related).
  - dumpsys batterystats for a package: Estimated power use (mAh) per UID; the script resolves your package to u0aXXXX and prints that line.

.PARAMETER Package
  Application id (default: com.apotris.android).

.PARAMETER WaitSeconds
  After resetting gfxinfo counters, wait this long while you play in the foreground, then capture.

.PARAMETER OutDir
  Directory for timestamped .txt/.pb files (default: ./android-perf-snapshots next to this script).

.PARAMETER Perfetto
  If set, pushes perfetto-game.textproto, records a trace on device, and adb pulls the .pb file.
  May fail on some devices if android.power is unsupported; edit the textproto to drop power.

.PARAMETER NoGfxReset
  Skip "dumpsys gfxinfo reset" (use if you want cumulative stats since last reset).

.EXAMPLE
  .\android-perf-snapshot.ps1 -WaitSeconds 120 -Perfetto
#>
[CmdletBinding()]
param(
  [string] $Package = "com.apotris.android",
  [int] $WaitSeconds = 60,
  [string] $OutDir = "",
  [switch] $Perfetto,
  [switch] $NoGfxReset
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path

function Repair-AdbBinaryCrlfInsertion {
  param([byte[]] $Data)
  $ms = New-Object System.IO.MemoryStream
  for ($i = 0; $i -lt $Data.Length; $i++) {
    if ($Data[$i] -eq 0x0D -and ($i + 1) -lt $Data.Length -and $Data[$i + 1] -eq 0x0A) {
      [void]$ms.WriteByte(0x0A)
      $i++
      continue
    }
    [void]$ms.WriteByte($Data[$i])
  }
  return $ms.ToArray()
}
if (-not $OutDir) {
  $OutDir = [System.IO.Path]::GetFullPath((Join-Path $here "..\..\android-perf-snapshots"))
}
$null = New-Item -ItemType Directory -Force -Path $OutDir
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$base = Join-Path $OutDir "${Package}_${stamp}"

function Invoke-Adb {
  param([string] $CmdLine)
  $tokens = $CmdLine.Split(" ", [System.StringSplitOptions]::RemoveEmptyEntries)
  $o = & adb @tokens 2>&1
  $code = $LASTEXITCODE
  return @{ Out = $o; Code = $code }
}

$r = Invoke-Adb "get-state"
if ($r.Code -ne 0 -or ($r.Out -notmatch "device")) {
  Write-Error "No adb device in 'device' state. Connect a device or start an emulator."
}

# --- Resolve app UID -> batterystats label (e.g. 10393 -> u0a393) ---
$pkgDump = (& adb shell "dumpsys package $Package" 2>&1) -join "`n"
if ($pkgDump -match "Unable to find package") {
  Write-Error "Package not installed: $Package"
}
$uidNum = 0
$listOut = (& adb shell "cmd package list packages -U" 2>&1) | ForEach-Object { $_.ToString() }
$pkgEsc = [regex]::Escape($Package)
foreach ($line in $listOut) {
  if ($line -match "^package:$pkgEsc\s+uid:(\d+)\s*$") {
    $uidNum = [int]$Matches[1]
    break
  }
}
if ($uidNum -eq 0) {
  $m = [regex]::Match($pkgDump, "Package \[$pkgEsc\][\s\S]{0,800}?appId=(\d+)", [System.Text.RegularExpressions.RegexOptions]::Singleline)
  if ($m.Success) { $uidNum = [int]$m.Groups[1].Value }
}
if ($uidNum -eq 0) {
  $m = [regex]::Match($pkgDump, "userId=(\d+)")
  if ($m.Success) { $uidNum = [int]$m.Groups[1].Value }
}
if ($uidNum -eq 0) {
  Write-Error "Could not resolve UID for $Package (tried package list -U, dumpsys appId/userId)."
}
if ($uidNum -lt 10000) {
  $uidLabel = "$uidNum"
} else {
  $uidLabel = "u0a$($uidNum - 10000)"
}

Write-Host "Package: $Package  UID: $uidNum  batterystats label: $uidLabel"

# --- gfxinfo reset + wait + capture ---
if (-not $NoGfxReset) {
  $null = & adb shell "dumpsys gfxinfo $Package reset" 2>&1
  Write-Host "gfxinfo counters reset. Play with $Package in foreground for $WaitSeconds s..."
} else {
  Write-Host "Skipping gfxinfo reset. Capturing after $WaitSeconds s wait..."
}
Start-Sleep -Seconds $WaitSeconds

$gfxPath = "$base-gfxinfo.txt"
& adb shell "dumpsys gfxinfo $Package" 2>&1 | Out-File -FilePath $gfxPath -Encoding utf8
Write-Host "Wrote $gfxPath"

# Summary lines from gfxinfo
$gfx = Get-Content -Raw $gfxPath
foreach ($pat in @(
    "Total frames rendered:\s*\d+",
    "Janky frames:\s*[^\r\n]+",
    "50th percentile:\s*[^\r\n]+",
    "90th percentile:\s*[^\r\n]+",
    "99th percentile:\s*[^\r\n]+",
    "HWC layers:\s*[^\r\n]+",
    "Profile data in ms:"
  )) {
  $mm = [regex]::Match($gfx, $pat, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
  if ($mm.Success) { Write-Host ("  " + $mm.Value) }
}

# --- batterystats: full dump + UID power line ---
$bsPath = "$base-batterystats.txt"
& adb shell "dumpsys batterystats $Package" 2>&1 | Out-File -FilePath $bsPath -Encoding utf8
Write-Host "Wrote $bsPath"

$bs = Get-Content -Raw $bsPath
$ep = [regex]::Match($bs, "Estimated power use \(mAh\):[\s\S]*?(?=\r?\n\r?\n|\z)")
if ($ep.Success) {
  Write-Host "--- Estimated power (mAh) section (all UIDs; find your UID line) ---"
  $lines = $ep.Value -split "`n" | Select-Object -First 25
  $lines | ForEach-Object { Write-Host $_ }
}
$uidLine = [regex]::Match($bs, "UID ${uidLabel}:[^\r\n]+")
if ($uidLine.Success) {
  Write-Host "--- Your app (model estimate) ---"
  Write-Host $uidLine.Value
} else {
  Write-Host "(No line 'UID $($uidLabel):' in this batterystats dump; try full bugreport + Battery Historian.)"
}

# meminfo snapshot
$memPath = "$base-meminfo.txt"
& adb shell "dumpsys meminfo $Package" 2>&1 | Out-File -FilePath $memPath -Encoding utf8
Write-Host "Wrote $memPath"

# --- Optional Perfetto ---
if ($Perfetto) {
  $cfg = Join-Path $here "perfetto-game.textproto"
  if (-not (Test-Path $cfg)) {
    Write-Warning "Missing $cfg ; skip Perfetto."
  } else {
    Write-Host "Recording Perfetto trace (adb shell + CRLF repair; see duration_ms in textproto)..."
    $cfgFull = (Resolve-Path -LiteralPath $cfg).Path
    $localPb = "$base-trace.pb"
    # adb shell on Windows often turns LF into CRLF on the host side, which corrupts protobuf on stdout.
    # adb exec-out does not reliably accept config stdin from the host; use shell + strip 0D 0A -> 0A.
    $cfgBytes = [System.IO.File]::ReadAllBytes($cfgFull)
    if ($cfgBytes.Length -ge 3 -and $cfgBytes[0] -eq 0xEF -and $cfgBytes[1] -eq 0xBB -and $cfgBytes[2] -eq 0xBF) {
      $cfgBytes = $cfgBytes[3..($cfgBytes.Length - 1)]
    }
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = "adb"
    $psi.Arguments = "shell perfetto -c - -o - --txt"
    $psi.RedirectStandardInput = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $pc = New-Object System.Diagnostics.Process
    $pc.StartInfo = $psi
    $null = $pc.Start()
    $rawMs = New-Object System.IO.MemoryStream
    $copyTask = $pc.StandardOutput.BaseStream.CopyToAsync($rawMs)
    $pc.StandardInput.BaseStream.Write($cfgBytes, 0, $cfgBytes.Length)
    $pc.StandardInput.Flush()
    $pc.StandardInput.Dispose()
    $copyTask.Wait() | Out-Null
    $pc.WaitForExit()
    $pr = $pc.StandardError.ReadToEnd()
    $fixed = Repair-AdbBinaryCrlfInsertion -Data $rawMs.ToArray()
    [System.IO.File]::WriteAllBytes($localPb, $fixed)
    $ok = (Test-Path $localPb) -and ((Get-Item $localPb).Length -ge 256)
    if (-not $ok) {
      if (Test-Path $localPb) { Remove-Item -LiteralPath $localPb -Force -ErrorAction SilentlyContinue }
      Write-Warning "perfetto failed or trace too small (exit $($pc.ExitCode)). Stderr:"
      Write-Warning $pr
      Write-Warning "If config parse failed, remove the android.power block from perfetto-game.textproto."
    } else {
      if ($pc.ExitCode -ne 0) {
        Write-Warning "perfetto exited $($pc.ExitCode) but a trace was captured. Stderr: $pr"
      }
      Write-Host "Wrote $localPb - open in https://ui.perfetto.dev/"
    }
  }
}

Write-Host "Done."
