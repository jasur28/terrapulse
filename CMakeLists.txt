cmake_minimum_required(VERSION 3.16)
project(TerraPulse VERSION 0.1 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Charts Core QuickControls2 SerialPort Sql Network Xml)

qt_standard_project_setup(REQUIRES 6.8)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ── ZeroMQ (fetched + built with the project toolchain) ───────────────────────
include(FetchContent)

# Build only what we need from libzmq (static lib, no tests/docs/tools).
set(BUILD_SHARED   OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC   ON  CACHE BOOL "" FORCE)
set(BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(WITH_DOCS      OFF CACHE BOOL "" FORCE)
set(WITH_PERF_TOOL OFF CACHE BOOL "" FORCE)
set(ENABLE_CPACK   OFF CACHE BOOL "" FORCE)
set(ENABLE_DRAFTS  OFF CACHE BOOL "" FORCE)

# libzmq 4.3.5 bug: ipc_address.hpp guards the AF_UNIX include with `_MSC_VER`,
# so a MinGW build falls back to <sys/socket.h> (which MinGW lacks) and fails.
# We only ever use tcp:// endpoints, so disable the IPC transport entirely.
# Pre-setting the cache var makes libzmq's check_include_files() skip its probe.
set(ZMQ_HAVE_IPC OFF CACHE BOOL "" FORCE)

FetchContent_Declare(libzmq
    GIT_REPOSITORY https://github.com/zeromq/libzmq.git
    GIT_TAG        v4.3.5
)
FetchContent_MakeAvailable(libzmq)

# cppzmq is header-only; we only want zmq.hpp, not its CMake (which would
# find_package(ZeroMQ) and miss our FetchContent build). Pointing SOURCE_SUBDIR
# at a non-existent dir makes MakeAvailable download the sources without running
# cppzmq's CMakeLists. ${cppzmq_SOURCE_DIR} still points at the repo root.
FetchContent_Declare(cppzmq
    GIT_REPOSITORY https://github.com/zeromq/cppzmq.git
    GIT_TAG        v4.10.0
    SOURCE_SUBDIR  do-not-build
)
FetchContent_MakeAvailable(cppzmq)

# libzmq's static target name varies by version; pick whichever exists.
if(TARGET libzmq-static)
    set(TP_ZMQ_TARGET libzmq-static)
elseif(TARGET libzmq)
    set(TP_ZMQ_TARGET libzmq)
else()
    message(FATAL_ERROR "libzmq target not found after FetchContent")
endif()

# ── libmseed (FDSN miniSEED, EarthScope, Apache-2.0) ──────────────────────────
# Waveform interchange format — lets us archive, replay and ingest data from any
# miniSEED-capable device (vendor-neutral), like SeisComp. libmseed ships a
# Makefile (no CMake), so we compile its C sources into a static lib ourselves.
FetchContent_Declare(libmseed
    GIT_REPOSITORY https://github.com/EarthScope/libmseed.git
    GIT_TAG        v3.1.5
)
FetchContent_MakeAvailable(libmseed)

file(GLOB LIBMSEED_SOURCES ${libmseed_SOURCE_DIR}/*.c)
add_library(libmseed STATIC ${LIBMSEED_SOURCES})
target_include_directories(libmseed PUBLIC ${libmseed_SOURCE_DIR})
if(WIN32)
    target_link_libraries(libmseed PUBLIC ws2_32)      # miniSEED time/byte-order helpers
endif()

# ── tpmseed — our C++ wrapper over libmseed (+ TDS archive) ───────────────────
add_library(tpmseed STATIC src/mseed/Mseed.cpp src/mseed/StreamId.cpp src/mseed/TdsArchive.cpp src/mseed/RecordStream.cpp)
target_include_directories(tpmseed PUBLIC src)
target_link_libraries(tpmseed PUBLIC libmseed)

# ── mseed_smoke — miniSEED encode/decode roundtrip test (console exe) ──────────
add_executable(mseed_smoke tools/mseed_smoke.cpp)
target_link_libraries(mseed_smoke PRIVATE tpmseed)

# ── Unit tests (CTest) ────────────────────────────────────────────────────────
# First test wired into the build. Layer "save": channel identity — locks the
# six paper-checked cases from docs/АРХИТЕКТУРА.md §14.
enable_testing()
add_executable(test_streamid tests/test_streamid.cpp)
target_link_libraries(test_streamid PRIVATE tpmseed)
add_test(NAME streamid COMMAND test_streamid)

# Layer "save": TDS archive round-trip + bytes/sample measurement.
add_executable(test_tdsarchive tests/test_tdsarchive.cpp)
target_link_libraries(test_tdsarchive PRIVATE tpmseed)
add_test(NAME tdsarchive COMMAND test_tdsarchive)

# SeedLink core: retention ring + framing (server) and the live WaveformClient
# consumers use instead of the raw. bus (Level 2 backbone).
add_library(tpslinkcore STATIC src/slink/SeedLinkRing.cpp src/slink/WaveformClient.cpp)
target_include_directories(tpslinkcore PUBLIC src)
target_link_libraries(tpslinkcore PUBLIC tpmseed Qt6::Core Qt6::Network)
add_executable(test_seedlinkring tests/test_seedlinkring.cpp)
target_link_libraries(test_seedlinkring PRIVATE tpslinkcore)
add_test(NAME seedlinkring COMMAND test_seedlinkring)

# Processing framework core: streaming filters + per-stream integrity monitor
# (Поток 2). Qt-free, so the tests need no Qt runtime.
add_library(tpfilter STATIC src/proc/Filter.cpp src/proc/StreamMonitor.cpp)
target_include_directories(tpfilter PUBLIC src)
add_executable(test_filter tests/test_filter.cpp)
target_link_libraries(test_filter PRIVATE tpfilter)
add_test(NAME filter COMMAND test_filter)
add_executable(test_streammonitor tests/test_streammonitor.cpp)
target_link_libraries(test_streammonitor PRIVATE tpfilter)
add_test(NAME streammonitor COMMAND test_streammonitor)

# ── tpdump — inspect archived waveforms via a RecordStream URI ─────────────────
add_executable(tpdump tools/tpdump.cpp)
target_include_directories(tpdump PRIVATE src)
target_link_libraries(tpdump PRIVATE tpmseed)

# ── tpcfg — layered configuration (etc/defaults -> etc -> ~/.terrapulse) ──────
add_library(tpcfg STATIC src/config/Config.cpp src/config/ConfigPlatform.cpp)
target_include_directories(tpcfg PUBLIC src)
target_link_libraries(tpcfg PUBLIC Qt6::Core)

# ── tpdumpcfg — print the effective configuration and where it came from ──────
add_executable(tpdumpcfg tools/tpdumpcfg.cpp)
target_include_directories(tpdumpcfg PRIVATE src)
target_link_libraries(tpdumpcfg PRIVATE tpcfg Qt6::Core)

# ── tpbus — ZeroMQ message-bus wrapper ────────────────────────────────────────
add_library(tpbus STATIC
    src/bus/Bus.cpp
    src/bus/BusMessage.cpp
)
target_include_directories(tpbus PUBLIC src ${cppzmq_SOURCE_DIR})
target_link_libraries(tpbus PUBLIC Qt6::Core ${TP_ZMQ_TARGET})

# ── tpbus_smoke — ZeroMQ de-risking test (console exe) ────────────────────────
add_executable(tpbus_smoke tools/bus_smoke.cpp)
target_link_libraries(tpbus_smoke PRIVATE tpbus)

add_library(tpbroker STATIC src/broker/Broker.cpp)
target_link_libraries(tpbroker PUBLIC tpbus tpcfg Qt6::Core)

add_library(tpclient STATIC src/client/ClientPlatform.cpp)
target_link_libraries(tpclient PUBLIC tpbroker tpbus tpmseed tpcfg Qt6::Core)

add_library(tpdatamodel STATIC src/datamodel/DataModel.cpp)
target_link_libraries(tpdatamodel PUBLIC Qt6::Core)

add_library(tpgeo STATIC src/geo/Geo.cpp)
target_link_libraries(tpgeo PUBLIC Qt6::Core)

add_library(tpio STATIC src/io/IO.cpp)
target_link_libraries(tpio PUBLIC tpmseed Qt6::Core Qt6::Sql Qt6::Xml)

add_library(tpgui STATIC
    src/gui/Gui.cpp
    include/terrapulse/gui/core/recordstreamthread.h
    include/terrapulse/gui/datamodel/eventdatarepository.h
    include/terrapulse/gui/datamodel/eventlistview.h
    include/terrapulse/gui/datamodel/inventorylistview.h
    include/terrapulse/gui/datamodel/stationdata.h
)
target_link_libraries(tpgui PUBLIC tpdatamodel tpgeo tpio Qt6::Core Qt6::Gui)

add_executable(broker_smoke tools/broker_smoke.cpp)
target_link_libraries(broker_smoke PRIVATE tpbroker Qt6::Core)

add_executable(public_headers_smoke tools/public_headers_smoke.cpp)
target_link_libraries(public_headers_smoke PRIVATE tp_backend tpclient tpdatamodel tpgeo tpio tpgui tpbroker tpbus tpmseed tpcfg Qt6::Core)

# ── tpacq — acquisition daemon (serial -> bus) ────────────────────────────────
add_executable(tpacq
    apps/tpacq/main.cpp
    src/serial/SerialStreamReceiver.cpp
)
target_include_directories(tpacq PRIVATE src)
target_link_libraries(tpacq PRIVATE tpclient tpbus tpmseed Qt6::Core Qt6::SerialPort Qt6::Network tpcfg)

# ── tpmon — bus debug monitor (subscribe + print) ─────────────────────────────
add_executable(tpmon tools/tpmon.cpp)
target_link_libraries(tpmon PRIVATE tpbus)

# ── tpmm — module monitor (STATUS/SOH live table) ─────────────────────────────
add_executable(tpmm tools/tpmm.cpp)
target_link_libraries(tpmm PRIVATE tpbus)

# ── tpproc — processing daemon (raw -> analysis/history -> saf/shf) ────────────
add_executable(tpproc
    apps/tpproc/main.cpp
    src/proc/ProcPipeline.cpp
)
target_include_directories(tpproc PRIVATE src)
target_link_libraries(tpproc PRIVATE tp_backend tpclient tpbus tpslinkcore tpfilter Qt6::Core Qt6::Network tpcfg)

# ── tpevent — group anomalies (shf) into structure-level events (evt) ─────────
add_executable(tpevent apps/tpevent/main.cpp)
target_include_directories(tpevent PRIVATE src)
target_link_libraries(tpevent PRIVATE tpclient tpbus Qt6::Core tpcfg)

# ── tpwfparam — strong-motion params (PGA/PGV/response spectra/JMA) ───────────
add_executable(tpwfparam apps/tpwfparam/main.cpp)
target_include_directories(tpwfparam PRIVATE src)
target_link_libraries(tpwfparam PRIVATE tp_backend tpclient tpbus tpslinkcore Qt6::Core Qt6::Network tpcfg)

# ── tpqc — data-quality metrics (gaps/latency/spikes/availability) ────────────
add_executable(tpqc apps/tpqc/main.cpp)
target_include_directories(tpqc PRIVATE src)
target_link_libraries(tpqc PRIVATE tpclient tpbus tpcfg tpslinkcore Qt6::Core Qt6::Network)

# ── tpalert — run an operator command on anomaly events ───────────────────────
add_executable(tpalert apps/tpalert/main.cpp)
target_include_directories(tpalert PRIVATE src)
target_link_libraries(tpalert PRIVATE tpclient tpbus tpcfg Qt6::Core)

# ── tprelay — forward selected groups to another tpmaster (node hierarchy) ────
add_executable(tprelay apps/tprelay/main.cpp)
target_include_directories(tprelay PRIVATE src)
target_link_libraries(tprelay PRIVATE tpclient tpbus tpcfg Qt6::Core)

# ── tpdbstrip — database retention ────────────────────────────────────────────
add_executable(tpdbstrip tools/tpdbstrip.cpp)
target_include_directories(tpdbstrip PRIVATE src)
target_link_libraries(tpdbstrip PRIVATE Qt6::Core Qt6::Sql)

# ── tpdiskmon — disk-space watchdog ───────────────────────────────────────────
add_executable(tpdiskmon tools/tpdiskmon.cpp)
target_include_directories(tpdiskmon PRIVATE src)
target_link_libraries(tpdiskmon PRIVATE tpbus tpcfg Qt6::Core)

# ── tpws — REST/JSON web service + live SSE stream (read-only) ────────────────
add_executable(tpws apps/tpws/main.cpp)
target_include_directories(tpws PRIVATE src)
target_link_libraries(tpws PRIVATE tpclient tpbus tpcfg tpmseed Qt6::Core Qt6::Network Qt6::Sql)

# ── tpevtlog — append each event's evolution to per-event log files ───────────
add_executable(tpevtlog apps/tpevtlog/main.cpp)
target_include_directories(tpevtlog PRIVATE src)
target_link_libraries(tpevtlog PRIVATE tpclient tpbus tpcfg Qt6::Core)

# ── tpevtstreams — streams/time windows contributing to an event ──────────────
add_executable(tpevtstreams tools/tpevtstreams.cpp)
target_include_directories(tpevtstreams PRIVATE src)
target_link_libraries(tpevtstreams PRIVATE Qt6::Core Qt6::Sql)

# ── tpxmldump — export the data model as XML ──────────────────────────────────
add_executable(tpxmldump tools/tpxmldump.cpp)
target_include_directories(tpxmldump PRIVATE src)
target_link_libraries(tpxmldump PRIVATE Qt6::Core Qt6::Sql)

# ── tpquery — named parameterised queries from etc/queries.cfg ────────────────
add_executable(tpquery tools/tpquery.cpp)
target_include_directories(tpquery PRIVATE src)
target_link_libraries(tpquery PRIVATE tpcfg Qt6::Core Qt6::Sql)

# ── tpart — archive check / export / import ───────────────────────────────────
add_executable(tpart tools/tpart.cpp)
target_include_directories(tpart PRIVATE src)
target_link_libraries(tpart PRIVATE tpmseed)

# ── tpreport — operator report (HTML / PDF / KML) ─────────────────────────────
add_executable(tpreport tools/tpreport.cpp)
target_include_directories(tpreport PRIVATE src)
target_link_libraries(tpreport PRIVATE Qt6::Core Qt6::Gui Qt6::Sql)

# ── tphubmon — acquisition status web page ────────────────────────────────────
add_executable(tphubmon apps/tphubmon/main.cpp)
target_include_directories(tphubmon PRIVATE src)
target_link_libraries(tphubmon PRIVATE tpclient tpbus tpcfg Qt6::Core Qt6::Network)

# ── tpconfig — configuration / bindings / inventory editor (GUI) ──────────────
qt_add_executable(tpconfig
    apps/tpconfig/main.cpp
    src/config/ConfigEditor.h
    src/config/ConfigEditor.cpp
)
qt_add_qml_module(tpconfig
    URI TPConfig
    VERSION 1.0
    QML_FILES apps/tpconfig/Main.qml
)
target_include_directories(tpconfig PRIVATE src)
target_link_libraries(tpconfig PRIVATE tpcfg tpbus Qt6::Quick Qt6::QuickControls2 Qt6::Core)
set_target_properties(tpconfig PROPERTIES WIN32_EXECUTABLE TRUE)

# ── tpstore — storage daemon (saf/shf -> SQLite) ──────────────────────────────
add_executable(tpstore apps/tpstore/main.cpp)
target_include_directories(tpstore PRIVATE src)
target_link_libraries(tpstore PRIVATE tpclient tpbus Qt6::Core Qt6::Sql)

# ── tpimport — replay archived miniSEED into a queue (like msrtsimul) ─────────
add_executable(tpimport apps/tpimport/main.cpp)
target_include_directories(tpimport PRIVATE src)
target_link_libraries(tpimport PRIVATE tpclient tpbus tpmseed Qt6::Core)

# ── tpslink — SeedLink client (third-party miniSEED -> bus + archive) ─────────
add_executable(tpslink apps/tpslink/main.cpp)
target_include_directories(tpslink PRIVATE src)
target_link_libraries(tpslink PRIVATE tpclient tpbus tpmseed Qt6::Core Qt6::Network)

# tpslinkserver — SeedLink server: raw. bus -> retention ring -> TCP (Stage 5)
add_executable(tpslinkserver apps/tpslinkserver/main.cpp)
target_include_directories(tpslinkserver PRIVATE src)
target_link_libraries(tpslinkserver PRIVATE tpclient tpbus tpmseed tpslinkcore Qt6::Core Qt6::Network)

# ── tpmaster — central message broker (XSUB/XPUB) + embedded dbstore ──────────
add_executable(tpmaster apps/tpmaster/main.cpp src/storage/DbStore.cpp)
target_include_directories(tpmaster PRIVATE src)
target_link_libraries(tpmaster PRIVATE tpbus Qt6::Core Qt6::Sql)

# ── tpinv — inventory tool (JSON -> INVENTORY notifiers) ──────────────────────
add_executable(tpinv apps/tpinv/main.cpp)
target_include_directories(tpinv PRIVATE src third_party/nlohmann)
target_link_libraries(tpinv PRIVATE tpclient tpbus Qt6::Core)

# ── tpjournal — operator journal action (-> JOURNAL group) ────────────────────
add_executable(tpjournal apps/tpjournal/main.cpp)
target_include_directories(tpjournal PRIVATE src)
target_link_libraries(tpjournal PRIVATE tpclient tpbus Qt6::Core)

# ── KissFFT ───────────────────────────────────────────────────────────────────
add_library(kissfft STATIC third_party/kissfft/kiss_fft.c)
target_include_directories(kissfft PUBLIC third_party/kissfft)

# ── Backend (core + storage + analysis + simulator + history) ─────────────────
add_library(tp_backend STATIC
    src/core/CorePlatform.cpp
    src/core/Crc32.cpp
    src/core/SdfRecord.cpp
    src/core/SafRecord.cpp
    src/core/ShfRecord.cpp
    src/simulator/SensorSimulator.cpp
    src/analysis/AnalysisEngine.cpp
    src/analysis/StrongMotion.cpp
    src/analysis/Spectrum.cpp
    src/history/HistoryEngine.cpp
)
target_include_directories(tp_backend PUBLIC src)
target_link_libraries(tp_backend PUBLIC Qt6::Core kissfft)

# ── Main executable ───────────────────────────────────────────────────────────
qt_add_executable(appTerraPulse
    main.cpp
    src/controllers/AppController.h
    src/controllers/AppController.cpp
    src/controllers/BusClient.h
    src/controllers/BusClient.cpp
    src/controllers/InventoryModel.h
    src/controllers/InventoryModel.cpp
    src/controllers/JournalController.h
    src/controllers/JournalController.cpp
    src/controllers/WaveformService.h
    src/controllers/WaveformService.cpp
)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(appTerraPulse
    URI TerraPulse
    VERSION 1.0
    QML_FILES
        Main.qml
        qml/Theme.qml
        qml/framework/TpGuiApplication.qml
        qml/apps/TpMapApp.qml
        qml/apps/TpRttvApp.qml
        qml/apps/TpOlvApp.qml
        qml/modules/map/StructureMapWorkbench.qml
        qml/modules/record/RealTimeTraceWorkbench.qml
        qml/modules/review/OperatorReviewWorkbench.qml
        qml/components/Panel.qml
        qml/components/ClassicMenuBar.qml
        qml/components/ClassicStatusBar.qml
        qml/components/ClassicToolButton.qml
        qml/components/MetricTile.qml
        qml/components/StatusPill.qml
        qml/components/HealthRing.qml
        qml/components/StatusBadge.qml
        qml/components/NavButton.qml
        qml/components/ObjectCard.qml
        qml/components/TrendPlot.qml
        qml/components/TraceView.qml
        qml/components/SpectrumPlot.qml
        qml/components/EventReviewPanel.qml
        qml/components/WaveformReview.qml
        qml/components/StrongMotionPanel.qml
        qml/components/ResponseSpectrumPlot.qml
        qml/components/MapView.qml
        qml/pages/DashboardPage.qml
        qml/pages/MonitoringPage.qml
        qml/pages/MapPage.qml
        qml/pages/ObjectsPage.qml
        qml/pages/SensorsPage.qml
        qml/pages/AnalysisPage.qml
        qml/pages/EventsPage.qml
        qml/pages/SettingsPage.qml
)

target_include_directories(appTerraPulse PRIVATE src)
target_link_libraries(appTerraPulse PRIVATE
    tp_backend
    tpbus
    tpmseed
    Qt6::Quick
    Qt6::Charts
    Qt6::Core
    Qt6::QuickControls2
)

set_target_properties(appTerraPulse PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
set(TP_RUNTIME_TARGETS
    appTerraPulse
    tpconfig
    tpacq
    tpalert
    tpevent
    tpevtlog
    tphubmon
    tpimport
    tpinv
    tpjournal
    tpmaster
    tpmm
    tpmon
    tpproc
    tpqc
    tprelay
    tpslink
    tpstore
    tpwfparam
    tpws
    tpdump
    tpdumpcfg
    tpdbstrip
    tpdiskmon
    tpevtstreams
    tpquery
    tpreport
    tpxmldump
    tpart
)

install(TARGETS ${TP_RUNTIME_TARGETS}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/terrapulse
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY etc/
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/terrapulse
)

install(DIRECTORY share/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/terrapulse
)

install(PROGRAMS bin/terrapulse
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(UNIX AND NOT APPLE)
    install(CODE "
        execute_process(COMMAND \${CMAKE_COMMAND} -E create_symlink appTerraPulse \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/tprttv\")
        execute_process(COMMAND \${CMAKE_COMMAND} -E create_symlink appTerraPulse \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/tpmap\")
        execute_process(COMMAND \${CMAKE_COMMAND} -E create_symlink appTerraPulse \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/tpmv\")
        execute_process(COMMAND \${CMAKE_COMMAND} -E create_symlink appTerraPulse \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/tpolv\")
    ")
endif()
