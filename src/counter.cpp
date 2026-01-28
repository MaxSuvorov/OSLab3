#include "counter.h"
#include <iostream>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

Counter& Counter::getInstance() {
    static Counter instance;
    return instance;
}

Counter::Counter() : value(0) {
#ifdef _WIN32
    // Windows shared memory
    sharedMemory = nullptr;
    mapHandle = CreateFileMapping(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        sizeof(int),
        L"Global\\CounterSharedMemory"
    );
    
    if (mapHandle == NULL) {
        std::cerr << "Failed to create file mapping: " << GetLastError() << std::endl;
        return;
    }
    
    sharedMemory = MapViewOfFile(mapHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(int));
    if (sharedMemory == NULL) {
        std::cerr << "Failed to map view of file: " << GetLastError() << std::endl;
        CloseHandle(mapHandle);
        return;
    }
    
    // Initialize if first process
    int* sharedValue = static_cast<int*>(sharedMemory);
    *sharedValue = 0;
#else
    // POSIX shared memory
    shm_fd = shm_open("/counter_shm", O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return;
    }
    
    if (ftruncate(shm_fd, sizeof(int)) == -1) {
        perror("ftruncate");
        return;
    }
    
    sharedMemory = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (sharedMemory == MAP_FAILED) {
        perror("mmap");
        return;
    }
    
    // Initialize if first process
    int* sharedValue = static_cast<int*>(sharedMemory);
    *sharedValue = 0;
#endif
    
    // Read initial value from shared memory
    value.store(*static_cast<int*>(sharedMemory));
}

void Counter::increment() {
    std::lock_guard<std::mutex> lock(mtx);
    int current = value.load();
    value.store(current + 1);
    
    // Update shared memory
    if (sharedMemory) {
        *static_cast<int*>(sharedMemory) = value.load();
    }
}

void Counter::setValue(int newValue) {
    std::lock_guard<std::mutex> lock(mtx);
    value.store(newValue);
    
    // Update shared memory
    if (sharedMemory) {
        *static_cast<int*>(sharedMemory) = value.load();
    }
    cv.notify_all();
}

int Counter::getValue() {
    return value.load();
}

void* Counter::getSharedMemory() {
    return sharedMemory;
}

size_t Counter::getSharedMemorySize() {
    return sizeof(int);
}
