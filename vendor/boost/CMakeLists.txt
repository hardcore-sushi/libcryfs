project(libboost)

add_library(boost INTERFACE)
set(BOOST_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/build/${CMAKE_ANDROID_ARCH_ABI}")
set(BOOST_STATIC_LIBS
	${BOOST_OUTPUT}/lib/libboost_atomic-clang-mt-a64-1_76.a
	${BOOST_OUTPUT}/lib/libboost_chrono-clang-mt-a64-1_76.a
	${BOOST_OUTPUT}/lib/libboost_container-clang-mt-a64-1_76.a
	${BOOST_OUTPUT}/lib/libboost_date_time-clang-mt-a64-1_76.a
	${BOOST_OUTPUT}/lib/libboost_exception-clang-mt-a64-1_76.a
	${BOOST_OUTPUT}/lib/libboost_filesystem-clang-mt-a64-1_76.a
	${BOOST_OUTPUT}/lib/libboost_serialization-clang-mt-a64-1_76.a
	${BOOST_OUTPUT}/lib/libboost_system-clang-mt-a64-1_76.a
	${BOOST_OUTPUT}/lib/libboost_thread-clang-mt-a64-1_76.a
	${BOOST_OUTPUT}/lib/libboost_wserialization-clang-mt-a64-1_76.a
)
add_custom_target(
	vendor_boost
	COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/android-build.sh ${CMAKE_ANDROID_NDK} ${CMAKE_ANDROID_ARCH_ABI}
	BYPRODUCTS ${BOOST_STATIC_LIBS}
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_dependencies(boost vendor_boost)
target_include_directories(boost SYSTEM INTERFACE ${BOOST_OUTPUT}/include/boost-1_76)
target_link_libraries(boost INTERFACE
	${BOOST_OUTPUT}/lib/libboost_atomic-clang-mt-a64-1_76.a
	${BOOST_OUTPUT}/lib/libboost_chrono-clang-mt-a64-1_76.a
	${BOOST_OUTPUT}/lib/libboost_container-clang-mt-a64-1_76.a
	${BOOST_OUTPUT}/lib/libboost_date_time-clang-mt-a64-1_76.a
	${BOOST_OUTPUT}/lib/libboost_exception-clang-mt-a64-1_76.a
	${BOOST_OUTPUT}/lib/libboost_filesystem-clang-mt-a64-1_76.a
	${BOOST_OUTPUT}/lib/libboost_serialization-clang-mt-a64-1_76.a
	${BOOST_OUTPUT}/lib/libboost_system-clang-mt-a64-1_76.a
	${BOOST_OUTPUT}/lib/libboost_thread-clang-mt-a64-1_76.a
	${BOOST_OUTPUT}/lib/libboost_wserialization-clang-mt-a64-1_76.a
)
