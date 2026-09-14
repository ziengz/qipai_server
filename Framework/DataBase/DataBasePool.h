#include "DataBaseConnection.h"
#include "Thread/ThreadBlocker.h"


#include <atomic>
#include <list>
#include <queue>

class DataBasePool {
  private:
    // 保留的连接数量
    const int _keepConnections;
    // 最大连接数
    const int _maxConnections;

    // mysql链接列表
    std::list<DataBaseConnection::Ptr> _connections;
    // 阻塞队列
    std::queue<ThreadBlocker::Ptr> _blockQueue;

    std::mutex _mtx;

  protected:
    // 停止标志
    std::atomic_bool _stopFlag;

  public:
    DataBasePool(int keepConnections, int maxConnections);
    virtual ~DataBasePool() = default;

    /**
     * 删除超时没有被删除的链接，仅保留设定的连接数
     * 定时发起测试查询，以免连接长时间不活动被数据库服务器关闭
     */
    bool onTimer();

    // 设置停止标志
    void stop();

  protected:
    /**
     * 请求数据库连接（当连接超出最大数量时阻塞调用线程）
     * @return 数据库连接
     */
    DataBaseConnection::Ptr getConnection();

    DataBaseConnection::Ptr onTimerImpl();

    virtual void onTimerImpl(const DataBaseConnection::Ptr &con) = 0;

    /**
     * 创建数据库连接实例
     * @return 数据库连接实例
     */
    virtual DataBaseConnection::Ptr createConnection() = 0;

    /**
     * 删除数据库连接
     * @param con 数据库连接
     */
    void removeConnection(const DataBaseConnection::Ptr &con);

    /**
     * 唤醒则色队列中的第一个线程
     */
    void notifyOne();

    // 清空链接
    void clearConnection();

  private:
    /**
     * 请求数据库连接
     * @param blocker 连接数量超出最大限制时，返回阻塞器，用于阻塞当前调用线程
     * @return 数据库连接
     */
    DataBaseConnection::Ptr getConnection(ThreadBlocker::Ptr &blocker);
};
