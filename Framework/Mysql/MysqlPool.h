#include "Base/Singleton.h"
#include "Thread/ThreadDispatch.h"
#include "Thread/ThreadPool.h"
#include "MysqlQueryTask.h"

#include <memory>


class MysqlPoolImpl;
class MysqlPool : public ThreadPool, public Singleton<MysqlPool>{
private:
    std::shared_ptr<MysqlPoolImpl> _impl;

    MysqlPool() = default;


public:
    virtual ~MysqlPool() = default;
    friend class Singleton<MysqlPool>;

    /**
     * 启动
     * @param hostName 数据库服务器地址(如：tcp://127.0.0.1:3306)
     * @param userName 用户名
     * @param password 密码
     * @param schemaName 数据库名
     * @param keepConnections 保留的连接数量
     * @param maxConnections 最大连接数量(超出该数量则请求访问数据库报错)
     * @param threadNum 异步线程数量(最小为1，最大为8)
     */
    void Start(const std::string& hostName,
        const std::string& userName,
        const std::string& password,
        const std::string& schemaName,
        int keepConnections,
        int maxConnections,
        int threadNum);
    virtual void Stop() override;

    /**
     * 同步执行mysql查询任务
     * @param task mysql查询任务
     */
    void SyncQuery(const MysqlQueryTask::Ptr& task);
    
    /**
     * 同步执行不需要返回结果的sql语句，例如insert、update、delete
     * @param sql sql语句
     * @param type 查询类型
     */
    void SyncQuery(const std::string& sql, const MysqlQueryTask::QueryType& type);
    /**
     * 异步执行mysql查询任务
     * @param task mysql查询任务
     * @param dispatcher 派遣者线程(若不为空，则在该线程中调用MysqlTask::onQueried方法)
     */
    void AsyncQuery(const MysqlQueryTask::Ptr& task,const ThreadWorker::Ptr& dispatcher);
    /**
     * 异步执行不需要返回结果的sql语句，例如insert、update、delete
     * @param sql sql语句
     * @param type 查询类型
     * @param dispatcher 派遣者线程(若不为空，则在该线程中调用MysqlTask::onQueried方法)
     */
    void AsyncQuery(const std::string& sql,const MysqlQueryTask::QueryType& type,const ThreadWorker::Ptr& dispatcher);

};
