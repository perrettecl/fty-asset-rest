#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>
#include <mariadb/mysql.h>
#include <filesystem>
#include "src/asset/db.h"
#include <thread>

class CharArray
{
public:
    template <typename... Args>
    CharArray(const Args&... args)
    {
        add(args...);
        m_data.push_back(nullptr);
    }

    CharArray(const CharArray&) = delete;

    ~CharArray()
    {
        for (size_t i = 0; i < m_data.size(); i++) {
            delete[] m_data[i];
        }
    }

    template <typename... Args>
    void add(const std::string& arg, const Args&... args)
    {
        add(arg);
        add(args...);
    }

    void add(const std::string& str)
    {
        char* s = new char[str.size() + 1];
        memset(s, 0, str.size() + 1);
        strncpy(s, str.c_str(), str.size());
        m_data.push_back(s);
    }

    char** data()
    {
        return m_data.data();
    }

    size_t size() const
    {
        return m_data.size();
    }

private:
    std::vector<char*> m_data;
};


static void createDB()
{
    tnt::Connection conn;
    conn.execute("CREATE DATABASE IF NOT EXISTS box_utf8 character set utf8 collate utf8_general_ci;");
    conn.execute("USE box_utf8");
    conn.execute("CREATE TABLE IF NOT EXISTS t_empty (id TINYINT);");
    conn.execute(R"(
        CREATE TABLE t_bios_asset_element_type (
            id_asset_element_type TINYINT UNSIGNED NOT NULL AUTO_INCREMENT,
            name                  VARCHAR(50)      NOT NULL,
            PRIMARY KEY (id_asset_element_type),
            UNIQUE INDEX `UI_t_bios_asset_element_type` (`name` ASC)
        ) AUTO_INCREMENT = 1;
    )");

    conn.execute(R"(
        CREATE TABLE t_bios_asset_device_type(
            id_asset_device_type TINYINT UNSIGNED   NOT NULL AUTO_INCREMENT,
            name                 VARCHAR(50)        NOT NULL,
            PRIMARY KEY (id_asset_device_type),
            UNIQUE INDEX `UI_t_bios_asset_device_type` (`name` ASC)
        );
    )");

    conn.execute(R"(
        CREATE TABLE t_bios_asset_element (
            id_asset_element  INT UNSIGNED        NOT NULL AUTO_INCREMENT,
            name              VARCHAR(50)         NOT NULL,
            id_type           TINYINT UNSIGNED    NOT NULL,
            id_subtype        TINYINT UNSIGNED    NOT NULL DEFAULT 11,
            id_parent         INT UNSIGNED,
            status            VARCHAR(9)          NOT NULL DEFAULT "nonactive",
            priority          TINYINT             NOT NULL DEFAULT 5,
            asset_tag         VARCHAR(50),

            PRIMARY KEY (id_asset_element),

            INDEX FK_ASSETELEMENT_ELEMENTTYPE_idx (id_type   ASC),
            INDEX FK_ASSETELEMENT_ELEMENTSUBTYPE_idx (id_subtype   ASC),
            INDEX FK_ASSETELEMENT_PARENTID_idx    (id_parent ASC),
            UNIQUE INDEX `UI_t_bios_asset_element_NAME` (`name` ASC),
            INDEX `UI_t_bios_asset_element_ASSET_TAG` (`asset_tag`  ASC),

            CONSTRAINT FK_ASSETELEMENT_ELEMENTTYPE
            FOREIGN KEY (id_type)
            REFERENCES t_bios_asset_element_type (id_asset_element_type)
            ON DELETE RESTRICT,

            CONSTRAINT FK_ASSETELEMENT_ELEMENTSUBTYPE
            FOREIGN KEY (id_subtype)
            REFERENCES t_bios_asset_device_type (id_asset_device_type)
            ON DELETE RESTRICT,

            CONSTRAINT FK_ASSETELEMENT_PARENTID
            FOREIGN KEY (id_parent)
            REFERENCES t_bios_asset_element (id_asset_element)
            ON DELETE RESTRICT
        );
    )");

    conn.execute(R"(
        CREATE TABLE t_bios_asset_ext_attributes(
            id_asset_ext_attribute    INT UNSIGNED NOT NULL AUTO_INCREMENT,
            keytag                    VARCHAR(40)  NOT NULL,
            value                     VARCHAR(255) NOT NULL,
            id_asset_element          INT UNSIGNED NOT NULL,
            read_only                 TINYINT      NOT NULL DEFAULT 0,

            PRIMARY KEY (id_asset_ext_attribute),

            INDEX FK_ASSETEXTATTR_ELEMENT_idx (id_asset_element ASC),
            UNIQUE INDEX `UI_t_bios_asset_ext_attributes` (`keytag`, `id_asset_element` ASC),

            CONSTRAINT FK_ASSETEXTATTR_ELEMENT
            FOREIGN KEY (id_asset_element)
            REFERENCES t_bios_asset_element (id_asset_element)
            ON DELETE RESTRICT
        );
    )");

    conn.execute(R"(
        INSERT INTO t_bios_asset_element_type (name)
        VALUES  ("group"),
                ("datacenter"),
                ("room"),
                ("row"),
                ("rack"),
                ("device");
    )");

    conn.execute(R"(
        INSERT INTO t_bios_asset_device_type (name)
        VALUES  ("ups"),
                ("genset"),
                ("epdu"),
                ("pdu"),
                ("server"),
                ("feed"),
                ("sts"),
                ("switch"),
                ("storage"),
                ("vm");
    )");

    conn.execute(R"(
        INSERT INTO t_bios_asset_device_type (id_asset_device_type, name) VALUES (11, "N_A");
    )");

    conn.execute(R"(
        CREATE VIEW v_bios_asset_element_type AS
            SELECT
                t1.id_asset_element_type AS id,
                t1.name
            FROM
                t_bios_asset_element_type t1;
    )");

    conn.execute(R"(
        CREATE VIEW v_bios_asset_element AS
            SELECT  v1.id_asset_element AS id,
                    v1.name,
                    v1.id_type,
                    v1.id_subtype,
                    v1.id_parent,
                    v2.id_type AS id_parent_type,
                    v1.status,
                    v1.priority,
                    v1.asset_tag
            FROM t_bios_asset_element v1
            LEFT JOIN  t_bios_asset_element v2
                ON (v1.id_parent = v2.id_asset_element) ;
    )");

    conn.execute(R"(
        CREATE VIEW v_bios_asset_ext_attributes AS
            SELECT * FROM t_bios_asset_ext_attributes ;
    )");

    conn.execute(R"(
        CREATE VIEW v_web_element AS
            SELECT
                t1.id_asset_element AS id,
                t1.name,
                t1.id_type,
                v3.name AS type_name,
                t1.id_subtype AS subtype_id,
                v4.name AS subtype_name,
                t1.id_parent,
                t2.id_type AS id_parent_type,
                t2.name AS parent_name,
                t1.status,
                t1.priority,
                t1.asset_tag
            FROM
                t_bios_asset_element t1
                LEFT JOIN t_bios_asset_element t2
                    ON (t1.id_parent = t2.id_asset_element)
                LEFT JOIN v_bios_asset_element_type v3
                    ON (t1.id_type = v3.id)
                LEFT JOIN t_bios_asset_device_type v4
                    ON (v4.id_asset_device_type = t1.id_subtype);
    )");
}

int main(int argc, char* argv[])
{
    std::stringstream ss;
    ss << std::this_thread::get_id();
    std::string pid = ss.str()+"_"+std::to_string(random());

    std::string path = "/tmp/mysql-"+pid;
    std::string sock = "/tmp/mysql-"+pid+".sock";

    std::filesystem::create_directory(path);

    CharArray options("mysql_test", "--datadir="+path, "--socket="+sock);
    CharArray groups("libmysqld_server", "libmysqld_client");

    mysql_library_init(options.size()-1, options.data(), groups.data());

    std::string url = "mysql:unix_socket="+sock;
    setenv("DBURL", url.c_str(), 1);

    createDB();

    url = "mysql:unix_socket="+sock+";db=box_utf8";
    setenv("DBURL", url.c_str(), 1);

    int result = Catch::Session().run(argc, argv);

    //mysql_library_end();

    std::filesystem::remove_all(path);

    return result;
}
