#include "ThreadWorkerDumb.h"

ThreadWorkerDumb::ThreadWorkerDumb(const ThreadStopFlag::Ptr& flag,int millisecond):
    _millisecond(millisecond),
    ThreadWorker(flag)
{

}

int ThreadWorkerDumb::oneLoopEx(){
    return _millisecond;
}