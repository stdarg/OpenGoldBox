# The game ships creature records; their poses belong to new-art-pack.
cmake_minimum_required(VERSION 3.24)
get_filename_component(repository "${CMAKE_CURRENT_LIST_DIR}/../.." REALPATH)
set(source "${repository}/data/monsters")
file(REAL_PATH "${DESTINATION}" destination)
cmake_path(IS_PREFIX repository "${destination}" NORMALIZE in_repository)
cmake_path(IS_PREFIX source "${destination}" NORMALIZE in_source)
get_filename_component(name "${destination}" NAME)
get_filename_component(parent "${destination}" DIRECTORY)
get_filename_component(parent_name "${parent}" NAME)
if(NOT in_repository OR in_source OR NOT name STREQUAL "monsters" OR
        NOT parent_name STREQUAL "data")
    message(FATAL_ERROR "Invalid monster record staging folder: ${DESTINATION}")
endif()

# Replace old copies too: a previous build may have left pose PNGs here.
file(REMOVE_RECURSE "${destination}")
file(GLOB records RELATIVE "${source}" "${source}/*/monster.json")
foreach(record IN LISTS records)
    get_filename_component(folder "${record}" DIRECTORY)
    file(MAKE_DIRECTORY "${destination}/${folder}")
    file(COPY_FILE "${source}/${record}" "${destination}/${record}")
endforeach()
