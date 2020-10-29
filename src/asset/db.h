#pragma once

#include <fty/traits.h>
#include <fty_common_db_dbpath.h>
#include <tntdb.h>

namespace tnt {

// =====================================================================================================================

template <typename T>
struct Arg
{
    std::string_view name;
    T                value;
};

template <>
struct Arg<void>
{
    std::string_view name;
};

struct Empty
{
};

// =====================================================================================================================

class Connection
{
public:
    class Statement;
    class ConstIterator;
    class Rows;

    // =================================================================================================================

    class Row
    {
    public:
        Row() = default;

        template <typename T>
        T get(const std::string& col) const;

        std::string get(const std::string& col) const;

        template <typename T>
        void get(const std::string& name, T& val) const;

    private:
        Row(const tntdb::Row& row);

        friend class Connection::Statement;
        friend class Connection::ConstIterator;
        friend class Connection::Rows;
        tntdb::Row m_row;
    };

    // =================================================================================================================

    class Rows
    {
    public:
        ConstIterator begin() const;
        ConstIterator end() const;
        size_t        size() const;
        bool          empty() const;
        Row           operator[](size_t off) const;

    private:
        Rows(const tntdb::Result& rows);

        tntdb::Result m_rows;
        friend class ConstIterator;
        friend class Connection::Statement;
    };

    // =================================================================================================================

    class ConstIterator : public std::iterator<std::random_access_iterator_tag, Row>
    {
    public:
        using ConstReference = const value_type&;
        using ConstPointer   = const value_type*;

    private:
        Rows   m_rows;
        Row    m_current;
        size_t m_offset;

        void setOffset(size_t off);

    public:
        ConstIterator(const Rows& r, size_t off);
        bool            operator==(const ConstIterator& it) const;
        bool            operator!=(const ConstIterator& it) const;
        ConstIterator&  operator++();
        ConstIterator   operator++(int);
        ConstIterator   operator--();
        ConstIterator   operator--(int);
        ConstReference  operator*() const;
        ConstPointer    operator->() const;
        ConstIterator&  operator+=(difference_type n);
        ConstIterator   operator+(difference_type n) const;
        ConstIterator&  operator-=(difference_type n);
        ConstIterator   operator-(difference_type n) const;
        difference_type operator-(const ConstIterator& it) const;
    };

    // =================================================================================================================

    class Statement
    {
    public:
        template <typename T>
        Statement& bind(const std::string& name, const T& value);

        template <typename TArg, typename... TArgs>
        Statement& bind(TArg&& val, TArgs&&... args);

        template <typename TArg>
        Statement& bind(Arg<TArg>&& arg);

    public:
        Row selectRow() const;
        Rows select() const;
        uint execute() const;

    private:
        Statement(const tntdb::Statement& st);

        mutable tntdb::Statement m_st;
        friend class Connection;
    };

    // =================================================================================================================

public:
    Connection();
    Statement prepare(const std::string& sql);

public:
    template <typename... Args>
    Row selectRow(const std::string& queryStr, Args&&... args);

    template <typename... Args>
    Rows select(const std::string& queryStr, Args&&... args);

    template <typename... Args>
    uint execute(const std::string& queryStr, Args&&... args);

private:
    tntdb::Connection m_connection;
};

} // namespace tnt

// =====================================================================================================================
// Arguments
// =====================================================================================================================

namespace tnt::internal {

struct Arg
{
    std::string_view str;

    template <typename T>
    auto operator=(T&& value) const
    {
        if constexpr(std::is_same_v<T, Empty>) {
            return tnt::Arg<void>{str};
        } else {
            return tnt::Arg<T>{str, std::forward<T>(value)};
        }
    }
};

} // namespace tnt::internal

constexpr tnt::internal::Arg operator"" _p(const char* s, size_t n)
{
    return {{s, n}};
}

// =====================================================================================================================
// Connection impl
// =====================================================================================================================

inline tnt::Connection::Connection()
    : m_connection(tntdb::connectCached(DBConn::url))
{
}

inline tnt::Connection::Statement tnt::Connection::prepare(const std::string& sql)
{
    return Statement(m_connection.prepareCached(sql));
}

template <typename... Args>
inline tnt::Connection::Row tnt::Connection::selectRow(const std::string& queryStr, Args&&... args)
{
    return Statement(m_connection.prepareCached(queryStr)).bind(std::forward<Args>(args)...).selectRow();
}

template <typename... Args>
inline tnt::Connection::Rows tnt::Connection::select(const std::string& queryStr, Args&&... args)
{
    return Statement(m_connection.prepareCached(queryStr)).bind(std::forward<Args>(args)...).select();
}

template <typename... Args>
uint tnt::Connection::execute(const std::string& queryStr, Args&&... args)
{
    return Statement(m_connection.prepareCached(queryStr)).bind(std::forward<Args>(args)...).execute();
}

// =====================================================================================================================
// Statement impl
// =====================================================================================================================

template <typename T>
inline tnt::Connection::Statement& tnt::Connection::Statement::bind(const std::string& name, const T& value)
{
    m_st.set(name, value);
    return *this;
}

template <typename TArg, typename... TArgs>
inline tnt::Connection::Statement& tnt::Connection::Statement::bind(TArg&& val, TArgs&&... args)
{
    bind(std::forward<TArg>(val));
    bind(std::forward<TArgs...>(args)...);
    return *this;
}

template <typename TArg>
inline tnt::Connection::Statement& tnt::Connection::Statement::bind(Arg<TArg>&& arg)
{
    if constexpr (std::is_same_v<TArg, void>) {
        m_st.setNull(arg.name.data());
    } else {
        m_st.set(arg.name.data(), arg.value);
    }
    return *this;
}

inline tnt::Connection::Row tnt::Connection::Statement::selectRow() const
{
    return Row(m_st.selectRow());
}

inline tnt::Connection::Rows tnt::Connection::Statement::select() const
{
    return Rows(m_st.select());
}

inline uint tnt::Connection::Statement::execute() const
{
    return m_st.execute();
}

inline tnt::Connection::Statement::Statement(const tntdb::Statement& st)
    : m_st(st)
{
}

// =====================================================================================================================
// Row impl
// =====================================================================================================================

template <typename T>
inline T tnt::Connection::Row::get(const std::string& col) const
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

inline std::string tnt::Connection::Row::get(const std::string& col) const
{
    return m_row.getString(col);
}

template <typename T>
inline void tnt::Connection::Row::get(const std::string& name, T& val) const
{
    val = get<std::decay_t<T>>(name);
}

inline tnt::Connection::Row::Row(const tntdb::Row& row)
    : m_row(row)
{
}

// =====================================================================================================================
// Rows iterator impl
// =====================================================================================================================


inline void tnt::Connection::ConstIterator::setOffset(size_t off)
{
    if (off != m_offset) {
        m_offset = off;
        if (m_offset < m_rows.m_rows.size())
            m_current = m_rows.m_rows.getRow(m_offset);
    }
}

inline tnt::Connection::ConstIterator::ConstIterator(const Rows& r, size_t off)
    : m_rows(r)
    , m_offset(off)
{
    if (m_offset < r.m_rows.size())
        m_current = r.m_rows.getRow(m_offset);
}

inline bool tnt::Connection::ConstIterator::operator==(const ConstIterator& it) const
{
    return m_offset == it.m_offset;
}

inline bool tnt::Connection::ConstIterator::operator!=(const ConstIterator& it) const
{
    return !operator==(it);
}

inline tnt::Connection::ConstIterator& tnt::Connection::ConstIterator::operator++()
{
    setOffset(m_offset + 1);
    return *this;
}

inline tnt::Connection::ConstIterator tnt::Connection::ConstIterator::operator++(int)
{
    ConstIterator ret = *this;
    setOffset(m_offset + 1);
    return ret;
}

inline tnt::Connection::ConstIterator tnt::Connection::ConstIterator::operator--()
{
    setOffset(m_offset - 1);
    return *this;
}

inline tnt::Connection::ConstIterator tnt::Connection::ConstIterator::operator--(int)
{
    ConstIterator ret = *this;
    setOffset(m_offset - 1);
    return ret;
}

inline tnt::Connection::ConstIterator::ConstReference tnt::Connection::ConstIterator::operator*() const
{
    return m_current;
}

inline tnt::Connection::ConstIterator::ConstPointer tnt::Connection::ConstIterator::operator->() const
{
    return &m_current;
}

inline tnt::Connection::ConstIterator& tnt::Connection::ConstIterator::operator+=(difference_type n)
{
    setOffset(m_offset + n);
    return *this;
}

inline tnt::Connection::ConstIterator tnt::Connection::ConstIterator::operator+(difference_type n) const
{
    ConstIterator it(*this);
    it += n;
    return it;
}

inline tnt::Connection::ConstIterator& tnt::Connection::ConstIterator::operator-=(difference_type n)
{
    setOffset(m_offset - n);
    return *this;
}

inline tnt::Connection::ConstIterator tnt::Connection::ConstIterator::operator-(difference_type n) const
{
    ConstIterator it(*this);
    it -= n;
    return it;
}

inline tnt::Connection::ConstIterator::difference_type tnt::Connection::ConstIterator::operator-(
    const ConstIterator& it) const
{
    return m_offset - it.m_offset;
}

// =====================================================================================================================
// Rows impl
// =====================================================================================================================

inline tnt::Connection::ConstIterator tnt::Connection::Rows::begin() const
{
    return tnt::Connection::ConstIterator(*this, 0);
}

inline tnt::Connection::ConstIterator tnt::Connection::Rows::end() const
{
    return tnt::Connection::ConstIterator(*this, size());
}

size_t tnt::Connection::Rows::size() const
{
    return m_rows.size();
}

bool tnt::Connection::Rows::empty() const
{
    return m_rows.empty();
}

tnt::Connection::Row tnt::Connection::Rows::operator[](size_t off) const
{
    return tnt::Connection::Row(m_rows.getRow(off));
}

inline tnt::Connection::Rows::Rows(const tntdb::Result& rows)
    : m_rows(rows)
{
}
