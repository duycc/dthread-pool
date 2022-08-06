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
    struct TaskFunc {
        TaskFunc(uint64_t _timeout) : timeout(_timeout) {}

        std::function<void()> func;
        uint64_t timeout{0}; // 任务超时时间
    };
    using TaskFuncPtr = std::shared_ptr<TaskFunc>;

    DThreadPool();

    virtual ~DThreadPool();

    bool init(size_t num);

    size_t getThreadNum();

    size_t getTaskNum();

    void stop();

    bool start();

    template <typename F, typename... Args>
    auto exec(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        return exec(0, f, args...);
    }

    template <typename F, typename... Args>
    auto exec(uint64_t timeout, F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        uint64_t expireTime = (timeout == 0 ? 0 : high_resolution_clock::now() + timeout); // TODO
        using RetType = decltype(f(args...));
        auto task =
            std::make_shared<std::packaged_task<RetType()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        TaskFuncPtr fPtr = std::make_shared<TaskFunc>(expireTime);
        fPtr->func = [task] { (*task)(); };

        std::unique_lock<std::mutex> lock(mutex_);
        tasks_.emplace(fPtr);
        condVar_.notify_one();
        return task->get_future();
    }

    bool waitForAllDone(int millsecond = -1);

    bool get(TaskFuncPtr& task);

    bool isTerminate() { return isTerminate_; }

    void run();

  private:
    std::queue<TaskFuncPtr> tasks_;
    std::vector<std::thread*> threads_;

    std::condition_variable condVar_;

    std::mutex mutex_;
    size_t threadNum_{1};
    bool isTerminate_{false};
    std::atomic<int> atomic_;
};

DThreadPool::DThreadPool() {}

DThreadPool::~DThreadPool() { stop(); }

bool DThreadPool::init(size_t num) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!threads_.empty()) {
        return false;
    }
    threadNum_ = num;
    return true;
}

size_t DThreadPool::getThreadNum() {
    std::unique_lock<std::mutex> lock(mutex_);
    return threads_.size();
}

size_t DThreadPool::getTaskNum() {
    std::unique_lock<std::mutex> lock(mutex_);
    return tasks_.size();
}

void DThreadPool::stop() {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        isTerminate_ = true;
        condVar_.notify_all();
    }
    for (auto* t : threads_) {
        if (t->joinable()) {
            t->join();
        }
        delete t;
        t = nullptr;
    }
    std::unique_lock<std::mutex> lock(mutex_);
    std::vector<std::thread*>().swap(threads_);
}

bool DThreadPool::start() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!threads_.empty()) {
        return false;
    }
    for (size_t i = 0; i < threadNum_; ++i) {
        threads_.emplace_back(new std::thread(&DThreadPool::run, this));
    }
    return true;
}

bool DThreadPool::get(TaskFuncPtr& task) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (tasks_.empty()) {
        condVar_.wait(lock, [this] { return isTerminate_ || !tasks_.empty(); });
    }
    if (isTerminate_) {
        return false;
    }
    if (!tasks_.empty()) {
        task = std::move(tasks_.front());
        tasks_.pop();
        return true;
    }
    return false;
}

void DThreadPool::run() {
    while (!isTerminate()) {
        TaskFuncPtr task;
        bool ok = get(task);
        if (ok) {
            ++atomic_;
            try {
                if (task->timeout != 0 && task->timeout < high_resolution_clock::now()) {

                } else {
                    task->func();
                }
            } catch (...) {
            }
            --atomic_;

            std::unique_lock<std::mutex> lock(mutex_);
            if (atomic_ == 0 && tasks_.empty()) {
                condVar_.notify_all();
            }
        }
    }
}

bool DThreadPool::waitForAllDone(int millsecond) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (tasks_.empty()) {
        return true;
    }
    if (millsecond < 0) {
        condVar_.wait(lock, [this] { return tasks_.empty(); });
        return true;
    }
    return condVar_.wait_for(lock, milliseconds(millsecond), [this] { return tasks_.empty(); });
}

} // namespace tp

#endif // D_THREAD_POOL_H
