cmake_minimum_required(VERSION 3.10)

project(dde-filemanager-textindex)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(TextIndex_XML ${DFM_DBUS_XML_DIR}/org.deepin.Filemanager.TextIndex.xml)
set(DFM_APP_SOURCE_DIR "${CMAKE_SOURCE_DIR}/src/apps" CACHE PATH "App source directory")
add_definitions(-DDFM_PLUGIN_EXTRACTOR_DIR=\"${DFM_PLUGIN_EXTRACTOR_DIR}\")

if(EXISTS "${DFM_APP_SOURCE_DIR}/config.h.in")
    configure_file(
        "${DFM_APP_SOURCE_DIR}/config.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/config.h"
    )
endif()

FILE(GLOB_RECURSE SRC_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.xml"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.policy"
    "${CMAKE_SOURCE_DIR}/include/dfm-base/dfm_log_defines.h"
    )

find_package(PkgConfig REQUIRED)
find_package(Qt5 COMPONENTS Core DBus Gui REQUIRED)
find_package(Dtk COMPONENTS Core)
find_package(dfm-search REQUIRED)

#qt5_generate_dbus_interface(
#   textindexdbus.h
#   ${TextIndex_XML}
#   OPTIONS -M -S
#)

qt5_add_dbus_adaptor(SRC_FILES ${TextIndex_XML}
    textindexdbus.h TextIndexDBus)

add_library(${PROJECT_NAME}
    SHARED
    ${SRC_FILES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ../../)

# Set runtime library search path to find extractor library
set_target_properties(${PROJECT_NAME} PROPERTIES
    INSTALL_RPATH "${DFM_PLUGIN_EXTRACTOR_DIR}"
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH_USE_LINK_PATH FALSE
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${GLIB_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
    DFM::base
    Qt5::Core
    Qt5::DBus
    Qt5::Gui
    Dtk::Core
    dde-file-manager-textindex-creator-lib
    dfm-search
    ${GLIB_LIBRARIES}
)

install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR}/deepin-service-manager/)
install(FILES ${PROJECT_NAME}.json DESTINATION share/deepin-service-manager/user/)
# install dbus
include(${CMAKE_SOURCE_DIR}/install_dbus_service.cmake)
INSTALL_DBUS_SERVICE(org.deepin.Filemanager.TextIndex)
include(${CMAKE_SOURCE_DIR}/install_dconfig.cmake)
INSTALL_DCONFIG("org.deepin.dde.file-manager.textindex.json")

# Install systemd drop-in configuration for resource limits
# This prevents OOM issues without modifying the shared service template
install(FILES
    systemd/memory-limit.conf
    DESTINATION
    lib/systemd/user/deepin-service-plugin@org.deepin.Filemanager.TextIndex.service.d/
)
