#include "process_manager.h"
#include "logger.h"
#include "counter.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>

#ifdef _WIN32
#include <tchar.h>
#else
#include <sys/wait.h>
#include <cstdlib>
#endif

ProcessManager& ProcessManager::getInstance() {
    static ProcessManager instance;
    return instance;
}

ProcessManager::ProcessManager() : isMasterProcess(false) {}

ProcessManager::~ProcessManager() {
    cleanup();
}

bool ProcessManager::launchChildProcess(int type) {
    std::lock_guard<std::mutex> lock(childrenMutex);
    
    std::string exePath = getExecutablePath();
    if (exePath.empty()) {
        return false;
    }
    
#ifdef _WIN32
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    
    std::string cmd = exePath + " --child " + std::to_string(type);
    
    if (!CreateProcess(
        NULL,
        const_cast<char*>(cmd.c_str()),
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        NULL,
        &si,
        &pi
    )) {
        std::cerr << "CreateProcess failed: " << GetLastError() << std::endl;
        return false;
    }
    
    ChildProcess child;
    child.handle = pi.hProcess;
    child.pid = pi.dwProcessId;
    child.type = type;
    child.startTime = time(nullptr);
    child.finished = false;
    
    children.push_back(child);
    
    CloseHandle(pi.hThread);
    
#else
    pid_t pid = fork();
    
    if (pid == 0) { // Child process
        std::string typeStr = std::to_string(type);
        
        // Prepare arguments for exec
        char* args[] = {
            const_cast<char*>(exePath.c_str()),
            const_cast<char*>("--child"),
            const_cast<char*>(typeStr.c_str()),
            NULL
        };
        
        execv(exePath.c_str(), args);
        
        // If exec fails
        perror("execv");
        exit(1);
    } else if (pid > 0) { // Parent process
        ChildProcess child;
        child.pid = pid;
        child.type = type;
        child.startTime = time(nullptr);
        child.finished = false;
        
        children.push_back(child);
    } else {
        perror("fork");
        return false;
    }
#endif
    
    return true;
}

void ProcessManager::checkFinishedProcesses() {
    std::lock_guard<std::mutex> lock(childrenMutex);
    
    auto it = children.begin();
    while (it != children.end()) {
        if (it->finished) {
            it = children.erase(it);
            continue;
        }
        
#ifdef _WIN32
        DWORD exitCode;
        if (GetExitCodeProcess(it->handle, &exitCode)) {
            if (exitCode != STILL_ACTIVE) {
                it->finished = true;
                CloseHandle(it->handle);
                it = children.erase(it);
                continue;
            }
        }
#else
        int status;
        pid_t result = waitpid(it->pid, &status, WNOHANG);
        if (result > 0) {
            it->finished = true;
            it = children.erase(it);
            continue;
        } else if (result == -1) {
            // Error
            it->finished = true;
            it = children.erase(it);
            continue;
        }
#endif
        
        ++it;
    }
}

bool ProcessManager::hasActiveChildren() {
    std::lock_guard<std::mutex> lock(childrenMutex);
    
    for (const auto& child : children) {
        if (!child.finished) {
            return true;
        }
    }
    
    return false;
}

void ProcessManager::cleanup() {
    std::lock_guard<std::mutex> lock(childrenMutex);
    
#ifdef _WIN32
    for (auto& child : children) {
        if (!child.finished) {
            TerminateProcess(child.handle, 0);
            CloseHandle(child.handle);
        }
    }
#else
    for (auto& child : children) {
        if (!child.finished) {
            kill(child.pid, SIGTERM);
        }
    }
#endif
    
    children.clear();
}

void ProcessManager::setMasterMode(bool isMaster) {
    isMasterProcess.store(isMaster);
}

bool ProcessManager::isMaster() const {
    return isMasterProcess.load();
}

std::string ProcessManager::getExecutablePath() {
#ifdef _WIN32
    char path[MAX_PATH];
    if (GetModuleFileName(NULL, path, MAX_PATH) == 0) {
        return "";
    }
    return std::string(path);
#else
    char path[1024];
    ssize_t count = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (count == -1) {
        return "";
    }
    path[count] = '\0';
    return std::string(path);
#endif
}
