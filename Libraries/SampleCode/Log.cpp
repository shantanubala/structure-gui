#include "Log.h"

#include <atomic>
#include <limits>
#include <math.h>
#include <stdio.h>

#include <ST/Utilities.h>

namespace Log = SampleCode::Log;

static std::atomic<Log::Level> logLevel(Log::Level::Normal);

static std::string formatString(const char *fmt, va_list ap0, va_list ap1) {
    int len = vsnprintf(NULL, 0, fmt, ap0);
    if (len < 0) {
        throw std::runtime_error("vsnprintf() failed");
    }
    else if ((size_t)len == std::numeric_limits<size_t>::max()) {
        throw std::runtime_error("vsnprintf() length would exceed size_t range");
    }
    char *s = new char[(size_t)len + 1];
    vsnprintf(s, (size_t)len + 1, fmt, ap1);
    std::string ss(s);
    delete[] s;
    return ss;
}

void Log::setLogLevel(Level lvl) {
    logLevel.store(lvl);
    switch (lvl) {
        case Level::Quiet: ST::setConsoleLoggingVerbosity(0); break;
        case Level::Normal: ST::setConsoleLoggingVerbosity(1); break;
        case Level::Verbose: ST::setConsoleLoggingVerbosity(2); break;
    }
}

void Log::log(const char *fmt, ...) {
    Level lvl = logLevel.load();
    if (lvl == Level::Normal || lvl == Level::Verbose) {
        va_list ap0, ap1;
        va_start(ap0, fmt);
        va_start(ap1, fmt);
        printf("LOG: %s\n", formatString(fmt, ap0, ap1).c_str());
        va_end(ap1);
        va_end(ap0);
    }
}

void Log::logv(const char *fmt, ...) {
    Level lvl = logLevel.load();
    if (lvl == Level::Verbose) {
        va_list ap0, ap1;
        va_start(ap0, fmt);
        va_start(ap1, fmt);
        printf("LOGV: %s\n", formatString(fmt, ap0, ap1).c_str());
        va_end(ap1);
        va_end(ap0);
    }
}
