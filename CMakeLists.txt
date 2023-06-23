cmake_minimum_required(VERSION 3.0.0)

# set(CMAKE_SYSTEM_VERSION )
set(CMAKE_CXX_STANDARD 17)

project(QuickTouchControl VERSION 0.1.0)

include(CTest)
enable_testing()

add_subdirectory(src)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
