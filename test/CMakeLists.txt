project(asset-test)

etn_test(asset-test
    SOURCES
        main.cpp
        db/insert.cpp
        db/names.cpp
        db/select.cpp

        test-utils.h
        read.cpp
        create.cpp
        import.cpp
        export.cpp
    CONFIGS
        conf/logger.conf
    USES
        fty-asset
        Catch2::Catch2
        mysqld
        fty_common_db
        tntdb
        cxxtools
        yaml-cpp
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(asset-test PRIVATE stdc++fs)
endif()
