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
add_core_test(test_emission   unit/emission_reference_test.cpp)
add_core_test(test_probe_reference probe_reference.cpp)
add_core_test(test_bench_stats unit/bench_stats_test.cpp)

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

    # Comparator self-verification (spec 0002 §"Testing"): no GPU needed.
    add_test(NAME test_compare
        COMMAND ${PYTHON3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/gpu/test_compare.py)
    set_tests_properties(test_compare PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    # Performance-gate self-verification (ticket 0013): the ±10 % band, the
    # reference-GPU budgets, the CSV contract and the fail-beats-skip rule are
    # exercised on synthetic rows — no GPU, so it runs everywhere while
    # test_perf_gate is the one that needs hardware.
    add_test(NAME test_bench_gate
        COMMAND ${PYTHON3_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/bench/test_gate.py)
    set_tests_properties(test_bench_gate PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    # Performance gate against the committed baseline (ticket 0013, standard
    # §4): every case is first normalised against a correctness render of the
    # same config, then timed with --bench and compared with bench/baseline.json
    # inside the ±10 % band and the reference-GPU budgets. Different
    # GL_RENDERER than the baseline records → honest skip (exit 2), never a
    # pass, and never masking a failure that was already found.
    add_test(NAME test_perf_gate
        COMMAND ${PYTHON3_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/bench/gate.py
                --renderer $<TARGET_FILE:${PROJECT_NAME}>
                --baseline ${CMAKE_SOURCE_DIR}/bench/baseline.json
                --workdir ${CMAKE_CURRENT_BINARY_DIR}/perf_gate)
    set_tests_properties(test_perf_gate PROPERTIES
        SKIP_RETURN_CODE 2
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    # GPU↔CPU probe cross-validation (ticket 0012, seam S3): the 48 shared
    # records of core/probe_cases.h are evaluated twice — by the core-only
    # probe_reference in double precision and by shaders/probe.comp as float32
    # GPU work items — and compared. Budget justified in probe_check.py
    # (rtol 1e-5 = ~85 ulp of float32, atol 1e-4 = ~0.6 ulp at the largest
    # absolute deviation measured).
    add_test(NAME test_probe_gpu_cpu
        COMMAND ${PYTHON3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/gpu/probe_check.py
                --renderer $<TARGET_FILE:${PROJECT_NAME}>
                --probe-ref $<TARGET_FILE:test_probe_reference>
                --workdir ${CMAKE_CURRENT_BINARY_DIR}/probe_check)
    set_tests_properties(test_probe_gpu_cpu PROPERTIES
        SKIP_RETURN_CODE 2
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    # Tolerance/threshold justification (standards §3 "Justify every
    # tolerance"): 1e-3 is ~1/4 of one 8-bit code value (1/255 = 3.9e-3), so a
    # passing comparison is already invisible in the quantised viewable form.
    # The jetoff scene bypasses this with 0.0 — byte identity with the pre-jet
    # baseline is a contract, not a tolerance (see tests/reference/README.md).
    add_test(NAME test_regression_golden
        COMMAND ${PYTHON3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/gpu/regression_check.py
                --renderer $<TARGET_FILE:${PROJECT_NAME}>
                --reference-dir ${CMAKE_SOURCE_DIR}/tests/reference
                --tolerance 1e-3
                --threshold 1e-3)
    set_tests_properties(test_regression_golden PROPERTIES
        SKIP_RETURN_CODE 2
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    # Full-render numeric sanity (ticket 0012, seam S4): the golden answers
    # "does the frame match the reference?", this answers "is the frame
    # physically sane?" — finite, non-negative, and inside the ADR-0001
    # tone-mapped ceiling of 1.0 (slack justified in no_nan_check.py).
    add_test(NAME test_no_nan
        COMMAND ${PYTHON3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/gpu/no_nan_check.py
                --renderer $<TARGET_FILE:${PROJECT_NAME}>
                --reference-dir ${CMAKE_SOURCE_DIR}/tests/reference
                --workdir ${CMAKE_CURRENT_BINARY_DIR}/no_nan)
    set_tests_properties(test_no_nan PROPERTIES
        SKIP_RETURN_CODE 2
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    # ── The gate publishes only alongside a passing correctness suite ──────────
    # Ticket 0013's last criterion: every perf result is published alongside a
    # passing correctness suite. Every other test in this file provides the
    # `correctness` fixture and test_perf_gate requires it, so a failing
    # correctness test leaves the gate Not Run instead of standing behind
    # timings nobody should trust — and `ctest -R test_perf_gate` pulls the
    # whole correctness suite in with it automatically.
    #
    # (Verified CTest semantics: a *failed* setup marks the dependent Not Run
    # and fails the run; a *skipped* setup does not block it — which is what we
    # want, since on a machine with no GL context the correctness tests skip
    # and the gate skips by itself, exit 2.)
    set(_correctness_tests
        test_smoke test_metric test_integrator test_config test_emission
        test_probe_reference test_bench_stats
        test_headless_render test_compare test_bench_gate
        test_probe_gpu_cpu test_regression_golden test_no_nan)
    set_tests_properties(${_correctness_tests}
        PROPERTIES FIXTURES_SETUP correctness)
    set_tests_properties(test_perf_gate PROPERTIES FIXTURES_REQUIRED correctness)
else()
    # Not silent: the headless determinism check needs the Python 3 stdlib.
    message(WARNING
        "python3 not found — the GPU checks (test_headless_render, "
        "test_probe_gpu_cpu, test_no_nan) will NOT be registered. "
        "Install python3 to enable them.")
endif()
