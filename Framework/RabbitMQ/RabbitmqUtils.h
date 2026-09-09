#pragma once
#include "rabbitmq-c/amqp.h"

class RabbitmqUtils{
private:
    RabbitmqUtils() = default;

public:
    /**
     * @brief 检查相应错误
     * 
     * @param reply 相应
     * @param context 上下文
     * @return int 0-无错误，1-发生错误且连接关闭，2-发生错误且通道关闭，3-发生错误
     */
    static int checkReplyError(const amqp_rpc_reply_t& reply,const char* context);

};
