cmake_minimum_required(VERSION 3.10)

project(dfmplugin-optical)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

FILE(GLOB_RECURSE OPTICAL_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.json"
    )

find_package(Qt5 COMPONENTS Svg REQUIRED)
set(SVGLIB Qt5::Svg)

add_library(${PROJECT_NAME}
    SHARED
    ${OPTICAL_FILES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${DFM_BUILD_PLUGIN_FILEMANAGER_DIR})

target_link_libraries(${PROJECT_NAME}
    ${SVGLIB}
    DFM::base
    DFM::framework
)

#install library file
install(TARGETS
    ${PROJECT_NAME}
    LIBRARY
    DESTINATION
    ${DFM_PLUGIN_FILEMANAGER_EDGE_DIR}
)
