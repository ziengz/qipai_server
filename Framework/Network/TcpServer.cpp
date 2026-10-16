#include "TcpServer.h"
#include "Base/Log.h"
#include "Network/Connection.h"
#include "SecurityManager.h"
#include "Session.h"
#include "Timer/TimerManager.h"

#include "Base/BaseUtils.h"
#include "asio/write.hpp"
#include "boost/asio/executor_work_guard.hpp"
#include "system/detail/error_code.hpp"
#include "uuid/random_generator.hpp"
#include <boost/asio.hpp>
#include <boost/asio/require.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstddef>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

class Service {
protected:
    boost::asio::io_context _context;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
        _work;

public:
    Service() : _work(boost::asio::make_work_guard(_context)) {
        // _work = boost::asio::require(
        //     _context.get_executor(),
        //     boost::asio::execution::outstanding_work.tracked);
    }
    virtual ~Service() {}

public:
    void run() {
        _context.run();
        InfoS << "Service is terminate.";
    }
    void stop() {
        _work.reset();

        // 没有调用 _context.stop()，因为 stop() 会暴力中断所有未完成的异步操作
        // _work = boost::asio::any_io_executor();
    }

    boost::asio::io_context &getContext() { return _context; }
};

class ConnectionImpl : public Connection {
private:
    std::weak_ptr<TcpServer> _server;     // 服务器（弱引用，避免循环引用）
    boost::asio::ip::tcp::socket _socket; // TCP socket
    std::string _uuid;                    // 连接唯一id
    std::string _remoteIp;                // 远端IP
    Session::Ptr _session;                // 对应的业务会话
    bool _sending;                        // 是否正在发送
    std::atomic<bool> _error;             // 是否出错
    std::atomic<bool> _activeClose;       // 是否主动关闭
    time_t _activeCloseTime;              // 主动关闭时间
    std::list<std::shared_ptr<std::string>> _sendQueue; // 发送队列
    std::mutex _mtxSend;                                // 发送队列锁
    char _data[1024];                                   // 接收缓冲区
    static const std::size_t BLOCK_SIZE;                // 分块大小16KB

public:
    ConnectionImpl(const std::shared_ptr<TcpServer> &srv,
                   std::shared_ptr<Service> &service)
        : _server(srv), _socket(service->getContext()), _error(false),
          _activeClose(false), _sending(false) {
        boost::uuids::uuid uuid = boost::uuids::random_generator()();
        _uuid = boost::uuids::to_string(uuid);
    }
    void start() {
        std::shared_ptr<TcpServer> srv = _server.lock();
        if (srv) {
            // 创建session

            do_read();
        }
    }

    void setRemoteIp(const std::string &remoteIp) { _remoteIp = remoteIp; }

    // 主动关闭连接
    void close() {
        _activeClose = true;
        _activeCloseTime = BaseUtils::getCurrentSecond();
        try {
            _socket.close();
        } catch (std::exception &ex) {
            ErrorS << "Close socket error: " << ex.what();
        }
    }

    bool isActiveClose() { return _activeClose; }

    // 主动关闭是否超过1秒
    bool activeCloseOver1s(const time_t &nowTime) {
        if (!_activeClose) {
            return false;
        }
        time_t delta = nowTime - _activeCloseTime;
        if (delta > 1LL) {
            return true;
        }
        return false;
    }

    friend class Acceptor;

private:
    void do_read() {
        std::weak_ptr<ConnectionImpl> weakSelf =
            std::dynamic_pointer_cast<ConnectionImpl>(shared_from_this());
        _socket.async_read_some(
            boost::asio::buffer(_data, 1024),
            [weakSelf](boost::system::error_code &ec, std::size_t length) {
                std::shared_ptr<ConnectionImpl> self = weakSelf.lock();
                if (self) {
                    self->OnAsyncRead(ec, length);
                }
            });
    }
    void OnAsyncRead(boost::system::error_code &ec, std::size_t length) {
        if (!ec) {
            try {
                if (_session) {
                    _session->onReceive(_data, length);
                }
            } catch (std::exception &ex) {
                ErrorS << "Receive message error: " << ex.what()
                       << ",length: " << length;
            }
            // 继续下一批读取
            do_read();
        } else {
            // 读出错。连接断开
            OnError(ec, true);
        }
    }

    void OnError(boost::system::error_code &ec, bool readOrWrite) {
        _error = true;

        if (_session)
            _session->onDisconnect();

        InfoS << "Session(id: " << _uuid << ") error,msg: " << ec.message();
    }

    bool addSendNode(const std::shared_ptr<std::string> &node) {
        std::lock_guard<std::mutex> lck(_mtxSend);
        _sendQueue.push_back(node);
        return _sending;
    }

    std::shared_ptr<std::string> popSendNode() {
        std::lock_guard<std::mutex> lck(_mtxSend);
        std::shared_ptr<std::string> node;
        if (_sendQueue.empty()) {
            _sending = false;
        } else {
            _sending = true;
            node = _sendQueue.front();
            _sendQueue.pop_front();
        }

        return node;
    }

    boost::asio::ip::tcp::socket &getSocket() { return _socket; }

    void do_write() {
        std::shared_ptr<std::string> node = popSendNode();
        if (!node)
            return;
        std::weak_ptr<ConnectionImpl> weakSelf =
            std::dynamic_pointer_cast<ConnectionImpl>(shared_from_this());
        boost::asio::async_write(
            _socket, boost::asio::buffer(node->data(), node->size()),
            [weakSelf](boost::system::error_code &ec, std::size_t length) {
                std::shared_ptr<ConnectionImpl> self = weakSelf.lock();
                if (self) {
                    self->OnAsyncWrite(ec, length);
                }
            });
    }
    void OnAsyncWrite(boost::system::error_code &ec, std::size_t length) {
        if (!ec) {
            do_write();
        } else {
            // 连接错误
            OnError(ec, false);
        }
    }

    void splitSend(const char *buf, std::size_t length) {
        // 分片发送，一次发送最多16KB,以免发送缓冲区溢出
        bool flag = false;
        std::size_t total = 0; // 发送总长度
        std::size_t size = 0;  // 此次发送的长度
        std::shared_ptr<std::string> node;
        while (total < length) {
            if (size + total > length)
                size = length - total;

            node = std::make_shared<std::string>((&buf[total]), size);
            flag = addSendNode(node);
            if (!flag) {
                // 说明没数据了，再跑一次
                do_write();
            }

            total += size;
        }
    }

public:
    virtual void getId(std::string &id) const override { id = _uuid; }

    virtual void getRemoteIp(std::string &remoteIp) const override {
        remoteIp = _remoteIp;
    }
    virtual void send(const char *buf, std::size_t length) {
        if (_error)
            return;
        if (buf == nullptr || length == 0)
            return;
        splitSend(buf, length);
    }

    virtual void Send(const std::shared_ptr<std::string> &data) override {
        if (_error)
            return;
        if (data->empty() || !data)
            return;
        if (data->size() <= BLOCK_SIZE) {
            bool flag = addSendNode(data);
            if (flag)
                do_write();
            return;
        }
        splitSend(data->data(), data->size());
    }

    bool isClose() override {
        if (_error)
            return true;
        if (_activeClose)
            return true;
        return false;
    }

    const Session::Ptr getSession() const { return _session; }
};

const std::size_t ConnectionImpl::BLOCK_SIZE = 16384;

class Acceptor : public Service {
private:
    // 监听器
    std::shared_ptr<boost::asio::ip::tcp::acceptor> _acceptor;
    // 服务吃所有者
    std::weak_ptr<TcpServer> _server;

public:
    Acceptor(std::shared_ptr<TcpServer> &srv,
             boost::asio::ip::tcp::endpoint endpoint)
        : _server(srv) {
        _acceptor = std::make_shared<boost::asio::ip::tcp::acceptor>(_context,
                                                                     endpoint);
        do_accept();
    }
    virtual ~Acceptor() {}

    void stop() {
        _acceptor->close();
        Service::stop();
    }

    void do_accept() {
        std::shared_ptr<TcpServer> strongServer = _server.lock();
        if (!strongServer)
            return;
        std::shared_ptr<Service> service = strongServer->getService();
        if (!service)
            return;

        std::shared_ptr<ConnectionImpl> connection =
            std::make_shared<ConnectionImpl>(_server, service);

        _acceptor->async_accept(
            connection->getSocket(),
            [this, connection](boost::system::error_code ec) {
                onAccept(ec, connection);
                if (_acceptor->is_open()) {
                    do_accept(); // 继续监听下一个连接
                }
            });
    }

    void onAccept(boost::system::error_code &ec,
                  const std::shared_ptr<ConnectionImpl> &con) {
        if (!ec) {
            std::string remoteIp;
            std::string addr;
            try {
                boost::asio::ip::tcp::socket &socket = con->getSocket();
                boost::asio::ip::tcp::endpoint endpoint =
                    socket.remote_endpoint();
                remoteIp = endpoint.address().to_string();
                unsigned short port = endpoint.port();
                addr = remoteIp + " " + std::to_string(port);
            } catch (std::exception &) {
                addr = "UnKnown";
            }
            // 判断远端IP是否在黑名单中
            if (SecurityManager::getSingleton().checkBlackList(remoteIp)) {
                con->close();
                return;
            }
            con->setRemoteIp(remoteIp);
            con->start();

            std::shared_ptr<TcpServer> strongServer = _server.lock();
            if (strongServer)
                strongServer->addConnection(con);

            // 打印日志
            std::string id;
            con->getId(id);
            DebugS << "New Connection id : " << id
                   << "remote address : " << addr;
        }
    }
};

TcpServer::TcpServer(const SessionCreater::Ptr &creator)
    : _creator(creator)

{}

TcpServer::~TcpServer() {}

void TcpServer::start(unsigned short port, unsigned int threadNum) {
    if (_started)
        return;

    std::shared_ptr<Service> service;
    std::shared_ptr<std::thread> thread;
    unsigned int cpuNum = std::thread::hardware_concurrency();
    if (threadNum < 1) {
        threadNum = cpuNum;
    }
    if (threadNum > 8) {
        threadNum = 8;
    }
    // 使用单个service多个线程，所有连接进来的socket都绑定到同一个service，而不是每个线程
    // 唯一对应一个service（即一个socket绑定到某个线程，固定由该线程做数据收发），这样做的
    // 好处是当某个socket的数据收发比较繁忙的时候，其他socket可以任意切换到其他线程来做数
    // 据收发，避免网络阻塞
    service = std::make_shared<Service>();
    _services.emplace_back(service);
    for (int i = 0; i < threadNum; i++) {
        thread = std::make_shared<std::thread>([service] { service->run(); });
        _threads.emplace_back(thread);
    }
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);
    std::shared_ptr<TcpServer> self = shared_from_this();
    _acceptor = std::make_shared<Acceptor>(self, endpoint);
    std::shared_ptr<Acceptor> acceptor = _acceptor;
    // 单独开一个线程跑接受链接，否则多线程 accept 反而会因为竞争 accept
    // 锁而浪费 CPU
    thread = std::make_shared<std::thread>([acceptor] { acceptor->run(); });

    // 开启定时任务
    std::weak_ptr<TcpServer> weakSelf = shared_from_this();
    TimerManager::getSingleton().addAsyncTimer(100, [weakSelf] {
        std::shared_ptr<TcpServer> strongSelf = weakSelf.lock();
        if (strongSelf) {
            return strongSelf->OnTimer();
        }
        return true;
    });

    InfoS << "TcpServer startup,port : " << port
          << ", thread num : " << threadNum;
    _started = true;
}

void TcpServer::stop() {
    if (!_started || _stopping)
        return;
    _stopping = true;

    if (_acceptor) {
        _acceptor->stop();
    }
    // 关闭所有链接
    closeAllConnections();
    // 关闭会话服务
    for (std::shared_ptr<Service> service : _services) {
        service->stop();
    }
    // 关闭所有线程
    for (std::shared_ptr<std::thread> thread : _threads) {
        thread->join();
    }

    _services.clear();
    _acceptor.reset();
    _threads.clear();
    clearConnection();

    _started = false;
    _stopping = false;
}

bool TcpServer::isStopping() { return _stopping; }

std::shared_ptr<Service> TcpServer::getService() {
    if (_services.empty())
        return std::shared_ptr<Service>();
    if (_pos > _services.size())
        _pos = 0;
    std::shared_ptr<Service> ret = _services.at(_pos);
    _pos++;
    return ret;
}

Session::Ptr TcpServer::newSession(const std::shared_ptr<Connection> &con) {
    return _creator->create(con);
}

void TcpServer::addConnection(const std::shared_ptr<ConnectionImpl> &con) {
    if (!con)
        return;
    std::string id;
    con->getId(id);

    std::lock_guard<std::mutex> lck(_mtxCon);
    ConnectionMap::iterator it = _connections.find(id);
    if (it == _connections.end()) {
        _connections.insert(std::make_pair(id, con));
        _sequence.push_back(id);
    } else {
        ErrorS << "Add new session with id : " << id
               << "error, session with the same id is already existed.";
    }
}

void TcpServer::removeConnection(const std::shared_ptr<ConnectionImpl> &con) {
    std::string id;
    con->getId(id);

    std::lock_guard<std::mutex> lck(_mtxCon);
    ConnectionMap::iterator it = _connections.find(id);
    if (it != _connections.end()) {
        _connections.erase(it);
    }
}

void TcpServer::closeAllConnections() {
    std::lock_guard<std::mutex> lck(_mtxCon);

    ConnectionMap::const_iterator it = _connections.begin();
    while (it != _connections.end()) {
        const std::shared_ptr<ConnectionImpl> &conn = it->second;
        conn->close();
        ++it;
    }
}

void TcpServer::clearConnection() {
    std::lock_guard<std::mutex> lck(_mtxCon);
    _connections.clear();
}

int TcpServer::getConnectionNums() {
    std::lock_guard<std::mutex> lxk(_mtxCon);
    int size = static_cast<int>(_connections.size());
    return size;
}

std::shared_ptr<ConnectionImpl> TcpServer::getConnectionInSequence() {
    std::lock_guard<std::mutex> lck(_mtxCon);
    return BaseUtils::getInSequence(_connections, _sequence);
}

void TcpServer::checkConnection() {
    // 每100毫秒执行一次，要在10秒内轮询完一遍所有链接，就算每一次需要遍历多少连接
    int count = getConnectionNums();
    if (count == 0)
        return;
    count = count / 100;
    std::string id;
    time_t nowTime = BaseUtils::getCurrentSecond();
    for (int i = 0; i < count; i++) {
        std::shared_ptr<ConnectionImpl> con = getConnectionInSequence();
        if (!con)
            break;
        Session::Ptr session = con->getSession();
        if (!session)
            removeConnection(con);
        else if (!session->isAlive(nowTime)) { // session 不活跃
            con->getId(id);
            if (con->activeCloseOver1s(nowTime)) {
                // 已经主动关闭超过1秒，连接还存在，正常不应该出现这种情况
                ErrorS << "Connection (uuid: " << id
                       << ")has been active close over on second ";
                session->onDisconnect();
                removeConnection(con);
            } else if (!(con->isActiveClose())) {
                // 不是主动断开的，session不活跃（可能原因：连接关闭/心跳超时）
                InfoS << "Active close connection (uuid" << id
                      << "),because it's inactive.";
                con->close();
            }
        }
    }
}

bool TcpServer::OnTimer() {
    if (!_started || _stopping)
        return false;
    checkConnection();
    return true;
}