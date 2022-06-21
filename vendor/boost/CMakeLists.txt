project(libboost)

add_library(boost INTERFACE)
add_custom_target(
	vendor_boost
	COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/android-build.sh ${CMAKE_ANDROID_NDK} ${CMAKE_ANDROID_ARCH_ABI}
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set(BOOST_OUTPUT "Boost-for-Android/build/out/${CMAKE_ANDROID_ARCH_ABI}")
file(GLOB BOOST_STATIC_LIBS ${BOOST_OUTPUT}/lib/libboost_*.a)
target_include_directories(boost SYSTEM INTERFACE ${BOOST_OUTPUT}/include/boost-1_76)
target_link_libraries(boost INTERFACE vendor_boost ${BOOST_STATIC_LIBS})
