#include "SecurityManager.h"
#include "Base/BaseUtils.h"
#include "Base/Log.h"
#include "Constant/RedisKeys.h"
#include "RabbitMQ/RabbitmqClient.h"
#include "RabbitMQ/RabbitmqConsumer.h"
#include "RabbitMQ/RabbitmqMessageHandler.h"
#include "RabbitMQ/RabbitmqMessageJsonHandler.h"
#include "Redis/RedisPool.h"

#include "jsoncpp/include/json/json.h"
#include "json/value.h"
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

class AbnormalRecord {
private:
    // 记录异常行为的时间戳列表
    std::list<time_t> _timestmaps;

    // 信号量
    std::mutex _mtx;

public:
    AbnormalRecord() {}
    virtual ~AbnormalRecord() {}

public:
    /**
     * @brief 记录一次异常行为（3秒内20次异常）
     *
     * @param nowTime 当前时间
     * @return true 超出阈值
     * @return false 没超出
     */
    bool AbnormalBehavior(const time_t &nowTime) {
        std::lock_guard<std::mutex> lck(_mtx);
        time_t delta = 0LL;
        while (!_timestmaps.empty()) {
            const time_t &t = _timestmaps.front();

            delta = nowTime - t;
            if (delta > 3) {
                _timestmaps.pop_front();
            } else
                break;
        }
        _timestmaps.emplace_back(nowTime);
        return (_timestmaps.size() > 19);
    }
};

template <> SecurityManager *Singleton<SecurityManager>::_inst = nullptr;

SecurityManager::SecurityManager() : _initFlag(false) {}

SecurityManager::~SecurityManager() {}

void SecurityManager::init(const std::string &fanoutExchange,
                           const std::string &consumerTag) {
    if (_initFlag)
        return;
    _fanoutExchange = fanoutExchange;
    _consumerTag = consumerTag;
    _initFlag = true;

    std::vector<std::string> field;

    time_t delta = 0L;
    time_t timestramp = 0L;
    time_t nowTime = BaseUtils::getCurrentSecond();
    RedisPool::getSingleton().hkeys(RedisKeys::IP_BLACKLIST, field);
    for (const std::string &ip : field) {
        if (!RedisPool::getSingleton().hget(RedisKeys::IP_BLACKLIST, ip,
                                            timestramp))
            continue;
        delta = nowTime - timestramp;
        if (delta > 300) { // 5分钟
            RedisPool::getSingleton().hdel(RedisKeys::IP_BLACKLIST, ip);
            continue;
        }
        // 加入本地黑名单
        Add2BlackList(ip, timestramp);
    }

    class BlacklistHandler : public RabbitmqMesaageJsonHandler {
    public:
        BlacklistHandler(const std::string &tag)
            : RabbitmqMesaageJsonHandler(tag) {};

        virtual ~BlacklistHandler() {}

    protected:
        virtual bool receive(const std::string &message) override {
            std::string::size_type pos = message.find("MsgIpBlacklist");
            return (pos != std::string::npos);
        }

        virtual void handleImpl(const std::string &msgType,
                                const std::string &json) override {
            SecurityManager::getSingleton().handlerMessage(msgType, json);
        }
    };
    RabbitmqMesaageHandler::Ptr handler(new BlacklistHandler(consumerTag));
    RabbitmqConsumer::getSingleton().addHandler(handler);
}

void SecurityManager::Add2BlackList(const std::string &remoteIp,
                                    const time_t &timestampe) {
    std::lock_guard<std::mutex> lck(_mtx);
    std::unordered_map<std::string, time_t>::iterator it =
        _blackLists.find(remoteIp);
    if (it != _blackLists.end()) {
        it->second = timestampe;
    } else
        _blackLists.insert(std::pair(remoteIp, timestampe));
}

void SecurityManager::removeFromBlacklist(const std::string &remoteIp) {
    std::lock_guard<std::mutex> lck(_mtx);
    std::unordered_map<std::string, time_t>::iterator it =
        _blackLists.find(remoteIp);
    if (it != _blackLists.end()) {
        _blackLists.erase(it);
    }
}

std::shared_ptr<AbnormalRecord>
SecurityManager::getRecord(const std::string &remoteIp, bool createIfNotExist) {
    std::lock_guard<std::mutex> lck(_mtx);
    std::shared_ptr<AbnormalRecord> record;
    std::unordered_map<std::string, std::shared_ptr<AbnormalRecord>>::iterator
        it = _abnormalRecords.find(remoteIp);
    if (it != _abnormalRecords.end()) {
        record = it->second;
        return record;
    } else if (createIfNotExist) {
        record = std::make_shared<AbnormalRecord>();
        _abnormalRecords.insert(std::pair(remoteIp, record));
    }
    return record;
}

void SecurityManager::abnormalBehavior(const std::string &remoteIp) {
    std::shared_ptr<AbnormalRecord> record = getRecord(remoteIp, true);
    time_t nowTime = BaseUtils::getCurrentSecond();
    if (record->AbnormalBehavior(nowTime)) {
        WarningS << "Add remote ip: " << remoteIp << " to blacklist.";
        // 超出阈值，添加进黑名单中
        Add2BlackList(remoteIp, nowTime);
        // 向Redis中添加黑名单
        RedisPool::getSingleton().hset(RedisKeys::IP_BLACKLIST, remoteIp,
                                       nowTime);

        // 发送RabbitMQ广播，通知其他服务器该IP进入黑名单
        if (_fanoutExchange.empty()) {
            return;
        }
        Json::Value obj(Json::objectValue);
        obj["remoteIp"] = remoteIp;
        obj["timestramp"] = static_cast<int64_t>(nowTime);
        RabbitmqClient::getSingleton().publishJson(
            _fanoutExchange, std::string(), "MsgIpBlacklistAdd",
            obj.toStyledString());
    }
}

bool SecurityManager::getTimeStramp(const std::string &remoteIp,
                                    time_t &timestramp) {
    std::lock_guard<std::mutex> lck(_mtx);
    std::unordered_map<std::string, time_t>::iterator it =
        _blackLists.find(remoteIp);
    if (it != _blackLists.end()) {
        timestramp = it->second;
        return true;
    }
    return false;
}

bool SecurityManager::checkBlackList(const std::string &remoteIp) {
    time_t timestramp = 0L;
    if (!getTimeStramp(remoteIp, timestramp)) {
        return false;
    }
    time_t nowtime = BaseUtils::getCurrentSecond();
    time_t delta = 0L;

    delta = nowtime - timestramp;
    if (delta < 300)
        return true;
    // 超过5分钟
    RedisPool::getSingleton().hdel(RedisKeys::IP_BLACKLIST, remoteIp);
    removeFromBlacklist(remoteIp);

    // 发送RabbitMQ广播消息
    Json::Value obj(Json::objectValue);
    obj["removeIp"] = remoteIp;
    RabbitmqClient::getSingleton().publishJson(_fanoutExchange, std::string(),
                                               "MsgIpBlacklistRemove",
                                               obj.toStyledString());
    return true;
}

void SecurityManager::handlerMessage(const std::string &msgType,
                                     const std::string &json) {
    int test = 0;
    if (msgType == "MsgIpBlacklistAdd")
        test = 1;
    else if (msgType == "MsgIpBlacklistRemove")
        test = 2;
    if (test == 0)
        return;
    Json::Value obj;
    std::stringstream ss(json);
    ss >> obj;
    Json::Value fieldIp = obj["remoteIp"];

    if (!fieldIp.isString()) {
        return;
    }

    std::string remoteIp = fieldIp.asString();
    if (test == 1) {
        // 添加到黑名单
        Json::Value &fieldtimestramp = obj["timestamp"];
        if (!fieldtimestramp.isIntegral()) {
            return;
        }
        time_t timestramp = fieldtimestramp.asInt64();
        Add2BlackList(remoteIp, timestramp);
    } else {
        // 从黑名单中删除ip
        removeFromBlacklist(remoteIp);
    }
}
