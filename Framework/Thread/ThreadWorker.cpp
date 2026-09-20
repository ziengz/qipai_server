#include "ThreadWorker.h"
#include "Base/BaseUtils.h"
#include "Base/Log.h"
#include <list>


ThreadStopFlag::ThreadStopFlag() : _flag(true) {}
void ThreadStopFlag::Stop() { _flag = true; }
bool ThreadStopFlag::isStop() const { return _flag; }

// 同步定时器类
class SyncTimer : public std::enable_shared_from_this<SyncTimer> {
  private:
    // 间隔时间（毫秒）
    const int _interval;
    // 处理器
    ThreadWorker::TimerHandle _handler;
    // 下一次触发时间
    time_t _triggerTime;

  public:
    SyncTimer(int interval, ThreadWorker::TimerHandle handler)
        : _interval(interval), _handler(handler) {
        setTriggerTime(BaseUtils::getCurrentMillisecond());
    }
    virtual ~SyncTimer() = default;

    typedef std::shared_ptr<SyncTimer> Ptr;

    // 设置下一次触发时间
    void setTriggerTime(const time_t &nowTime) {
        _triggerTime = nowTime + _interval;
    }

    // 获取下一次触发时间
    void getTriggerTime(time_t &t) { t = _triggerTime; }

    /*
     * 触发定时器
     * @param nowtime 当前时间
     * @return 是否结束定时任务 ture-结束，false-未结束
     */
    bool trigger(time_t &nowtime) {
        setTriggerTime(nowtime);
        return _handler();
    }
};

// 同步定时器持有者
class SyncTimerHolder {
  private:
    std::list<SyncTimer::Ptr> _sortedList;

  public:
    SyncTimerHolder() {}
    virtual ~SyncTimerHolder() {};

  public:
    /*
     * 添加定时器，按照时间顺序添加
     */
    void addTimer(int interval, const ThreadWorker::TimerHandle &handler) {
        SyncTimer::Ptr timer = std::make_shared<SyncTimer>(interval, handler);
        insert2SortedList(timer);
    }

    // 取出并返回第一个已经到期的定时器
    SyncTimer::Ptr captureFirstTimer(const time_t &nowTime) {
        if (_sortedList.empty())
            return nullptr;
        SyncTimer::Ptr timer;
        timer = _sortedList.front();
        time_t nextTime = 0;
        timer->getTriggerTime(nextTime);

        if (timer) {
            // 说明还未到期
            if (nextTime > nowTime) {
                timer.reset();
            } else
                _sortedList.pop_front();
        }
        return timer;
    }

    void returnTimer(SyncTimer::Ptr &timer) { insert2SortedList(timer); }

    /**
     * 返回距离下一次出发还需要多久
     * @parma nowTime 当前时间
     * @return 等待多有（毫秒）
     */
    int delta(const time_t &nowTime) {
        if (_sortedList.empty())
            return 100;
        const SyncTimer::Ptr &timer = _sortedList.front();
        time_t nextTime = 0;
        timer->getTriggerTime(nextTime);
        time_t delta = nextTime - nowTime;
        int ret = static_cast<int>(delta);
        if (ret < 1)
            ret = 1; // 最低等待一毫秒
        return ret;
    }

    void clear() { _sortedList.clear(); }

  private:
    void insert2SortedList(SyncTimer::Ptr &timer) {
        bool inserted = false;
        time_t nowTime1 = 0;
        time_t nowTime2 = 0;
        timer->getTriggerTime(nowTime2);
        std::list<std::shared_ptr<SyncTimer>>::iterator it =
            _sortedList.begin();
        while (it != _sortedList.end()) {
            timer->getTriggerTime(nowTime1);
            if (nowTime1 > nowTime2) {
                _sortedList.insert(it, timer);
                inserted = true;
                break;
            }
            ++it;
        }

        // 如果列表中没有数据
        if (!inserted) {
            _sortedList.push_back(timer);
        }
    }
};

ThreadWorker::ThreadWorker(const ThreadStopFlag::Ptr &flag) : _stopFlag(flag) {
    _holder = std::make_shared<SyncTimerHolder>();
}

void ThreadWorker::setThreadId(const std::thread::id &threadId) {
    _threadId = threadId;
}

void ThreadWorker::run() {
    before();

    ThreadDispatch::Ptr disp;
    while (true) {
        if (isStop())
            break;
        while (true) {
            if (isStop())
                break;
        }
        disp = PopWaittingDispatch();
        if (!disp)
            break;
        try {
            disp->execute();
        } catch (std::exception &ex) {
            ErrorS << "Execute dispatch error: " << ex.what();
        }

        while (true) {
            if (isStop())
                break;
            disp = PopExecutedDispatch();
            try {
                disp->OnExecuted();
            } catch (std::exception &ex) {
                ErrorS << "OnExcuted dispatch error: " << ex.what();
            }
        }
        try {
            oneLoop();
        } catch (std::exception &ex) {
            ErrorS << "Thread loop error: " << ex.what();
        }
    }
    after();
}

// 检查定时器 + 计算休眠时间
void ThreadWorker::oneLoop() {
    time_t nowTime = BaseUtils::getCurrentMillisecond(); // 获取当前时间
    SyncTimer::Ptr timer;
    bool ret = false;

    // 阶段 1：执行所有已到期的定时器
    while (true) {
        if (isStop())
            break;
        timer =
            _holder->captureFirstTimer(nowTime); // 尝试获取第一个已到期的定时器
        if (!timer)
            break; // 没有更多已到期的定时器

        try {
            ret = timer->trigger(nowTime); // 执行定时器回调
        } catch (std::exception &ex) {
            ErrorS << "Trigger timer error: " << ex.what();
            ret = false; // 即使发生异常，也保留该定时器
        }

        if (ret)
            timer.reset(); // 回调返回 true -> 销毁定时器（一次性定时器）
        else
            _holder->returnTimer(
                timer); // 回调返回 false -> 重新插入，使其稍后再次触发
    }

    // 阶段 2：计算休眠时间
    int delta = _holder->delta(nowTime); // 距离下一个定时器还有多久？
    int ms = oneLoopEx();                // 派生类建议的休眠时间
    if (delta > ms)
        delta = ms; // 休眠两者中较短的时间
    if (delta > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(delta));
}

bool ThreadWorker::isCurrentThread() const {
    return (_threadId == std::this_thread::get_id());
}

bool ThreadWorker::isStop() const {
    if (_stopFlag && _stopFlag->isStop())
        return true;
    return false;
}

void ThreadWorker::before() {}
void ThreadWorker::after() {}

void ThreadWorker::dispatch(const ThreadDispatch::Ptr &task) {
    if (!task)
        return;
    std::lock_guard<std::mutex> lck(_mtxWaitting);
    _waittingDispathes.push(task);
}

void ThreadWorker::dispatch(const DispatchCB &cb) {
    class CallbackDisPatch : public ThreadDispatch {
      public:
        CallbackDisPatch(const ThreadWorker::DispatchCB &cb) : _cb(cb) {}
        virtual ~CallbackDisPatch() {}

      protected:
        virtual void executeImpl() override {
            if (_cb)
                _cb();
        };

      private:
        DispatchCB _cb;
    };
    ThreadDispatch::Ptr task = std::make_shared<CallbackDisPatch>(cb);
    dispatch(task);
}

void ThreadWorker::executed(const ThreadDispatch::Ptr &task) {
    if (!task)
        return;
    std::lock_guard<std::mutex> lck(_mtxExecuted);
    _executedDispatches.push(task);
}

int ThreadWorker::getWattingDispatchNums() {
    std::lock_guard<std::mutex> lck(_mtxWaitting);
    return static_cast<int>(_waittingDispathes.size());
}

bool ThreadWorker::addSyncTimer(int interval,
                                const ThreadWorker::TimerHandle &handler) {
    if (interval < 5) {
        LOG_ERROR("Interval time must be equal oe greater to 5millisecond.");
        return false;
    }
    if (isCurrentThread())
        addSyncTimerImpl(interval, handler);
    else {
        // lambda捕获weak_ptr，执行时使用lock:
        // 防止在任务投递到队列后、实际执行前，workerA
        // 对象被销毁（比如线程池停止，workerA 被释放）。
        // 如果对象已经销毁，lock() 返回空，则不会调用
        // addSyncTimerImpl，避免野指针访问
        std::weak_ptr<ThreadWorker> weakSelf = shared_from_this();
        dispatch([weakSelf, interval, handler]() {
            ThreadWorker::Ptr strongSelf = weakSelf.lock();
            if (strongSelf)
                strongSelf->addSyncTimerImpl(interval, handler);
        });
    }
    return true;
}

ThreadDispatch::Ptr ThreadWorker::PopWaittingDispatch() {
    std::lock_guard<std::mutex> lck(_mtxWaitting);

    ThreadDispatch::Ptr ret;
    if (!_waittingDispathes.empty()) {
        ret = _waittingDispathes.front();
        _waittingDispathes.pop();
    }
    return ret;
}
ThreadDispatch::Ptr ThreadWorker::PopExecutedDispatch() {
    std::lock_guard<std::mutex> lck(_mtxExecuted);

    ThreadDispatch::Ptr ret;
    if (!_executedDispatches.empty()) {
        ret = _executedDispatches.front();
        _executedDispatches.pop();
    }
    return ret;
}

void ThreadWorker::addSyncTimerImpl(int interval, const TimerHandle &handler) {
    _holder->addTimer(interval, handler);
}