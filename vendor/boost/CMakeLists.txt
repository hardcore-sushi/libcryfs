project(libboost)

add_library(boost INTERFACE)
target_include_directories(boost SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/${CMAKE_ANDROID_ARCH_ABI}/include/boost-1_76)

file(GLOB BOOST_STATIC_LIBS ${CMAKE_CURRENT_SOURCE_DIR}/${ANDROID_ABI}/lib/libboost_*.a)
target_link_libraries(boost INTERFACE ${BOOST_STATIC_LIBS})
