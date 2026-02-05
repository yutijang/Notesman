add_executable(webkitgtk_viewer
    webkitgtk_viewer.cpp
)

if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)

    set(ENV{PKG_CONFIG_PATH}
        "$ENV{PKG_CONFIG_PATH}:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/lib/pkgconfig"
    )

    pkg_check_modules(GTK REQUIRED gtk+-3.0)
    pkg_check_modules(WEBKIT REQUIRED webkit2gtk-4.1)
endif()

target_include_directories(webkitgtk_viewer PRIVATE
    ${GTK_INCLUDE_DIRS}
    ${WEBKIT_INCLUDE_DIRS}
)

target_link_libraries(webkitgtk_viewer PRIVATE
    ${GTK_LIBRARIES}
    ${WEBKIT_LIBRARIES}
)
