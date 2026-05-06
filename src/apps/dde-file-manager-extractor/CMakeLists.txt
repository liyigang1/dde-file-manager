cmake_minimum_required(VERSION 3.10)

project(dde-file-manager-textindex-creator)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_definitions(-DDFM_PLUGIN_EXTRACTOR_DIR=\"${DFM_PLUGIN_EXTRACTOR_DIR}\")

# Add subdirectories
add_subdirectory(libtextindexcreator)
add_subdirectory(textindexcreator)
add_subdirectory(plugins)

message(STATUS "DFM: dde-file-manager-textindex-creator configured")
