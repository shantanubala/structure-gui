#pragma once

namespace SampleCode {
    namespace Log {
        enum class Level {
            Quiet,
            Normal,
            Verbose,
        };
        void setLogLevel(Level level);
        void log(const char *fmt, ...);
        void logv(const char *fmt, ...);
    }
}
