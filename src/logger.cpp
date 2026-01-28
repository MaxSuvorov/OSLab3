#include "logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#include <sys/types.h>
#endif

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger() : logFile(nullptr) {
#ifdef _WIN32
    winProcessId = GetCurrentProcessId();
#else
    processId = getpid();
#endif
}

Logger::~Logger() {
    close();
}

bool Logger::initialize(const std::string& filename) {
    std::lock_guard<std::mutex> lock(logMutex);
    this->filename = filename;
    
#ifdef _WIN32
    logFile = _fsopen(filename.c_str(), "a", _SH_DENYNO);
#else
    logFile = fopen(filename.c_str(), "a");
#endif
    
    if (!logFile) {
        std::cerr << "Failed to open log file: " << filename << std::endl;
        return false;
    }
    
    // Log startup
    std::string startupMsg = "Process started. PID: ";
#ifdef _WIN32
    startupMsg += std::to_string(winProcessId);
#else
    startupMsg += std::to_string(processId);
#endif
    startupMsg += " Time: " + getCurrentTime(true);
    
    fprintf(logFile, "%s\n", startupMsg.c_str());
    fflush(logFile);
    
    return true;
}

void Logger::log(const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex);
    if (logFile) {
        fprintf(logFile, "%s\n", message.c_str());
        fflush(logFile);
    }
}

void Logger::logWithTime(const std::string& prefix, int counterValue) {
    std::lock_guard<std::mutex> lock(logMutex);
    if (logFile) {
        std::string message = getCurrentTime(true) + " - ";
#ifdef _WIN32
        message += "PID: " + std::to_string(winProcessId) + " - ";
#else
        message += "PID: " + std::to_string(processId) + " - ";
#endif
        message += prefix;
        if (counterValue >= 0) {
            message += " Counter: " + std::to_string(counterValue);
        }
        fprintf(logFile, "%s\n", message.c_str());
        fflush(logFile);
    }
}

std::string Logger::getCurrentTime(bool withMilliseconds) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ) % 1000;
    
    std::stringstream ss;
    
    // For Windows, use safe localtime_s
#ifdef _WIN32
    struct tm timeinfo;
    localtime_s(&timeinfo, &time);
    ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
#else
    char buffer[80];
    struct tm* timeinfo = localtime(&time);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    ss << buffer;
#endif
    
    if (withMilliseconds) {
        ss << "." << std::setfill('0') << std::setw(3) << ms.count();
    }
    
    return ss.str();
}

void Logger::close() {
    std::lock_guard<std::mutex> lock(logMutex);
    if (logFile) {
        fclose(logFile);
        logFile = nullptr;
    }
}
