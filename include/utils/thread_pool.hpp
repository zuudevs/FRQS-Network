#pragma once

/**
 * @file utils/thread_pool.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief 
 * @version 1.0.0
 * @date 2025-12-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <stdexcept>

namespace frqs::utils {

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency()) ;
    ~ThreadPool() ;
    
    // Delete copy and move operations
    ThreadPool(const ThreadPool&) = delete ;
    ThreadPool& operator=(const ThreadPool&) = delete ;
    ThreadPool(ThreadPool&&) = delete ;
    ThreadPool& operator=(ThreadPool&&) = delete ;
    
    // Submit a task to the pool
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> ;
    
    // Get the number of threads
    [[nodiscard]] size_t size() const noexcept { return workers_.size() ; }
    
    // Get the number of pending tasks
    [[nodiscard]] size_t pendingTasks() const noexcept ;

private:
    std::vector<std::thread> workers_ ;
    std::queue<std::function<void()>> tasks_ ;
    
    mutable std::mutex queue_mutex_ ;
    std::condition_variable condition_ ;
    bool stop_ = false ;
    
    void workerThread() ;
} ;

// Template implementation must be in header
template<typename F, typename... Args>
auto ThreadPool::submit(F&& f, Args&&... args) 
    -> std::future<std::invoke_result_t<F, Args...>> {
    
    using return_type = std::invoke_result_t<F, Args...> ;
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    ) ;
    
    std::future<return_type> result = task->get_future() ;
    
    {
        std::lock_guard<std::mutex> lock(queue_mutex_) ;
        
        if (stop_) {
            throw std::runtime_error("ThreadPool is stopped") ;
        }
        
        tasks_.emplace([task]() { (*task)() ; }) ;
    }
    
    condition_.notify_one() ;
    return result ;
}

} // namespace frqs::utils