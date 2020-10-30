#pragma once
#include <fty_log.h>
#include <fmt/format.h>
#include <fty/translate.h>

#define logError(...)\
    log(log4cplus::ERROR_LOG_LEVEL, __VA_ARGS__)

#define logDebug(...)\
    log(log4cplus::DEBUG_LOG_LEVEL, __VA_ARGS__)

#define logInfo(...)\
    log(log4cplus::INFO_LOG_LEVEL, __VA_ARGS__)

#define logWarn(...)\
    log(log4cplus::WARN_LOG_LEVEL, __VA_ARGS__)

#define logFatal(...)\
    log(log4cplus::FATAL_LOG_LEVEL, __VA_ARGS__)

#define log(level, ...) \
    ftylog_getInstance()->insertLog(level, __FILE__, __LINE__, __func__, fty::logger::format(__VA_ARGS__).c_str())

namespace fty::logger {

inline std::string format(const std::string& str)
{
    return str;
}

template<typename... Args>
inline std::string format(const std::string& str, const Args&... args)
{
    return fmt::format(str, args...);
}

template<typename... Args>
inline std::string format(Translate& trans, const Args&... args)
{
    return trans.format(args...).toString();
}

}
