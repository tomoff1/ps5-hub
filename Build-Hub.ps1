# PS5 Hub - Lance le pipeline Make complet dans WSL.
param([string]$Distribution = 'Ubuntu')
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$linuxPath = (& wsl.exe -d $Distribution -- wslpath -a $root).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Impossible de convertir le chemin WSL.' }
& wsl.exe -d $Distribution -- bash "$linuxPath/tools/hub-build.sh"
if ($LASTEXITCODE -ne 0) { throw 'La compilation de PS5 Hub a échoué.' }
