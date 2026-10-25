add_library(opengold_dos_probe_machine STATIC cpu.cpp ega.cpp machine.cpp png.cpp script_trace.cpp)
target_compile_features(opengold_dos_probe_machine PUBLIC cxx_std_20)
target_include_directories(opengold_dos_probe_machine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opengold_dos_probe_machine PUBLIC opengold_formats)
add_executable(opengold_dos_probe main.cpp)
target_link_libraries(opengold_dos_probe PRIVATE opengold_dos_probe_machine)
if(OPENGOLD_BUILD_TESTS)
    add_executable(opengold_dos_probe_tests cpu_tests.cpp)
    target_link_libraries(opengold_dos_probe_tests PRIVATE opengold_dos_probe_machine)
    add_test(NAME opengold_dos_probe_tests COMMAND opengold_dos_probe_tests)
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME opengold_dos_probe_experiments
            COMMAND "${CMAKE_COMMAND}" "-DPYTHON=${Python3_EXECUTABLE}"
            "-DPROBE=$<TARGET_FILE:opengold_dos_probe>" "-DMAPS=$<TARGET_FILE:opengold_maps>"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/experiments"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/experiments/run_experiments.cmake")
        set_tests_properties(opengold_dos_probe_experiments PROPERTIES SKIP_RETURN_CODE 77)
    endif()
endif()
opengold_enable_warnings("${CMAKE_CURRENT_SOURCE_DIR}")
