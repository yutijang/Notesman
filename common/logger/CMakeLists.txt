add_library(common_logger STATIC
    Logger.cpp
)

add_library(common::logger ALIAS common_logger)

target_link_libraries(common_logger
    PUBLIC
        spdlog::spdlog
)

target_include_directories(common_logger
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
