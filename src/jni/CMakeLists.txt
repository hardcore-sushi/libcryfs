project (libcryfs-jni)

add_library(${PROJECT_NAME} STATIC libcryfs-jni.cpp)

target_link_libraries(${PROJECT_NAME} PUBLIC fspp-fuse)

target_enable_style_warnings(${PROJECT_NAME})
target_activate_cpp14(${PROJECT_NAME})

set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER include/libcryfs-jni.h)
target_include_directories(${PROJECT_NAME} PUBLIC include)

