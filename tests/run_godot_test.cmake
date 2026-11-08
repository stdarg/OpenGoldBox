# Require both a successful process and the script's completion marker. Godot
# script errors do not consistently produce a nonzero process exit status.
set(display_arguments --headless)
set(virtual_display)
if(GRAPHICAL)
    set(display_arguments)
    include("${CMAKE_CURRENT_LIST_DIR}/virtual_display.cmake")
endif()
if(NOT DEFINED TEST_TIMEOUT)
    set(TEST_TIMEOUT 30)
endif()
# USER_HOME gives the check its own, empty Godot user data and settings.cfg,
# so checks run at the same time never share saves or settings.
if(DEFINED USER_HOME)
    file(REMOVE_RECURSE "${USER_HOME}")
    file(MAKE_DIRECTORY "${USER_HOME}")
    foreach(variable HOME APPDATA XDG_DATA_HOME)
        set(ENV{${variable}} "${USER_HOME}")
    endforeach()
    set(ENV{OPENGOLD_SETTINGS_FILE} "${USER_HOME}/settings.cfg")
endif()
execute_process(
    COMMAND ${virtual_display} "${GODOT}" ${display_arguments} --path "${PROJECT}" --script "${SCRIPT}" -- ${ARGS}
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT "${TEST_TIMEOUT}")
message("${output}${errors}")
if(NOT result STREQUAL "0" OR NOT output MATCHES "${EXPECTED}" OR
        "${output}${errors}" MATCHES "SCRIPT ERROR|Assertion failed")
    message(FATAL_ERROR "Godot check failed (${result}): ${SCRIPT}")
endif()
