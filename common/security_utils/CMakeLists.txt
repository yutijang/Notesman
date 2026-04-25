# INTERFACE chỉ dùng cho header-only.
add_library(security_utils INTERFACE)

target_include_directories(security_utils
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(security_utils
    INTERFACE
        bcrypt
        shell32
)
