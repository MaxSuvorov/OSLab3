#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <mutex>

class Logger {
public:
    static Logger& getInstance();
    
    bool initialize(const std::string& filename);
    void log(const std::string& message);
    void logWithTime(const std::string& prefix, int counterValue = -1);
    std::string getCurrentTime(bool withMilliseconds = false);
    
    void close();
    
private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    FILE* logFile;
    std::mutex logMutex;
    std::string filename;
    pid_t processId; // Unix
#ifdef _WIN32
    DWORD winProcessId;
#endif
};

#endif // LOGGER_H
