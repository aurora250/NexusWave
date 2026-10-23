if (WIN32)
    find_program(CLANG_FORMAT_EXECUTABLE
            NAMES clang-format
            DOC "clang-format executable"
    )
elseif(UNIX AND NOT APPLE)
    find_program(CLANG_FORMAT_EXECUTABLE
            NAMES clang-format-19
            DOC "clang-format executable"
    )
endif()

if(NOT CLANG_FORMAT_EXECUTABLE)
    message(WARNING "clang-format not found. Code formatting targets will not be available.")
    return()
endif()

message(STATUS "Found clang-format: ${CLANG_FORMAT_EXECUTABLE}")

file(GLOB_RECURSE NEXUSWAVE_FORMAT_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.hpp"
)

list(FILTER NEXUSWAVE_FORMAT_SOURCES EXCLUDE REGEX ".*/build/.*")
list(FILTER NEXUSWAVE_FORMAT_SOURCES EXCLUDE REGEX ".*/vcpkg_installed/.*")

list(LENGTH NEXUSWAVE_FORMAT_SOURCES NUM_SOURCES)
message(STATUS "Found ${NUM_SOURCES} source files for formatting")

if(NUM_SOURCES EQUAL 0)
    message(STATUS "No source files found for formatting. Skipping format targets.")
    return()
endif()

set(FORMAT_COMMANDS "")
foreach(SOURCE_FILE ${NEXUSWAVE_FORMAT_SOURCES})
    list(APPEND FORMAT_COMMANDS
            COMMAND ${CLANG_FORMAT_EXECUTABLE} -style=file -i "${SOURCE_FILE}"
    )
endforeach()

add_custom_target(format
        ${FORMAT_COMMANDS}
        COMMENT "Formatting ${NUM_SOURCES} files with ${CLANG_FORMAT_EXECUTABLE}..."
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        VERBATIM
)
