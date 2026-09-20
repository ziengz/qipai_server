#include "MessageManager.h"
#include "Base/BaseUtils.h"
#include "Base/Log.h"
#include "MessageHandler.h"
#include "MsgBase.h"
#include "MsgCreator.h"
#include "MsgWrapper.h"
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

template <> MessageManager *Singleton<MessageManager>::_inst = nullptr;
MessageManager::MessageManager() : _handlerSN(0) {}

MessageManager::~MessageManager() {}

void MessageManager::registerCreator(const std::string &type,
                                     const IMsgCreatot::Ptr &creator) {
    if (type.empty() || !creator)
        return;

    std::unordered_map<std::string, IMsgCreatot::Ptr>::iterator it =
        _creatorMap.find(type);
    if (it != _creatorMap.end()) {
        _creatorMap.insert(std::make_pair(type, creator));
    } else {
        ErrorS << "Regist message creator error,message creator with name \""
               << type << "\"already existed.";
    }
}

MsgBase::Ptr MessageManager::createMessage(const MsgWrapper::Ptr &wrapper) {
    if (!wrapper)
        return MsgBase::Ptr();
    std::unordered_map<std::string, IMsgCreatot::Ptr>::const_iterator it =
        _creatorMap.find(wrapper->getType());
    if (it == _creatorMap.end()) {
        return MsgBase::Ptr();
    }
    const IMsgCreatot::Ptr creator = it->second;
    return creator->deserialize(wrapper);
}

void MessageManager::registHandler(const MessageHandler::Ptr &handler) {
    std::lock_guard<std::mutex> lck(_mtxHandler);

    // 先查一下是否已经存在
    if (BaseUtils::contain(_handlers, handler)) {
        return;
    }
    _handlers.push_back(handler);
    _handlerSN++;
}

void MessageManager::unregisterHandler(const MessageHandler::Ptr &handler) {
    if (!handler)
        return;
    bool flag = false;
    std::vector<MessageHandler::Ptr>::iterator it = _handlers.begin();
    while (it != _handlers.end()) {
        if (*it == handler) {
            _handlers.erase(it);
            flag = true;
            break;
        }
        ++it;
    }
    if (flag) {
        _handlerSN++;
    }
}

int MessageManager::getHandlerSN() {
    std::lock_guard<std::mutex> lck(_mtxHandler);
    return _handlerSN;
}

void MessageManager::getAllHandlers(std::vector<MessageHandler::Ptr> &vec) {
    vec.clear();
    std::lock_guard<std::mutex> lck(_mtxHandler);
    for (const MessageHandler::Ptr handler : _handlers) {
        vec.emplace_back(handler);
    }
}