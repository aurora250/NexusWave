find_package(GTest CONFIG QUIET)
if (NOT TARGET GTest::gtest)
    message(WARNING "GTest not found — NexusWave tests are skipped")
    return()
endif()

add_executable(nexuswave_tests
        unit/test_agent_policy.cpp
        unit/test_agent_provider.cpp
        unit/test_agent_runtime.cpp
        unit/test_agent_sse.cpp
        unit/test_agent_session.cpp
        unit/test_agent_tools.cpp
        unit/test_commands.cpp
        unit/test_keymap.cpp
        unit/test_state.cpp
        unit/test_views.cpp
        unit/test_shell.cpp
        unit/test_shell_agent.cpp
)
target_link_libraries(nexuswave_tests PRIVATE nexuswave_core GTest::gtest_main)
newave_compiler_options(nexuswave_tests)

add_test(NAME nexuswave_tests COMMAND nexuswave_tests)
