add_library(viewer_pack STATIC
    ViewerPackWriter.cpp
    ViewerPackReader.cpp
)

add_library(common::viewer_pack ALIAS viewer_pack)

target_include_directories(viewer_pack
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
