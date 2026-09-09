#include <string>
#include <memory>
#include <stdint.h>

namespace sql{
    class ResultSet;
}

class MysqlQueryTask : public std::enable_shared_from_this<MysqlQueryTask>{
private:
    //任务成功执行标志
    bool _succeed;
    //执行select返回行数
    int _rows;
    //执行insert,update,delete 受影响行数
    int _affectedRecords;
    //新插入记录的自增ID
    int64_t _autoInc;

public:
    MysqlQueryTask():
        _succeed(false),
        _rows(0),
        _affectedRecords(0),
        _autoInc(0LL)
    {}

    virtual ~MysqlQueryTask() = default;

    typedef std::shared_ptr<MysqlQueryTask> Ptr;

    //查询任务类型
    enum class QueryType{
        Select = 0,     //查找记录
        Insert = 1,     //增加记录
        InsertAI = 2,   //插入记录并获取自增ID(Auto Increment)
        Update = 3,     //更新记录
        Delete = 4      //删除记录
    };
public:
    /**
     * 构造查询语句（完整可执行的语句）
     * @param sql
     */
    virtual QueryType buildQuery(std::string& sql) = 0;

    /**
     * 获取查询结果
     * 仅任务类型为Select时才会调用该方法
     * @param res mysql查询集
     * @return 返回查询到的行数
     */
    virtual int fetchResult(sql::ResultSet* res){ return 0; }

    /**
     * 通知查询任务已经执行
     * 不管任务是否执行成功，结束后都会调用该方法
     * 异步查询时，如果设置了派遣者，则在派遣这线程中调用该方法
     * 如果没有设置，则在执行者线程中调用
     */
    virtual void onQueried() {}

    // 设置执行成功标志
    void setSucceed(){
        _succeed = true;
    }

    bool getSucceed(){
        return _succeed;
    }

    void setRows(int row){
        _rows = row;
    }
    int getRows() const{
        return _rows;
    }
    void setAffectedRecored(int s){
        _affectedRecords = s;
    }
    int getAffectedRecored() const{
        return _affectedRecords; 
    }

    void setAutoInc(const int64_t& ai){
        _autoInc = ai;
    }
    const int64_t& getAutoInc() const{
        return _autoInc;
    }
};

/**
 * muysql 通用查询任务
 * 如果有查询任务，则继承该类进行读取
 */
class MysqlCommandTask : public MysqlQueryTask{
public:
    MysqlCommandTask(const std::string& sql,QueryType type):
        _sql(sql),
        _type(type)
    {}
    virtual ~MysqlCommandTask(){};

    virtual QueryType buildQuery(std::string& sql) override{
        sql = _sql;
        return _type;
    } 

private:
    std::string _sql;
    QueryType _type;
};

/**
 * 查询记录数量任务
 * select count(*)...
 */
class MysqlCountTask : public MysqlCommandTask{
private:
    int _count;
public:
    MysqlCountTask(const std::string& sql);
    virtual ~MysqlCountTask();

    virtual int fetchResult(sql::ResultSet* res) override;

    int getCount() const;

};