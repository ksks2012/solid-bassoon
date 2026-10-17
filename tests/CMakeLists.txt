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
