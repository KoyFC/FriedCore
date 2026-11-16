add_executable(fried_core_tests
    main.cpp
    build_target_test.cpp
    build_test.cpp
    engine_location_test.cpp
    project_test.cpp
)
target_link_libraries(fried_core_tests PRIVATE fried_core)
target_compile_definitions(fried_core_tests PRIVATE FRIED_CORE_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data")

add_test(NAME fried_core_tests COMMAND fried_core_tests)
