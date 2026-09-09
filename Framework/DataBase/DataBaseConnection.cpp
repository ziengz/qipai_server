#include "DataBaseConnection.h"
#include "Base/BaseUtils.h"

DataBaseConnection::DataBaseConnection():
    _isOccupy(false),
    _laseOccupyTime(0),
    _lastQueryTime(0)
{}

void DataBaseConnection::occupy(bool flag){
    std::lock_guard<std::mutex> lck(_mtx);
    _isOccupy = true;
    if(flag)
        _laseOccupyTime = BaseUtils::getCurrentSecond();
}

void DataBaseConnection::recycle(){
    std::lock_guard<std::mutex> lck(_mtx);
    _isOccupy = false;
}

bool DataBaseConnection::isOccupied(){
    return _isOccupy;
}

void DataBaseConnection::setQueryTime(){
    std::lock_guard<std::mutex> lck(_mtx);
    _lastQueryTime = BaseUtils::getCurrentSecond();
}

void DataBaseConnection::getlastTime(time_t& occupyTime,time_t& queryTime){
    std::lock_guard<std::mutex> lck(_mtx);
    occupyTime = _laseOccupyTime;
    queryTime = _lastQueryTime;
}
