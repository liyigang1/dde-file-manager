cmake_minimum_required(VERSION 3.10)

project(daemonplugin-opticalshare)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(OpticalShare_XML ${DFM_DBUS_XML_DIR}/org.deepin.Filemanager.OpticalShare.xml)

FILE(GLOB FILEOPERATIONS_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.json"
    "${OpticalShare_XML}"
    )

find_package(Qt5 COMPONENTS
    DBus
    REQUIRED
)

qt5_add_dbus_adaptor(FILEOPERATIONS_FILES ${OpticalShare_XML}
    opticalsharedbus.h OpticalShareDBus)

add_library(${PROJECT_NAME}
    SHARED
    ${FILEOPERATIONS_FILES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ../../)

target_link_libraries(${PROJECT_NAME}
    DFM::framework
    DFM::base
    Qt5::DBus
)

#install library file
install(TARGETS
    ${PROJECT_NAME}
    LIBRARY
    DESTINATION
    ${DFM_PLUGIN_DAEMON_EDGE_DIR}
)
