#include "Base/Log.h"
#include <cstdio>
#include "RabbitmqUtils.h"

int RabbitmqUtils::checkReplyError(const amqp_rpc_reply_t& reply,const char* context){
    // 无事发生，继续执行
    if(reply.reply_type == AMQP_RESPONSE_NORMAL)
        return 0;
    // 返回 3（未知/其他）：一般指协议层解析错误，通常记录日志后放弃本次操作。
    int ret = 3;
    if(reply.reply_type == AMQP_RESPONSE_NONE){
        if(context == nullptr)
            LOG_ERROR("Missing RPC reply type");
        else
            ErrorS << context << "missing RPC reply type";
    }
    else if(reply.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION){
        if(context == nullptr)
            LOG_ERROR(amqp_error_string2(reply.library_error));
        else
            ErrorS << context << " " << amqp_error_string2(reply.library_error);
    }
    else if(reply.reply_type == AMQP_RESPONSE_SERVER_EXCEPTION){
        if(reply.reply.id == AMQP_CONNECTION_BLOCKED_METHOD){
            // 返回1（连接关闭）：意味着 TCP 连接已断开或登录失效。
            // 上层逻辑会立刻执行 conn->setOk(false)，并清空所有缓存的 Channel ID，等待重连线程（threadFunc）重建连接。
            ret = 1;
            amqp_connection_close_t* m = (amqp_connection_close_t*)reply.reply.decoded;
            std::string msg((const char*)(m->reply_text.bytes),m->reply_text.len);
            if(context == nullptr)
                ErrorS << "Connection error code: " << m->reply_code <<", message: " << msg;
        
            else{
                ErrorS << context <<" connection error code: " << m->reply_code << ", message: "<< msg;
            }
        }
        else if(AMQP_CHANNEL_CLOSE_METHOD == reply.reply.id){
            // 返回 2（通道关闭）：意味着 Channel 发生了致命异常（例如向不存在的 Exchange 发布消息）。
            // 上层逻辑只需要把当前这个 Channel 关闭并置为 0，下次调用时重新 openChannel 即可，无需重建 TCP 连接。
            ret = 2;
            amqp_channel_close_t* m = (amqp_channel_close_t*)reply.reply.decoded;
            std::string msg((const char*)(m->reply_text.bytes),m->reply_text.len);
            if(context == nullptr){
                ErrorS << "Server channel error code: "<<m->reply_code << ", message: " << msg;
            }
            else{
                ErrorS << context << "Server channel error code: "<<m->reply_code << ", message: " << msg;

            }
        }
        else {
            char tmp[16] = {'\0'};
            snprintf(tmp, sizeof(tmp), "%08X",reply.reply.id);
            if(context == nullptr){
                ErrorS << "Unkown server error,method id: "<< tmp;
            }
            else {
                ErrorS << context << "Unkown server error,method id: " << tmp;
            }
        }
    }
    return ret;
}