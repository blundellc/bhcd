cmake_minimum_required(VERSION 3.5)

    
add_executable(bhcd bhcd.c)

find_package(GSL REQUIRED)
target_include_directories(bhcd PUBLIC ${GSL_INCLUDE_DIRS})
if(WIN32)
    find_package(unofficial-glib REQUIRED)
endif()
