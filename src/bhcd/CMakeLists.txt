cmake_minimum_required(VERSION 3.5)

add_library(libbhcd dataset.c params.c tree.c merge.c build.c dataset_gml.c tree_io.c sscache.c labelset.c dataset_gen.c islands.c)
    
add_executable(bhcd bhcd.c)



if(WIN32)
    find_package(unofficial-glib REQUIRED)
    target_link_libraries(bhcd unofficial::glib::glib)
endif()

target_link_libraries(bhcd debug ${GSL_LIBRARY_DEBUG})
target_link_libraries(bhcd optimized ${GSL_LIBRARY})

target_link_libraries(bhcd libbhcd)
target_link_libraries(bhcd libhccd)