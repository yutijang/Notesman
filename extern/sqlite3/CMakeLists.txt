# Thêm một thư viện tĩnh riêng cho SQLite
add_library(sqlite3_wrapper STATIC
    sqlite3.c # Chỉ file nguồn C
)

# Thiết lập cờ cần thiết cho file C này
target_compile_definitions(sqlite3_wrapper PRIVATE
    SQLITE_THREADSAFE=1
    SQLITE_ENABLE_FTS5=1
)

# Thêm thư viện pthreads nếu cần
if(NOT WIN32)
    target_link_libraries(sqlite3_wrapper PRIVATE pthread)
endif()

# Đặt các include directories để các target khác có thể thấy
target_include_directories(sqlite3_wrapper SYSTEM PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
) # Thư mục chứa sqlite3.h