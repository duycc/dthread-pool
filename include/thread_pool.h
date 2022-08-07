//===--------------------------- include/thread_pool.h - [dthread-pool] ---------------------------------*- C++ -*-===//
// Brief :
//
//
// Author: YongDu
// Date  : 2022-08-07
//===--------------------------------------------------------------------------------------------------------------===//

#if !defined(D_THREAD_POOL_H)
#define D_THREAD_POOL_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

using namespace std::chrono;

namespace tp {
class DThreadPool {
  public:
    explicit DThreadPool(size_t);
    DThreadPool(const DThreadPool&) = delete;
    DThreadPool& operator=(const DThreadPool&) = delete;

    virtual ~DThreadPool();

    // 启动线程池
    void start();

    // 线程入口函数
    void run();

    // 获取一个待执行的任务
    std::function<void()> get();

    // 提交一个待执行的任务
    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))>;

    // 等待队列中所有任务执行完毕，如果超时未完成，不继续等待
    bool waitForAllDone(int timeoutMs = -1);

    // 关闭线程池
    void stop();

    // 获取线程个数
    size_t getThreadNum() { return threadNum_; }

    // 获取任务个数
    size_t getTaskNum() {
        std::unique_lock<std::mutex> lock(taskMutex_);
        return tasks_.size();
    }

  private:
    std::queue<std::function<void()>>         tasks_;   // 任务队列
    std::vector<std::shared_ptr<std::thread>> threads_; // 工作线程

    std::mutex              taskMutex_;     // 线程间互斥
    std::condition_variable condVar_;       // 线程间同步
    std::atomic<bool>       stop_{false};   // 线程池是否退出
    std::atomic<int>        runTaskNum_{0}; // 运行中的任务个数
    size_t                  threadNum_;     // 线程数
};

DThreadPool::DThreadPool(size_t threadNum) {
    threadNum_ = std::min(threadNum, static_cast<size_t>(std::thread::hardware_concurrency()) * 2 + 2);
    threads_.reserve(threadNum_);
}

DThreadPool::~DThreadPool() { stop(); }

void DThreadPool::start() {
    for (size_t i = 0; i < threadNum_; ++i) {
        threads_.emplace_back(std::make_shared<std::thread>(&DThreadPool::run, this));
    }
}

void DThreadPool::run() {
    while (!stop_) {
        auto&& task = get();
        if (task) {
            ++runTaskNum_;
            try {
                task();
            } catch (...) {
            }
            --runTaskNum_;

            std::unique_lock<std::mutex> lock(taskMutex_);
            if (runTaskNum_ == 0 && tasks_.empty()) {
                condVar_.notify_all(); // 通知 waitForAllDone()
            }
        }
    }
}

std::function<void()> DThreadPool::get() {
    std::unique_lock<std::mutex> lock(taskMutex_);
    if (tasks_.empty()) {
        condVar_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
    }
    if (stop_) {
        return std::function<void()>(nullptr);
    }
    auto task = std::move(tasks_.front());
    tasks_.pop();
    return task;
}

template <typename F, typename... Args>
auto DThreadPool::submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
    using RetType = decltype(f(args...));
    auto task =
        std::make_shared<std::packaged_task<RetType()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    std::unique_lock<std::mutex> lock(taskMutex_);
    tasks_.emplace([task] { (*task)(); });
    condVar_.notify_one();
    return task->get_future();
}

bool DThreadPool::waitForAllDone(int timeoutMs) {
    std::unique_lock<std::mutex> lock(taskMutex_);
    if (tasks_.empty()) {
        return true;
    }
    if (timeoutMs <= 0) {
        condVar_.wait(lock, [this] { return tasks_.empty(); });
        return true;
    }
    return condVar_.wait_for(lock, milliseconds(timeoutMs), [this] { return tasks_.empty(); });
}

void DThreadPool::stop() {
    stop_ = true;
    condVar_.notify_all();
    for (auto& t : threads_) {
        if (t->joinable()) {
            t->join();
        }
    }
    decltype(threads_)().swap(threads_); // 释放内存，clear()只是清空元素
}

} // namespace tp

#endif // D_THREAD_POOL_H
