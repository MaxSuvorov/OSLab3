#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include <vector>
#include <string>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

struct ChildProcess {
#ifdef _WIN32
    HANDLE handle;
    DWORD pid;
#else
    pid_t pid;
#endif
    int type; // 1 or 2
    time_t startTime;
    bool finished;
};

class ProcessManager {
public:
    static ProcessManager& getInstance();
    
    bool launchChildProcess(int type);
    void checkFinishedProcesses();
    bool hasActiveChildren();
    void cleanup();
    
    void setMasterMode(bool isMaster);
    bool isMaster() const;
    
private:
    ProcessManager();
    ~ProcessManager();
    ProcessManager(const ProcessManager&) = delete;
    ProcessManager& operator=(const ProcessManager&) = delete;
    
    std::vector<ChildProcess> children;
    std::mutex childrenMutex;
    std::atomic<bool> isMasterProcess;
    
    std::string getExecutablePath();
};

#endif // PROCESS_MANAGER_H
