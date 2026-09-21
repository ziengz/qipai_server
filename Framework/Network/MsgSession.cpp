#include "MsgSession.h"
#include "Base/BaseUtils.h"
#include "Base/Log.h"
#include "Message/MessageHandler.h"
#include "Message/MessageManager.h"
#include "Message/MsgDisconnect.h"
#include "Message/MsgWrapper.h"
#include "Network/Connection.h"
#include "SecurityManager.h"
#include "Session.h"
#include <exception>
#include <memory>
#include <mutex>
#include <string>

class MsgSessionData {
public:
    MsgSessionData(int heartbeatValid)
        : _heartbeatValid(heartbeatValid), _handlerSN(0) {
        _heartbeat = BaseUtils::getCurrentSecond();
    }

    virtual ~MsgSessionData() {}

public:
    bool next() {
        bool ret = false;
        try {
            ret = _unpacker.next(_object_handle);
        } catch (std::exception &ex) {
            // 重置解包器
            _unpacker.reset();
            _unpacker.remove_nonparsed_buffer();
            ErrorS << "Unpack data error: " << ex.what();
        }
        return ret;
    }

    void synchronizeHandlers() {
        int sn = MessageManager::getSingleton().getHandlerSN();
        if (sn != _handlerSN) {
            _handlers.clear();
            MessageManager::getSingleton().getAllHandlers(_handlers);
            _handlerSN = sn;
        }
    }

    void heartbeat() {
        std::lock_guard<std::mutex> lck(_mtx);
        _heartbeat = BaseUtils::getCurrentSecond();
    }

    // 检测心跳是否超时
    bool isTimeOut(const time_t &nowTime) {
        if (_heartbeatValid < 1)
            return false;

        std::lock_guard<std::mutex> lck(_mtx);
        time_t delta = nowTime - _heartbeat;
        if (delta < _heartbeatValid) {
            return true;
        }
        return false;
    }

public:
    // 解包器
    msgpack::unpacker _unpacker;

    // 接收到的对象处理
    msgpack::object_handle _object_handle;

    // 消息处理器列表
    // 每个消息绘画拷贝一份处理器列表，避免接受网络消息时线程锁带来的性能消耗
    std::vector<MessageHandler::Ptr> _handlers;

    // 消息处理器序号
    int _handlerSN;

private:
    // 心跳有效时间，及超过时间，单位秒，小于等于0表示永不超时
    const int _heartbeatValid;
    // 上一次心跳时间
    time_t _heartbeat;
    // 信号量
    std::mutex _mtx;
};

MsgSession::MsgSession(const std::shared_ptr<Connection> &conn,
                       int heartbeatValid)
    : Session(conn) {
    _data = new MsgSessionData(heartbeatValid);
}

MsgSession::~MsgSession() {
    if (_data != nullptr) {
        delete _data;
        _data = nullptr;
    }
}

void MsgSession::onReceive(char *buf, std::size_t length) {
    if (buf == nullptr || length == 0)
        return;
    std::string remoteIp = getRemoteIp();
    if (SecurityManager::getSingleton().checkBlackList(remoteIp))
        return;

    _data->_unpacker.reserve_buffer(length);

    memcpy(_data->_unpacker.buffer(), buf, length);
    _data->_unpacker.buffer_consumed(length);

    bool test = true;
    while (_data->next()) {
        try {
            msgpack::object obj = _data->_object_handle.get();
            std::stringstream ss;
            ss << obj;
            MsgWrapper::Ptr wrapper = std::make_shared<MsgWrapper>();
            obj.convert(*wrapper);

            MsgBase::Ptr msg =
                MessageManager::getSingleton().createMessage(wrapper);
            if (msg) {
                NetMessage::Ptr netMsg = std::make_shared<NetMessage>(
                    shared_from_this(), msg, wrapper->getType());
                pushMsg(netMsg);
            } else {
                ErrorS << "Deserialize message of type: \""
                       << wrapper->getType() << "\"failed.";
                if (test) {
                    test = false;
                    // 记录一次异常行为
                    SecurityManager::getSingleton().abnormalBehavior(remoteIp);
                }
            }
        } catch (std::exception &ex) {
            ErrorS << "Unpack message error: " << ex.what();
            if (test) {
                test = false;
                // 记录一次异常行为
                SecurityManager::getSingleton().abnormalBehavior(remoteIp);
            }
            // 重置解包器
            _data->_unpacker.reset();
            _data->_unpacker.remove_nonparsed_buffer();
            break;
        }
    }
}

void MsgSession::pushMsg(const NetMessage::Ptr &msg) {
    _data->synchronizeHandlers();
    for (const MessageHandler::Ptr &handler : _data->_handlers) {
        // 判断该消息是否能接收
        if (handler->isReceive(msg)) {
            handler->push(msg);
            break;
        }
    }
}

void MsgSession::onDisconnect() {
    Session::onDisconnect();
    std::string sessionId;
    getId(sessionId);

    std::shared_ptr<MsgDisconnect> msg(new MsgDisconnect);
    msg->setSessionId(sessionId);

    NetMessage::Ptr netMsg =
        std::make_shared<NetMessage>(nullptr, msg, MsgDisconnect::TYPE);
    pushMsg(netMsg);
}

bool MsgSession::isAlive(const time_t &nowTime) const {
    if (!Session::isAlive(nowTime))
        return false;
    if (_data->isTimeOut(nowTime))
        return false;
    return true;
}

void MsgSession::heartbeat() { _data->heartbeat(); }
