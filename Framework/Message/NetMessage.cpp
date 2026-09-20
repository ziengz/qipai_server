#include "NetMessage.h"
#include "MsgBase.h"

NetMessage::NetMessage(const Session::Ptr &session, const MsgBase::Ptr &msg,
                       const std::string &type)
    : _session(session), _msg(msg), _type(type) {}

const std::string NetMessage::getType() const { return _type; }

Session::Ptr NetMessage::getSession() const {
    Session::Ptr session = _session.lock();
    return session;
}

MsgBase::Ptr NetMessage::getMessage() const { return _msg; }