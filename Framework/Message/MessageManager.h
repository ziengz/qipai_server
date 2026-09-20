#pragma once
#include "Base/Singleton.h"
#include "MessageHandler.h"
#include "MsgCreator.h"
#include "MsgWrapper.h"
#include <string>
#include <unordered_map>

class MessageManager : public Singleton<MessageManager> {
  private:
    // 消息生成器
    // 原则上该表会被多个线程访问，但是因为该表的插入式在程序初始化阶段完成的
    // 后续其他线程只会读取该表而不是修改，所以不需要该表做线程同步操作
    std::unordered_map<std::string, IMsgCreatot::Ptr> _creatorMap;

    // 消息处理器表
    std::vector<MessageHandler::Ptr> _handlers;

    // 消息处理器表更新序号
    int _handlerSN = 0;

    std::mutex _mtxHandler;

  public:
    MessageManager();
    virtual ~MessageManager();

    friend class Singleton<MessageManager>;

  public:
    /**
     * @brief 注册消息生成器
     *
     * @param type 消息类型
     * @param creator 生成器
     */
    void registerCreator(const std::string &type,
                         const IMsgCreatot::Ptr &creator);

    /**
     * @brief 创建消息实例
     *
     * @param wrapper 消息数据
     * @return MsgBase::Ptr
     */
    MsgBase::Ptr createMessage(const MsgWrapper::Ptr &wrapper);

    /**
     * @brief 注册消息处理器
     *
     * @param handler 处理器
     */
    void registHandler(const MessageHandler::Ptr &handler);

    /**
     * @brief 注销消息处理器
     *
     * @param handler 处理器
     */
    void unregisterHandler(const MessageHandler::Ptr &handler);

    // 查询消息处理器表更新序号
    int getHandlerSN();

    void getAllHandlers(std::vector<MessageHandler::Ptr> &vec);
};