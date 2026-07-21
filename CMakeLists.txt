cmake_minimum_required(VERSION 3.16)
project(TerraPulse VERSION 0.1 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Charts Core QuickControls2 SerialPort Sql)

qt_standard_project_setup(REQUIRES 6.8)

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

# ── tpacq — acquisition daemon (serial -> bus) ────────────────────────────────
add_executable(tpacq
    apps/tpacq/main.cpp
    src/serial/SerialStreamReceiver.cpp
)
target_include_directories(tpacq PRIVATE src)
target_link_libraries(tpacq PRIVATE tpbus Qt6::Core Qt6::SerialPort)

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
target_link_libraries(tpproc PRIVATE tp_backend tpbus Qt6::Core)

# ── tpstore — storage daemon (saf/shf -> SQLite) ──────────────────────────────
add_executable(tpstore apps/tpstore/main.cpp)
target_include_directories(tpstore PRIVATE src)
target_link_libraries(tpstore PRIVATE tpbus Qt6::Core Qt6::Sql)

# ── tpmaster — central message broker (XSUB/XPUB) + embedded dbstore ──────────
add_executable(tpmaster apps/tpmaster/main.cpp src/storage/DbStore.cpp)
target_include_directories(tpmaster PRIVATE src)
target_link_libraries(tpmaster PRIVATE tpbus Qt6::Core Qt6::Sql)

# ── tpinv — inventory tool (JSON -> INVENTORY notifiers) ──────────────────────
add_executable(tpinv apps/tpinv/main.cpp)
target_include_directories(tpinv PRIVATE src third_party/nlohmann)
target_link_libraries(tpinv PRIVATE tpbus Qt6::Core)

# ── tpjournal — operator journal action (-> JOURNAL group) ────────────────────
add_executable(tpjournal apps/tpjournal/main.cpp)
target_include_directories(tpjournal PRIVATE src)
target_link_libraries(tpjournal PRIVATE tpbus Qt6::Core)

# ── KissFFT ───────────────────────────────────────────────────────────────────
add_library(kissfft STATIC third_party/kissfft/kiss_fft.c)
target_include_directories(kissfft PUBLIC third_party/kissfft)

# ── Backend (core + storage + analysis + simulator + history) ─────────────────
add_library(tp_backend STATIC
    src/core/Crc32.cpp
    src/core/SdfRecord.cpp
    src/core/SafRecord.cpp
    src/core/ShfRecord.cpp
    src/storage/SdfFile.cpp
    src/storage/SafFile.cpp
    src/storage/ShfFile.cpp
    src/simulator/SensorSimulator.cpp
    src/analysis/AnalysisEngine.cpp
    src/history/HistoryEngine.cpp
)
target_include_directories(tp_backend PUBLIC src)
target_link_libraries(tp_backend PUBLIC Qt6::Core)

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
)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(appTerraPulse
    URI TerraPulse
    VERSION 1.0
    QML_FILES
        Main.qml
        qml/Theme.qml
        qml/components/Panel.qml
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
install(TARGETS appTerraPulse
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
