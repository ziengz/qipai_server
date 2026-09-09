#pragma once
#include <memory>
#include <string>
#include <mutex>


/**
 * @brief 会话基类
 * 一个TCP网络连接对应一个会话，用于处理业务逻辑，而TCP网络连接类自身则封装了
 * 底层网络IO逻辑，并且不暴露到上层业务逻辑
 */
class Connection;
class Session: public std::enable_shared_from_this<Session>{
private:
public:
    Session(const std::shared_ptr<Connection>& con);
    virtual ~Session();
    typedef std::shared_ptr<Session> Ptr;

    /**
     * @brief 获取连接ID
     * 
     * @param id 返回链接id
     */
    void getId(std::string& id) const;

    /**
     * @brief Get the Remote Ip object
     * 
     * @return 返回远端IP地址
     */
    const std::string& getRemoteIp() const;
    
    /**
     * @brief 绘画是否有效
     * 
     * @return true 有效
     * @return false 无效
     */
    bool isValid();

    /**
     * @brief 接收到数据事件
     * 
     * @param buf 数据缓存
     * @param length 数据长度
     */
    virtual void onReceive(char* buf,std::size_t length) = 0;

    /**
     * @brief 会话断开事件
     */
    virtual void onDisconnect();

    /**
     * @brief 发送数据
     * 
     * @param buf 数据缓存
     * @param length 数据长度
     */
    void send(const char* buf,size_t length);

    /**
     * @brief 发送数据
     * 
     * @param data 数据缓存
     */
    void send(const std::shared_ptr<std::string>& data);

    /**
     * @brief 返回会话是否仍然活跃，用于支持心跳检测
     * @return 默认在连接未断开时返回true
     */
    virtual bool isAlive(const time_t& nowTime) const;

    /**
     * @brief 心跳
     * 默认不支持心跳，空实现
     */
    virtual void heartbeat();

private:
    //设置会话无效
    void setInvalid();
private:
    //连接
    std::weak_ptr<Connection> _connection;
    //远端ip
    std::string _remoteIp;
    //有效标志
    bool _valid;
    //信号量
    std::mutex _mtx;
};

//会话构造器基类
class SessionCreater{
public: 
    SessionCreater();
    virtual ~SessionCreater();
    typedef std::shared_ptr<SessionCreater> Ptr;

public: 
    virtual Session::Ptr create(const std::shared_ptr<Connection>& con) const = 0;

};