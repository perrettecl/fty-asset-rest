project(asset-test)

etn_test(asset-test
    SOURCES
        main.cpp
        insert.cpp
        names.cpp
        select.cpp
    USES
        asset
        Catch2::Catch2
        mysqld
        fty_common_db
        tntdb
        cxxtools
)
