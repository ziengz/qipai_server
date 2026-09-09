#pragma once
#include <memory>
#include <mutex>
#include <list>
#include <string>

class RabbitmqMesaageHandler{
private:
    std::string _tag;
    std::mutex _mtx;
    std::list<std::shared_ptr<std::string>> _msgQueue;

public:
    RabbitmqMesaageHandler(const std::string& tag);
    virtual ~RabbitmqMesaageHandler();

    typedef std::shared_ptr<RabbitmqMesaageHandler> Ptr;

public:
    const std::string& getTag() const;

    /**
     * @brief 压入消息
     * 
     * @param message 
     */
    void push(const std::shared_ptr<std::string>& message);

    /**
     * @brief 处理消息
     */
    void handle();

protected:
    /**
     * @brief 判断是否接受压入的消息 默认全不接受
     * 
     * @param message 压入的消息体
     * @return true 接收
     * @return false 不接受
     */
    virtual bool receive(const std::string& message);

    virtual void handleImpl(const std::string& message) = 0;
private:
    std::shared_ptr<std::string> pop();

};