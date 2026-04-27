cmake_minimum_required(VERSION 3.10)

add_definitions(-DDFM_PLUGIN_EXTRACTOR_DIR=\"${DFM_PLUGIN_EXTRACTOR_DIR}\")

add_subdirectory(textindex)
add_subdirectory(usbrepair)
