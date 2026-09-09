#include "Base/Singleton.h"
#include "Thread/ThreadWorker.h"
#include "Thread/ThreadPool.h"
#include <memory>

class AsyncTimerHolder;
class TimerManager : public ThreadPool, public Singleton<TimerManager>{
private:
    std::shared_ptr<AsyncTimerHolder> _holder;

    TimerManager();
public:
    virtual ~TimerManager() = default;
    friend class Singleton<TimerManager>;
    /**
     * 开始线程
     */
    void Start(int threadNum);

    /**
     * 结束线程
     */
    virtual void Stop() override;

    /**
     * 添加定时异步处理器
     * @param interval 时间间隔，间隔需大于5毫秒，
     * @param handler 定时处理器，返回true时结束定时任务
     * @return true-成功添加，false-添加失败
     */
    bool addAsyncTimer(int interval,const ThreadWorker::TimerHandle& handler);

};