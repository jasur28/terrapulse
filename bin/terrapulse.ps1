<#
  terrapulse — one-command control for the TerraPulse module suite (Windows).
  Like SeisComp's `seiscomp` script: starts/stops all modules together.

  Usage:
    .\bin\terrapulse.ps1 start            # backend + console, synthetic source
    .\bin\terrapulse.ps1 start -Port COM6 # backend + console, real device on COM6
    .\bin\terrapulse.ps1 start -NoGui     # backend only (no console window)
    .\bin\terrapulse.ps1 stop             # stop everything
    .\bin\terrapulse.ps1 status           # what is running

  Adjust $QtBin / $BuildDir below if your Qt or build path differs.
#>
param(
    [ValidateSet('start','stop','status','restart')]
    [string]$Command = 'start',
    [string]$Port = '',           # e.g. COM6 for the real device; empty => --sim
    [int]$Rate = 200,
    [string]$Db = 'terrapulse.db',
    [switch]$NoGui,
    [switch]$NoInventory
)

# ── Configuration ─────────────────────────────────────────────────────────────
$QtBin    = 'D:\Qt\6.9.3\mingw_64\bin'
$MinGWBin = 'D:\Qt\Tools\mingw1310_64\bin'
$Root     = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$BuildDir = Join-Path $Root 'build\Desktop_Qt_6_9_3_MinGW_64_bit-Debug'
$Inv      = Join-Path $Root 'config\inventory.example.json'
$LogDir   = Join-Path $Root 'var\logs'

$Modules = @('tpmaster','tpproc','tpacq','tpstore','tpmm','tpjournal','tpinv','appTerraPulse')
$env:Path = "$QtBin;$MinGWBin;$env:Path"

function Start-Module([string]$name, [string[]]$argv, [switch]$Gui) {
    $exe = Join-Path $BuildDir "$name.exe"
    if (-not (Test-Path $exe)) { Write-Host "  ! $name.exe not found (build first)" -ForegroundColor Yellow; return }
    $opts = @{ WorkingDirectory = $BuildDir }
    if ($argv -and $argv.Count -gt 0) { $opts.ArgumentList = $argv }
    if (-not $Gui) {
        $log = Join-Path $LogDir "$name.log"
        $opts.WindowStyle = 'Hidden'
        $opts.RedirectStandardOutput = $log
        $opts.RedirectStandardError  = "$log.err"
    }
    Start-Process $exe @opts | Out-Null
    Write-Host "  + $name $($argv -join ' ')" -ForegroundColor Green
}

function Stop-All {
    Get-Process $Modules -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Write-Host "TerraPulse stopped." -ForegroundColor Cyan
}

switch ($Command) {
    'stop'    { Stop-All }
    'status'  {
        $r = Get-Process $Modules -ErrorAction SilentlyContinue
        if ($r) { $r | Select-Object Name, Id, @{n='CPU(s)';e={[math]::Round($_.CPU,1)}} | Format-Table -AutoSize }
        else    { Write-Host "TerraPulse is not running." }
    }
    { $_ -in 'start','restart' } {
        if ($Command -eq 'restart') { Stop-All; Start-Sleep -Milliseconds 500 }
        New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
        Write-Host "Starting TerraPulse..." -ForegroundColor Cyan

        Start-Module 'tpmaster' @('--db', $Db)
        Start-Sleep -Milliseconds 500
        if (-not $NoInventory -and (Test-Path $Inv)) { Start-Module 'tpinv' @('--file', $Inv) }
        Start-Module 'tpproc' @()

        if ($Port) { Start-Module 'tpacq' @('--port', $Port) }
        else       { Start-Module 'tpacq' @('--sim', '--rate', "$Rate"); Write-Host "  (synthetic source; use -Port COM6 for the device)" -ForegroundColor DarkGray }

        if (-not $NoGui) { Start-Module 'appTerraPulse' @() -Gui }

        Write-Host "TerraPulse is up.  Logs: $LogDir   Stop: .\bin\terrapulse.ps1 stop" -ForegroundColor Cyan
    }
}
