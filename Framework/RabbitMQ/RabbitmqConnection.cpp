#include "RabbitmqConnection.h"
#include <mutex>

RabbitmqConnection::RabbitmqConnection():_ok(false){

}

RabbitmqConnection::~RabbitmqConnection(){
    
}

bool RabbitmqConnection::isOk() const{
    std::lock_guard<std::mutex> lck(_mtx);
    return _ok;
}

void RabbitmqConnection::setOk(bool setting){
    std::lock_guard<std::mutex>lck(_mtx);
    _ok = setting;
}
