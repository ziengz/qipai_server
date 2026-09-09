#include "MysqlPool.h"
#include "DataBase/DataBaseConnection.h"
#include "DataBase/DataBasePool.h"
#include "Thread/ThreadWorker.h"
#include "Timer/TimerManager.h"
#include "Base/Log.h"
#include <string>
#include <mysql/jdbc.h>

class MysqlConnection : public DataBaseConnection{
private:
    sql::Connection* _connection;
public:
    MysqlConnection(sql::Connection* connection):_connection(connection){

    }
    virtual ~MysqlConnection(){
        if(_connection){
            delete _connection;
            _connection = nullptr;
        }
    }

    sql::Connection* getConnection(){
        return _connection;
    }
};


class MysqlPoolImpl : public DataBasePool{
private:
    std::string _hostName;         //主机地址
    std::string _userName;         //用户名
    std::string _password;         //数据库密码
    std::string _schemaName;       //数据库名
    int _threadNum;                //线程数量
    sql::Driver* _driver;
    std::mutex _mtx;
    std::queue<ThreadDispatch::Ptr> _asyncTasks;
public:
    MysqlPoolImpl(const std::string& hostName,
        const std::string& userName,
        const std::string& password,
        const std::string& schemaName,
        int keepConnections,
        int maxConnection,
        int threadNum):
        _hostName(hostName),_userName(userName),_password(password),_schemaName(schemaName),
       DataBasePool(keepConnections,maxConnection),_threadNum(threadNum),_driver(nullptr){

    }

    virtual ~MysqlPoolImpl(){}
    typedef std::shared_ptr<MysqlPoolImpl> Ptr;

    DataBaseConnection::Ptr createConnection() override{
        DataBaseConnection::Ptr con;
        sql::Connection* sqlCon = nullptr;
        try{
            if(!_driver)
                _driver = sql::mysql::get_driver_instance();
            sqlCon = _driver->connect(_hostName,_userName,_password);
            if(sqlCon){
                sqlCon->setSchema(_schemaName);
            }
            con = std::make_shared<MysqlConnection>(sqlCon);
            return con;
        }catch(sql::SQLException& ex){
            ErrorS << "SQLException : " << ex.what()
                   << ", ErrorCode : " << ex.getErrorCode()
                   << ", ErrorState: "<< ex.getSQLState();
        }catch(std::exception& ex){
            ErrorS << "Exception : " << ex.what();
        }
        if(!con && sqlCon != nullptr){
            delete sqlCon;
        }
        return con;
    }

    virtual void onTimerImpl(DataBaseConnection::Ptr& con) override{
        std::string sql = "select 1";
        MysqlQueryTask::Ptr task = std::make_shared<MysqlCommandTask>(sql,MysqlQueryTask::QueryType::Select);
        //执行任务
        execute(task,con);
    }

    void execute(const MysqlQueryTask::Ptr& task){
        if(!task)
            return;
        DataBaseConnection::Ptr con = getConnection();
        if(con){
            execute(task,con);
        }
    }

    void pushAsyncTask(const ThreadDispatch::Ptr& task){
        std::lock_guard<std::mutex> lck(_mtx);
        _asyncTasks.push(task);
    }
    ThreadDispatch::Ptr popASyncTask(){
        std::lock_guard<std::mutex> lck(_mtx);
        ThreadDispatch::Ptr dispatch;
        if(!_asyncTasks.empty()){
            dispatch = _asyncTasks.front();
            _asyncTasks.pop();
        }
        return dispatch;
    }

    void clear(){
        clearAsyncTask();
        clearConnection();
    }
private:
    void execute(const MysqlQueryTask::Ptr& task,DataBaseConnection::Ptr& conIn){
        std::shared_ptr<MysqlConnection> con = std::dynamic_pointer_cast<MysqlConnection>(conIn);
        if(!con)
            return;

        bool disconnection = false;
        sql::Statement* stmt = nullptr;
        sql::ResultSet* res = nullptr;
        try{
            
            con->setQueryTime();
            std::string sql;
            
            MysqlQueryTask::QueryType type = task->buildQuery(sql);
            sql::Connection* sqlCon = con->getConnection(); 
            stmt = sqlCon->createStatement();
            if(type == MysqlQueryTask::QueryType::Select){
                res = stmt->executeQuery(sql);
                int rows = task->fetchResult(res);
                task->setRows(rows);
            }
            else if(type == MysqlQueryTask::QueryType::Delete||
                    type == MysqlQueryTask::QueryType::Insert||
                    type == MysqlQueryTask::QueryType::Update){
                int rows = stmt->executeUpdate(sql);
                task->setAffectedRecored(rows);
            }
            else if(type == MysqlQueryTask::QueryType::InsertAI){
                int rows = stmt->executeUpdate(sql);
                task->setAffectedRecored(rows);
                res = stmt->executeQuery("select LAST_INSERT_ID()");
                task->setAutoInc(res->getInt(1));
            }
            else{
                throw std::runtime_error("Unknow Query type");
            }
        }catch(sql::SQLException& ex){
            int errNo = ex.getErrorCode();
            std::string state = ex.getSQLState();
            if(errNo == 2008 || errNo == 2013 || state == "08S01"){
                //数据库连接失败
                disconnection = true;
            }
            ErrorS << "Exception : " << ex.what()
                   << ", ErrorCode : " << errNo
                   << ", ErrorState : "<< state;
        }
        catch(std::exception& ex){
            ErrorS << "Exception is "<<ex.what();
        }
        if(res)
            delete res;
        if(stmt)
            delete stmt;
        if(disconnection)
            removeConnection(con);
        else{
            con->recycle();
            notifyOne();
        }
    }
    void clearAsyncTask(){
        std::lock_guard<std::mutex> lck(_mtx);
        while(!_asyncTasks.empty()){
            _asyncTasks.pop();
        }
    }
};

class MysqlDispatch : public ThreadDispatch{
private:
    MysqlQueryTask::Ptr _task;
    std::weak_ptr<MysqlPoolImpl> _impl;
public:
    MysqlDispatch(const ThreadWorker::Ptr& dispatch, const MysqlQueryTask::Ptr& task, const MysqlPoolImpl::Ptr& impl):
        ThreadDispatch(dispatch),_task(task),_impl(impl)
    {

    }

    //任务被执行之后，派遣者做后续处理
    virtual void OnExecuted() override{
        _task->onQueried();
    }

    void executeImpl() override{
        MysqlPoolImpl::Ptr impl = _impl.lock();
        if(impl)
            impl->execute(_task);
        ThreadWorker::Ptr disp = _dispatcher.lock();
        if(!disp)
            _task->onQueried();
    }

};

class MysqlThreadWorker : public ThreadWorker{
private:
    MysqlPoolImpl::Ptr _impl;
public:
    MysqlThreadWorker(const ThreadStopFlag::Ptr& flag,MysqlPoolImpl::Ptr& impl):
        ThreadWorker(flag),_impl(impl){
        
    }
    virtual ~MysqlThreadWorker() = default;
    
    virtual int oneLoopEx()override{
        ThreadDispatch::Ptr task;
        while((task = _impl->popASyncTask()) != nullptr){
            dispatch(task);
        }
        return 10;      // 休眠10毫秒
    }
};

template<> MysqlPool* Singleton<MysqlPool>::_inst = nullptr;
void MysqlPool::Start(const std::string& hostName,
    const std::string& userName,
    const std::string& password,
    const std::string& schemaName,
    int keepConnections,
    int maxConnections,
    int threadNum){
    if(_impl)
        return;
    if(threadNum < 1)
        threadNum = 1;
    else if(threadNum > 8)
        threadNum = 8;
    if(isStart())
        return;
    if(keepConnections < 1)
        keepConnections = 1;
    if(maxConnections < 1)
        maxConnections = 1;
    
    _impl = std::make_shared<MysqlPoolImpl>(hostName,userName,password,schemaName,keepConnections,maxConnections,threadNum);
    ThreadStopFlag::Ptr flag = std::make_shared<ThreadStopFlag>();
    std::vector<ThreadWorker::Ptr> workers;
    for(int i = 0;i < threadNum;i++){
        ThreadWorker::Ptr worker = std::make_shared<MysqlThreadWorker>(flag,_impl);
        workers.push_back(worker);
    }
    ThreadPool::Start(flag,workers);

    std::weak_ptr<MysqlPoolImpl> weakImpl = _impl;
    TimerManager::getSingleton().addAsyncTimer(2000,[weakImpl](){
        MysqlPoolImpl::Ptr impl = weakImpl.lock();
        if(impl){
            return impl->onTimer();
        }
        return true;
    });
    InfoS << "Mysql pool started,host = "<< hostName <<",username = " << userName << ",schema = "<< schemaName <<
            ",keepConnection = "<< keepConnections << ",maxConnection = " << maxConnections << ",threadNum = " << threadNum;
}

void MysqlPool::Stop(){
    ThreadPool::Stop();
    if(_impl){
        _impl->clear();
        _impl->stop();
        _impl.reset();
    }
}

void MysqlPool::SyncQuery(const MysqlQueryTask::Ptr& task){
    _impl->execute(task);
    task->onQueried();
}

void MysqlPool::SyncQuery(const std::string& sql, const MysqlQueryTask::QueryType& type){
    MysqlQueryTask::Ptr task = std::make_shared<MysqlCommandTask>(sql,type);
    _impl->execute(task);
}

void MysqlPool::AsyncQuery(const MysqlQueryTask::Ptr& task,const ThreadWorker::Ptr& dispatcher){
    ThreadDispatch::Ptr disp = std::make_shared<MysqlDispatch>(dispatcher, task, _impl);
    _impl->pushAsyncTask(disp);
}

void MysqlPool::AsyncQuery(const std::string& sql,const MysqlQueryTask::QueryType& type,const ThreadWorker::Ptr& dispatcher){
    MysqlQueryTask::Ptr task = std::make_shared<MysqlCommandTask>(sql,type);
    AsyncQuery(task,dispatcher);
}