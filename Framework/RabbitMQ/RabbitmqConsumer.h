#pragma once
#include "Base/Singleton.h"
#include "RabbitmqMessageHandler.h"
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <vector>
#include <memory>



class RabbitmqConsumer: public Singleton<RabbitmqConsumer>{
private:
    // 启动标志
    std::atomic_bool _StartFlag;

    // 定时任务持有者，当该持有者被销毁，则说明单例实例被销毁
    // 定时任务可直接退出
    std::shared_ptr<int> _timer;

    // 消息处理器表
    // key-消费标签，value-消息处理器（一个 Tag 下可能注册多个不同的处理器实例）
    std::unordered_map<std::string,std::unordered_set<RabbitmqMesaageHandler::Ptr>> _handlers;
    
    // 消息处理器列表，用于在定时任务遍历
    std::vector<RabbitmqMesaageHandler::Ptr> _handlerList;

    // 处理器是否发生变化
    bool _changed;

    // 信号量
    std::mutex _mtx;

private:
    RabbitmqConsumer();

public:
    virtual ~RabbitmqConsumer();
    friend class Singleton<RabbitmqConsumer>;

    //启动
    void Start();

    //添加消息处理器
    void addHandler(const RabbitmqMesaageHandler::Ptr& handler);

    // 删除消息处理器
    bool removeHandler(const RabbitmqMesaageHandler::Ptr& handler);

    /**
     * @brief 消费消息
     * 
     * @param message 消息体
     * @param tag 消费标签
     */
    void consume(const std::shared_ptr<std::string>& message,const std::string& tag); 

private:
    /**
     * @brief Get the Handlers object
     * 
     * @param tag 消费标签
     * @param handlers 返回消费标签对应的处理器列表
     */
    void getHandlers(const std::string& tag,std::vector<RabbitmqMesaageHandler::Ptr>& handlers);

    bool hasHandler(const RabbitmqMesaageHandler::Ptr& handler);

    /**
     * @brief 查询处理器是否发生变化
     * 
     * @return true 发生
     * @return false 没发生
     */
    bool isChange();

    // 同步处理器列表
    void SyncHandlerList();

    // 定时任务
    bool OnTimer();
};