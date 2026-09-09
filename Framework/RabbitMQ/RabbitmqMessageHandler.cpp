#include "RabbitmqMessageHandler.h"
#include <memory>
#include <mutex>

RabbitmqMesaageHandler::RabbitmqMesaageHandler(const std::string& tag):
    _tag(tag)
{}

RabbitmqMesaageHandler::~RabbitmqMesaageHandler(){}

const std::string& RabbitmqMesaageHandler::getTag() const{
    return _tag;
}

void RabbitmqMesaageHandler::push(const std::shared_ptr<std::string>& message){
    if(!message || !receive(*message))
        return;
    std::lock_guard<std::mutex> lxk(_mtx);
    _msgQueue.emplace_back(message);
}

std::shared_ptr<std::string> RabbitmqMesaageHandler::pop(){
    std::shared_ptr<std::string> ret;
    if(_msgQueue.empty())
        return ret;
    ret = _msgQueue.front();
    _msgQueue.pop_front();
    return ret;
}

void RabbitmqMesaageHandler::handle(){
    std::shared_ptr<std::string> ret;
    while(true){
        ret = pop();
        if(!ret)
            break;
        handleImpl(*ret);
    }
}

bool RabbitmqMesaageHandler::receive(const std::string& message){
    return false;
}