project (gitversion)

set(SOURCES
    gitversion.cpp
    versionstring.cpp
    parser.cpp
    VersionCompare.cpp
)

add_library(${PROJECT_NAME} STATIC ${SOURCES})
target_link_libraries(${PROJECT_NAME} PUBLIC boost)
if (NOT DEFINED GIT_VERSION)
	message(FATAL_ERROR "GIT_VERSION not defined")
endif()
target_compile_definitions(${PROJECT_NAME} PRIVATE GIT_VERSION_STRING="${GIT_VERSION}")
target_add_boost(${PROJECT_NAME})
target_enable_style_warnings(${PROJECT_NAME})
target_activate_cpp14(${PROJECT_NAME})
