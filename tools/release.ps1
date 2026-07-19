<#
  GodotLuau - release.ps1
  Compila debug+release, fija la version, regenera GodotLuau.zip y publica en GitHub.

  Uso:
    ./tools/release.ps1 -Version v1.7.0     # fija esa version y publica
    ./tools/release.ps1                      # usa la version actual del archivo Version
    ./tools/release.ps1 -Version v1.8.0 -NoPush   # compila y commitea, pero no hace push

  Nota: scons.exe esta bloqueado por Windows Smart App Control, por eso se
  invoca SCons a traves de python (python -m SCons).
#>
param(
    [string]$Version,
    [string]$Message,   # mensaje del commit (por defecto = la versión)
    [switch]$NoPush
)

$ErrorActionPreference = "Stop"

# Raiz del repo = carpeta padre de este script (tools/)
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

# Toolchain MSYS2/MinGW
$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;$env:PATH"

# Version: parametro, o la del archivo Version si no se pasa
if (-not $Version) {
    $Version = (Get-Content (Join-Path $Root "Version") -TotalCount 1).Trim()
}
Write-Host "==> GodotLuau release $Version" -ForegroundColor Cyan

# 1. Compilar (debug + release)
Write-Host "==> Compilando debug..." -ForegroundColor Cyan
python -m SCons platform=windows use_mingw=yes target=template_debug -j8
if ($LASTEXITCODE -ne 0) { throw "Fallo el build debug" }

Write-Host "==> Compilando release..." -ForegroundColor Cyan
python -m SCons platform=windows use_mingw=yes target=template_release -j8
if ($LASTEXITCODE -ne 0) { throw "Fallo el build release" }

# 2. Fijar la version en Version y en plugin.cfg (sin BOM, ASCII)
[System.IO.File]::WriteAllText((Join-Path $Root "Version"), "$Version`n", [System.Text.Encoding]::ASCII)
$cfgPath = Join-Path $Root "addons/GodotLuauUpdater/plugin.cfg"
$verNum  = $Version.TrimStart('v')
$cfg     = Get-Content $cfgPath -Raw
$cfg     = [regex]::Replace($cfg, 'version=".*?"', "version=`"$verNum`"")
[System.IO.File]::WriteAllText($cfgPath, $cfg, [System.Text.Encoding]::ASCII)

# 3. Regenerar el paquete del auto-updater (GodotLuau.zip + .sha256)
Write-Host "==> Regenerando GodotLuau.zip..." -ForegroundColor Cyan
python tools/generar_release.py
if ($LASTEXITCODE -ne 0) { throw "Fallo generar_release.py" }

# 4. Commit + push
git add -A
$pending = git status --porcelain
if ([string]::IsNullOrWhiteSpace($pending)) {
    Write-Host "==> Nada que commitear: el repositorio ya esta al dia." -ForegroundColor Yellow
    return
}

if (-not $Message) { $Message = $Version }
git commit -m $Message
if ($LASTEXITCODE -ne 0) { throw "Fallo el commit" }

if ($NoPush) {
    Write-Host "==> Commit hecho ($Version). Push omitido por -NoPush." -ForegroundColor Green
} else {
    git push
    if ($LASTEXITCODE -ne 0) { throw "Fallo el push" }
    Write-Host "==> Publicado $Version en GitHub." -ForegroundColor Green
}
