# The Godot the checks run: on macOS, a copy of the installed Godot that never
# shows in the Dock, so a test run does not fill the player's Dock with Godot
# icons. Godot makes itself a Dock app when it starts, whatever its bundle
# says, so the copy is started with tools/godot_out_of_dock/no_dock.c loaded,
# which turns that request into a UI element's (windows, no Dock icon). The
# copy is ad-hoc signed without the hardened runtime, which would refuse the
# library. The installed Godot is not changed; the export, packaging and the
# developer tools' own windows keep using it. Elsewhere the checks use the
# installed Godot itself.
function(opengold_check_godot godot result)
    set(${result} "${godot}" PARENT_SCOPE)
    if(NOT APPLE)
        return()
    endif()
    # The installed Godot's bundle: .../Godot_mono.app/Contents/MacOS/Godot.
    get_filename_component(macos "${godot}" DIRECTORY)
    get_filename_component(contents "${macos}" DIRECTORY)
    get_filename_component(bundle "${contents}" DIRECTORY)
    get_filename_component(executable "${godot}" NAME)
    if(NOT bundle MATCHES "\\.app$")
        return()
    endif()
    set(folder "${CMAKE_BINARY_DIR}/godot-out-of-dock")
    set(launcher "${folder}/godot")
    set(source "${PROJECT_SOURCE_DIR}/tools/godot_out_of_dock/no_dock.c")
    # Made again when the installed Godot or the library's source changes.
    file(TIMESTAMP "${godot}" installed "%s")
    file(SHA256 "${source}" library)
    set(stamp "${godot} ${installed} ${library}")
    if(EXISTS "${folder}/stamp" AND EXISTS "${launcher}")
        file(READ "${folder}/stamp" made)
        if(made STREQUAL stamp)
            set(${result} "${launcher}" PARENT_SCOPE)
            return()
        endif()
    endif()
    file(REMOVE_RECURSE "${folder}")
    file(MAKE_DIRECTORY "${folder}")
    set(copy "${folder}/Godot.app")
    # An APFS clone: no space is used until either copy changes.
    execute_process(COMMAND cp -cR "${bundle}" "${copy}" RESULT_VARIABLE copied)
    execute_process(COMMAND /usr/libexec/PlistBuddy -c "Delete :LSUIElement"
                    "${copy}/Contents/Info.plist" OUTPUT_QUIET ERROR_QUIET)
    execute_process(COMMAND /usr/libexec/PlistBuddy -c "Add :LSUIElement bool true"
                    "${copy}/Contents/Info.plist" RESULT_VARIABLE marked)
    execute_process(COMMAND codesign --force --deep --sign - "${copy}"
                    RESULT_VARIABLE signed OUTPUT_QUIET ERROR_QUIET)
    execute_process(COMMAND xcrun clang -dynamiclib -arch arm64 -arch x86_64 -framework AppKit
                            -framework ApplicationServices -o "${folder}/no_dock.dylib" "${source}"
                    RESULT_VARIABLE compiled)
    if(NOT copied EQUAL 0 OR NOT marked EQUAL 0 OR NOT signed EQUAL 0 OR NOT compiled EQUAL 0)
        message(WARNING "The checks' out-of-Dock Godot could not be made; they use ${godot}")
        file(REMOVE_RECURSE "${folder}")
        return()
    endif()
    file(WRITE "${launcher}"
         "#!/bin/sh\n"
         "# Godot for the checks, kept out of the Dock (cmake/godot_out_of_dock.cmake).\n"
         "DYLD_INSERT_LIBRARIES=\"${folder}/no_dock.dylib\" exec \"${copy}/Contents/MacOS/${executable}\" \"$@\"\n")
    file(CHMOD "${launcher}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ
         GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    file(WRITE "${folder}/stamp" "${stamp}")
    set(${result} "${launcher}" PARENT_SCOPE)
endfunction()
