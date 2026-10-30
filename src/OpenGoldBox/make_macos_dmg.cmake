# Writes the disk image players download, then opens it and checks the app in
# it still verifies and starts. Also writes the debug symbols that turn a crash
# report's addresses into source lines; those are kept by the developer, not
# shipped.
#
#   APP_BUNDLE  mac-package/OpenGoldBox.app, already packaged and signed
#   LIBRARY     the game's GDExtension as built, whose objects hold the debug info
#   DMG         disk image to write
#   SYMBOLS     zip of the dSYM to write
#   WORK        scratch folder

function(run_or_stop)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE result ERROR_VARIABLE errors OUTPUT_QUIET)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Failed (${result}): ${ARGN}\n${errors}")
    endif()
endfunction()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/image")
run_or_stop(ditto "${APP_BUNDLE}" "${WORK}/image/OpenGoldBox.app")
file(CREATE_LINK /Applications "${WORK}/image/Applications" SYMBOLIC)
run_or_stop(hdiutil create -ov -volname OpenGoldBox -fs APFS -format ULFO
    -srcfolder "${WORK}/image" "${DMG}")
run_or_stop(hdiutil verify "${DMG}")

set(mount_point "${WORK}/mounted")
file(MAKE_DIRECTORY "${mount_point}")
run_or_stop(hdiutil attach -nobrowse -readonly -mountpoint "${mount_point}" "${DMG}")
execute_process(COMMAND codesign --verify --deep --strict "${mount_point}/OpenGoldBox.app"
    RESULT_VARIABLE signature_result ERROR_VARIABLE signature_errors)
# A throwaway home: this launch must not touch the developer's settings or saves.
set(ENV{HOME} "${WORK}/home")
execute_process(
    COMMAND "${mount_point}/OpenGoldBox.app/Contents/MacOS/OpenGoldBox" --headless --quit-after 30
    RESULT_VARIABLE launch_result OUTPUT_QUIET ERROR_VARIABLE launch_errors TIMEOUT 120)
run_or_stop(hdiutil detach "${mount_point}")
if(NOT signature_result EQUAL 0)
    message(FATAL_ERROR "The app in the disk image does not verify: ${signature_errors}")
endif()
if(NOT launch_result EQUAL 0)
    message(FATAL_ERROR "The app in the disk image does not start (${launch_result}): ${launch_errors}")
endif()

get_filename_component(library_name "${LIBRARY}" NAME)
run_or_stop(dsymutil "${LIBRARY}" -o "${WORK}/${library_name}.dSYM")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar cf "${SYMBOLS}" --format=zip "${library_name}.dSYM"
    WORKING_DIRECTORY "${WORK}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Cannot write the debug symbols: ${SYMBOLS}")
endif()
file(REMOVE_RECURSE "${WORK}")
message(STATUS "macOS package: ${DMG}; symbols: ${SYMBOLS}")
