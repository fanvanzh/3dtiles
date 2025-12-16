CMAKE_MINIMUM_REQUIRED(VERSION 3.22)

PROJECT(3dtiles)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Strict C++ compilation flags to match CI (Ubuntu GCC)
# Set ENABLE_STRICT_CHECKS=ON to enable strict mode locally
option(ENABLE_STRICT_CHECKS "Enable strict compilation checks (like CI)" OFF)

if(ENABLE_STRICT_CHECKS)
    message(STATUS "Strict compilation mode enabled (matching CI behavior)")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(
            -Wall                    # Enable all common warnings
            -Wextra                  # Enable extra warnings
            -Wpedantic               # Strict ISO C++ compliance
            -Werror=reorder          # Error on member initialization order issues
            -Werror=designated-init  # Error on designated initializer issues (GCC specific)
        )
    endif()
else()
    message(STATUS "Normal compilation mode (use -DENABLE_STRICT_CHECKS=ON for CI-like strictness)")
endif()

# vcpkg integration
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

# Add draco compression support
option(ENABLE_OPTIMIZER "Enable optimizer" ON)
if(ENABLE_OPTIMIZER)
    add_definitions(-DOPTIMIZER)
endif()

file(GLOB_RECURSE SOURCES src/*.cpp)
file(GLOB_RECURSE HEADERS src/*.h src/json.hpp)
add_library(_3dtile STATIC ${SOURCES})
target_include_directories(_3dtile PUBLIC src)

find_package(spdlog CONFIG REQUIRED)
target_link_libraries(_3dtile PUBLIC spdlog::spdlog_header_only)

# fmt (needed by spdlog and project sources)
find_package(fmt CONFIG REQUIRED)
target_link_libraries(_3dtile PUBLIC fmt::fmt-header-only)

if(APPLE)
    # 在 macOS 或其他 Apple 系统下
    add_compile_options(-Wno-error=unguarded-availability-new)

    # macOS M1 specific settings
    if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "arm64")
        set(CMAKE_OSX_ARCHITECTURES "arm64")
        message(STATUS "Building for macOS M1 (arm64)")
    endif()

    # Set RPATH for macOS
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH "@loader_path")
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

    # Explicitly link OpenThreads on arm64 to resolve undefined symbols
    find_package(OpenThreads REQUIRED)
    if(OpenThreads_FOUND)
        target_link_libraries(_3dtile PRIVATE ${OPENTHREADS_LIBRARY})
        target_include_directories(_3dtile PUBLIC ${OPENTHREADS_INCLUDE_DIR})
    endif()
endif()

if (WIN32)
    target_compile_definitions(_3dtile PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_precompile_headers(_3dtile PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/osg_fix.h")
endif()

# Find packages using vcpkg
find_package(GDAL CONFIG REQUIRED)
if (GDAL_FOUND)
    message(STATUS "GDAL found: ${GDAL_VERSION}, include dirs: ${GDAL_INCLUDE_DIRS}, libraries: ${GDAL_LIBRARIES}")
    target_link_libraries(_3dtile PRIVATE GDAL::GDAL)
endif()

# Find OpenSceneGraph with all required components
set(osg_OPENGL_PROFILE GL3)
find_package(OpenSceneGraph REQUIRED COMPONENTS
    osg
    osgDB
    osgUtil
    OpenThreads
)

if (OpenSceneGraph_FOUND)
    message(STATUS "OpenSceneGraph include dirs: ${OSG_INCLUDE_DIR}, libraries: ${OSG_LIBRARIES}")
    target_link_libraries(_3dtile PRIVATE
        osg
        osgDB
        osgUtil
        OpenThreads
    )
    # On Linux/macOS, OSG plugins are static libraries that must be linked
    # On Windows, plugins may be dynamic and loaded at runtime
    if(UNIX)
        target_link_libraries(_3dtile PRIVATE osgdb_osg)
    endif()
    target_include_directories(_3dtile PUBLIC ${OSG_INCLUDE_DIR})
endif()

# basis-universal
find_package(basisu CONFIG REQUIRED)
target_link_libraries(_3dtile PRIVATE basisu::basisu_encoder)

# zstd
find_package(zstd CONFIG REQUIRED)
target_link_libraries(_3dtile PRIVATE zstd::libzstd)

# draco
find_package(draco CONFIG REQUIRED)
target_link_libraries(_3dtile PRIVATE draco::draco)

# meshoptimizer
find_package(meshoptimizer CONFIG REQUIRED)
target_link_libraries(_3dtile PRIVATE meshoptimizer::meshoptimizer)

# glm
find_package(glm CONFIG REQUIRED)
target_link_libraries(_3dtile PRIVATE glm::glm-header-only)

# earcut.hpp
find_path(EARCUT_HPP_INCLUDE_DIRS "mapbox/earcut.hpp")
target_include_directories(_3dtile PRIVATE ${EARCUT_HPP_INCLUDE_DIRS})

# tinygltf
find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")
target_include_directories(_3dtile PRIVATE ${TINYGLTF_INCLUDE_DIRS})

# stb
find_package(Stb REQUIRED)
target_include_directories(_3dtile PUBLIC ${Stb_INCLUDE_DIR})

# nlohmann_json(json.hpp)
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(_3dtile PUBLIC nlohmann_json::nlohmann_json)

# Eigen3
find_package(Eigen3 CONFIG REQUIRED)
target_link_libraries(_3dtile PUBLIC Eigen3::Eigen)

# sqlite3
find_package(unofficial-sqlite3 CONFIG REQUIRED)
target_link_libraries(_3dtile PRIVATE unofficial::sqlite3::sqlite3)

install(TARGETS _3dtile DESTINATION lib)