#!/usr/bin/env pwsh
# Clone gitea-hosted Meson subprojects that are not in the wrap DB and were never committed
# (listed in main/apotris/.gitmodules). Safe to re-run: skips if meson.build already exists.
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$subprojects = Join-Path $repoRoot "main\apotris\subprojects"

$repos = @(
    @{ Name = "general-tools"; Url = "https://gitea.com/Apotris/general-tools.git"; Branch = "main" }
)

foreach ($r in $repos) {
    $dest = Join-Path $subprojects $r.Name
    $marker = Join-Path $dest "meson.build"
    if (Test-Path $marker) {
        Write-Host "OK: $($r.Name) already present"
        continue
    }
    Write-Host "Cloning $($r.Name) (branch $($r.Branch)) ..."
    if (Test-Path $dest) {
        Remove-Item $dest -Recurse -Force -ErrorAction SilentlyContinue
    }
    git clone --depth 1 --branch $r.Branch $r.Url $dest
    if ($LASTEXITCODE -ne 0) { throw "git clone failed: $($r.Name)" }
}

Write-Host "Apotris subproject sync done."
