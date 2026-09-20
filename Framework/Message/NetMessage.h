#pragma once
#include "MsgBase.h"
#include "Network/Session.h"
#include <memory>

class NetMessage{
private:
    std::string _type;
    std::weak_ptr<Session> _session;
    MsgBase::Ptr _msg;
public:
    NetMessage(const Session::Ptr& session,const MsgBase::Ptr& msg,const std::string& type);
    virtual ~NetMessage();
    typedef std::shared_ptr<NetMessage> Ptr;

    const std::string getType() const;
    
    Session::Ptr getSession() const;
    
    MsgBase::Ptr getMessage() const;

};