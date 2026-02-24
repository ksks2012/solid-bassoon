#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <memory>

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};

// Abstract logger interface
class ILogger {
public:
    virtual ~ILogger() = default;
    virtual void log(LogLevel level, const std::string& module, const std::string& message) = 0;
    virtual void set_level(LogLevel level) = 0;
};

// Process-wide logger accessor for code that doesn't receive a logger via DI
class GlobalLogger {
public:
    static void set(std::shared_ptr<ILogger> logger) { instance_ = std::move(logger); }
    static ILogger* get() { return instance_.get(); }
private:
    static inline std::shared_ptr<ILogger> instance_;
};

// Convenience macros
#define LOG_DEBUG(module, msg) \
    do { if (auto* _gl = GlobalLogger::get()) _gl->log(LogLevel::DEBUG, module, msg); } while(0)
#define LOG_INFO(module, msg) \
    do { if (auto* _gl = GlobalLogger::get()) _gl->log(LogLevel::INFO, module, msg); } while(0)
#define LOG_WARN(module, msg) \
    do { if (auto* _gl = GlobalLogger::get()) _gl->log(LogLevel::WARN, module, msg); } while(0)
#define LOG_ERROR(module, msg) \
    do { if (auto* _gl = GlobalLogger::get()) _gl->log(LogLevel::ERROR, module, msg); } while(0)

#endif // LOGGER_H
