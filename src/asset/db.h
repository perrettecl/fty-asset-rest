#pragma once
#include <tntdb.h>
#include <fty/traits.h>
#include <fty/expected.h>
#include <fty_common_db_dbpath.h>
#include <fty/translate.h>

namespace tnt {

template <typename T>
struct Arg
{
    std::string_view name;
    T                value;
};

namespace internal {

    struct Arg
    {
        std::string_view str;

        template <typename T>
        tnt::Arg<T> operator=(T&& value) const
        {
            return {str, std::forward<T>(value)};
        }
    };

} // namespace internal

class Connection
{
public:
    class Statement;
    class Row
    {
    public:
        template <typename T>
        T get(const std::string& col)
        {
            if constexpr (std::is_same_v<T, std::string>) {
                return m_row.getString(col);
            } else if constexpr (std::is_same_v<T, bool>) {
                return m_row.getBool(col);
            } else if constexpr (std::is_same_v<T, int64_t>) {
                return m_row.getInt64(col);
            } else if constexpr (std::is_same_v<T, int32_t>) {
                return m_row.getInt32(col);
            } else if constexpr (std::is_same_v<T, int16_t>) {
                return m_row.getInt(col);
            } else if constexpr (std::is_same_v<T, int8_t>) {
                return int8_t(m_row.getInt(col));
            } else if constexpr (std::is_same_v<T, uint64_t>) {
                return m_row.getUnsigned64(col);
            } else if constexpr (std::is_same_v<T, uint32_t>) {
                return m_row.getUnsigned32(col);
            } else if constexpr (std::is_same_v<T, uint16_t>) {
                return m_row.getUnsigned(col);
            } else if constexpr (std::is_same_v<T, uint8_t>) {
                return uint8_t(m_row.getUnsigned(col));
            } else if constexpr (std::is_same_v<T, float>) {
                return m_row.getFloat(col);
            } else if constexpr (std::is_same_v<T, double>) {
                return m_row.getDouble(col);
            } else {
                static_assert(fty::always_false<T>, "Unsupported type");
            }
        }

    private:
        Row(const tntdb::Row& row)
            : m_row(row)
        {
        }

        friend class Connection::Statement;
        tntdb::Row m_row;
    };

    class Statement
    {
    public:
        template <typename T>
        Statement& bind(const std::string& name, const T& value)
        {
            m_st.set(name, value);
            return *this;
        }

        template <typename TArg, typename... TArgs>
        Statement& bind(TArg&& val, TArgs&&... args)
        {
            arg(std::forward<TArg>(val));
            arg(std::forward<TArgs...>(args)...);
            return *this;
        }

        template <typename TArg>
        Statement& bind(Arg<TArg>&& arg)
        {
            m_st.set(arg.name.data(), arg.value);
            return *this;
        }

    public:
        Row selectRow()
        {
            return Row(m_st.selectRow());
        }

    private:
        Statement(const tntdb::Statement& st)
            : m_st(st)
        {
        }

        tntdb::Statement m_st;
        friend class Connection;
    };

public:
    Connection()
        : m_connection(tntdb::connectCached(DBConn::url))
    {
    }

    Statement prepare(const std::string& sql)
    {
        return Statement(m_connection.prepareCached(sql));
    }

    template<typename... Args>
    Statement query(const std::string& queryStr, Args&&... args)
    {
        return Statement(m_connection.prepareCached(queryStr)).bind(std::forward<Args>(args)...);
    }

    template<typename... Args>
    fty::Expected<Row> selectRow(const std::string& queryStr, Args&&... args)
    {
        try {
            return Statement(m_connection.prepareCached(queryStr)).bind(std::forward<Args>(args)...).selectRow();
        } catch (const tntdb::NotFound&) {
            return fty::unexpected("Element '{}' not found."_tr);
        } catch (const std::exception& e) {
            return unexpected(error(Errors::ExceptionForElement).format(e.what(), assetId));
        }
    }
private:
    tntdb::Connection m_connection;
};

} // namespace tnt

constexpr tnt::internal::Arg operator"" _a(const char* s, size_t n)
{
    return {{s, n}};
}
