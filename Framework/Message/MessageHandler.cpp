#include "Base/Log.h"
#include "MessageHandler.h"
#include "MessageManager.h"
#include "NetMessage.h"
#include "Thread/ThreadWorker.h"
#include <mutex>
#include <unordered_set>
#include <wincon.h>

MessageQueue::MessageQueue() {}

MessageQueue::~MessageQueue() {}

void MessageQueue::push(const NetMessage::Ptr &msg) {
    std::lock_guard<std::mutex> lck(_mtx);

    _msgQueue.push(msg);
}

NetMessage::Ptr MessageQueue::pop() {
    std::lock_guard<std::mutex> lck(_mtx);
    NetMessage::Ptr msg;
    if (!_msgQueue.empty()) {
        msg = _msgQueue.front();
        if (msg) {
            _msgQueue.pop();
        }
    }
    return msg;
}

MessageHandler::MessageHandler() {}

MessageHandler::~MessageHandler() {}

void MessageHandler::registerSelf() {
    MessageManager::getSingleton().registHandler(shared_from_this());
}

void MessageHandler::addMessage(const std::string &msgType) {
    if (msgType.empty()) {
        return;
    }
    _receiveMessages.insert(msgType);
}

bool MessageHandler::isReceive(const NetMessage::Ptr &netMsg) const {
    if (!netMsg)
        return false;
    std::unordered_set<std::string>::const_iterator it =
        _receiveMessages.find(netMsg->getType());
    return (it != _receiveMessages.end());
}

void MessageHandler::initialize() {}

void MessageHandler::push(const NetMessage::Ptr &msg) { _msgQueue->push(msg); }

NetMessage::Ptr MessageHandler::pop() { return _msgQueue->pop(); }

void MessageHandler::setWorker(const ThreadWorker::Ptr& worker){
    _worker = worker;
}

ThreadWorker::Ptr MessageHandler::getWorker() const{
    return _worker.lock();
}

bool MessageHandler::handle(){
    bool ret = false;
    while(true){
        NetMessage::Ptr msg = pop();
        if(!msg)
            break;
        ret = true;
        if(!preprocess(msg)){
            continue;
        }
        if(!OnMessage(msg)){
            WarningS << "NetWork message of type \"" << msg->getType() << "\" was not processed.";
        }
    }
    return ret;
}

bool MessageHandler::preprocess(const NetMessage::Ptr &netMsg) { return true; }

bool MessageHandler::OnMessage(const NetMessage::Ptr &netMsg) { return true; }

