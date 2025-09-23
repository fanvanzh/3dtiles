CMAKE_MINIMUM_REQUIRED(VERSION 3.22)

PROJECT(3dtiles)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    # 在 macOS 或其他 Apple 系统下
    add_compile_options(-Wno-error=unguarded-availability-new)
endif()

if (WIN32)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}scripts/buildsystems/vcpkg.cmake")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SOURCES src/*.cpp)
file(GLOB_RECURSE HEADERS src/*.h src/json.hpp)
add_library(_3dtile STATIC ${SOURCES})
target_include_directories(_3dtile PUBLIC src)

find_package(GDAL CONFIG REQUIRED)
if (GDAL_FOUND)
    message(STATUS "GDAL found: ${GDAL_VERSION}, include dirs: ${GDAL_INCLUDE_DIRS}, libraries: ${GDAL_LIBRARIES}")
    target_link_libraries(_3dtile PRIVATE GDAL::GDAL)
    target_include_directories(_3dtile PUBLIC ${GDAL_INCLUDE_DIRS})
endif()


set(osg_OPENGL_PROFILE GL3)
find_package(OpenSceneGraph 3.6.5 REQUIRED COMPONENTS osgDB osgUtil osgViewer OpenThreads)
if (OpenSceneGraph_FOUND)
    message(STATUS "OpenSceneGraph include dirs: ${OSG_INCLUDE_DIR}, libraries: ${OSG_LIBRARIES}")
    target_link_libraries(_3dtile PRIVATE osgDB osgUtil osgViewer OpenThreads)
    target_include_directories(_3dtile PUBLIC ${OSG_INCLUDE_DIR})
endif()

install(TARGETS _3dtile DESTINATION lib)
