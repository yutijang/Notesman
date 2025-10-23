# -----------------------------------
# Thêm thư viện tĩnh riêng cho SQLite
# -----------------------------------
add_library(sqlite3_wrapper STATIC
    sqlite3.c
)

target_compile_definitions(sqlite3_wrapper
    PRIVATE
        SQLITE_THREADSAFE=1
        SQLITE_ENABLE_FTS5=1
)

if(NOT WIN32)
    target_link_libraries(sqlite3_wrapper
        PRIVATE
            pthread
    )
endif()

target_include_directories(sqlite3_wrapper
    SYSTEM PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)