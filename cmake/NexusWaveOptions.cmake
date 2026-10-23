# NexusWaveOptions.cmake
#
# One shared set of compiler/link options for every NexusWave target, modelled
# after NexusForceCompilerOptions.cmake. The flags live on the `newave_options`
# INTERFACE target, and newave_compiler_options(<target>) attaches that object,
# so the executable, the core library and the tests can never drift apart.
#
# Sanitizers are a developer/CI facility and are applied centrally:
#   cmake -B build-asan -DNEXUSWAVE_ENABLE_ASAN=ON -DNEXUSWAVE_ENABLE_UBSAN=ON
# ASan and TSan are mutually exclusive.

option(NEXUSWAVE_ENABLE_ASAN "Build NexusWave with AddressSanitizer" OFF)
option(NEXUSWAVE_ENABLE_UBSAN "Build NexusWave with UndefinedBehaviorSanitizer" OFF)
option(NEXUSWAVE_ENABLE_TSAN "Build NexusWave with ThreadSanitizer" OFF)

# Internal: compiler identity, spelled once for all branches below.
function(_newave_is_msvc_style out_var)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        set(${out_var} ON PARENT_SCOPE)
    else()
        set(${out_var} OFF PARENT_SCOPE)
    endif()
endfunction()

# Internal: the sanitizer flags requested by the cache options.
function(_newave_sanitizer_flags out_var)
    set(_flags "")
    if(NEXUSWAVE_ENABLE_ASAN)
        list(APPEND _flags -fsanitize=address -fno-omit-frame-pointer)
    endif()
    if(NEXUSWAVE_ENABLE_UBSAN)
        list(APPEND _flags -fsanitize=undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
    endif()
    if(NEXUSWAVE_ENABLE_TSAN)
        list(APPEND _flags -fsanitize=thread -fno-omit-frame-pointer)
    endif()
    set(${out_var} ${_flags} PARENT_SCOPE)
endfunction()

# Internal: reject combinations that cannot be linked into one binary.
function(_newave_validate_sanitizers)
    if(NEXUSWAVE_ENABLE_ASAN AND NEXUSWAVE_ENABLE_TSAN)
        message(FATAL_ERROR
                "NexusWave: NEXUSWAVE_ENABLE_ASAN and NEXUSWAVE_ENABLE_TSAN are mutually exclusive. "
                "Configure two separate build directories instead.")
    endif()

    if(NEXUSWAVE_ENABLE_ASAN OR NEXUSWAVE_ENABLE_UBSAN OR NEXUSWAVE_ENABLE_TSAN)
        if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            message(FATAL_ERROR
                    "NexusWave: sanitizers require GCC or Clang; the current compiler is "
                    "${CMAKE_CXX_COMPILER_ID}.")
        endif()
        if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT CMAKE_CONFIGURATION_TYPES)
            message(WARNING
                    "NexusWave: sanitizers are enabled with CMAKE_BUILD_TYPE='${CMAKE_BUILD_TYPE}'. "
                    "Debug is recommended so inlined frames keep usable symbols.")
        endif()
    endif()
endfunction()

function(_newave_sanitizer_summary out_var)
    set(_names "")
    if(NEXUSWAVE_ENABLE_ASAN)
        list(APPEND _names "ASan")
    endif()
    if(NEXUSWAVE_ENABLE_UBSAN)
        list(APPEND _names "UBSan")
    endif()
    if(NEXUSWAVE_ENABLE_TSAN)
        list(APPEND _names "TSan")
    endif()
    if(_names)
        list(JOIN _names "+" _joined)
        set(${out_var} "${_joined}" PARENT_SCOPE)
    else()
        set(${out_var} "OFF" PARENT_SCOPE)
    endif()
endfunction()

_newave_validate_sanitizers()
_newave_is_msvc_style(_newave_msvc)

# The shared options object. Every target links it through
# newave_compiler_options(), which keeps flags in exactly one place.
add_library(newave_options INTERFACE)
target_compile_features(newave_options INTERFACE cxx_std_20)

if(_newave_msvc)
    target_compile_options(newave_options INTERFACE
            $<$<COMPILE_LANGUAGE:CXX>:
            /utf-8 /wd4819 /W4
            $<$<CONFIG:Debug>:/WX /Od /Zi /MDd /bigobj>
            $<$<CONFIG:Release>:/O2 /Zc:__cplusplus /MD>>
    )
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(newave_options INTERFACE
            -finput-charset=UTF-8 -fexec-charset=UTF-8
            -Wall -Wextra -Wno-unused-result -Wno-psabi
            $<$<CONFIG:Debug>:-Werror -O0 -g -gdwarf-4>
            $<$<CONFIG:Release>:-O2>
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(newave_options INTERFACE -Wno-atomic-alignment)
    endif()
    if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_definitions(newave_options INTERFACE
                $<$<CONFIG:Debug>:_DEBUG>
                _MT
                _DLL
        )
    endif()
else()
    message(FATAL_ERROR "NexusWave: unsupported compiler ${CMAKE_CXX_COMPILER_ID}")
endif()

# Internal: attach the requested sanitizer runtime to one target.
function(_newave_sanitizer_options target)
    if(NOT (NEXUSWAVE_ENABLE_ASAN OR NEXUSWAVE_ENABLE_UBSAN OR NEXUSWAVE_ENABLE_TSAN))
        return()
    endif()
    _newave_sanitizer_flags(_flags)
    if(NOT _flags)
        return()
    endif()
    target_compile_options(${target} PRIVATE ${_flags})
    target_link_options(${target} PRIVATE ${_flags})
endfunction()

# Apply NexusWave's shared options to a target.
#
# Every executable/library in src/ and tests/ must call this so warnings,
# language level, runtime library and sanitizer settings stay aligned.
function(newave_compiler_options target)
    target_link_libraries(${target} PRIVATE newave_options)
    _newave_sanitizer_options(${target})
    _newave_sanitizer_summary(_sanitizers)
    message(STATUS "Configured NexusWave compiler options for ${target} (${CMAKE_BUILD_TYPE}, sanitizers=${_sanitizers})")
endfunction()

_newave_sanitizer_summary(_newave_sanitizers)
if(NOT _newave_sanitizers STREQUAL "OFF")
    message(STATUS "NexusWave sanitizers: ${_newave_sanitizers}")
endif()
