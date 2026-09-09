#include "RabbitmqClient.h"
#include "Base/BaseUtils.h"
#include "Base/Log.h"
#include "RabbitmqConfig.h"
#include "RabbitmqConnection.h"
#include "RabbitmqConsumer.h"
#include "RabbitmqUtils.h"
#include "json/value.h"
#include <cerrno>
#include <functional>
#include <memory>
#include <mutex>
#include <rabbitmq-c/framing.h>
#include <rabbitmq-c/tcp_socket.h>
#include <string>
#include <thread>

typedef std::function<void(bool)> ConnectionListener;

class RabbitmqConnectionImpl : public RabbitmqConnection {
  public:
    // 服务器主机地址
    std::string _host;

    // 端口
    int _port;

    // 虚拟主机
    std::string _vhost;

    // 用户名
    std::string _userName;

    // 密码
    std::string _password;

    // 连接监听函数
    ConnectionListener _listener;

    // 连接内部结构体
    amqp_connection_state_t _state;

  private:
    // 信号量
    std::mutex _mtxId;

    // 通道id分配器，只能递增
    int _idAllocator;

  public:
    RabbitmqConnectionImpl(const std::string &host, const int port,
                           const std::string &vhost,
                           const std::string &userName,
                           const std::string &password,
                           const ConnectionListener &listener)
        : _host(host), _port(port), _userName(userName), _password(password),
          _listener(listener), _state(nullptr), _idAllocator(1) {}

    virtual ~RabbitmqConnectionImpl() {}

    virtual void *getState() override { return static_cast<void *>(_state); }

    virtual void setOk(bool setting) override {
        RabbitmqConnection::setOk(setting);

        if (_listener)
            _listener(setting);
    }

    void setState(amqp_connection_state_t state) { _state = state; }

    int allocateId() {
        std::lock_guard<std::mutex> lck(_mtxId);
        _idAllocator++;
        return _idAllocator;
    }

    void resetId() {
        std::lock_guard<std::mutex> lck(_mtxId);
        _idAllocator = 1;
    }

    void destory() {
        // 只有发生错误的时候_state 不为空
        if (_state != nullptr) {
            amqp_rpc_reply_t reply =
                amqp_connection_close(_state, AMQP_REPLY_SUCCESS);
            RabbitmqUtils::checkReplyError(reply, "Closing connection");
            amqp_destroy_connection(_state);
            _state = nullptr;
        }
    }
};

class PublishItem : public std::enable_shared_from_this<PublishItem> {
  public:
    std::string _exchange;
    std::string _routingkey;
    std::string _message;

  public:
    PublishItem(const std::string &exchange, const std::string &routingKey,
                const std::string &message)
        : _exchange(exchange), _routingkey(routingKey), _message(message) {}

    virtual ~PublishItem() = default;
    typedef std::shared_ptr<PublishItem> Ptr;
};

template <> RabbitmqClient *Singleton<RabbitmqClient>::_inst = nullptr;
bool RabbitmqClient::Start(const std::string &host, const int port,
                           const std::string &vhost,
                           const std::string &hostName,
                           const std::string &password,
                           const RabbitmqConfig::Ptr &config) {

    if (_connection) {
        ErrorS << "Client has alread start";
        return false;
    }

    _connection = std::make_shared<RabbitmqConnectionImpl>(
        host, port, vhost, hostName, password,
        [](bool isOk) { RabbitmqClient::getSingleton().onConnection(isOk); });
    _config = config;
    if (!checkConnection()) {
        _connection.reset();
        return false;
    }
    _thread = std::make_shared<std::thread>(
        []() { RabbitmqClient::getSingleton().threadFunc(); });

    return true;
}

void RabbitmqClient::Stop(const RabbitmqConfig::Ptr &config) {
    if (!_connection)
        return;

    _stopFlag = true;
    if (_thread) {
        _thread->join();
        _thread.reset();
    }
    if (_connection->isOk()) {
        if (config) {
            int id = getPublishChannel();
            if (id != 0) {
                closeChannel(id);
                setPublishChannel(0);
            }
        }
    }
    _connection.reset();
}

bool RabbitmqClient::checkConnection() {
    if (_connection->isOk()) {
        return true;
    }
    amqp_connection_state_t state = amqp_new_connection();
    amqp_socket_t *socket = amqp_tcp_socket_new(state);
    if (socket == nullptr) {
        ErrorS << "Create Tcp socket failed";
        amqp_destroy_connection(state);
        return false;
    }
    std::shared_ptr<RabbitmqConnectionImpl> _impl =
        std::dynamic_pointer_cast<RabbitmqConnectionImpl>(_connection);
    int status = amqp_socket_open(socket, _impl->_host.c_str(), _impl->_port);
    if (status != 0) {
        ErrorS << "Opening TCP socket failed,status = " << state;
        amqp_destroy_connection(state);
        return false;
    }

    amqp_rpc_reply_t reply = amqp_login(
        state, _impl->_vhost.c_str(), 0, 131072, 3, AMQP_SASL_METHOD_PLAIN,
        _impl->_userName.c_str(), _impl->_password.c_str());

    int ret = RabbitmqUtils::checkReplyError(reply, "Logging in");
    if (ret != 0) {
        amqp_destroy_connection(state);
        return false;
    }
    _impl->setState(state);
    _impl.reset();
    _impl->setOk(true);

    if (_config) {
        _config->config(_connection);
    }
    InfoS << "Connection to RabbitMQ server (" << _impl->_host << ":"
          << _impl->_port << "), virtual host: " << _impl->_vhost;
    return true;
}

void RabbitmqClient::onConnection(bool isOk) {
    if (_stopFlag)
        return;
    if (!isOk) {
        setPublishChannel(0);
    }
}

int RabbitmqClient::openChannel() {
    if (!(_connection->isOk()))
        return 0;
    std::shared_ptr<RabbitmqConnectionImpl> impl =
        std::dynamic_pointer_cast<RabbitmqConnectionImpl>(_connection);

    int id = impl->allocateId();
    amqp_channel_open(impl->_state, id);

    amqp_rpc_reply_t reply = amqp_get_rpc_reply(impl->_state);
    int ret = RabbitmqUtils::checkReplyError(reply, "Opening channel");

    if (ret) {
        return 0;
    }
    return id;
}

void RabbitmqClient::closeChannel(int channelId) {
    std::shared_ptr<RabbitmqConnectionImpl> impl =
        std::dynamic_pointer_cast<RabbitmqConnectionImpl>(_connection);

    amqp_rpc_reply_t reply =
        amqp_channel_close(impl->_state, static_cast<amqp_channel_t>(channelId),
                           AMQP_REPLY_SUCCESS);
    RabbitmqUtils::checkReplyError(reply, "Closing Channel");
}

bool RabbitmqClient::isConnectionOk() const {
    if (!_connection)
        return false;
    return _connection->isOk();
}

bool RabbitmqClient::publish(const std::string &exchange,
                             const std::string &routingKey,
                             const std::string &message) {
    if (_connection || !(_connection->isOk())) {
        LOG_ERROR("Publish message error,connection is not ready");
        return false;
    }
    if (exchange.empty() || message.empty()) {
        return false;
    }
    PublishItem::Ptr item =
        std::make_shared<PublishItem>(exchange, routingKey, message);

    std::lock_guard<std::mutex> lck(_mtx);

    if (_publishItem.size() > 2999) {
        // 最多缓存3000条
        LOG_ERROR("Cache message quantity maximum(3000),the head of publish "
                  "queue will be discarded,this will cause message lost");
        while (_publishItem.size() > 2999) {
            _publishItem.pop();
        }
    }

    _publishItem.push(item);
    return true;
}

bool RabbitmqClient::publishJson(const std::string &exchange,
                                 const std::string &routingKey,
                                 const std::string &msgType,
                                 const std::string &json) {
    std::string base64;
    if (!BaseUtils::encodeBase64(base64, json.c_str(),
                                 static_cast<int>(json.size()))) {
        LOG_ERROR("Encode base64 failed");
        return false;
    }
    Json::Value msg(Json::objectValue);
    msg["msgType"] = msgType;
    msg["msgPack"] = base64;
    std::string message = msg.toStyledString();

    return publish(exchange, routingKey, message);
}

bool RabbitmqClient::publishInner() {
    int id = checkPublish();
    if (id == 0)
        return false;
    PublishItem::Ptr item;
    amqp_rpc_reply_t reply;
    while (true) {
        item = popPublishItem();
        if (!item)
            break;
        amqp_connection_state_t state =
            reinterpret_cast<amqp_connection_state_t>(_connection->getState());

        int ret = amqp_basic_publish(
            state, static_cast<amqp_channel_t>(id),
            amqp_cstring_bytes(item->_exchange.c_str()),
            amqp_cstring_bytes(item->_routingkey.c_str()), 0, 0, nullptr,
            amqp_cstring_bytes(item->_message.c_str()));
        if (ret < 0) {
            ErrorS << "Publish failed, error: " << ret;
            reply = amqp_get_rpc_reply(state);
            ret = RabbitmqUtils::checkReplyError(reply, "Published");
            if (ret == 1) {
                _connection->setOk(false);
                return true;
            } else if (ret == 2) {
                closeChannel(id);
                setPublishChannel(0);
                break;
            }
        }
    }
    return false;
}

int RabbitmqClient::checkPublish() {
    int id = getPublishChannel();
    if (id != 0) {
        return id;
    }
    id = openChannel();
    setPublishChannel(id);
    return id;
}

std::shared_ptr<PublishItem> RabbitmqClient::popPublishItem() {
    std::lock_guard<std::mutex> lck(_mtx);
    if (_publishItem.empty())
        return nullptr;
    PublishItem::Ptr item = _publishItem.front();
    _publishItem.pop();
    return item;
}

int RabbitmqClient::getPublishChannel() {
    std::lock_guard<std::mutex> lck(_mtx);
    return _publishId;
}

void RabbitmqClient::setPublishChannel(int id) {
    std::lock_guard<std::mutex> lck(_mtx);
    _publishId = id;
}

// 客户端库状态错乱
static int consumeLibraryException(amqp_connection_state_t state,
                                   const amqp_rpc_reply_t &reply) {
    amqp_frame_t frame;
    // 检查是否是意料之外的协议状态
    if (reply.library_error == AMQP_STATUS_UNEXPECTED_STATE) {
        // 从socket强行抓取下一帧数据
        if (AMQP_STATUS_OK != amqp_simple_wait_frame(state, &frame)) {
            return 0;
        }
        if (frame.frame_type == AMQP_FRAME_METHOD) {
            switch (frame.payload.method.id) {
            case AMQP_BASIC_ACK_METHOD:
                // 这是发布者确认模式下的 ACK，忽略它
                break;
            case AMQP_BASIC_RETURN_METHOD: {
                // 消息无法路由且设置了 mandatory
                // 标志，服务端把消息退回来了，这条消息需要去读
                amqp_message_t message;
                amqp_rpc_reply_t tmp =
                    amqp_read_message(state, frame.channel, &message, 0);
                if (AMQP_RESPONSE_NORMAL != tmp.reply_type) {
                    return 0;
                }
                amqp_destroy_message(&message); // 读取后直接销毁（不处理）
            } break;
            case AMQP_CHANNEL_CLOSE_METHOD:
                LOG_ERROR("channel closed.");
                return 2;
            case AMQP_CONNECTION_CLOSE_METHOD:
                LOG_ERROR("Connection close.");
                return 1;
            case AMQP_CHANNEL_OPEN_OK_METHOD:
                return 0; // 这是通道开启成功的回复，忽略
            default: {
                char tmp[16] = {'\0'};
                snprintf(tmp, sizeof(tmp), "%08X", frame.payload.method.id);
                ErrorS << "An unexception method was receive" << tmp;
            } break;
            }
        }
    } else if (reply.library_error == AMQP_STATUS_CONNECTION_CLOSED) {
        if (reply.library_error == AMQP_STATUS_SOCKET_CLOSED ||
            reply.library_error == AMQP_STATUS_SOCKET_ERROR) {
            LOG_ERROR("Connection close.");
            return 1;
        }
    }

    return 0;
}

static int consumeServerException(amqp_connection_state_t state,
                                  const amqp_rpc_reply_t &reply) {
    if (reply.reply.id == AMQP_CONNECTION_CLOSE_METHOD) {
        // 连接关闭
        LOG_ERROR("connection close.");
        return 1;
    } else if (reply.reply.id == AMQP_CHANNEL_CLOSE_METHOD) {
        LOG_ERROR("channel closed.");
        return 2;
    }
    return 0;
}

void RabbitmqClient::threadFunc() {
    amqp_rpc_reply_t reply;
    amqp_envelope_t envelope;

    std::shared_ptr<RabbitmqConnectionImpl> impl =
        std::dynamic_pointer_cast<RabbitmqConnectionImpl>(_connection);
    struct timeval tv = {0, 10000};
    while (!_stopFlag) {
        if (!(_connection->isOk())) {
        }

        reply = amqp_consume_message(impl->_state, &envelope, &tv, 0);
        if (reply.reply_type == AMQP_RESPONSE_NORMAL) {
            // 发送ACK
            std::shared_ptr<std::string> message =
                std::make_shared<std::string>(
                    (const char *)envelope.message.body.bytes,
                    envelope.message.body.len);
            std::string tag((const char *)envelope.consumer_tag.bytes,
                            envelope.consumer_tag.len);
            amqp_basic_ack(impl->_state, envelope.channel,
                           envelope.delivery_tag, 0);
            RabbitmqConsumer::getSingleton().consume(message, tag);
            amqp_destroy_envelope(&envelope);
        }
        // 客户端库状态错乱
        else if (reply.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION) {
            int ret = consumeLibraryException(impl->_state,reply);
            if(ret == 1){
                _connection->setOk(false);
            }
            ErrorS << "Consume message error: " << amqp_error_string2(reply.library_error);

        }
        // 服务端主动丢过来的异常，可能是心跳超时、权限变更、或管理员手动踢下线
        else if (reply.reply_type == AMQP_RESPONSE_SERVER_EXCEPTION) {
            int ret = consumeServerException(impl->_state, reply);
            if(ret != 0){
                _connection->setOk(false);
            }

        }
    }
}