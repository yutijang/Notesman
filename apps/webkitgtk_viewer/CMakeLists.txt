add_executable(webkitgtk_viewer
    webkitgtk_viewer.cpp
)

if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)

    set(LIB_PATH "/usr/lib/x86_64-linux-gnu/pkgconfig")
    set(SYS_PATH "/usr/lib/pkgconfig")

    if(EXISTS "${LIB_PATH}")
        set(ENV{PKG_CONFIG_PATH} "${LIB_PATH}:${SYS_PATH}:$ENV{PKG_CONFIG_PATH}")
    else()
        message(WARNING "Path ${LIB_PATH} not found. Build might use wrong Harfbuzz version!")
    endif()

    pkg_check_modules(GTK REQUIRED gtk+-3.0)
    pkg_check_modules(WEBKIT REQUIRED webkit2gtk-4.1)
endif()

target_include_directories(webkitgtk_viewer
    PRIVATE
        ${GTK_INCLUDE_DIRS}
        ${WEBKIT_INCLUDE_DIRS}
)

target_link_libraries(webkitgtk_viewer
    PRIVATE
        ${GTK_LIBRARIES}
        ${WEBKIT_LIBRARIES}
        project_warnings
)
