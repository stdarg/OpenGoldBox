# Apply only to project-owned native targets, including standalone dev tools.
# Keep fetched dependencies on their own warning policy.
include_guard(GLOBAL)

function(opengold_enable_warnings directory)
    if(MSVC)
        # getenv and sscanf are standard C++; MSVC's advice to use its _s variants
        # is not portable.
        set(OPENGOLD_WARNING_FLAGS /W4 /permissive- /D_CRT_SECURE_NO_WARNINGS)
    else()
        set(OPENGOLD_WARNING_FLAGS -Wall -Wextra -Wno-missing-field-initializers)
        # Clang can also report the 64-to-32-bit narrowing MSVC's /W4 warns about.
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            list(APPEND OPENGOLD_WARNING_FLAGS -Wshorten-64-to-32)
        endif()
        # GCC 14, which builds the Linux package for players (tools/linux_release_build.sh),
        # reports std::optional members read behind a has_value() check as maybe
        # uninitialized. GCC 15 and later check the same code without these false alarms.
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15)
            list(APPEND OPENGOLD_WARNING_FLAGS -Wno-maybe-uninitialized)
        endif()
    endif()
    get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target IN LISTS targets)
        get_target_property(type ${target} TYPE)
        if(type MATCHES "^(EXECUTABLE|STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|OBJECT_LIBRARY)$")
            target_compile_options(${target} PRIVATE ${OPENGOLD_WARNING_FLAGS})
            set_target_properties(${target} PROPERTIES COMPILE_WARNING_AS_ERROR ON)
        endif()
    endforeach()
endfunction()
