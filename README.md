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
| **tpacq** | Acquisition: serial device, synthetic source (`--sim`), or replay of a recording (`--replay`); optional `--record`, miniSEED `--archive`, and `--buffer` store-and-forward |
| **tpproc** | Analysis: windowing → RMS/energy/frequency/health-index → anomaly detection (SAF/SHF) |
| **tpslink** | SeedLink client: streams miniSEED from third-party dataloggers into the bus + archive |
| **tpimport** | Replays an archived recording (miniSEED TDS) back into a queue — like SeisComp's `msrtsimul` |
| **tpdump** | Inspects archived waveforms via a RecordStream URI (`tds://`, `file://`) |
| **tpevent** | Groups individual anomalies into structure-level events (open → extend → resolved) |
| **tpwfparam** | Strong-motion parameters: PGA, PGV, 5%-damped response spectra, JMA seismic intensity |
| **tpqc** | Data quality: gaps, latency, spikes, availability — is the data trustworthy? |
| **tpalert** | Runs an operator-configured command on an anomaly event (mail/SMS/siren) |
| **tpevtlog** | Appends each event's evolution to per-event log files (audit history) |
| **tprelay** | Forwards selected groups to another tpmaster (building → city → country) |
| **tpws** | REST/JSON web service + live SSE stream + waveform endpoint (read-only) |
| **tphubmon** | Self-refreshing HTML acquisition status page (no client needed) |
| **tpconfig** | Configuration editor (GUI): module settings, per-sensor bindings/profiles, inventory |
| **tpinv** | Inventory: loads structures→sensors→channels from JSON as add/update/remove notifiers |
| **tpjournal** | Operator actions (confirm/reject/reclassify) → audit trail |
| **tpmm** | Module monitor — live state-of-health table |
| **tpstore** | Optional standalone SQLite writer (alternative to tpmaster's embedded store) |
| **appTerraPulse** | Qt/QML operator console (Dashboard, Monitoring, Map, Objects, Sensors, Analysis, Events, Settings) |

Command-line tools: **tpdump** / **tpart** (inspect, check, export, import the archive),
**tpevtstreams** (streams for an event), **tpquery** (named reports), **tpxmldump**
(export the data model), **tpreport** (HTML/PDF/KML report), **tpdbstrip**
(retention), **tpdiskmon** (disk watchdog), **tpdumpcfg** (effective configuration).

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
## Waveforms & interoperability (miniSEED)

Raw waveforms are stored as standard **miniSEED** (512-byte records) in a TDS
archive — one file per channel per day, FDSN source ids. This makes the system
vendor-neutral: our own device (via `tpacq`) and any third-party instrument that
speaks miniSEED share the same pipeline and storage.

```powershell
.\tpacq.exe --sim --archive var\tds --buffer 60   # record waveforms (+ store-and-forward)
.\tpdump.exe tds://var/tds                          # inspect the archive
.\tpslink.exe --server host:18000 --net GE --sta MORC --archive var\tds  # ingest a SeedLink feed
.\tpimport.exe --tds var\tds --object 1 --sensor 1 --queue playback       # replay a recording
```

The console's Events page reviews the recorded X/Y/Z around a selected anomaly
straight from this archive (`TP_TDS` points it at the archive location).

## Configuration

Every tunable lives in a text file, so thresholds and filter bands can be adjusted
without recompiling. Layers, later overriding earlier:

```
etc/defaults/global.cfg   etc/defaults/<module>.cfg   # shipped, documents every key
etc/global.cfg            etc/<module>.cfg            # site settings (edit these)
~/.terrapulse/…                                       # per-user overrides
<command-line flags>                                  # win over all files
```

Per-sensor **bindings** let one structure be judged by different limits than
another: `etc/key/sensor_<object>_<sensor>` names the modules and profiles, and a
profile is `etc/<module>/profile_<name>.cfg`.

```powershell
.\bin\terrapulse.ps1 update-config   # validate bindings and profiles
.\build\...\tpdumpcfg.exe tpproc --files   # what is in force, and from where
.\build\...\tpconfig.exe             # GUI editor for all of the above
```

`tpconfig` shows every setting with its shipped default, writes only the values
you deliberately change into `etc/<module>.cfg`, and edits bindings without
touching files by hand.

## Web access

`tpws` exposes the data model over HTTP (read-only) for web and mobile clients,
and `tphubmon` serves a status page for on-site technicians:

```
http://<host>:8080/api/health      /api/structures  /api/sensors
http://<host>:8080/api/events      /api/anomalies   /api/features
http://<host>:8080/api/waveform?object=1&sensor=1&start=<ms>&end=<ms>
http://<host>:8080/api/stream      live updates (Server-Sent Events)
http://<host>:8081/                acquisition status page
```

## Reports

```powershell
.\tpreport.exe --db terrapulse.db --out report.html --days 7   # or --pdf / --kml
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
