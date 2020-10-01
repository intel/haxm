# Minimum requirement for CMake version
cmake_minimum_required(VERSION 3.4)

# Project information
project(checktool)

set(CMAKE_CXX_STANDARD 11)

set(COMMON_SRCS)
list(APPEND COMMON_SRCS
    ${PROJECT_SOURCE_DIR}/arg_parser.cpp
    ${PROJECT_SOURCE_DIR}/common.cpp
    ${PROJECT_SOURCE_DIR}/cpuid.cpp
    ${PROJECT_SOURCE_DIR}/feature_detector.cpp
    ${PROJECT_SOURCE_DIR}/main.cpp
)

set(PLATFORM_SRCS)
if(WIN32)
    list(APPEND PLATFORM_SRCS
        ${PROJECT_SOURCE_DIR}/os_windows.cpp
    )
    # Replace MDd/MD with MTd/MT to use a multi-threaded statically-linked
    # runtime library for building a standalone executable.
    set(CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
    )
    foreach(flag ${CXX_FLAGS})
        string(REPLACE "/MD" "/MT" ${flag} "${${flag}}")
    endforeach()
elseif(APPLE)
    list(APPEND PLATFORM_SRCS
        ${PROJECT_SOURCE_DIR}/os_darwin.cpp
    )
else()
    message(FATAL_ERROR "Unsupported Platform")
endif()

if(WIN32)
    set(RESOURCES)
    list(APPEND RESOURCES
        ${PROJECT_SOURCE_DIR}/version.rc
    )
endif()

# Target
add_executable(checktool ${COMMON_SRCS} ${PLATFORM_SRCS} ${RESOURCES})

if(APPLE)
    target_link_options(checktool PRIVATE
        LINKER:-sectcreate,__TEXT,__info_plist,${PROJECT_SOURCE_DIR}/Info.plist
    )
endif()
