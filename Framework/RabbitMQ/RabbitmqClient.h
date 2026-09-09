#pragma once
#include "Base/Singleton.h"
#include "RabbitmqConfig.h"
#include "RabbitmqConnection.h"
#include <atomic>
#include <memory>
#include <queue>
#include <thread>


class PublishItem;
class RabbitmqClient : public Singleton<RabbitmqClient> {
  public:
    RabbitmqClient();
    virtual ~RabbitmqClient();
    friend class Singleton<RabbitmqClient>;

  private:
    // Rabbitmq连接
    RabbitmqConnection::Ptr _connection;

    // 配置回调函数
    RabbitmqConfig::Ptr _config;

    // 停止标志
    std::atomic_bool _stopFlag;

    // 发布通道ID,用于发布线程
    int _publishId;

    // 信号量
    std::mutex _mtx;

    // 线程
    std::shared_ptr<std::thread> _thread;

    // 发布消息队列
    std::queue<std::shared_ptr<PublishItem>> _publishItem;

    // 发布队列信号量
    std::mutex _mtxPublish;

  public:
    /**
     * @brief 启动RabbitMQ客户端
     *
     * @param host 主机地址
     * @param port 主机端口
     * @param vhost 虚拟主机
     * @param hostName 主机名称
     * @param password 密码
     * @param config RabbitMQ配置表，用于绑定，订阅等操作
     * @return true 开启成功
     * @return false 开启失败
     */
    bool Start(const std::string &host, const int port,
               const std::string &vhost, const std::string &hostName,
               const std::string &password, const RabbitmqConfig::Ptr &config);

    /**
     * @brief 停止
     *
     * @param config Rabbitmq配置表，用于解绑、关闭等对资源的操作
     */
    void Stop(const RabbitmqConfig::Ptr &config);

    // 查询链接是否就绪
    bool isConnectionOk() const;

    /**
     * @brief 发布消息
     *
     * @param exchange  交换机名称
     * @param routingKey 路由键，
     * @param message 消息
     * @return true 发布成功
     * @return false 发布失败
     */
    bool publish(const std::string &exchange, const std::string &routingKey,
                 const std::string &message);

    /**
     * @brief 发布Json消息
     *
     * @param exchange 交换机名称
     * @param routingKey 路由键
     * @param msgType 消息类型
     * @param json 消息体
     * @return true 发布成功
     * @return false 发布失败
     */
    bool publishJson(const std::string &exchange, const std::string &routingKey,
                     const std::string &msgType, const std::string &json);

  private:
    // 检查链接
    bool checkConnection();

    //
    void onConnection(bool isOk);

    // 打开通道
    int openChannel();

    // 关闭通道
    void closeChannel(int channelId);

    // 获取发布通道ID
    int getPublishChannel();

    // 设置发布通道ID
    void setPublishChannel(int id);

    // 检查发布通道
    int checkPublish();

    // 发布函数
    bool publishInner();

    // 弹出发布消息
    std::shared_ptr<PublishItem> popPublishItem();

    // 发布线程
    void threadFunc();
};