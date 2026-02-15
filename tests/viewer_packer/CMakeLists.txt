add_executable(viewer-pack-writer
    test_ViewerPackWriter.cpp
)

target_link_libraries(viewer-pack-writer
    PRIVATE
        common::viewer_pack
        Catch2::Catch2WithMain  # target có sẵn main() của Catch2 v3
)

catch_discover_tests(viewer-pack-writer)