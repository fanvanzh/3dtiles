# Tests CMakeLists.txt

# Enable testing
enable_testing()

# Find gtest
find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

# Test utilities
add_library(test_utils STATIC
    utils/test_helpers.cpp
)
target_link_libraries(test_utils PUBLIC
    _3dtile
    GTest::gtest
    glm::glm-header-only
)
target_include_directories(test_utils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

# Add subdirectories
add_subdirectory(unit)
