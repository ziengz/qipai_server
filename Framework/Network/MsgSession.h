#pragma once
#include "Message/NetMessage.h"
#include "Session.h"
#include <memory>
class MsgSessionData;
class MsgSession : public Session {
private:
    MsgSessionData *_data;

public:
    virtual void onDisconnect() override;

    virtual void onReceive(char *buf, std::size_t length) override;

    virtual bool isAlive(const time_t &nowTime) const override;

    virtual void heartbeat() override;

private:
    void pushMsg(const NetMessage::Ptr &msg);

public:
    MsgSession(const std::shared_ptr<Connection> &con, int heartbeatValid);
    virtual ~MsgSession();

    class Creator : public SessionCreater {
    public:
        Creator(int heartbeatValid = -1) : _heartbeatValid(heartbeatValid) {}

        virtual Session::Ptr
        create(const std::shared_ptr<Connection> &con) const override {
            Session::Ptr sess =
                std::make_shared<MsgSession>(con, _heartbeatValid);
            return sess;
        }

    private:
        // 心跳有效时间，即超时时间，单位秒，小于等于0表示心跳永不超时
        int _heartbeatValid;
    };
};