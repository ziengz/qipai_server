#pragma once
#include "MessageHandler.h"
#include "Thread/ThreadPool.h"

class MessageThreadPool : public ThreadPool {
  public:
    MessageThreadPool() = default;
    virtual ~MessageThreadPool() = default;

    /**
     * @brief 启动
     *
     * @param threadNum 线程数量
     * @param handlers 消息处理器列表
     */
    void Start(const int threadNum,
               const std::vector<MessageHandler::Ptr> &handlers);
};