CMAKE_MINIMUM_REQUIRED(VERSION 3.22)

PROJECT(3dtiles)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

find_package(basisu CONFIG REQUIRED)
target_link_libraries(_3dtile PRIVATE basisu::basisu_encoder)
find_package(draco CONFIG REQUIRED)
target_link_libraries(_3dtile PRIVATE draco::draco)
find_package(meshoptimizer CONFIG REQUIRED)
target_link_libraries(_3dtile PRIVATE meshoptimizer::meshoptimizer)

install(TARGETS _3dtile DESTINATION lib)