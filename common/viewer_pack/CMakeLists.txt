add_library(viewer_pack STATIC
    ViewerPackWriter.cpp
    ViewerPackReader.cpp
    SanitizeFileName.cpp
    ViewerPackCrc32.cpp
)

add_library(common::viewer_pack ALIAS viewer_pack)

target_include_directories(viewer_pack
    PUBLIC ${PROJECT_SOURCE_DIR}
)
