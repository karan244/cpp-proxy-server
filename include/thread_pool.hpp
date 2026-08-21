#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <pthread.h>
#include <queue>
#include <vector>
#include <functional>

class ThreadPool {
public:
    // Initialize the thread pool with a specific number of workers
    ThreadPool(size_t num_threads);
    ~ThreadPool();

    // Add a new task (a function) to the queue
    void enqueue_task(std::function<void()> task);

private:
    std::vector<pthread_t> workers_;
    std::queue<std::function<void()>> task_queue_;

    // POSIX Synchronization primitives
    pthread_mutex_t queue_mutex_;
    pthread_cond_t condition_;
    bool stop_;

    // The function that each worker thread will run in a loop
    static void* worker_loop(void* arg);
};

#endif // THREAD_POOL_HPP
