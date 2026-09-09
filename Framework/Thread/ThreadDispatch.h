#pragma once
#include <memory>

class ThreadWorker;
class ThreadDispatch : public std::enable_shared_from_this<ThreadDispatch>{
protected:
    //派遣者
    std::weak_ptr<ThreadWorker> _dispatcher;

public:
    ThreadDispatch(const std::shared_ptr<ThreadWorker>& dispatcher = nullptr);
    virtual ~ThreadDispatch() = default;

    typedef std::shared_ptr<ThreadDispatch> Ptr;
    
    /*
    * 执行者线程执行任务
    */
    void execute();

    //任务被执行之后，派遣者做后续处理
    virtual void OnExecuted();

protected:
    virtual void executeImpl() = 0;
};