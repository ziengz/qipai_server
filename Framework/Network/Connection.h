#pragma once
#include <memory>
#include <string>



class Connection : public std::enable_shared_from_this<Connection>{
public:
    Connection();
    virtual ~Connection();

    typedef std::shared_ptr<Connection> Ptr;
public:
    /**
     * @brief 查询链接ID
     * 
     * @param id 返回链接ID
     */
    virtual void getId(std::string& id) const = 0;

    /**
     * @brief 获取远端IP
     * 
     * @param remoteIp 返回远端IP
     */
    virtual void getRemoteIp(std::string& remoteIp) const = 0;
    
    /**
     * @brief 发送数据
     * 
     * @param data 数据缓存
     * @param length 数据长度
     */
    virtual void Send(const char* buf,size_t length) = 0;

    /**
     * @brief 发送数据
     * 
     * @param data 数据缓存
     */
    virtual void Send(const std::shared_ptr<std::string>& data) = 0;

    /**
     * @brief 连接时都已经断开
     */
    virtual bool isClose() = 0;
};