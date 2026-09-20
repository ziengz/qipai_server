#pragma once

#include "MsgBase.h"

// 网络连接断开消息
class MsgDisconnect : public MsgBase {
  private:
    // 会话ID
    std::string _sessionId;

  public:
    MsgDisconnect() = default;
    virtual ~MsgDisconnect() = default;

    static const std::string TYPE;

  public:
    virtual const std::string &getType() const override;

    void setSessionId(std::string &sessionId) { _sessionId = sessionId; }

    std::string getSession() { return _sessionId; }
};