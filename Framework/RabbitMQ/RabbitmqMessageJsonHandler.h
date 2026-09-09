#pragma once
#include "RabbitmqMessageHandler.h"

class RabbitmqMesaageJsonHandler: public RabbitmqMesaageHandler{
public:
    RabbitmqMesaageJsonHandler(const std::string& tag);
    virtual ~RabbitmqMesaageJsonHandler();

protected:
    virtual void handleImpl(const std::string& message) override;
    
    /**
     * @brief 处理消息
     * 
     * @param msgType 消息类型
     * @param json json消息体
     */
    virtual void handleImpl(const std::string& msgType,const std::string& json) = 0;
};