#pragma once

#include "RabbitmqConnection.h"
#include <memory>
#include <list>
#include <string>

class RabbitmqClient;
class ConfigItem;
class RabbitmqConfig: public std::enable_shared_from_this<RabbitmqConfig>{
private:
    // 配置项列表
    std::list<std::shared_ptr<ConfigItem>> _items;

public:
    RabbitmqConfig();
    virtual ~RabbitmqConfig();

    typedef std::shared_ptr<RabbitmqConfig> Ptr;
    friend class RabbitmqClient;

    /**
     * @brief 添加配置项，声明交换机（exchange）
     * 
     * @param exchange 交换机名称
     * @param type 类型（fanout,direct,topic）
     * @param durable 是否永久化，永久化交换机在RabbitMQ server中重启不会丢失
     * @param autoDelete 是否自动删除，true=当没有队列和交换机绑定且声明连接断开是自动删除
     * @param internal 是否内置的，true-Publisher无法直接发送消息到这个交换机中，只能通过其他交换机路由到这个交换机
     * @param redo 断线重连后是否需要重新执行该配置
     */
    void exchangeDeclare(const std::string& exchange,std::string& type,bool durable,bool autoDelete,bool internal,bool redo = false);

    /**
     * @brief 添加配置项：将交换机绑定到交换机
     * 
     * @param destination 目标交换机
     * @param source 源交换机
     * @param routingKey 路由键（定向或者通配符）
     * @param redo 断线重连后是否重新执行该配置
     */
    void exchangeBind(const std::string& destination,const std::string& source,const std::string&routingKey,bool redo = false);

    /**
     * @brief 添加配置项，解除交换机与交换机的绑定
     * 
     * @param destination 目的交换机
     * @param source 原交换机
     * @param routingKey 路由键（定向或者通配符）
     * @param redo 断线重连后是否需要重新执行该配置
     */
    void exchangeUnbind(const std::string& destination,const std::string& source,const std::string& routingKey,bool redo = false);
    
    /**
     * @brief 添加配置项：删除指定交换机
     * 
     * @param exchange 交换机名称
     * @param isUnused 是否在交换机没有使用的情况下删除，true-只有在交换机没有在使用的情况下才会被删除，false-强制删除
     * @param redo 断线重连后是否需要重新执行该配置
     */
    void exchangeDelete(const std::string& exchange,bool isUnused,bool redo = false);

    /**
     * @brief 添加配置项，声明队列，注意如果指定队列名称已存在，则直接返回true，不会重复声明
     * 
     * @param queue 队列名称 
     * @param durable 是否永久化，永久化交换机在RabbitMQ Server 重启后不丢失
     * @param exclusive 该属性为truea的队列之队首次声明他的连接可见，其他链接不能访问该队列，并且在连接断开时自动删除，即便durable为true
     * @param autoDelete 是否自动删除，true-当没有通道与该队列绑定且声明的连接断开时自动删除
     * @param redo 断线重连后是否需要重新执行该配置
     */
    void queueDeclare(const std::string& queue,bool durable,bool exclusive,bool autoDelete,bool redo = false);

    /**
     * @brief 添加配置项：将队列与交换机进行绑定，由Consumerr调用
     * 
     * @param queue 队列名称
     * @param exchange 交换机名称
     * @param routingKey 路由键（定向后者通配符）
     * @param redo 断线重连之后是否需要重新执行该配置
     */
    void queueBind(const std::string& queue,const std::string& exchange,const std::string& routingKey,bool redo = false);
    
    /**
     * @brief 添加配置项：将队列与交换机进行解绑，由Consumerr调用
     * 
     * @param queue 队列名称
     * @param exchange 交换机名称
     * @param routingKey 路由键（定向后者通配符）
     * @param redo 断线重连之后是否需要重新执行该配置
     */
    void queueUnbind(const std::string& queue,const std::string& exchange,const std::string& routingKey,bool redo = false);

    /**
     * @brief 添加配置项：删除指定队列
     * 
     * @param queue 队列名称
     * @param isUnused 是否在队列没有使用的情况下删除，true-只有在队列没有在使用的情况下才会被删除，false-强制删除
     * @param redo 断线重连后是否需要重新执行该配置
     */
    void queueDelete(const std::string& queue,bool ifUnused,bool ifEmpty,bool redo = false);

    /**
     * @brief 添加配置项：订阅
     * 
     * @param queue 队列名称
     * @param tag  消费者标签
     * @param noLocal 
     * @param noAck 是否需要返回ack
     * @param redo 段线重连之后是否需要重新执行该配置
     */
    void queueConsume(const std::string& queue,const std::string& tag,bool noLocal = false,bool noAck = false,bool exclusive = false,bool redo = false);

private:
    bool config(const RabbitmqConnection::Ptr& conn);
};
