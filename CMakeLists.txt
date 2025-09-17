CMAKE_MINIMUM_REQUIRED(VERSION 3.22)

PROJECT(3dtiles)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wno-error=unguarded-availability-new)

set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

file(GLOB_RECURSE SOURCES src/*.cpp)
file(GLOB_RECURSE HEADERS src/*.h src/json.hpp)
add_library(_3dtile STATIC ${SOURCES})
target_include_directories(_3dtile PUBLIC src)

find_package(GDAL CONFIG REQUIRED)
if (GDAL_FOUND)
    message(STATUS "GDAL found: ${GDAL_VERSION}")
endif()

target_link_libraries(_3dtile PRIVATE GDAL::GDAL)
set(osg_OPENGL_PROFILE GL3)
find_package(OpenSceneGraph 3.6.5 REQUIRED COMPONENTS osgDB osgUtil osgViewer OpenThreads)
if (OpenSceneGraph_FOUND)
    message(STATUS "OpenSceneGraph found: ${OpenSceneGraph_VERSION}")
endif()
target_link_libraries(_3dtile PRIVATE osgDB osgUtil osgViewer OpenThreads)

install(TARGETS _3dtile DESTINATION lib)
