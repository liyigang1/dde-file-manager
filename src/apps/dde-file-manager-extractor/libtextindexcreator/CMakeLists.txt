cmake_minimum_required(VERSION 3.10)

project(dde-file-manager-textindex-creator-lib)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(LIB_NAME ${PROJECT_NAME})

set(TEXTINDEXCREATOR_LIB_FILES
    controllerpipe.cpp
    textindex_creator_ipc_logging.cpp
    workerpipe.cpp
    controllerpipe.h
    textindexcreatortypes.h
    textindex_creator_ipc_global.h
    workerpipe.h
)

add_library(${LIB_NAME}
    SHARED
    ${TEXTINDEXCREATOR_LIB_FILES}
)

set_target_properties(${LIB_NAME} PROPERTIES
    OUTPUT_NAME dde-file-manager-textindex-creator
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/..
    VERSION 1.0.0
    SOVERSION 1
)

# Find dependencies
find_package(Qt5 COMPONENTS Core REQUIRED)

target_link_libraries(${LIB_NAME}
    PUBLIC
        Qt5::Core
    PRIVATE
        DFM::base
)

target_include_directories(${LIB_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Install the private runtime library
install(TARGETS ${LIB_NAME}
    LIBRARY
    DESTINATION ${DFM_PLUGIN_EXTRACTOR_DIR}
)

message(STATUS "DFM: textindex creator library configured")
