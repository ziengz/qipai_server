#include "EchoSession.h"
#include "Connection.h"
#include "Session.h"

EchoSession::EchoSession(const std::shared_ptr<Connection>& con):Session(con){

}

EchoSession::~EchoSession(){}

void EchoSession::onReceive(char* buf,std::size_t length){
    send(buf,length);
}