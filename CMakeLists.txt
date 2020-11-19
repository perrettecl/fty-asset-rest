cmake_minimum_required(VERSION 3.13)

########################################################################################################################

project(fty-asset-rest
    VERSION 1.0.0
    DESCRIPTION "Fty asset library for tntnet"
)

########################################################################################################################
find_package(fty-cmake PATHS ${CMAKE_BINARY_DIR}/fty-cmake)
########################################################################################################################
add_subdirectory(asset)
########################################################################################################################

etn_target(shared ${PROJECT_NAME}
    SOURCES
        conf/43_assets.xml.in

        src/read.cpp
        src/read.h
        src/create.cpp
        src/create.h
        src/list.cpp
        src/list.h
        src/delete.cpp
        src/delete.h
        src/import.cpp
        src/import.h
        src/export.cpp
        src/export.h
    USES
        fty-cmake-rest
        tntnet
        fty_common_rest
        fty-asset
    TARGET_DESTINATION /usr/lib/tntnet
)

########################################################################################################################

# discovery.xml for tntnet
etn_configure_file(
    conf/43_assets.xml.in

    TARGET      ${PROJECT_NAME}
    DESTINATION /etc/tntnet/bios.d
)

########################################################################################################################

if (BUILD_TESTING)
    add_subdirectory(test)
endif()

