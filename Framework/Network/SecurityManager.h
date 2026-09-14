#pragma once

#include "Base/Singleton.h"
#include <atomic>
#include <ctime>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

/**
 * 网络安全管理者
 * 记录那些网络行为异常的ip地址，当在3秒内网络行为异常的次数达到20，则将该ip地址加入临时黑名单，
 * 在临时黑名单内的ip地址在5分钟内不允许再次连接到服务器
 */
class AbnormalRecord;
class SecurityManager : public Singleton<SecurityManager> {
  private:
    // 异常记录表
    // key-远端ip,value-异常记录
    std::unordered_map<std::string, std::shared_ptr<AbnormalRecord>>
        _abnormalRecords;

    // 黑名单列表
    // key-远端ip,value-被加入黑名单的时间
    std::unordered_map<std::string, time_t> _blackLists;

    // 信号量
    std::mutex _mtx;
    // 初始化标志
    std::atomic_bool _initFlag;

    // RabbitMQ扇出交换机（广播消息）名称
    std::string _fanoutExchange;

    // RabbitMQ广播消费者标签
    std::string _consumerTag;

  public:
    SecurityManager();
    virtual ~SecurityManager();

    friend class Singleton<SecurityManager>;

  public:
    /**
     * @brief 初始化
     *
     * @param fanoutExchange RabbitMQ交换机名称
     * @param consumerTag RabbitMQ广播消费者标签
     */
    void init(const std::string &fanoutExchange,
              const std::string &consumerTag);

    /**
     * @brief 记录一次网络异常行为
     *
     * @param remoteIp 产生异常行为的网络连接的远端IP
     */
    void abnormalBehavior(const std::string &remoteIp);

    /**
     * @brief 检察远端IP是否在黑名单中
     *
     * @param remoteIp 产生异常行为的网络连接的远端IP
     * @return true 在黑名单中
     * @return false 不在黑名单中
     */
    bool checkBlackList(const std::string &remoteIp);

    /**
     * @brief 添加远端ip到本地黑名单
     *
     * @param remoteIp 远端ip
     * @param timestampe 时间戳
     */
    void Add2BlackList(const std::string &remoteIp, const time_t &timestampe);

    /**
     * @brief 从黑名单中删除
     *
     * @param remoteIp 远端IP
     */
    void removeFromBlacklist(const std::string &remoteIp);

    /**
     * @brief 查询远端IP加入黑名单的时间戳
     *
     * @param remoteIp 远端IP
     * @param timestramp 时间戳
     * @return true 获取成功
     * @return false 获取失败，指定ip不在黑名单中
     */
    bool getTimeStramp(const std::string &remoteIp, time_t &timestramp);

    /**
     * @brief 处理RabbitMQ消息
     *
     * @param msgType 消息类型
     * @param json 消息体
     */
    void handlerMessage(const std::string &msgType, const std::string &json);

    /**
     * @brief 获取指定远端IP异常记录
     *
     * @param remoteIp 远端IP
     * @param createIfNotExist 若不存在则创建
     * @return std::shared_ptr<AbnormalRecord>
     */
    std::shared_ptr<AbnormalRecord> getRecord(const std::string &remoteIp,
                                              bool createIfNotExist);
};