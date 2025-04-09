cmake_minimum_required(VERSION 3.10)

project(test-dfmplugin-workspace)

set(PluginPath ${PROJECT_SOURCE_PATH}/plugins/filemanager/core/dfmplugin-workspace)

# UI files
file(GLOB_RECURSE UT_CXX_FILE
    FILES_MATCHING PATTERN "*.cpp" "*.h")
file(GLOB_RECURSE SRC_FILES
    FILES_MATCHING PATTERN "${PluginPath}/*.cpp" "${PluginPath}/*.h")

find_package(Qt5 REQUIRED COMPONENTS DBus)
# use TextIndex interface
set(DBUS_INTERFACE_FILE "${CMAKE_SOURCE_DIR}/assets/dbus/org.deepin.Filemanager.TextIndex.xml")
qt5_add_dbus_interface(INTERFACE_SOURCES ${DBUS_INTERFACE_FILE} textindex_interface)

add_executable(${PROJECT_NAME}
    ${SRC_FILES}
    ${UT_CXX_FILE}
    ${CPP_STUB_SRC}
)

find_package(Dtk COMPONENTS Widget REQUIRED)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${PluginPath}")
target_link_libraries(${PROJECT_NAME} PRIVATE
    DFM::base
    DFM::framework
    ${DtkWidget_LIBRARIES}
)

add_test(
    NAME workspace
    COMMAND $<TARGET_FILE:${PROJECT_NAME}>
)
