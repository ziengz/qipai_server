#include "Base/BaseUtils.h"
#include "DataBasePool.h"

DataBasePool::DataBasePool(int keepConnections,int maxConnections):
    _keepConnections(keepConnections),_maxConnections(maxConnections)
{}

bool DataBasePool::onTimer(){
    if(_stopFlag)
        return true;
    DataBaseConnection::Ptr con = onTimerImpl();
    if(con){
        onTimerImpl(con);
    }
    return false;
}

void DataBasePool::stop(){
    _stopFlag = true;
}

DataBaseConnection::Ptr DataBasePool::getConnection(){
    ThreadBlocker::Ptr blocker;
    DataBaseConnection::Ptr con;
    while(true){
        if(_stopFlag)
            return con;
        con = getConnection(blocker);
        if(con)
            break;
        if(blocker){
            //开始阻塞
            blocker->block();
            //阻塞结束
            blocker = nullptr;
        }
        else
            break;
    }
    return con;
}

DataBaseConnection::Ptr DataBasePool::getConnection(ThreadBlocker::Ptr& blocker){
    DataBaseConnection::Ptr con;
    if(_stopFlag)
        return con;
    std::lock_guard<std::mutex> lck(_mtx);
    for(const DataBaseConnection::Ptr& tmp : _connections){
        if(tmp->isOccupied()){
            continue;;
        }
        tmp->occupy();
        con = tmp;
        return con;
    }

    int num = static_cast<int>(_connections.size());
    if(num >= _maxConnections){
        //连接数已达上限：创建阻塞器排队等待，不再新建连接
        blocker = std::make_shared<ThreadBlocker>();
        _blockQueue.push(blocker);
        return con;
    }

    con = createConnection();
    if(con){
        con->occupy();
        _connections.push_back(con);
    }
    return con;
}

void DataBasePool::removeConnection(const DataBaseConnection::Ptr& con){
    std::lock_guard<std::mutex> lck(_mtx);
    std::list<DataBaseConnection::Ptr>::iterator it = _connections.begin();
    for(;it != _connections.end();++it){
        if(*it == con){
            _connections.erase(it);
            break;      //连接在列表中唯一，删除后即可结束
        }
    }
    if(!_blockQueue.empty()){
        ThreadBlocker::Ptr blocker = _blockQueue.front();
        _blockQueue.pop();
        blocker->singal();
    }
}

void DataBasePool::notifyOne(){
    if(_blockQueue.empty()){
        return;
    }
    ThreadBlocker::Ptr blocker = _blockQueue.front();
    _blockQueue.pop();
    blocker->singal();
}

void DataBasePool::clearConnection(){
    std::lock_guard<std::mutex> lck(_mtx);
    _connections.clear();
}

DataBaseConnection::Ptr DataBasePool::onTimerImpl(){
    //获取当前时间
    time_t nowTime = BaseUtils::getCurrentSecond();

    std::lock_guard<std::mutex> lck(_mtx);
    DataBaseConnection::Ptr con;
    int num = static_cast<int>(_connections.size());
    bool removeFlag = true;
    bool test = false;

    if(num < _keepConnections)
        removeFlag = false;
    const time_t MIN5 = 300LL;   //5分钟
    const time_t SEC30 = 30LL;   //30秒
    time_t occupyTime = 0LL;
    time_t queryTime = 0LL;
    time_t delta = 0LL;

    std::list<DataBaseConnection::Ptr>::iterator it = _connections.begin();
    while(it != _connections.end()){
        DataBaseConnection::Ptr con = *it;
        if(con->isOccupied()){
            ++it;
            continue;
        }
        con->getlastTime(occupyTime,queryTime);
        delta = nowTime - occupyTime;
        if(delta > MIN5){
            // 链接超时5分钟未被占用，删除
            test = true;
        }
    }
    if(test){
        it = _connections.erase(it);
        --num;
        if(!_blockQueue.empty()){
            ThreadBlocker::Ptr block = _blockQueue.front();
            _blockQueue.pop();
            block->singal();
        }
        if(num < _keepConnections)
            removeFlag = false;
    }
    else{
        delta = nowTime - queryTime;
        //如果查询最新时间大于30秒
        if(delta > SEC30){
            con->occupy(false);
            return con;
        }
        ++it;
    }
    return nullptr;
}