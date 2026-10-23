add_library(nexuswave_core STATIC
        agent/json_util.cpp
        agent/policy.cpp
        agent/provider.cpp
        agent/runtime.cpp
        agent/session.cpp
        agent/sse.cpp
        agent/tools.cpp
        app/app_state.cpp
        app/commands.cpp
        app/keymap.cpp
        app/shell.cpp
        ui/views.cpp
)
target_include_directories(nexuswave_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(nexuswave_core PUBLIC NexusForce::NexusForce)
newave_compiler_options(nexuswave_core)

add_executable(nexuswave main.cpp)
target_link_libraries(nexuswave PRIVATE nexuswave_core)
newave_compiler_options(nexuswave)
