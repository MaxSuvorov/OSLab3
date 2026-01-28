#ifndef COUNTER_H
#define COUNTER_H

#include <mutex>
#include <atomic>
#include <condition_variable>
#include <memory>

class Counter {
public:
    static Counter& getInstance();
    
    void increment();
    void setValue(int newValue);
    int getValue();
    
    // For shared memory between processes
    void* getSharedMemory();
    size_t getSharedMemorySize();
    
private:
    Counter();
    Counter(const Counter&) = delete;
    Counter& operator=(const Counter&) = delete;
    
    std::atomic<int> value;
    std::mutex mtx;
    std::condition_variable cv;
    
#ifdef _WIN32
    void* sharedMemory;
    void* mapHandle;
#else
    int shm_fd;
    void* sharedMemory;
#endif
};

#endif // COUNTER_H
