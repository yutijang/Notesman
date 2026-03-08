if(WIN32)
    set(UPDATER_SRC updater_windows.cpp)
elseif(UNIX)
    set(UPDATER_SRC updater_linux.cpp)
endif()

set(UPDATER_PROJECT_NAME updater)
add_executable(${UPDATER_PROJECT_NAME} ${UPDATER_SRC})

target_link_libraries(${UPDATER_PROJECT_NAME}
    PRIVATE    
        $<$<PLATFORM_ID:Windows>:common::logger>
        $<$<PLATFORM_ID:Windows>:miniz::miniz>
)

target_link_options(${UPDATER_PROJECT_NAME}
    PRIVATE
        $<$<PLATFORM_ID:Windows>:/SUBSYSTEM:CONSOLE>
)

target_compile_definitions(${UPDATER_PROJECT_NAME}
    PRIVATE
        $<$<PLATFORM_ID:Windows>:UNICODE>
        $<$<PLATFORM_ID:Windows>:_UNICODE>
        $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
        $<$<PLATFORM_ID:Windows>:NOMINMAX>
)
