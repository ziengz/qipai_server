#pragma once
#include "Thread/ThreadWorker.h"
#include "NetMessage.h"
#include <memory>
#include <queue>
#include <string>
#include <unordered_set>

class MessageQueue {
  public:
    MessageQueue();
    virtual ~MessageQueue();

    typedef std::shared_ptr<MessageQueue> Ptr;

  public:
    // 消费队列
    void push(const NetMessage::Ptr &msg);
    // 弹出消息
    NetMessage::Ptr pop();

  private:
    std::queue<NetMessage::Ptr> _msgQueue;
    std::mutex _mtx;
};

/**
 * 消息处理器
 * 一个消息处理器可能有独占的消息队列，也可能多个消息处理器共享一个消息队列
 * 消息处理器共享消息队列的目的是为了支持同一类网络消息可以由多个消息处理器
 * （对应多个线程）共同处理，这类网络消息的处理通常涉及阻塞操作，例如数据库
 * 访问，多个线程共同处理可以提高系统并发
 * 消息处理器一般在程序启动时创建固定数量的实例，且创建之后这些实例伴随着程
 * 序的整个生命周期
 */
class MessageHandler : public std::enable_shared_from_this<MessageHandler> {
  private:
    // 消息队列
    MessageQueue::Ptr _msgQueue;

  protected:
    std::weak_ptr<ThreadWorker> _worker;
    // 接收到消息类型表
    std::unordered_set<std::string> _receiveMessages;

  public:
    MessageHandler();
    virtual ~MessageHandler();
    typedef std::shared_ptr<MessageHandler> Ptr;

  public:
    // 注册自身
    void registerSelf();

    // 添加接收到消息类型
    // 该函数只在实例初始化时（例如构造函数中）调用
    void addMessage(const std::string& msgType);

    // 初始化
    virtual void initialize();

    // 判断是否接受网络消息
    bool isReceive(const NetMessage::Ptr& netMsg) const;

    // 压入消息
    void push(const NetMessage::Ptr& netMsg);

    // 设置工作线程
    void setWorker(const ThreadWorker::Ptr& worker);

    ThreadWorker::Ptr getWorker() const;

    /**
     * @brief 处理消息
     * 
     * @return true 处理了消息
     * @return false 未处理消息
     */
    bool handle();

private:
    // 弹出消息
    NetMessage::Ptr pop();
 


  protected:
    /**
     * @brief 消息处理
     *
     * @param netMsg 网络消息
     * @return true 消息被处理
     * @return false 消息未被处理
     */
    virtual bool OnMessage(const NetMessage::Ptr &netMsg);

    /**
     * @brief 消息预处理
     *
     * @param netMsg 网络消息
     * @return true 预处理成功，
     * @return false 预处理失败，失败则消息不会真正处理
     */
    virtual bool preprocess(const NetMessage::Ptr &netMsg);
};
