# PS5 Hub - Les caches et fichiers temporaires restent dans le projet.
param([ValidateSet('test', 'build', 'dev', 'deploy', 'whoami')][string]$Action = 'dev')
$ErrorActionPreference = 'Stop'
$taskLocal = Join-Path $PSScriptRoot '.local'
$taskTemp = Join-Path $taskLocal 'tmp'
$taskNpm = Join-Path $taskLocal 'npm-cache'
$taskLogs = Join-Path $taskLocal 'wrangler-logs'
New-Item -ItemType Directory -Force -Path $taskTemp, $taskNpm, $taskLogs | Out-Null
$taskOldTemp = $env:TEMP
$taskOldTmp = $env:TMP
$taskOldCache = $env:npm_config_cache
$taskOldLogs = $env:WRANGLER_LOG_PATH
$taskExit = 0
try {
    $env:TEMP = $taskTemp
    $env:TMP = $taskTemp
    $env:npm_config_cache = $taskNpm
    $env:WRANGLER_LOG_PATH = $taskLogs
    Push-Location (Join-Path $PSScriptRoot 'server')
    try {
        if ($Action -eq 'whoami') { & npx.cmd --no-install wrangler whoami }
        else { & npm.cmd run $Action }
        $taskExit = $LASTEXITCODE
    } finally { Pop-Location }
} finally {
    $env:TEMP = $taskOldTemp
    $env:TMP = $taskOldTmp
    $env:npm_config_cache = $taskOldCache
    $env:WRANGLER_LOG_PATH = $taskOldLogs
}
if ($taskExit -ne 0) { throw "La commande serveur '$Action' a échoué (code $taskExit)." }
