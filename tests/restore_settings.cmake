# A settings check that was stopped (settings_check.cmake) leaves its own
# settings.cfg in the project folder and the developer's file, if any, in KEPT.
# Every Godot check would then read the check's settings, so they are put back
# before any check runs.
if(NOT EXISTS "${KEPT}")
    return()
endif()
if(EXISTS "${KEPT}/settings.cfg")
    file(COPY_FILE "${KEPT}/settings.cfg" "${PROJECT}/settings.cfg")
else()
    file(REMOVE "${PROJECT}/settings.cfg")
endif()
file(REMOVE_RECURSE "${KEPT}")
message("Restored the settings a stopped settings check left behind")
