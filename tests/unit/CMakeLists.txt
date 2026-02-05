# Unit tests CMakeLists.txt

set(UNIT_TEST_SOURCES
    test_coordinate_converter.cpp
    test_enu_context.cpp
    test_transform_builder.cpp
)

add_executable(unit_tests ${UNIT_TEST_SOURCES})

target_link_libraries(unit_tests PRIVATE
    test_utils
    _3dtile
    GTest::gtest_main
)

# Include directories
target_include_directories(unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
)

if(OpenSceneGraph_FOUND)
    target_include_directories(unit_tests PRIVATE ${OSG_INCLUDE_DIR})
endif()

# Auto-discover tests
gtest_discover_tests(unit_tests
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    PROPERTIES LABELS "unit"
)
