#include "Base/Log.h"
#include "TimerManager.h"
#include "Base/BaseUtils.h"
#include <list>

class AsyncTimer : public std::enable_shared_from_this<AsyncTimer>{
private:
    const int _interval;
    ThreadWorker::TimerHandle _handler;
    time_t _triggerTime;
    std::mutex _mtx;

public:
    AsyncTimer(int interval,const ThreadWorker::TimerHandle& handler):
        _interval(interval),_handler(handler){
        setTriggerTime(BaseUtils::getCurrentMillisecond());
    }
    virtual ~AsyncTimer() = default;
    typedef std::shared_ptr<AsyncTimer> Ptr;

    int getInterval() const{
        return _interval;
    }
    void setTriggerTime(const time_t& nowtime){
        _triggerTime = nowtime + _interval;
    }
    void getTriggleTime(time_t& time){
        std::lock_guard<std::mutex> lck(_mtx);
        time = _triggerTime;
    }

    bool triggle(const time_t& nowTime){
        setTriggerTime(nowTime);
        return _handler();
    }
};

//定时异步持有者
class AsyncTimerHolder{
private:
    std::list<AsyncTimer::Ptr> _SortedList;
    std::mutex _mtx;

public:
    AsyncTimerHolder(){}
    virtual ~AsyncTimerHolder(){}

    bool addTimer(int interval,const ThreadWorker::TimerHandle& handler){
        if(interval < 5){
            LOG_ERROR("Interval time must be equal or greater to 5 millisecond");
            return false;
        }
        std::lock_guard<std::mutex> lck(_mtx);
        AsyncTimer::Ptr timer = std::make_shared<AsyncTimer>(interval,handler);
        Insert2SortedList(timer);
        return true;
    }

    AsyncTimer::Ptr CaptureFirstTimer(const time_t& nowTime){
        AsyncTimer::Ptr timer;
        if(_SortedList.empty()){
            return timer;
        }
        timer = _SortedList.front();
        if(timer){
            time_t nextTime;
            timer->getTriggleTime(nextTime);
            if(nextTime > nowTime){
                timer.reset();
            }else{
                _SortedList.pop_front();
            }
        }
        return timer;
    }

    void returnTimer(const AsyncTimer::Ptr& timer){
        std::lock_guard<std::mutex> lck(_mtx);
        _SortedList.push_back(timer);
    }

    void clear(){
        std::lock_guard<std::mutex> lck(_mtx);
        _SortedList.clear();
    }

    void Insert2SortedList(const AsyncTimer::Ptr& timer){
        bool inserted = false;
        time_t time1 = 0LL;
        time_t time2 = 0LL;
        std::list<AsyncTimer::Ptr>::iterator it = _SortedList.begin();
        timer->getTriggleTime(time2);
        while(it != _SortedList.end()){
            AsyncTimer::Ptr tmp = *it;
            tmp->getTriggleTime(time1);
            if(time1 > time2){
                _SortedList.insert(it, timer);
                inserted = true;
                break;
            }
            ++it;
        }
        if(!inserted){
            _SortedList.push_back(timer);
        }
    }
};

class TimerThreadWorker : public ThreadWorker{
private:
    std::shared_ptr<AsyncTimerHolder> _holder;
public:
    TimerThreadWorker(const ThreadStopFlag::Ptr& flag,const std::shared_ptr<AsyncTimerHolder> holder):
        ThreadWorker(flag),_holder(holder){

    }
    virtual ~TimerThreadWorker() = default;
protected:
    virtual int oneLoopEx() override{
        time_t nowTime = BaseUtils::getCurrentMillisecond();
        AsyncTimer::Ptr timer;
        bool ret = false;
        while(true){
            if(isStop())
                break;
            timer = _holder->CaptureFirstTimer(nowTime);
            if(!timer)
                break;
            try{
                ret = timer->triggle(nowTime);
            }
            catch(std::exception& ex){
                ErrorS << "Triggle timer error : "<< ex.what();
                ret = false;
            }
            if(ret){
                timer.reset();
            }
            else{
                _holder->returnTimer(timer);
            }
        }
        return 1;   //等待1毫秒
    }
};

template<> TimerManager* Singleton<TimerManager>::_inst = nullptr;

TimerManager::TimerManager(){
    _holder = std::make_shared<AsyncTimerHolder>();
}

void TimerManager::Start(int threadNum){
    if(threadNum < 1){
        threadNum = 1;
    }
    else if(threadNum > 8){
        threadNum = 8;
    }
    std::vector<ThreadWorker::Ptr> workers;
    ThreadStopFlag::Ptr flag = std::make_shared<ThreadStopFlag>();
    for(int i = 0;i < threadNum;i++){
        ThreadWorker::Ptr worker = std::make_shared<TimerThreadWorker>(flag,_holder);
        workers.push_back(worker);
    }
    ThreadPool::Start(flag,workers);
    InfoS << "TimerWorker Started,threadNum = " << threadNum;
}

void TimerManager::Stop(){
    ThreadPool::Stop();
    _holder->clear();
}

bool TimerManager::addAsyncTimer(int interval,const ThreadWorker::TimerHandle& handler){
    return _holder->addTimer(interval,handler);
}