#include "ThreadDispatch.h"
#include "ThreadWorker.h"
#include "Base/Log.h"

ThreadDispatch::ThreadDispatch(const std::shared_ptr<ThreadWorker>& dispather):_dispatcher(dispather)
{

}

void ThreadDispatch::execute(){
    try{
        executeImpl();
    }catch(std::exception& ex){
        ErrorS << "Execete dispatch error: "<<ex.what();
    }
    ThreadWorker::Ptr disp = _dispatcher.lock();
    if(disp)
        disp->executed(shared_from_this());

}

void ThreadDispatch::OnExecuted(){}


