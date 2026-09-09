#pragma once
#include "ThreadDispatch.h"
#include <atomic>
#include <queue>
#include <mutex>
#include <functional>
#include <memory>
#include <thread>

class ThreadStopFlag{
    private:
    std::atomic_bool _flag;
    public:
    typedef std::shared_ptr<ThreadStopFlag> Ptr;
    ThreadStopFlag();
    virtual ~ThreadStopFlag() = default;

    void Stop();
    bool isStop() const;
};

//线程工作者，一个线程工作者代表一个线程
class ThreadPool;
class SyncTimerHolder;
class ThreadWorker : public std::enable_shared_from_this<ThreadWorker>{
private:
    //线程ID
    std::thread::id _threadId;
    //线程结束标志
    ThreadStopFlag::Ptr _stopFlag;
    
    //等待执行的派遣任务，（有其他线程派遣的任务列表）
    std::queue<ThreadDispatch::Ptr> _waittingDispathes;
    
    //执行完成的派遣任务，（有其他线程执行后返回的任务列表）
    std::queue<ThreadDispatch::Ptr> _executedDispatches;

    //信号量
    std::mutex _mtxWaitting;
    //信号量
    std::mutex _mtxExecuted;
    //定时器持有者
    std::shared_ptr<SyncTimerHolder> _holder;

public:
    ThreadWorker(const ThreadStopFlag::Ptr& flag);
    virtual ~ThreadWorker() = default;

    typedef std::shared_ptr<ThreadWorker> Ptr;
    typedef std::function<void()> DispatchCB;
    typedef std::function<bool()> TimerHandle;

    friend class ThreadPool;

private:
    //设置线程id
    void setThreadId(const std::thread::id& threadId);
    //线程循环
    void run();

    //但此线程循环
    void oneLoop();

protected:
    //判断是否为当前线程
    bool isCurrentThread() const;
    //当前是否正在等待线程结束
    bool isStop() const; 
    /*
    * 工作线程循环开始前
    * 执行线程初始化工作
    */
    virtual void before();
    /**
     * 工作线程循环结束后
     * 执行线程结束操作
     */
    virtual void after();

    /**
     * 单次线程循环
     * @return 返回接下来休眠多少时间（单位毫秒）
     */
    virtual int oneLoopEx() = 0;
    
public:
    /**
     * 其他线程派遣任务给本线程（this为执行者）
     * @param task 派遣任务报错
     */
    void dispatch(const ThreadDispatch::Ptr& task);

    /**
     * 其他线程派遣任务给本线程（this为本线程）
     *  @param cb 任务回调函数
     */
    void dispatch(const DispatchCB& cb);

    /**
     * 派遣任务已经被执行，执行者*其他线程）通知派遣者（本线程）做后续处理
     * @param task 派遣任务
     */
    void executed(const ThreadDispatch::Ptr& task);

    /**
     * 获取当前等待执行的派遣任务数量
     * @return 当前等待执行的派遣任务数量
     */
    int getWattingDispatchNums();

    /**
     * 添加同步定时器，记载本线程执行的定时器任务
     * @param interval 时间间隔（毫秒）间隔必须大于5毫秒
     * @param handler 定时器任务处理器（当返回true，结束定时任务）
     * @return true 成功添加，false 添加失败
     */
    bool addSyncTimer(int interval,const ThreadWorker::TimerHandle& handler);

private:
    /**
     * 弹出等待执行的派遣任务
     * @return 等待执行的派遣任务
     */
    ThreadDispatch::Ptr PopWaittingDispatch();

    /**
     * 弹出已经执行完的派遣任务
     * @return 已经执行的任务
     */
    ThreadDispatch::Ptr PopExecutedDispatch();

    /**
     * 添加同步定时器
     * @param interval 时间间隔（毫秒），间隔必须大于5毫秒
     * @param handler 定时任务处理器，当返回true时，结束定时任务
     */
    void addSyncTimerImpl(int interval,const TimerHandle& handler);

};
