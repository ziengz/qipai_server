#pragma once

#include "Base/Singleton.h"
#include "Session.h"
#include <atomic>
#include <list>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>

// io_context的封装
class Service;
class ConnectionImpl;
class Acceptor;
class TcpServer : public std::enable_shared_from_this<TcpServer> {
public:
    TcpServer(const SessionCreater::Ptr &creator);
    virtual ~TcpServer();

    friend class Singleton<TcpServer>;
    friend class ConnectionImpl;
    friend class Acceptor;

public:
    /**
     * @brief 启动服务器
     *
     * @param port 端口号
     * @param threadNum 接收数据线程数量（最大不超过8个）
     */
    void start(unsigned short port, unsigned int threadNum = 0);

    // 关闭
    void stop();

    // 是否正在关闭
    bool isStopping();

private:
    // 循环获取服务
    std::shared_ptr<Service> getService();

    // 创建会话
    Session::Ptr newSession(const std::shared_ptr<Connection> &con);

    // 断开所有链接
    void closeAllConnections();

    // 清空链接表
    void clearConnection();

    // 定时检查链接
    void checkConnection();

    void removeConnection(const std::shared_ptr<ConnectionImpl> &con);

    // 按顺序从连接表中取出连接
    std::shared_ptr<ConnectionImpl> getConnectionInSequence();

    // 获取当前连接数量
    int getConnectionNums();

    // 添加新的连接
    void addConnection(const std::shared_ptr<ConnectionImpl> &conn);

    // 定时任务
    bool OnTimer();

private:
    // 会话创建者
    SessionCreater::Ptr _creator;
    // 启动标志
    std::atomic<bool> _started;
    // 正在关闭
    std::atomic<bool> _stopping;

    // 线程数组
    std::vector<std::shared_ptr<std::thread>> _threads;
    // 服务列表
    std::vector<std::shared_ptr<Service>> _services;

    // 连接表关键字顺序序列，用于顺序从连接表中取出链接
    std::list<std::string> _sequence;

    std::shared_ptr<Acceptor> _acceptor;

    typedef std::unordered_map<std::string, std::shared_ptr<ConnectionImpl>>
        ConnectionMap;
    ConnectionMap _connections;
    // 获取service服务的索引
    int _pos;
    // 连接表信号量
    std::mutex _mtxCon;
};