#pragma once
#include <memory>
#include <mutex>

class DataBaseConnection : public std::enable_shared_from_this<DataBaseConnection>{
private:
    //最新查询时间(单位秒)
    time_t _lastQueryTime;
    //最新占据时间（单位秒）
    time_t _laseOccupyTime;
    //是否占据
    bool _isOccupy;

    std::mutex _mtx;

public:
    DataBaseConnection();
    virtual ~DataBaseConnection();

    typedef std::shared_ptr<DataBaseConnection> Ptr;
    /**
     * 占用
     * @param flag true-修改最近占据时间，false-不修改
     */
    void occupy(bool flag = true);

    //回收
    void recycle();

    //是否占据
    bool isOccupied();

    //设置最近查询时间
    void setQueryTime();

    //获取最近时间
    void getlastTime(time_t& occupyTime,time_t& queryTime);
};