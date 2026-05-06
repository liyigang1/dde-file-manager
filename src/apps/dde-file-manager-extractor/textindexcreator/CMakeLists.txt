cmake_minimum_required(VERSION 3.10)

set(BIN_NAME "dde-file-manager-extractor")

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(FULL_TEXT_PATH "${CMAKE_SOURCE_DIR}/3rdparty/fulltext")

find_package(PkgConfig REQUIRED)
find_package(Qt5 COMPONENTS Core Gui REQUIRED)
find_package(Dtk COMPONENTS Core)
find_package(dfm-search REQUIRED)
#full-text search dependences
pkg_check_modules(Lucene REQUIRED IMPORTED_TARGET liblucene++ liblucene++-contrib)
pkg_check_modules(GLIB REQUIRED glib-2.0)

FILE(GLOB_RECURSE EXTRACTOR_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*/*.cpp"
    )

add_executable(${BIN_NAME}
    ${EXTRACTOR_FILES}
)

target_link_libraries(${BIN_NAME} PRIVATE
    Qt5::Core
    Qt5::Gui
    DFM::base
    Dtk::Core
    dde-file-manager-textindex-creator-lib
    dfm-search
    ${GLIB_LIBRARIES}
    PkgConfig::Lucene
)

target_include_directories(${BIN_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../libtextindexcreator
    ${CMAKE_CURRENT_SOURCE_DIR}/../plugins
    ${CMAKE_SOURCE_DIR}/include
    ${FULL_TEXT_PATH}/..
)

# Enable position-independent executables for improved security
target_link_options(${BIN_NAME} PRIVATE -pie)

set_target_properties(${BIN_NAME} PROPERTIES
    INSTALL_RPATH ${DFM_PLUGIN_EXTRACTOR_DIR}
)

install(TARGETS ${BIN_NAME} DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})

message(STATUS "DFM: dde-file-manager-textindex-creator configured")
