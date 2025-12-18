# updatepack

if(MSVC)
    add_compile_options(/source-charset:utf-8)
endif ()
find_package(minizip-ng CONFIG REQUIRED)

add_executable(updater updater.cpp)

target_link_libraries(updater PRIVATE MINIZIP::minizip-ng)

###################