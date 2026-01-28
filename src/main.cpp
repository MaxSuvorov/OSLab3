#include "counter.h"
#include "logger.h"
#include "process_manager.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <string>
#include <sstream>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#endif

std::atomic<bool> running(true);

// Signal handler
void signalHandler(int signal) {
    running.store(false);
}

// Non-blocking keyboard input check
bool kbhit() {
#ifdef _WIN32
    return _kbhit() != 0;
#else
    struct termios oldt, newt;
    int ch;
    int oldf;
    
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    
    ch = getchar();
    
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    
    if (ch != EOF) {
        ungetc(ch, stdin);
        return true;
    }
    
    return false;
#endif
}

// Child process logic
void runAsChild(int type) {
    Logger& logger = Logger::getInstance();
    Counter& counter = Counter::getInstance();
    
    logger.initialize("lab.log");
    logger.logWithTime("Child " + std::to_string(type) + " started");
    
    int currentValue = counter.getValue();
    
    if (type == 1) {
        // Type 1: increase by 10
        counter.setValue(currentValue + 10);
        logger.logWithTime("Child 1 increased counter by 10");
    } else if (type == 2) {
        // Type 2: multiply by 2, wait 2 seconds, divide by 2
        counter.setValue(currentValue * 2);
        logger.logWithTime("Child 2 multiplied counter by 2");
        
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        currentValue = counter.getValue();
        counter.setValue(currentValue / 2);
        logger.logWithTime("Child 2 divided counter by 2");
    }
    
    logger.logWithTime("Child " + std::to_string(type) + " finished");
    logger.close();
    
    exit(0);
}

// Master process logic
void runMaster() {
    Logger& logger = Logger::getInstance();
    Counter& counter = Counter::getInstance();
    ProcessManager& pm = ProcessManager::getInstance();
    
    pm.setMasterMode(true);
    
    auto lastIncrement = std::chrono::steady_clock::now();
    auto lastLog = std::chrono::steady_clock::now();
    auto lastChildLaunch = std::chrono::steady_clock::now();
    
    while (running.load()) {
        auto now = std::chrono::steady_clock::now();
        
        // Increment every 300ms
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastIncrement).count() >= 300) {
            counter.increment();
            lastIncrement = now;
        }
        
        // Log every 1 second
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastLog).count() >= 1000) {
            logger.logWithTime("Master log", counter.getValue());
            lastLog = now;
        }
        
        // Launch child processes every 3 seconds
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastChildLaunch).count() >= 3000) {
            
            pm.checkFinishedProcesses();
            
            if (!pm.hasActiveChildren()) {
                if (pm.launchChildProcess(1) && pm.launchChildProcess(2)) {
                    logger.logWithTime("Launched child processes 1 and 2");
                }
            } else {
                logger.logWithTime("Previous child processes still active, skipping launch");
            }
            
            lastChildLaunch = now;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// Slave process logic
void runSlave() {
    Counter& counter = Counter::getInstance();
    
    auto lastIncrement = std::chrono::steady_clock::now();
    
    while (running.load()) {
        auto now = std::chrono::steady_clock::now();
        
        // Increment every 300ms
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastIncrement).count() >= 300) {
            counter.increment();
            lastIncrement = now;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

int main(int argc, char* argv[]) {
    // Handle command line arguments
    bool isChild = false;
    int childType = 0;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--child") == 0 && i + 1 < argc) {
            isChild = true;
            childType = std::stoi(argv[i + 1]);
            break;
        }
    }
    
    if (isChild) {
        runAsChild(childType);
        return 0;
    }
    
    // Set up signal handlers
#ifdef _WIN32
    SetConsoleCtrlHandler([](DWORD signal) -> BOOL {
        if (signal == CTRL_C_EVENT) {
            running.store(false);
            return TRUE;
        }
        return FALSE;
    }, TRUE);
#else
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
#endif
    
    // Initialize components
    Logger& logger = Logger::getInstance();
    if (!logger.initialize("lab.log")) {
        std::cerr << "Failed to initialize logger" << std::endl;
        return 1;
    }
    
    Counter& counter = Counter::getInstance();
    ProcessManager& pm = ProcessManager::getInstance();
    
    // Try to become master
    // Simple approach: first process to write to log becomes master
    bool isMaster = true; // For simplicity, all processes can become masters
    // In a real implementation, you would use inter-process synchronization
    
    std::cout << "Lab program started. PID: ";
#ifdef _WIN32
    std::cout << GetCurrentProcessId();
#else
    std::cout << getpid();
#endif
    std::cout << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  Enter a number to set counter value" << std::endl;
    std::cout << "  'q' to quit" << std::endl;
    std::cout << "  'm' to toggle master mode" << std::endl;
    std::cout << std::endl;
    
    if (isMaster) {
        std::cout << "Running as MASTER process" << std::endl;
        pm.setMasterMode(true);
    } else {
        std::cout << "Running as SLAVE process" << std::endl;
        pm.setMasterMode(false);
    }
    
    // Start worker thread
    std::thread workerThread;
    if (isMaster) {
        workerThread = std::thread(runMaster);
    } else {
        workerThread = std::thread(runSlave);
    }
    
    // Main thread handles user input
    std::string input;
    
    while (running.load()) {
        if (kbhit()) {
            char c = getchar();
            
            if (c == 'q' || c == 'Q') {
                running.store(false);
                break;
            } else if (c == 'm' || c == 'M') {
                // Toggle master mode (for testing)
                isMaster = !isMaster;
                pm.setMasterMode(isMaster);
                
                // Restart worker thread with new mode
                running.store(false);
                if (workerThread.joinable()) {
                    workerThread.join();
                }
                
                running.store(true);
                if (isMaster) {
                    workerThread = std::thread(runMaster);
                    std::cout << "Switched to MASTER mode" << std::endl;
                } else {
                    workerThread = std::thread(runSlave);
                    std::cout << "Switched to SLAVE mode" << std::endl;
                }
            } else if (c == '\n') {
                if (!input.empty()) {
                    try {
                        int newValue = std::stoi(input);
                        counter.setValue(newValue);
                        logger.logWithTime("User set counter to " + input);
                        std::cout << "Counter set to: " << newValue << std::endl;
                    } catch (const std::exception& e) {
                        std::cout << "Invalid number: " << input << std::endl;
                    }
                    input.clear();
                }
            } else if (isdigit(c) || c == '-') {
                input += c;
                std::cout << c;
                fflush(stdout);
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Cleanup
    if (workerThread.joinable()) {
        workerThread.join();
    }
    
    logger.logWithTime("Process terminating");
    logger.close();
    pm.cleanup();
    
    std::cout << "\nProgram terminated." << std::endl;
    
    return 0;
}
