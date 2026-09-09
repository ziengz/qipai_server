#pragma once
#include "ThreadWorker.h"
#include <vector>

class ThreadPool: public std::enable_shared_from_this<ThreadPool>{
public:
    ThreadPool();
    virtual ~ThreadPool() = default;
    typedef std::shared_ptr<ThreadPool> Ptr;
    /**
     * 启动线程
     */
    void Start(const ThreadStopFlag::Ptr& flag,const std::vector<ThreadWorker::Ptr>& workers);

    /**
     * 结束线程
     */
    virtual void Stop();

protected:
    bool isStart();
private:
    std::atomic_bool _startFlag;
    std::vector<std::shared_ptr<std::thread>> _threads;
    ThreadStopFlag::Ptr _stopFlag;

};
