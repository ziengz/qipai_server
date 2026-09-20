#pragma once
#include "Network/Session.h"

#include <memory>

#define MSG_PACK_IMPL                                                          \
    virtual std::shared_ptr<std::string> pack() const override {               \
        MsgWrapper mw;                                                         \
        std::shared_ptr<std::string> data = std::make_shared<std::string>();   \
        if (mw.packMessage(*this, *data))                                      \
            return data;                                                       \
        return nullptr;                                                        \
    }

class MsgBase {
  public:
    MsgBase();
    virtual ~MsgBase();

    typedef std::shared_ptr<MsgBase> Ptr;

  public:
    /**
     * @brief 获取消息类型
     *
     * @return const std::string&
     */
    virtual const std::string &getType() const = 0;

    /**
     * @brief 消息打包
     *
     * @return 打包数据，默认返回nullptr
     */
    virtual std::shared_ptr<std::string> pack() const;

    /**
     * @brief 发送消息
     *
     * @param session 连接会话
     */
    void send(Session::Ptr &session) const;
};