<#
  terrapulse — one-command control for the TerraPulse module suite (Windows).
  Like SeisComp's `seiscomp` script: starts/stops all modules together.

  Usage:
    .\bin\terrapulse.ps1 start            # backend + console, synthetic source
    .\bin\terrapulse.ps1 start -Port COM6 # backend + console, real device on COM6
    .\bin\terrapulse.ps1 start -View tprttv
    .\bin\terrapulse.ps1 start -View tpolv -NoInventory
    .\bin\terrapulse.ps1 start -NoGui     # backend only (no console window)
    .\bin\terrapulse.ps1 stop             # stop everything
    .\bin\terrapulse.ps1 status           # what is running

  Adjust $QtBin / $BuildDir below if your Qt or build path differs.
#>
param(
    [ValidateSet('start','stop','status','restart','update-config','check')]
    [string]$Command = 'start',
    [string]$Port = '',           # e.g. COM6 for the real device; empty => --sim
    [string]$Port2 = '',          # second device (e.g. SM-3) on another COM port
    [int]$Rate = 200,
    [string]$Db = 'terrapulse.db',
    [ValidateSet('full','dashboard','tprttv','tpmap','tpolv')]
    [string]$View = 'full',
    [switch]$NoGui,
    [switch]$NoInventory,
    [switch]$NoAcq                # monitoring center only: no local source; data
)                                 # arrives from a field node feeding tpslinkserver

# ── Configuration ─────────────────────────────────────────────────────────────
$QtBin    = 'D:\Qt\6.9.3\mingw_64\bin'
$MinGWBin = 'D:\Qt\Tools\mingw1310_64\bin'
$Root     = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$BuildDir = Join-Path $Root 'build\Desktop_Qt_6_9_3_MinGW_64_bit-Debug'
$Inv      = Join-Path $Root 'config\inventory.example.json'
$LogDir   = Join-Path $Root 'var\logs'
$Tds      = Join-Path $Root 'var\tds'

$Modules = @('tpmaster','tpslinkserver','tpproc','tpevent','tpwfparam','tpqc','tpalert','tpevtlog','tprelay',
             'tpws','tpslmon','tpdiskmon','tpconfig','tpacq','tpslink','tpstore','tpmm','tpjournal','tpinv',
             'appTerraPulse')
$env:Path = "$QtBin;$MinGWBin;$env:Path"
$env:TP_TDS = $Tds          # so the console's waveform review finds the archive

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
    { $_ -in 'update-config','check' } {
        # Validate bindings: every key file must reference a known module, and any
        # named profile must exist. Catches typos before a module silently ignores
        # them (SeisComp's update-config / scchkcfg role).
        $env:TP_ROOT = $Root
        $KeyDir = Join-Path $Root 'etc\key'
        $problems = 0; $bindCount = 0
        Write-Host "Configuration root: $Root" -ForegroundColor Cyan
        Write-Host "Modules with shipped defaults:" -ForegroundColor Cyan
        Get-ChildItem (Join-Path $Root 'etc\defaults') -Filter *.cfg -ErrorAction SilentlyContinue |
            ForEach-Object { Write-Host ("  {0}" -f $_.BaseName) }

        if (-not (Test-Path $KeyDir)) {
            Write-Host "No bindings (etc\key missing) - all sensors use global config." -ForegroundColor Yellow
        } else {
            Write-Host "`nBindings:" -ForegroundColor Cyan
            foreach ($kf in Get-ChildItem $KeyDir -Filter 'sensor_*' -ErrorAction SilentlyContinue) {
                Write-Host ("  {0}" -f $kf.Name)
                foreach ($line in Get-Content $kf.FullName) {
                    $t = ($line -split '#')[0].Trim()
                    if (-not $t) { continue }
                    $mod, $prof = ($t -split ':', 2)
                    $mod = $mod.Trim()
                    $bindCount++
                    if (-not (Test-Path (Join-Path $BuildDir "$mod.exe"))) {
                        Write-Host ("    ! unknown module '{0}'" -f $mod) -ForegroundColor Red; $problems++
                        continue
                    }
                    if ($prof) {
                        $prof = $prof.Trim()
                        $pf = Join-Path $Root ("etc\{0}\profile_{1}.cfg" -f $mod, $prof)
                        if (Test-Path $pf) { Write-Host ("    {0} -> profile '{1}'" -f $mod, $prof) -ForegroundColor Green }
                        else { Write-Host ("    ! {0}: profile '{1}' not found ({2})" -f $mod, $prof, $pf) -ForegroundColor Red; $problems++ }
                    } else {
                        Write-Host ("    {0} (global config)" -f $mod) -ForegroundColor Green
                    }
                }
            }
        }
        Write-Host ""
        if ($problems -eq 0) { Write-Host "OK - $bindCount binding(s), no problems." -ForegroundColor Green }
        else { Write-Host "$problems problem(s) found in $bindCount binding(s)." -ForegroundColor Red; exit 1 }
    }
    'status'  {
        $r = Get-Process $Modules -ErrorAction SilentlyContinue
        if ($r) { $r | Select-Object Name, Id, @{n='CPU(s)';e={[math]::Round($_.CPU,1)}} | Format-Table -AutoSize }
        else    { Write-Host "TerraPulse is not running." }
    }
    { $_ -in 'start','restart' } {
        if ($Command -eq 'restart') { Stop-All; Start-Sleep -Milliseconds 500 }
        New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
        New-Item -ItemType Directory -Force -Path $Tds | Out-Null
        Write-Host "Starting TerraPulse..." -ForegroundColor Cyan

        Start-Module 'tpmaster' @('--db', $Db)
        Start-Sleep -Milliseconds 500
        if (-not $NoInventory -and (Test-Path $Inv)) { Start-Module 'tpinv' @('--file', $Inv) }

        # SeedLink backbone (SeisComp RecordStream model): tpacq feeds ready records
        # to the server's feed port (18001); the server keeps a live ring and serves
        # SeedLink on 18000 to the console's live trace and any --slink consumer.
        # Waveforms travel here, not over the broker; the durable archive stays with
        # tpacq --archive below (single writer). Inventory resolves FDSN stream ids.
        $SlinkArgs = @('--port', '18000', '--feed-port', '18001')
        if (-not $NoInventory -and (Test-Path $Inv)) { $SlinkArgs += @('--inventory', $Inv) }
        Start-Module 'tpslinkserver' $SlinkArgs
        Start-Sleep -Milliseconds 300

        Start-Module 'tpproc' @()
        Start-Module 'tpevent' @()
        Start-Module 'tpwfparam' @()
        Start-Module 'tpqc' @()
        Start-Module 'tpalert' @()
        Start-Module 'tpevtlog' @('--dir', (Join-Path $Root 'var\events'))
        Start-Module 'tpws' @('--db', $Db, '--port', '8080', '--tds', $Tds)
        Start-Module 'tpslmon' @('--port', '8081')

        # tpacq: archive raw waveforms to TDS (feeds console review + replay) and
        # feed the SeedLink backbone (--slink -> tpslinkserver feed port). It still
        # publishes raw. to the broker so bus-based pages (Monitoring) keep working;
        # the live trace reads the SeedLink ring instead.
        # -NoAcq: run as a monitoring centre only — start no local source. A field
        # node (e.g. a Raspberry Pi running tpacq) feeds tpslinkserver over the LAN.
        if ($NoAcq) {
            Write-Host "  (no local source; waiting for a field node to feed :18001)" -ForegroundColor DarkGray
        }
        elseif ($Port) { Start-Module 'tpacq' @('--port', $Port, '--object', '1', '--station', '1', '--sensor', '1', '--archive', $Tds, '--slink', '127.0.0.1:18001') }
        else           { Start-Module 'tpacq' @('--sim', '--rate', "$Rate", '--archive', $Tds, '--slink', '127.0.0.1:18001'); Write-Host "  (synthetic source; use -Port COM6 for the device)" -ForegroundColor DarkGray }

        # Second device (SM-3) = object 2, sensor 2 — matches the SM-3 entry in
        # config/inventory.example.json (Tower-A, basement, seismometer).
        if (-not $NoAcq -and $Port2) { Start-Module 'tpacq' @('--port', $Port2, '--object', '2', '--station', '2', '--sensor', '2', '--archive', $Tds, '--slink', '127.0.0.1:18001') }

        # Console: analysis results over the broker (AppController), live waveforms
        # over the SeedLink backbone (--slink -> tpslinkserver serve port 18000).
        if (-not $NoGui) {
            $GuiArgs = @('--view', $View, '--slink', '127.0.0.1:18000')
            if (-not $NoInventory -and (Test-Path $Inv)) { $GuiArgs += @('--inventory', $Inv) }
            Start-Module 'appTerraPulse' $GuiArgs -Gui
        }

        Write-Host "TerraPulse is up.  Logs: $LogDir   Stop: .\bin\terrapulse.ps1 stop" -ForegroundColor Cyan
    }
}
