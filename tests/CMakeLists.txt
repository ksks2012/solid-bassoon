# Test targets link `blackhole_core` only — never the GL/GLFW/ImGui app.
# Each test file is its own executable so it can be run and inspected alone.

function(add_core_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE blackhole_core)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_core_test(test_smoke      unit/smoke_test.cpp)
add_core_test(test_metric     unit/metric_test.cpp)
add_core_test(test_integrator unit/integrator_test.cpp)
add_core_test(test_config     unit/config_test.cpp)

# ── GPU-dependent tests ───────────────────────────────────────────────────────
# These need a live GL 4.3 compute context. When none exists the renderer exits
# 2, which CTest maps to "Skipped" — never a false pass (ticket 0010).
# The renderer loads shaders relative to the repo root, hence WORKING_DIRECTORY.
find_program(PYTHON3_EXECUTABLE NAMES python3 python)
if(PYTHON3_EXECUTABLE)
    add_test(NAME test_headless_render
        COMMAND ${PYTHON3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/gpu/headless_check.py
                $<TARGET_FILE:${PROJECT_NAME}>
                ${CMAKE_SOURCE_DIR}/etc/config.json
                ${CMAKE_CURRENT_BINARY_DIR}/headless_run_a.bin
                ${CMAKE_CURRENT_BINARY_DIR}/headless_run_b.bin)
    set_tests_properties(test_headless_render PROPERTIES
        SKIP_RETURN_CODE 2
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
else()
    # Not silent: the headless determinism check needs the Python 3 stdlib.
    message(WARNING
        "python3 not found — the GPU determinism check (test_headless_render) "
        "will NOT be registered. Install python3 to enable it.")
endif()
