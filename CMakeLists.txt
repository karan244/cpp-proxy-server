cmake_minimum_required(VERSION 3.10)
project(ProxyServer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add pthread for later phases
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

include_directories(include)

add_executable(proxy_server
    src/main.cpp
    src/server.cpp
    src/http_proxy.cpp
    src/thread_pool.cpp
    src/access_control.cpp
    src/logger.cpp
)

# Copy the config folder to the build directory so the executable can find it
file(COPY config DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(proxy_server PRIVATE Threads::Threads)
