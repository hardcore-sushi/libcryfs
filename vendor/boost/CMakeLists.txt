project(libboost)

add_library(boost INTERFACE)
set(BOOST_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/build/${CMAKE_ANDROID_ARCH_ABI}")

if(CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
	set(BOOST_ARCH "a64")
elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "armeabi-v7a")
	set(BOOST_ARCH "a32")
elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "x86")
	set(BOOST_ARCH "x32")
elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "x86_64")
	set(BOOST_ARCH "x64")
else()
	message(FATAL_ERROR "Android ABI not supported: ${CMAKE_ANDROID_ARCH_ABI}")
endif()
set(BOOST_STATIC_LIBS
	${BOOST_OUTPUT}/lib/libboost_atomic-clang-mt-${BOOST_ARCH}-1_77.a
	${BOOST_OUTPUT}/lib/libboost_chrono-clang-mt-${BOOST_ARCH}-1_77.a
	${BOOST_OUTPUT}/lib/libboost_container-clang-mt-${BOOST_ARCH}-1_77.a
	${BOOST_OUTPUT}/lib/libboost_date_time-clang-mt-${BOOST_ARCH}-1_77.a
	${BOOST_OUTPUT}/lib/libboost_exception-clang-mt-${BOOST_ARCH}-1_77.a
	${BOOST_OUTPUT}/lib/libboost_filesystem-clang-mt-${BOOST_ARCH}-1_77.a
	${BOOST_OUTPUT}/lib/libboost_serialization-clang-mt-${BOOST_ARCH}-1_77.a
	${BOOST_OUTPUT}/lib/libboost_system-clang-mt-${BOOST_ARCH}-1_77.a
	${BOOST_OUTPUT}/lib/libboost_thread-clang-mt-${BOOST_ARCH}-1_77.a
	${BOOST_OUTPUT}/lib/libboost_wserialization-clang-mt-${BOOST_ARCH}-1_77.a
)
add_custom_target(
	vendor_boost
	COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/android-build.sh ${CMAKE_ANDROID_NDK} ${CMAKE_ANDROID_ARCH_ABI}
	BYPRODUCTS ${BOOST_STATIC_LIBS}
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_dependencies(boost vendor_boost)
target_include_directories(boost SYSTEM INTERFACE ${BOOST_OUTPUT}/include/boost-1_77)
target_link_libraries(boost INTERFACE ${BOOST_STATIC_LIBS})
