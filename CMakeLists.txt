cmake_minimum_required(VERSION 3.13)

########################################################################################################################
find_package(fty-cmake PATHS ${CMAKE_BINARY_DIR}/fty-cmake)
########################################################################################################################

project(fty-asset-rest
    VERSION 1.0.0
    DESCRIPTION "Fty asset library for tntnet"
)

########################################################################################################################
etn_target(static asset
    SOURCES
        src/asset/json.cpp
        src/asset/json.h
        src/asset/asset-manager.cpp
        src/asset/asset-manager.h
        src/asset/asset-computed.cpp
        src/asset/asset-computed.h
        src/asset/asset-helpers.cpp
        src/asset/asset-helpers.h
        src/asset/asset-db.cpp
        src/asset/asset-db.h
        src/asset/asset-licensing.cpp
        src/asset/asset-licensing.h
        src/asset/asset-import.cpp
        src/asset/asset-import.h
        src/asset/asset-configure-inform.cpp
        src/asset/asset-configure-inform.h
        src/asset/csv.cpp
        src/asset/csv.h
        src/asset/error.h
        src/asset/logger.h
        src/asset/db.h
    USES
        fty_common_logging
        mlm
        fty_asset_activator
        fty_common
        fty-utils
        fty_shm
        fty_common_rest
    PRIVATE
)

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

    USES
        fty-cmake-rest
        tntnet
        fty_common_rest
        asset
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
