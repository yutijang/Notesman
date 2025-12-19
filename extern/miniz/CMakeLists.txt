add_library(miniz_static STATIC
    miniz.c
)

target_include_directories(miniz_static
    SYSTEM PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)