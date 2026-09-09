#include "ThreadWorker.h"

class ThreadWorkerDumb : public ThreadWorker{
private:
    int _millisecond;
public:

    /**
     * 线程阻塞
     */
    ThreadWorkerDumb(const ThreadStopFlag::Ptr& flag,int millisecond);
    ~ThreadWorkerDumb() = default;

    int oneLoopEx() override;
};