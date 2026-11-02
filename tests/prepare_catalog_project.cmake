# Imports the Combat Sprite Catalog's project. A fresh project's first import can
# crash after writing what the checks need, so its extension list decides.
execute_process(COMMAND "${GODOT}" --headless --editor --path "${PROJECT}" --import --quit
    RESULT_VARIABLE result TIMEOUT 240)
file(SIZE "${PROJECT}/.godot/extension_list.cfg" extension_list_size)
if(NOT extension_list_size GREATER 0)
    message(FATAL_ERROR "The catalog's project did not import (${result})")
endif()
