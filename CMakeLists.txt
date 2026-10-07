# AegisROM — unified hybrid AI flashing suite (flashrom + FlashromGUI + IMSProg)
# Root build tree. Builds aegisrom-cli (no Qt) and aegisrom (Qt6 GUI) from one tree.
cmake_minimum_required(VERSION 3.21)

project(AegisROM
  VERSION 0.1.0
  DESCRIPTION "Autonomous hybrid AI hardware flashing / EEPROM programming suite"
  LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------- options ---
option(BUILD_CLI "Build aegisrom-cli (no Qt overhead)" ON)
option(BUILD_GUI "Build aegisrom Qt6 GUI" ON)
option(AEGISROM_ENABLE_AGENT "Build AegisAgent core (heuristics + LLM hooks)" ON)
option(AEGISROM_USE_MOCK_HAL "Compile HAL in mock mode (no libflashrom link). Auto-enabled when libflashrom is absent." OFF)
option(AEGISROM_BUILD_TESTS "Configure smoke tests" ON)

# Sibling checkout layout: <workspace>/flashrom, <workspace>/AegisROM
set(AEGISROM_FLASHROM_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../flashrom"
  CACHE PATH "Path to flashrom source tree (provides include/libflashrom.h)")
set(AEGISROM_IMSPROG_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../IMSProg"
  CACHE PATH "Path to IMSProg source tree (QHexEdit + chip database reference)")

# ------------------------------------------------------- libflashrom lookup ---
# flashrom upstream builds with Meson, not CMake, so we do NOT add_subdirectory()
# it. Instead we (1) prefer a system/installed libflashrom, (2) accept headers
# straight from the sibling source tree so the HAL always compiles against the
# exact reviewed C API (include/libflashrom.h), and (3) fall back to MOCK HAL
# when no binary library is present (dev machines / Windows without WinUSB build).
set(FLASHROM_INCLUDE_CANDIDATES
  "${AEGISROM_FLASHROM_SOURCE_DIR}/include"
  /usr/include
  /usr/local/include
)

find_path(FLASHROM_INCLUDE_DIR
  NAMES libflashrom.h
  PATHS ${FLASHROM_INCLUDE_CANDIDATES}
  NO_DEFAULT_PATH
)
if(NOT FLASHROM_INCLUDE_DIR)
  find_path(FLASHROM_INCLUDE_DIR NAMES libflashrom.h)
endif()

find_library(FLASHROM_LIBRARY NAMES flashrom libflashrom)

if(FLASHROM_INCLUDE_DIR AND FLASHROM_LIBRARY AND NOT AEGISROM_USE_MOCK_HAL)
  set(AEGISROM_HAVE_FLASHROM TRUE)
  add_library(Flashrom::flashrom UNKNOWN IMPORTED)
  set_target_properties(Flashrom::flashrom PROPERTIES
    IMPORTED_LOCATION "${FLASHROM_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${FLASHROM_INCLUDE_DIR}"
  )
  message(STATUS "[AegisROM] libflashrom: ${FLASHROM_LIBRARY} (headers: ${FLASHROM_INCLUDE_DIR})")
else()
  set(AEGISROM_HAVE_FLASHROM FALSE)
  set(AEGISROM_USE_MOCK_HAL ON)
  message(STATUS "[AegisROM] libflashrom binary not found — HAL will build in MOCK mode.")
  if(FLASHROM_INCLUDE_DIR)
    message(STATUS "[AegisROM] ... but headers found at ${FLASHROM_INCLUDE_DIR}; API signatures still checked in.")
  else()
    message(STATUS "[AegisROM] ... headers also absent (expected: ${AEGISROM_FLASHROM_SOURCE_DIR}/include).")
    message(STATUS "[AegisROM] To link real hardware: install Meson+Ninja, build sibling ../flashrom,")
    message(STATUS "[AegisROM] then re-configure with -DFLASHROM_LIBRARY=<path> -DFLASHROM_INCLUDE_DIR=<path>.")
  endif()
endif()

message(STATUS "[AegisROM] Flashrom src ref: ${AEGISROM_FLASHROM_SOURCE_DIR}")
message(STATUS "[AegisROM] IMSProg src ref:  ${AEGISROM_IMSPROG_SOURCE_DIR}")

# ------------------------------------------------------------------ Qt6 lookup
if(BUILD_GUI)
  find_package(Qt6 QUIET COMPONENTS Widgets Svg Xml)
  if(NOT Qt6_FOUND)
    message(WARNING "[AegisROM] Qt6 (Widgets/Svg/Xml) not found — GUI target 'aegisrom' will be skipped. CLI still builds.")
    set(BUILD_GUI OFF)
  endif()
endif()

# ------------------------------------------------------------ common library --
# No Qt dependency: chip DB + hashing used by both CLI and GUI.
add_library(aegis_common STATIC
  src/common/ChipDatabaseManager.cpp
  src/common/HashUtils.cpp
)
target_include_directories(aegis_common PUBLIC src/common)
target_compile_features(aegis_common PUBLIC cxx_std_20)

# --------------------------------------------------------------- HAL library --
add_library(aegis_hal STATIC
  src/hal/FlashromHAL.cpp
)
target_include_directories(aegis_hal PUBLIC src/hal)
target_link_libraries(aegis_hal PUBLIC aegis_common)
target_compile_features(aegis_hal PUBLIC cxx_std_20)

if(AEGISROM_USE_MOCK_HAL)
  target_compile_definitions(aegis_hal PUBLIC AEGISROM_MOCK_HAL=1)
else()
  target_link_libraries(aegis_hal PUBLIC Flashrom::flashrom)
endif()
if(FLASHROM_INCLUDE_DIR)
  target_include_directories(aegis_hal PUBLIC "${FLASHROM_INCLUDE_DIR}")
endif()

# ------------------------------------------------------------- agent library --
if(AEGISROM_ENABLE_AGENT)
  add_library(aegis_agent STATIC
    src/agent/AegisAgent.cpp
  )
  target_include_directories(aegis_agent PUBLIC src/agent src/hal src/common)
  target_link_libraries(aegis_agent PUBLIC aegis_hal aegis_common)
  target_compile_features(aegis_agent PUBLIC cxx_std_20)
  target_compile_definitions(aegis_agent PUBLIC AEGISROM_ENABLE_AGENT=1)
endif()

# ------------------------------------------------------------------- CLI ----
if(BUILD_CLI)
  add_executable(aegisrom-cli src/cli/main.cpp)
  target_link_libraries(aegisrom-cli PRIVATE aegis_hal aegis_common)
  if(AEGISROM_ENABLE_AGENT)
    target_link_libraries(aegisrom-cli PRIVATE aegis_agent)
  endif()
  target_compile_features(aegisrom-cli PRIVATE cxx_std_20)
endif()

# ------------------------------------------------------------------- GUI ----
if(BUILD_GUI)
  set(CMAKE_AUTOMOC ON)
  set(CMAKE_AUTOUIC ON)
  set(CMAKE_AUTORCC ON)

  add_executable(aegisrom
    src/gui/MainWindow.cpp
    src/gui/main.cpp
    src/gui/widgets/PinoutViewer.cpp
    src/gui/widgets/AutonomousWidget.cpp
    # Phase 2: IMSProg QHexEdit port lives here (see src/gui/components/hexeditor/README_NOTE).
    src/gui/components/hexeditor/HexEditorBridge.cpp
  )
  target_include_directories(aegisrom PRIVATE
    src/gui src/gui/widgets src/gui/components/hexeditor
    src/hal src/agent src/common
  )
  target_link_libraries(aegisrom PRIVATE
    Qt6::Widgets Qt6::Svg Qt6::Xml
    aegis_hal aegis_common
  )
  if(AEGISROM_ENABLE_AGENT)
    target_link_libraries(aegisrom PRIVATE aegis_agent)
  endif()
  target_compile_features(aegisrom PRIVATE cxx_std_20)
endif()

# ------------------------------------------------------------------- tests --
if(AEGISROM_BUILD_TESTS)
  enable_testing()
  # Smoke test: CLI --help must exit 0 (runs in mock HAL on dev machines).
  if(BUILD_CLI AND TARGET aegisrom-cli)
    add_test(NAME cli_help COMMAND aegisrom-cli --help)
  endif()
endif()

# ------------------------------------------------------------------ summary --
message(STATUS "==== AegisROM configure summary ====")
message(STATUS "  BUILD_CLI=${BUILD_CLI}  BUILD_GUI=${BUILD_GUI}")
message(STATUS "  AEGISROM_ENABLE_AGENT=${AEGISROM_ENABLE_AGENT}")
message(STATUS "  AEGISROM_USE_MOCK_HAL=${AEGISROM_USE_MOCK_HAL}")
message(STATUS "  AEGISROM_HAVE_FLASHROM=${AEGISROM_HAVE_FLASHROM}")
