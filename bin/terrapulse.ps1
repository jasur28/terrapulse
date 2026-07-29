<#
  terrapulse.ps1 - Windows shim over the cross-platform `bin/terrapulse` (Python),
  like SeisComp's `seiscomp`. It only sets the Qt runtime PATH and TP_ROOT, then
  forwards every argument to the one management command. There is a single engine
  now (etc/roles.json + etc/init/*.py), used identically on the dev box and a Pi.

  Examples:
    .\bin\terrapulse.ps1 setup --role standalone
    .\bin\terrapulse.ps1 start
    .\bin\terrapulse.ps1 status
    .\bin\terrapulse.ps1 exec tprttv            # a GUI view (blocks this terminal)
    .\bin\terrapulse.ps1 stop
    .\bin\terrapulse.ps1 setup --role acquisition --source COM6
    .\bin\terrapulse.ps1 setup --role center --backbone-host 192.168.1.50
    .\bin\terrapulse.ps1 list | roles | check | update-config

  Adjust $QtBin / $MinGWBin below if your Qt path differs.
#>

$QtBin    = 'D:\Qt\6.9.3\mingw_64\bin'
$MinGWBin = 'D:\Qt\Tools\mingw1310_64\bin'
$Root     = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)

# Qt DLLs for the daemons the Python command spawns; TP_ROOT/TP_TDS for config+archive.
$env:Path = "$QtBin;$MinGWBin;$env:Path"
$env:TP_ROOT = $Root
if (-not $env:TP_TDS) { $env:TP_TDS = Join-Path $Root 'var\tds' }

$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) { $python = Get-Command py -ErrorAction SilentlyContinue }
if (-not $python) {
    Write-Host "Python 3 not found on PATH - the terrapulse command needs it." -ForegroundColor Red
    exit 1
}

$cmd = Join-Path $Root 'bin\terrapulse'

# Convenience: a fresh checkout's first `start` with no role set -> standalone,
# so one command still brings the whole demo up.
if ($args.Count -ge 1 -and $args[0] -eq 'start' -and -not (Test-Path (Join-Path $Root 'etc\enabled.json'))) {
    Write-Host "No role set - defaulting to standalone (terrapulse setup --role standalone)." -ForegroundColor DarkGray
    & $python.Source $cmd setup --role standalone | Out-Null
}

& $python.Source $cmd @args
exit $LASTEXITCODE
