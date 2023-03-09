cmake_minimum_required(VERSION 3.5)

set(CMAKE_MODULE_PATH
	${CMAKE_CURRENT_SOURCE_DIR}/../../../deps/Dependencies/cmake-modules
	${CMAKE_MODULE_PATH}
)

include(Header)

project(GSH_Metal)

if(NOT TARGET Framework)
    add_subdirectory(
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../deps/Framework/build_cmake/Framework
        ${CMAKE_CURRENT_BINARY_DIR}/Framework
    )
endif()
list(APPEND GSH_METAL_PROJECT_LIBS Framework)

add_library(gsh_metal STATIC
	GSH_Metal.cpp
	GSH_Metal.h
    metalutil.h metalutil.mm
)

target_link_libraries(gsh_metal ${GSH_METAL_PROJECT_LIBS})
target_include_directories(gsh_metal PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/gs/GSH_Metal/"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../deps/metal-cpp"
    )
