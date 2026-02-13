# Tạo executable cho test
add_executable(notes-core-tests
    # test_main.cpp    
    test_model_utils.cpp
    test_sqldb_raii.cpp
    test_resource_repository.cpp
    test_text_content_repository.cpp
    test_tag_repository.cpp
    test_file_repository.cpp
    test_resource_service.cpp
    test_file_service.cpp
)

# Liên kết với notes-core và Catch2
target_link_libraries(notes-core-tests
    PRIVATE
        notes-core              # thư viện logic chính
        Boost::url
        Catch2::Catch2WithMain  # target có sẵn main() của Catch2 v3
)

catch_discover_tests(notes-core-tests)