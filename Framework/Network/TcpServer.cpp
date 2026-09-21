#include "TcpServer.h"
#include "Base/Log.h"
#include "Network/Connection.h"
#include "Session.h"

#include "asio/registered_buffer.hpp"
#include "boost/asio/executor_work_guard.hpp"
#include "system/detail/error_code.hpp"
#include "uuid/random_generator.hpp"
#include "winapi/error_codes.hpp"
#include <boost/asio.hpp>
#include <boost/asio/require.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <exception>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

class Service {
private:
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
};