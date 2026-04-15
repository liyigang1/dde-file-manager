cmake_minimum_required(VERSION 3.10)

project(dde-filemanager-usbrepair)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(UsbRepair_XML ${DFM_DBUS_XML_DIR}/org.deepin.Filemanager.UsbRepair.xml)

FILE(GLOB_RECURSE SRC_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.xml"
    )

find_package(PkgConfig REQUIRED)
find_package(Qt5 COMPONENTS Core DBus REQUIRED)

# PolKit dependencies
pkg_check_modules(PolkitAgent REQUIRED polkit-agent-1)
pkg_check_modules(PolkitQt5 REQUIRED polkit-qt5-1)

# D-Bus adaptor generation
qt5_add_dbus_adaptor(SRC_FILES ${UsbRepair_XML}
    usbrepairdbus.h UsbRepairDBus)

add_library(${PROJECT_NAME}
    SHARED
    ${SRC_FILES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ../../)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/private
        ${CMAKE_SOURCE_DIR}/include
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${PolkitAgent_INCLUDE_DIRS}
        ${PolkitQt5_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}
    DFM::base
    Qt5::Core
    Qt5::DBus
    ${PolkitAgent_LIBRARIES}
    ${PolkitQt5_LIBRARIES}
)

# Install DSM plugin .so
install(TARGETS ${PROJECT_NAME}
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/deepin-service-manager/)

# Install DSM service JSON descriptor (system directory for root services)
install(FILES ${PROJECT_NAME}.json
    DESTINATION share/deepin-service-manager/system/)

# Install D-Bus system service (2 args = system service with User=root)
include(${CMAKE_SOURCE_DIR}/install_dbus_service.cmake)
INSTALL_DBUS_SERVICE(org.deepin.Filemanager.UsbRepair system)

# Install D-Bus security policy
install(FILES org.deepin.filemanager.usbrepair.conf
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/dbus-1/system.d/)

# Install PolKit policy
set(PolicyDir "${CMAKE_INSTALL_PREFIX}/share/polkit-1/actions")
install(FILES org.deepin.filemanager.usbrepair.policy
    DESTINATION ${PolicyDir})

# Install systemd security hardening drop-in
install(FILES systemd/security-hardening.conf
    DESTINATION
    lib/systemd/system/deepin-service-plugin@org.deepin.Filemanager.UsbRepair.service.d/)
