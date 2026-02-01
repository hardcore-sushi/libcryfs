project (cryfs-cli)
INCLUDE(GNUInstallDirs)

set(SOURCES
        Cli.cpp
        VersionChecker.cpp
        CallAfterTimeout.cpp
        Environment.cpp
        program_options/utils.cpp
        program_options/ProgramOptions.cpp
)

add_library(${PROJECT_NAME}_lib ${SOURCES})
target_link_libraries(${PROJECT_NAME}_lib PUBLIC cryfs cpp-utils gitversion fspp-fuse)
target_enable_style_warnings(${PROJECT_NAME}_lib)
target_activate_cpp14(${PROJECT_NAME}_lib)
