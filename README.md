# TerraPulse

**Intelligent structural-health monitoring for buildings and bridges.**

TerraPulse continuously reads vibration from accelerometer sensors, computes
diagnostic features (RMS, energy, dominant/natural frequency, health index),
detects anomalies, and presents the state of every monitored structure to an
operator in real time. Its architecture follows the proven modular design of
seismological platforms: a central message broker, thin cooperating modules, a
single data model, and an operator console.

> Status: **working prototype (MVP)** — real accelerometer → acquisition →
> analysis → storage → operator console, end to end.

Authors: Lutfulla Yuldashev · Jasur Yuldashev

---

## Architecture

```
[sensor] → tpacq → ┌── tpmaster (broker + DB) ──┐ → appTerraPulse (console)
                   │  production / playback     │ → tpstore, tpmm, ...
        tpproc ────┘  write-before-notify DB    └── tpinv, tpjournal
```

| Module | Role |
|--------|------|
| **tpmaster** | Central ZeroMQ broker (XSUB/XPUB); production/playback queues; embedded SQLite store (write-before-notify); snapshot backfill for clients |
| **tpacq** | Acquisition: serial device, synthetic source (`--sim`), or replay of a recording (`--replay`); optional `--record` |
| **tpproc** | Analysis: windowing → RMS/energy/frequency/health-index → anomaly detection (SAF/SHF) |
| **tpinv** | Inventory: loads structures→sensors→channels from JSON as add/update/remove notifiers |
| **tpjournal** | Operator actions (confirm/reject/reclassify) → audit trail |
| **tpmm** | Module monitor — live state-of-health table |
| **tpstore** | Optional standalone SQLite writer (alternative to tpmaster's embedded store) |
| **appTerraPulse** | Qt/QML operator console (Dashboard, Monitoring, Map, Objects, Sensors, Analysis, Events, Settings) |

Every module addresses just one thing — the tpmaster host (`--master`, like
SeisComp's `connection.server`). Canonical ports: 5561 in / 5562 out / 5563
control (production); 5571/5572 for the playback queue.

## Build

Requirements: **Qt 6.9** (MinGW 64-bit), **CMake 3.16+**. ZeroMQ is fetched and
built automatically by CMake.

- **Qt Creator:** open `CMakeLists.txt`, select the Qt 6.9 MinGW kit, Build.
- **Command line:**
  ```powershell
  cmake --build build\Desktop_Qt_6_9_3_MinGW_64_bit-Debug
  ```

## Run

One command starts the whole suite (adjust Qt paths at the top of the script if
needed):

```powershell
# synthetic source (no hardware):
.\bin\terrapulse.ps1 start

# real accelerometer on a COM port:
.\bin\terrapulse.ps1 start -Port COM6

.\bin\terrapulse.ps1 status     # what is running
.\bin\terrapulse.ps1 stop       # stop everything
```

This launches `tpmaster` (+ inventory), `tpproc`, `tpacq`, and the console.
Backend logs go to `var/logs/`.

<details>
<summary>Manual start (one process per terminal)</summary>

```powershell
.\tpmaster.exe --db terrapulse.db
.\tpinv.exe --file config\inventory.example.json     # one-time inventory
.\tpproc.exe
.\tpacq.exe --port COM6        # or: --sim --rate 200
.\appTerraPulse.exe
```
</details>

### Reviewing a recording (isolated playback)

.\tpacq.exe --port COM6 --record rec.csv             # record from the device
.\tpmaster.exe --db terrapulse.db --playback --playback-db pb.db
.\tpproc.exe --queue playback
.\tpacq.exe --replay rec.csv --queue playback --historic
.\appTerraPulse.exe --queue playback                 # review console, isolated from live
```

## Offline map

The Map page uses an offline tile set placed in `share/maps/` (SeisComp-style
`world<quad>.png` quadtree). Tiles are not bundled in the repository — drop your
tile folder there. `TP_SHARE` overrides the location for deployment.

## Repository layout

```
apps/        module daemons (tpmaster, tpacq, tpproc, tpstore, tpinv, tpjournal)
src/         shared libraries (bus, storage, analysis, history, controllers, core)
qml/         operator console — components (tpgui) + pages
tools/       tpmm, tpmon (bus/monitor utilities)
bin/         terrapulse control script
config/      example inventory
accelerometer/  device firmware (STM32)
```

## License

Proprietary. All rights reserved. See [LICENSE](LICENSE). Contact:
yuldashevj@gmail.com
