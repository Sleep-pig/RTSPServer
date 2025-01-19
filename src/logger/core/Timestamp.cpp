#include "Log/Timestamp.hpp"
#include <chrono>
#include <sstream>
using namespace pjie;
std::string Timestamp::Localtime()
{
    std::ostringstream ss;
    auto now = std::chrono::system_clock::now();
    time_t tt = std::chrono::system_clock::to_time_t(now);

#if defined(_WIN32) || defined(WIN32)
    struct tm tm;
    localtime_s(&tm, &tt);
    ss << std::put_time(&tm, "%F %T");
#elif defined(__linux) || defined(__linux__)
    char buffer[200] = { 0 };
    std::string timeString;
    std::strftime(buffer, 200, "%F %T", std::localtime(&tt));
    ss << buffer;
#endif
    return ss.str();
}