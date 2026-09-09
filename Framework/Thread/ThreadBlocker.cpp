#include "ThreadBlocker.h"

ThreadBlocker::ThreadBlocker():_flag(true){

}

void ThreadBlocker::block(){
    std::unique_lock<std::mutex> lck(_mtx);
    while(_flag)
        _cond.wait(lck);   //阻塞
}

void ThreadBlocker::singal(){
    std::unique_lock<std::mutex> lck(_mtx);
    _flag = false;
    _cond.notify_all();
}

void ThreadBlocker::reset(){
    std::unique_lock<std::mutex> lck(_mtx);
    _flag = true;
}

