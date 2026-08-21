#include "thread_pool.hpp"
#include <iostream>

ThreadPool::ThreadPool(size_t num_threads) : stop_(false) {
    // Initialize the POSIX mutex and condition variable
    pthread_mutex_init(&queue_mutex_, nullptr);
    pthread_cond_init(&condition_, nullptr);

    // Create the fixed number of worker threads
    for (size_t i = 0; i < num_threads; ++i) {
        pthread_t thread;
        // We pass 'this' (the ThreadPool instance) as the argument to the worker loop
        // so that the static function can access our member variables
        if (pthread_create(&thread, nullptr, worker_loop, this) != 0) {
            std::cerr << "Failed to create thread\n";
        } else {
            workers_.push_back(thread);
        }
    }
}

ThreadPool::~ThreadPool() {
    // Lock the queue to safely change the stop_ flag
    pthread_mutex_lock(&queue_mutex_);
    stop_ = true;
    pthread_mutex_unlock(&queue_mutex_);

    // Wake up ALL sleeping threads so they can check the stop_ flag and exit
    pthread_cond_broadcast(&condition_);

    // Wait for all threads to finish their current task and exit
    for (pthread_t& thread : workers_) {
        pthread_join(thread, nullptr);
    }

    // Clean up POSIX primitives
    pthread_mutex_destroy(&queue_mutex_);
    pthread_cond_destroy(&condition_);
}

void ThreadPool::enqueue_task(std::function<void()> task) {
    // Lock the queue before modifying it to prevent race conditions
    pthread_mutex_lock(&queue_mutex_);
    
    task_queue_.push(task);
    
    // Unlock the queue so workers can access it
    pthread_mutex_unlock(&queue_mutex_);

    // Signal exactly ONE sleeping worker thread that a new task is available
    pthread_cond_signal(&condition_);
}

void* ThreadPool::worker_loop(void* arg) {
    // Cast the argument back to a ThreadPool pointer
    ThreadPool* pool = static_cast<ThreadPool*>(arg);

    while (true) {
        std::function<void()> task;

        // Lock the queue before checking if it's empty
        pthread_mutex_lock(&pool->queue_mutex_);

        // While the queue is empty AND we are not stopping, go to sleep
        // pthread_cond_wait automatically unlocks the mutex while sleeping,
        // and re-locks it when it wakes up. This prevents busy-waiting.
        while (pool->task_queue_.empty() && !pool->stop_) {
            pthread_cond_wait(&pool->condition_, &pool->queue_mutex_);
        }

        // If the pool is stopping and the queue is empty, exit the thread
        if (pool->stop_ && pool->task_queue_.empty()) {
            pthread_mutex_unlock(&pool->queue_mutex_);
            break;
        }

        // Grab the next task from the queue
        task = pool->task_queue_.front();
        pool->task_queue_.pop();

        // Unlock the queue BEFORE executing the task, so other threads
        // can grab tasks while this thread is busy working.
        pthread_mutex_unlock(&pool->queue_mutex_);

        // Execute the task! (This will call handle_client)
        task();
    }

    return nullptr;
}
