#include "Base/Log.h"
#include "RabbitmqConsumer.h"
#include "RabbitmqMessageHandler.h"
#include "Thread/ThreadWorker.h"
#include "Timer/TimerManager.h"
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

template<> RabbitmqConsumer* Singleton<RabbitmqConsumer>::_inst = nullptr;

void RabbitmqConsumer::Start(){
    if(_StartFlag)
        return;
    _StartFlag = true;

    _timer = std::make_shared<int>();
    std::weak_ptr<int> weak(_timer);
    TimerManager::getSingleton().addAsyncTimer(50, [weak]{
        std::shared_ptr<int> strong = weak.lock();
        if(strong)
            return true;
        return RabbitmqConsumer::getSingleton().OnTimer();
    });
}

bool RabbitmqConsumer::hasHandler(const RabbitmqMesaageHandler::Ptr& handler){
    if(!handler)
        return false;
    std::unordered_map<std::string, std::unordered_set<RabbitmqMesaageHandler::Ptr>>::iterator it = _handlers.find(handler->getTag());
    if(it == _handlers.end())
        return false;
    const std::unordered_set<RabbitmqMesaageHandler::Ptr> setRef = it->second;
    std::unordered_set<RabbitmqMesaageHandler::Ptr>::const_iterator it_s = setRef.find(handler);
    return (it_s != setRef.end());
}


void RabbitmqConsumer::addHandler(const RabbitmqMesaageHandler::Ptr& handler){
    if(handler)
        return;
    if(hasHandler(handler))
        return;

    std::lock_guard<std::mutex>lxk(_mtx);

    std::unordered_map<std::string,std::unordered_set<RabbitmqMesaageHandler::Ptr>>::iterator it = _handlers.find(handler->getTag());
    //没找到
    if(it == _handlers.end()){
        _handlers.insert(std::make_pair(handler->getTag(),std::unordered_set<RabbitmqMesaageHandler::Ptr>()));
        
        it = _handlers.find(handler->getTag());
    }
    if(it != _handlers.end()){
        // 这里使用引用，相当于it->second->insert()
        std::unordered_set<RabbitmqMesaageHandler::Ptr>& setRef = it->second;
        setRef.insert(handler);
    }

    _changed = true;
}

bool RabbitmqConsumer::removeHandler(const RabbitmqMesaageHandler::Ptr& handler){
    if(!handler)
        return false;
    bool ret = false;

    std::lock_guard<std::mutex>lxk(_mtx);

    std::unordered_map<std::string, std::unordered_set<RabbitmqMesaageHandler::Ptr>>::iterator it = _handlers.find(handler->getTag());
    if(it != _handlers.end()){
        std::unordered_set<RabbitmqMesaageHandler::Ptr>& setRef = it->second;
        std::unordered_set<RabbitmqMesaageHandler::Ptr>::iterator it_s = setRef.find(handler);
        if(it_s != setRef.end()){
            setRef.erase(it_s);
            ret = true;
            _changed = true;
        }
        if(setRef.empty())
            _handlers.erase(it);
    }
    return ret;
}

void RabbitmqConsumer::getHandlers(const std::string& tag,std::vector<RabbitmqMesaageHandler::Ptr>& handlers){
    std::lock_guard<std::mutex>lck(_mtx);

    std::unordered_map<std::string, std::unordered_set<RabbitmqMesaageHandler::Ptr>>::iterator it = _handlers.find(tag);
    if(it != _handlers.end()){
        std::unordered_set<RabbitmqMesaageHandler::Ptr>& setRef = it->second;
        
        for(const RabbitmqMesaageHandler::Ptr& handler:setRef){
            handlers.emplace_back(handler);
        }
    }
}

void RabbitmqConsumer::consume(const std::shared_ptr<std::string>& message,const std::string& tag){
    std::vector<RabbitmqMesaageHandler::Ptr> handlers;
    getHandlers(tag, handlers);
    if(handlers.empty()){
        return;
    }
    for(RabbitmqMesaageHandler::Ptr handler : handlers){
        handler->push(message);
    }
}


bool RabbitmqConsumer::isChange(){
    std::lock_guard<std::mutex>lck(_mtx);
    return _changed;
}

void RabbitmqConsumer::SyncHandlerList(){
    _handlerList.clear();
    std::unordered_map<std::string, std::unordered_set<RabbitmqMesaageHandler::Ptr>>::iterator it;
    for(it = _handlers.begin();it != _handlers.end();++it){
        const std::unordered_set<RabbitmqMesaageHandler::Ptr>& setRef = it->second;
        for(const RabbitmqMesaageHandler::Ptr& handler : setRef)
            _handlerList.emplace_back(handler);
    }
    _changed = true;
}

bool RabbitmqConsumer::OnTimer(){
    if(isChange())
        SyncHandlerList();
    if(_handlerList.empty())
        return false;
    std::vector<RabbitmqMesaageHandler::Ptr>::iterator it;
    for(it = _handlerList.begin();it!=_handlerList.end();++it){
        const RabbitmqMesaageHandler::Ptr &handler = *it;
        try{
            handler->handle();
        }catch(std::exception& ex){
            ErrorS << "Handler message error: "<<ex.what();
        }
    }  
    return false;
}
