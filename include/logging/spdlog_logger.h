#ifndef SPDLOG_LOGGER_H
#define SPDLOG_LOGGER_H

#include "logging/logger.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>

class SpdlogLogger : public ILogger {
public:
    SpdlogLogger();
    ~SpdlogLogger() override = default;

    void log(LogLevel level, const std::string& module, const std::string& message) override;
    void set_level(LogLevel level) override;

private:
    std::shared_ptr<spdlog::logger> logger_;
};

#endif // SPDLOG_LOGGER_H
