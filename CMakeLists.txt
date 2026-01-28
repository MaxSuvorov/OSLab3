cmake_minimum_required(VERSION 3.10)
project(LabWork VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform-specific settings
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
endif()

find_package(Threads REQUIRED)

# Executable
add_executable(labwork
    main.cpp
    counter.cpp
    logger.cpp
    process_manager.cpp
)

target_link_libraries(labwork
    Threads::Threads
)

if(WIN32)
    target_link_libraries(labwork
        ws2_32
        dbghelp
    )
endif()

# Installation
install(TARGETS labwork DESTINATION bin)