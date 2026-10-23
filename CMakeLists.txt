cmake_minimum_required(VERSION 3.20)

# --- NexusForce discovery -----------------------------------------------------

set(NEXUSWAVE_NEXUSFORCE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../NexusForce" CACHE PATH
        "NexusForce source tree (used when no installed prefix is available)")
set(NEXUSWAVE_NEXUSFORCE_PREFIX "" CACHE PATH
        "Installed NexusForce prefix (written by scripts/install_nexusforce.py)")
set(NEXUSWAVE_VCPKG_INSTALLED_DIR "${NEXUSWAVE_NEXUSFORCE_SOURCE}/vcpkg_installed" CACHE PATH
        "vcpkg_installed directory reused for the NexusForce source build")
option(NEXUSWAVE_BUILD_FROM_SOURCE "Force building NexusForce from a source checkout" OFF)
option(NEXUSWAVE_NEXUSFORCE_MINIMAL "Disable NexusForce tests/examples/benchmarks/docs in source mode" ON)

if (NEXUSWAVE_BUILD_FROM_SOURCE)
    set(NEXUSWAVE_NEXUSFORCE_MODE "source")
elseif (NEXUSWAVE_NEXUSFORCE_PREFIX)
    set(NEXUSWAVE_NEXUSFORCE_MODE "prefix")
elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.deps/nexusforce/lib/cmake/NexusForce/NexusForceConfig.cmake")
    set(NEXUSWAVE_NEXUSFORCE_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/.deps/nexusforce")
    set(NEXUSWAVE_NEXUSFORCE_MODE "prefix")
elseif (EXISTS "${NEXUSWAVE_NEXUSFORCE_SOURCE}/CMakeLists.txt")
    set(NEXUSWAVE_NEXUSFORCE_MODE "source")
else()
    message(FATAL_ERROR
            "NexusWave needs NexusForce. Either install it with\n"
            "  python3 scripts/install_nexusforce.py\n"
            "or clone it next to this project (expected: ${NEXUSWAVE_NEXUSFORCE_SOURCE}),\n"
            "or set NEXUSWAVE_NEXUSFORCE_PREFIX to an installed prefix.")
endif()

if (NEXUSWAVE_NEXUSFORCE_MODE STREQUAL "source")
# Older clones may not have the TUI tree NexusWave needs; the reference Linux
# workspace keeps its checkout next to this project under NexusForce-Linux, so
# fall back to that before reporting the tree as unusable.
file(REAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../NexusForce" _newave_default_source)
if ((NEXUSWAVE_NEXUSFORCE_SOURCE STREQUAL _newave_default_source
        OR NEXUSWAVE_NEXUSFORCE_SOURCE STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}/../NexusForce")
        AND NOT EXISTS "${NEXUSWAVE_NEXUSFORCE_SOURCE}/include/NeForce/tui/dom/theme.hpp"
        AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../NexusForce-Linux/include/NeForce/tui/dom/theme.hpp")
    set(NEXUSWAVE_NEXUSFORCE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../NexusForce-Linux")
    message(STATUS "NexusWave: ../NexusForce is missing the TUI headers, using ${NEXUSWAVE_NEXUSFORCE_SOURCE}")
endif()
    if (NOT EXISTS "${NEXUSWAVE_NEXUSFORCE_SOURCE}/config.json"
            OR NOT EXISTS "${NEXUSWAVE_NEXUSFORCE_SOURCE}/include/NeForce/tui/dom/theme.hpp")
        message(FATAL_ERROR
                "NexusForce source tree at ${NEXUSWAVE_NEXUSFORCE_SOURCE} is incomplete or outdated.\n"
                "Point NEXUSWAVE_NEXUSFORCE_SOURCE at a full checkout, or install a prefix with "
                "scripts/install_nexusforce.py.")
    endif()
    if (NOT CMAKE_TOOLCHAIN_FILE)
        file(READ "${NEXUSWAVE_NEXUSFORCE_SOURCE}/config.json" NEXUSWAVE_NEXUSFORCE_CONFIG)
        if (WIN32)
            string(JSON NEXUSWAVE_VCPKG_ROOT ERROR_VARIABLE _nw_vcpkg_error GET "${NEXUSWAVE_NEXUSFORCE_CONFIG}" vcpkg windows)
        else()
            string(JSON NEXUSWAVE_VCPKG_ROOT ERROR_VARIABLE _nw_vcpkg_error GET "${NEXUSWAVE_NEXUSFORCE_CONFIG}" vcpkg linux)
        endif()
        if (NOT _nw_vcpkg_error AND NEXUSWAVE_VCPKG_ROOT)
            if (NEXUSWAVE_VCPKG_ROOT MATCHES "^~/(.*)$")
                set(NEXUSWAVE_VCPKG_ROOT "$ENV{HOME}/${CMAKE_MATCH_1}")
            endif()
            if (EXISTS "${NEXUSWAVE_VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
                set(CMAKE_TOOLCHAIN_FILE "${NEXUSWAVE_VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
                        CACHE FILEPATH "vcpkg toolchain detected from NexusForce config.json")
            endif()
        endif()
    endif()
    # Let the vcpkg toolchain install NexusForce's manifest into the shared tree
    # instead of creating one for NexusWave (which has no vcpkg.json).
    set(VCPKG_MANIFEST_MODE ON)
    set(VCPKG_MANIFEST_DIR "${NEXUSWAVE_NEXUSFORCE_SOURCE}")
    set(VCPKG_INSTALLED_DIR "${NEXUSWAVE_VCPKG_INSTALLED_DIR}")
    set(VCPKG_MANIFEST_INSTALL ON)
endif()

project(NexusWave VERSION 0.1.0 LANGUAGES CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (NEXUSWAVE_NEXUSFORCE_MODE STREQUAL "source")
    message(STATUS "NexusWave: NexusForce from source at ${NEXUSWAVE_NEXUSFORCE_SOURCE}")
    if (NEXUSWAVE_NEXUSFORCE_MINIMAL)
        foreach (_nw_opt NEXUSFORCE_BUILD_TESTS NEXUSFORCE_BUILD_EXAMPLES NEXUSFORCE_BUILD_BENCHMARKS
                NEXUSFORCE_BUILD_DOCS NEXUSFORCE_FORMAT)
            if (NOT DEFINED ${_nw_opt})
                set(${_nw_opt} OFF CACHE BOOL "Disabled by NexusWave for a lean IDE build")
            endif()
        endforeach()
    endif()
    add_subdirectory("${NEXUSWAVE_NEXUSFORCE_SOURCE}" "${CMAKE_BINARY_DIR}/nexusforce")
else()
    message(STATUS "NexusWave: NexusForce prefix ${NEXUSWAVE_NEXUSFORCE_PREFIX}")
    # A previously cached NexusForce_DIR (for example a system-wide /usr/local install)
    # would silently win over the selected prefix, so pin it here.
    if (EXISTS "${NEXUSWAVE_NEXUSFORCE_PREFIX}/lib/cmake/NexusForce/NexusForceConfig.cmake")
        set(NexusForce_DIR "${NEXUSWAVE_NEXUSFORCE_PREFIX}/lib/cmake/NexusForce" CACHE PATH
                "NexusForce package directory selected by NEXUSWAVE_NEXUSFORCE_PREFIX" FORCE)
    endif()
    if (NOT EXISTS "${NEXUSWAVE_NEXUSFORCE_PREFIX}/include/NeForce/tui/dom/theme.hpp")
        message(FATAL_ERROR
                "The NexusForce prefix at ${NEXUSWAVE_NEXUSFORCE_PREFIX} looks incomplete or outdated.\n"
                "Re-run: python3 scripts/install_nexusforce.py")
    endif()
    list(PREPEND CMAKE_PREFIX_PATH "${NEXUSWAVE_NEXUSFORCE_PREFIX}")
    # Written by scripts/install_nexusforce.py: adds the dependency prefixes
    # NexusForce was built against (OpenSSL, pcre2, zlib).
    include("${NEXUSWAVE_NEXUSFORCE_PREFIX}/prefix.cmake" OPTIONAL)
    find_package(NexusForce REQUIRED)
endif()

include(cmake/NexusWaveOptions.cmake)
include(cmake/NexusWaveClangFormat.cmake)

option(NEXUSWAVE_BUILD_TESTS "Build NexusWave unit tests" ON)

add_subdirectory(src)

if (NEXUSWAVE_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if (DEFINED NEXUSFORCE_VERSION AND NOT NEXUSFORCE_VERSION STREQUAL "")
    message(STATUS "NexusWave configured against NexusForce ${NEXUSFORCE_VERSION}")
else()
    message(STATUS "NexusWave configured (NexusForce built from source)")
endif()
