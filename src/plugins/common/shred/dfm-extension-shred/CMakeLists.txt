cmake_minimum_required(VERSION 2.8)

# 设置项目名称
project(dfm-extension-shred)

set(CMAKE_CXX_STANDARD 17)

find_package(Qt5 COMPONENTS Core Widgets REQUIRED)
find_package(Dtk COMPONENTS Widget REQUIRED)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS
    "./*.h"
    "./*.cpp"
)

add_library(${PROJECT_NAME} SHARED ${SRCS})

target_link_libraries(${PROJECT_NAME}
    PUBLIC 
    DFM::base
    DFM::extension
    Qt5::Core
    Qt5::Widgets
    ${DtkWidget_LIBRARIES}
    )

set_target_properties(${PROJECT_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ../../../extensions/)

# 安裝配置
include(GNUInstallDirs)
if(NOT DEFINED LIB_INSTALL_DIR)
    set(LIB_INSTALL_DIR ${CMAKE_INSTALL_FULL_LIBDIR})
endif()

if(NOT DEFINED DFM_EXT_PLUGIN_DIR)
    set(DFM_EXT_PLUGIN_DIR ${LIB_INSTALL_DIR}/dde-file-manager/plugins/extensions)
endif()

# 安裝插件
install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${DFM_EXT_PLUGIN_DIR})

# install dconfig files.
include(${CMAKE_SOURCE_DIR}/install_dconfig.cmake)
INSTALL_DCONFIG("org.deepin.dde.file-manager.shred.json")

