cmake_minimum_required(VERSION 3.16)

project(dwrgFpsUnlocker VERSION 1.1 LANGUAGES CXX)


set(DEPLOY_DIR ${CMAKE_SOURCE_DIR}/deploy)          #部署目录
if (NOT EXISTS ${DEPLOY_DIR})
    file(MAKE_DIRECTORY ${DEPLOY_DIR})
endif ()


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB CORE_DIR core)
file(GLOB RES_DIR resource)

add_subdirectory(src_nogui)
add_subdirectory(src_gui)







