#include "logging/spdlog_logger.h"
#include <spdlog/sinks/stdout_color_sinks.h>

SpdlogLogger::SpdlogLogger() {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    logger_ = std::make_shared<spdlog::logger>("blackhole", console_sink);
    logger_->set_pattern("[%H:%M:%S.%e] [%^%l%$] [%n] %v");
    logger_->set_level(spdlog::level::info);
}

void SpdlogLogger::log(LogLevel level, const std::string& module, const std::string& message) {
    std::string formatted = "[" + module + "] " + message;
    switch (level) {
        case LogLevel::DEBUG: logger_->debug(formatted); break;
        case LogLevel::INFO:  logger_->info(formatted);  break;
        case LogLevel::WARN:  logger_->warn(formatted);  break;
        case LogLevel::ERROR: logger_->error(formatted);  break;
    }
}

void SpdlogLogger::set_level(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: logger_->set_level(spdlog::level::debug); break;
        case LogLevel::INFO:  logger_->set_level(spdlog::level::info);  break;
        case LogLevel::WARN:  logger_->set_level(spdlog::level::warn);  break;
        case LogLevel::ERROR: logger_->set_level(spdlog::level::err);   break;
    }
}
