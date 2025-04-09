cmake_minimum_required(VERSION 3.10)

project(test-dfmplugin-search)

set(PluginPath ${PROJECT_SOURCE_PATH}/plugins/filemanager/dfmplugin-search/)

find_package(PkgConfig REQUIRED)
find_package(Boost REQUIRED
    COMPONENTS
    system
)
find_package(dfm-search REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS DBus)

#full-text search dependences
pkg_check_modules(Lucene REQUIRED IMPORTED_TARGET liblucene++ liblucene++-contrib)
pkg_check_modules(Docparser REQUIRED IMPORTED_TARGET docparser)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(PCRE REQUIRED libpcre)

# use TextIndex interface
qt5_add_dbus_interface(INTERFACE_SOURCES ${DFM_DBUS_XML_DIR}/org.deepin.Filemanager.TextIndex.xml textindex_interface)

# UT文件
file(GLOB_RECURSE UT_CXX_FILE
    FILES_MATCHING PATTERN "*.cpp" "*.h")
file(GLOB_RECURSE SRC_FILES
    FILES_MATCHING PATTERN "${PluginPath}/*.cpp" "${PluginPath}/*.h")

add_executable(${PROJECT_NAME}
    ${SRC_FILES}
    ${UT_CXX_FILE}
    ${CPP_STUB_SRC}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${PluginPath}
    ${DtkWidget_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${PCRE_INCLUDE_DIRS}
    ${dfm-search_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/3rdparty
    ${dfm-search_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    DFM::base
    DFM::framework
    ${DtkWidget_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${PCRE_LIBRARIES}
    PkgConfig::Lucene
    PkgConfig::Docparser
    ${Boost_LIBRARIES}
    dfm-search
)

add_test(
  NAME search
  COMMAND $<TARGET_FILE:${PROJECT_NAME}>
)
