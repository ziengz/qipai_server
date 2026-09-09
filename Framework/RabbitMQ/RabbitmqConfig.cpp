#include "RabbitmqConfig.h"
#include "RabbitmqConnection.h"
#include <memory>
#include "RabbitmqUtils.h"

class ConfigItem{
public:
    ConfigItem(bool redo):_redo(redo){
        
    };
    virtual ~ConfigItem();
    typedef std::shared_ptr<ConfigItem> Ptr;

public:
    /**
     * @brief 执行配置
     * 
     * @param conn 与Rabbitmq连接
     * @param channel 通道id,若执行配置出错导致通道关闭，则置0
     */
    virtual bool config(const RabbitmqConnection::Ptr& conn,int& channel) = 0;

    // 是否用独占的通道进行配置
    // 有些配置，例如订阅需要用独占的通道进行配置，避免在完成配置之后，通道后面用于其他调用发生错误导致通道关闭，进而导致配置失效
    virtual bool exclusiveChannel() {return false;}

    bool redo(){
        return _redo;
    }

private:
    bool _redo;

};

#define CHECK_REPLY_ERROR(reply,context) \
    int ret = RabbitmqUtils::checkReplyError(reply,context); \
    if(ret == 0)  \
        return true;  \
    if(ret == 1){  \
        conn->setOk(false);  \
        channel = 0;  \
    }  \
    else if(ret == 2)  \
        channel = 0;  \
    return false;

class ExchangeDeclare : public ConfigItem{
public:
    ExchangeDeclare(const std::string& exchange,std::string& type,bool durable,bool autoDelete,bool internal,bool redo):
        ConfigItem(redo),
        _exchange(exchange),
        _durable(durable),
        _type(type),
        _autoDelete(autoDelete),
        _internal(internal)
    {

    }
    virtual ~ExchangeDeclare(){}

    virtual bool config(const RabbitmqConnection::Ptr& conn,int& channel) override{
        if(!conn || !(conn->isOk()) || channel == 0)
            return false;
        amqp_connection_state_t state = reinterpret_cast<amqp_connection_state_t>(conn->getState());
        if(state == nullptr)
            return false;
        const amqp_table_t argument_table = {0,nullptr};
        amqp_exchange_declare(state, static_cast<amqp_channel_t>(channel), 
            amqp_cstring_bytes(_exchange.c_str()), amqp_cstring_bytes(_type.c_str()),
            0, _durable ? 1 : 0, _autoDelete ? 1 : 0, _internal ? 1 : 0, argument_table);
        amqp_rpc_reply_t reply = amqp_get_rpc_reply(state);
        CHECK_REPLY_ERROR(reply, "Declaring exchange");
    }
    
private:
    std::string _exchange;
    std::string _type;
    bool _durable;
    bool _autoDelete;
    bool _internal;
};

void RabbitmqConfig::exchangeDeclare(const std::string& exchange,std::string& type,bool durable,bool autoDelete,bool internal,bool redo){
    ConfigItem::Ptr item = std::make_shared<ExchangeDeclare>(exchange,type,durable,autoDelete,internal,redo);
    _items.push_back(item);
}

class ExchangeBind: public ConfigItem{
public:
    ExchangeBind(const std::string& destination,const std::string& source,const std::string&routingKey,bool redo = false)
        : _destination(destination)
        , _source(source)
        , _routingKey(routingKey)
        , ConfigItem(redo)
    {

    }
    virtual ~ExchangeBind(){}
    virtual bool config(const RabbitmqConnection::Ptr& conn,int& channel) override{
        if(!conn || !(conn->isOk()) || channel == 0)
            return false;

        amqp_connection_state_t state = reinterpret_cast<amqp_connection_state_t>(conn->getState());
        if(state == nullptr)
            return false;

        const amqp_table_t arguments_table = {0,nullptr};
        amqp_exchange_bind(state, static_cast<amqp_channel_t>(channel),
            amqp_cstring_bytes(_destination.c_str()), amqp_cstring_bytes(_source.c_str()),
            amqp_cstring_bytes(_routingKey.c_str()), arguments_table);
        
        amqp_rpc_reply_t reply = amqp_get_rpc_reply(state);
        CHECK_REPLY_ERROR(reply, "Binding exchange");
    }


private:
    std::string _destination;
    std::string _source;
    std::string _routingKey;
};

void RabbitmqConfig::exchangeBind(const std::string& destination,const std::string& source,const std::string&routingKey,bool redo){
    ConfigItem::Ptr item = std::make_shared<ExchangeBind>(destination,source,routingKey,redo);
    _items.push_back(item);
}

class ExchangeUnbind: public ConfigItem{
public:
    ExchangeUnbind(const std::string& destination,const std::string& source,const std::string&routingKey,bool redo = false)
        : _destination(destination)
        , _source(source)
        , _routingKey(routingKey)
        , ConfigItem(redo)
    {

    }

    virtual ~ExchangeUnbind(){}

    virtual bool config(const RabbitmqConnection::Ptr& conn,int& channel) override{
        if(!conn || !(conn->isOk()) || channel == 0)
            return false;
        amqp_connection_state_t state = reinterpret_cast<amqp_connection_state_t>(conn->getState());
        if(state == nullptr)
            return false;

        const amqp_table_t arguments_table = {0,nullptr};
        amqp_exchange_unbind(state, static_cast<amqp_channel_t>(channel),
            amqp_cstring_bytes(_destination.c_str()), amqp_cstring_bytes(_source.c_str()),
            amqp_cstring_bytes(_routingKey.c_str()), arguments_table);
        
        amqp_rpc_reply_t reply = amqp_get_rpc_reply(state);
        CHECK_REPLY_ERROR(reply, "Unbinding exchange");
    }


private:
    std::string _destination;
    std::string _source;
    std::string _routingKey;
};

void RabbitmqConfig::exchangeUnbind(const std::string& destination,const std::string& source,const std::string&routingKey,bool redo){
    ConfigItem::Ptr item = std::make_shared<ExchangeBind>(destination,source,routingKey,redo);
    _items.push_back(item);
}

class ExchangeDelete: public ConfigItem{
public:
    ExchangeDelete(const std::string& exchange,bool isUnused,bool redo)
        : _exchange(exchange)
        , _isUnused(isUnused)
        , ConfigItem(redo)
    {

    }
    virtual ~ExchangeDelete(){}

public:
    virtual bool config(const RabbitmqConnection::Ptr& conn,int& channel) override{
        if(!conn || !(conn->isOk()) || channel == 0)
            return false;
        amqp_connection_state_t state = reinterpret_cast<amqp_connection_state_t>(conn->getState());
        if(state == nullptr)
            return false;
        amqp_exchange_delete(state, static_cast<amqp_channel_t>(channel), 
            amqp_cstring_bytes(_exchange.c_str()), _isUnused ? 1 : 0);
        amqp_rpc_reply_t reply = amqp_get_rpc_reply(state);
        CHECK_REPLY_ERROR(reply, "Deleting exchange");
    }

private:
    std::string _exchange;
    bool _isUnused;
};


void RabbitmqConfig::exchangeDelete(const std::string& exchange,bool isUnused,bool redo){
    ConfigItem::Ptr item = std::make_shared<ConfigItem>(exchange,isUnused,redo);
    _items.push_back(item);
}


class QueueDeclare : public ConfigItem{
public:
    QueueDeclare(const std::string& queue,bool durable,bool exclusive,bool autoDelete,bool redo):
        ConfigItem(redo),
        _queue(queue),
        _durable(durable),
        _autoDelete(autoDelete),
        _exclusive(exclusive)
    {

    }

    virtual bool config(const RabbitmqConnection::Ptr& conn,int& channel) override{
        if(!conn || !(conn->isOk()) || channel == 0)
            return false;
        amqp_connection_state_t state = reinterpret_cast<amqp_connection_state_t>(conn->getState());
        if(state == nullptr)
            return false;
        const amqp_table_t argument_table = {0,nullptr};
        amqp_queue_declare(state, static_cast<amqp_channel_t>(channel), 
            amqp_cstring_bytes(_queue.c_str()), 
            0, _durable ? 1 : 0, _exclusive ? 1 : 0, _autoDelete ? 1 : 0, argument_table);
        amqp_rpc_reply_t reply = amqp_get_rpc_reply(state);
        CHECK_REPLY_ERROR(reply, "Declaring Queue");
    }
    
private:
    std::string _queue;
    bool _durable;
    bool _autoDelete;
    bool _exclusive;
};

void RabbitmqConfig::queueDeclare(const std::string& exchange,bool durable,bool exclusive,bool autoDelete,bool redo){
    ConfigItem::Ptr item = std::make_shared<ExchangeDeclare>(exchange,durable,exclusive,autoDelete,redo);
    _items.push_back(item);
}

class QueueBind: public ConfigItem{
public:
    QueueBind(const std::string& queue,const std::string& exchange,const std::string&routingKey,bool redo = false)
        : _queue(queue)
        , _exchange(exchange)
        , _routingKey(routingKey)
        , ConfigItem(redo)
    {

    }
    virtual ~QueueBind(){}

    virtual bool config(const RabbitmqConnection::Ptr& conn,int& channel) override{
        if(!conn || !(conn->isOk()) || channel == 0)
            return false;

        amqp_connection_state_t state = reinterpret_cast<amqp_connection_state_t>(conn->getState());
        if(state == nullptr)
            return false;

        const amqp_table_t arguments_table = {0,nullptr};
        amqp_queue_bind(state, static_cast<amqp_channel_t>(channel),
            amqp_cstring_bytes(_queue.c_str()), amqp_cstring_bytes(_exchange.c_str()),
            amqp_cstring_bytes(_routingKey.c_str()), arguments_table);
        
        amqp_rpc_reply_t reply = amqp_get_rpc_reply(state);
        CHECK_REPLY_ERROR(reply, "Binding queue");
    }


private:
    std::string _queue;
    std::string _exchange;
    std::string _routingKey;
};

void RabbitmqConfig::queueBind(const std::string& queue,const std::string& exchange,const std::string&routingKey,bool redo){
    ConfigItem::Ptr item = std::make_shared<ExchangeBind>(queue,exchange,routingKey,redo);
    _items.push_back(item);
}

class QueueUnbind: public ConfigItem{
public:
    QueueUnbind(const std::string& queue,const std::string& exchange,const std::string&routingKey,bool redo = false)
        : _queue(queue)
        , _exchange(exchange)
        , _routingKey(routingKey)
        , ConfigItem(redo)
    {

    }

    virtual ~QueueUnbind(){}

    virtual bool config(const RabbitmqConnection::Ptr& conn,int& channel) override{
        if(!conn || !(conn->isOk()) || channel == 0)
            return false;
        amqp_connection_state_t state = reinterpret_cast<amqp_connection_state_t>(conn->getState());
        if(state == nullptr)
            return false;

        const amqp_table_t arguments_table = {0,nullptr};
        amqp_queue_unbind(state, static_cast<amqp_channel_t>(channel),
            amqp_cstring_bytes(_queue.c_str()), amqp_cstring_bytes(_exchange.c_str()),
            amqp_cstring_bytes(_routingKey.c_str()), arguments_table);
        
        amqp_rpc_reply_t reply = amqp_get_rpc_reply(state);
        CHECK_REPLY_ERROR(reply, "Unbinding queue");
    }


private:
    std::string _queue;
    std::string _exchange;
    std::string _routingKey;
};

void RabbitmqConfig::queueUnbind(const std::string& queue,const std::string& exchange,const std::string&routingKey,bool redo){
    ConfigItem::Ptr item = std::make_shared<ExchangeBind>(queue,exchange,routingKey,redo);
    _items.push_back(item);
}

class QueueDelete: public ConfigItem{
public:
    QueueDelete(const std::string& queue,bool ifUnused,bool ifEmpty,bool redo)
        : _exchange(queue)
        , _ifUnused(ifUnused)
        , _ifEmpty(ifEmpty)
        , ConfigItem(redo)
    {

    }
    virtual ~QueueDelete(){}

public:
    virtual bool config(const RabbitmqConnection::Ptr& conn,int& channel) override{
        if(!conn || !(conn->isOk()) || channel == 0)
            return false;
        amqp_connection_state_t state = reinterpret_cast<amqp_connection_state_t>(conn->getState());
        if(state == nullptr)
            return false;
        amqp_queue_delete(state, static_cast<amqp_channel_t>(channel), 
            amqp_cstring_bytes(_exchange.c_str()), _ifUnused ? 1 : 0,_ifEmpty ? 1 : 0);
        amqp_rpc_reply_t reply = amqp_get_rpc_reply(state);
        CHECK_REPLY_ERROR(reply, "Deleting queue");
    }

private:
    std::string _exchange;
    bool _ifUnused;
    bool _ifEmpty;
};


void RabbitmqConfig::queueDelete(const std::string& exchange,bool ifUnused,bool ifEmpty,bool redo){
    ConfigItem::Ptr item = std::make_shared<ConfigItem>(exchange,ifUnused,ifEmpty,redo);
    _items.push_back(item);
}

class QueueConsume: public ConfigItem{
public:
    QueueConsume(const std::string& queue,const std::string& tag,bool noLocal,bool noAck,bool exclusive,bool redo)
        : _queue(queue)
        , _tag(tag)
        , _noLocal(noLocal)
        , _noAck(noAck)
        , _exclusive(exclusive)
        , ConfigItem(redo)
    {

    }
    virtual ~QueueConsume(){}

    virtual bool config(const RabbitmqConnection::Ptr& conn,int& channel) override{
        if(!conn || !(conn->isOk()) || channel == 0)
            return false;
        amqp_connection_state_t state = reinterpret_cast<amqp_connection_state_t>(conn->getState());
        if(state == nullptr)
            return false;
        const amqp_table_t arguments_table = {0,nullptr};
        amqp_basic_consume(state, static_cast<amqp_channel_t>(channel), 
            amqp_cstring_bytes(_queue.c_str()), amqp_cstring_bytes(_tag.c_str()),
            _noLocal ? 1 : 0,_noAck ? 1 : 0,_exclusive ? 1 : 0,arguments_table);
        amqp_rpc_reply_t reply = amqp_get_rpc_reply(state);
        CHECK_REPLY_ERROR(reply, "Consuming queue");
    }
    
    // 订阅需要独占
    virtual bool exclusiveChannel() override{ return true; }

private:
    std::string _queue;
    std::string _tag;
    bool _noLocal;
    bool _noAck;
    bool _exclusive;
};

void RabbitmqConfig::queueConsume(const std::string& queue,const std::string& tag,bool noLocal,bool noAck,bool exclusive,bool redo){
    ConfigItem::Ptr item = std::make_shared<QueueConsume>(queue,tag,noLocal,noAck,exclusive);
    _items.push_back(item);
}


bool RabbitmqConfig::config(const RabbitmqConnection::Ptr& conn){
    if(_items.empty())
        return false;

    bool ret = true;
    bool test = false;    
    int channel = 0;
    int sharedChannel = 0;   // 共享通道
    std::list<std::shared_ptr<ConfigItem>>::iterator it;
    it = _items.begin();
    while(it != _items.end()){
        ConfigItem::Ptr& config = *it;
        test = config->exclusiveChannel();
        if(test){
            // 开辟一个通道
        }
        else{
            if(sharedChannel == 0){
                //开辟一个通道
            }
            channel = sharedChannel;
        }
        // 如果 config 返回 false（内部 CHECK_REPLY_ERROR 触发了非致命错误），
        // 只是把总标志 ret 置为 false，但不中断循环。因为即使某个交换机声明失败，后续的队列绑定或许还能救回来
        if(!config->config(conn, channel)){
            ret = false;
        }
        if(!conn->isOk())
            break;
        if(!test)
            sharedChannel = channel;
        if(config->redo())
            ++it;  // 保留这个任务对象在链表中
        else 
            _items.erase(it);
    }
    return ret;
}