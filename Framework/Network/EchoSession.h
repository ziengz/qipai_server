#pragma once
#include "Connection.h"
#include "Session.h"
#include <memory>


/**
 * @brief 回声会话（测试用）
 * 
 */
class EchoSession: public Session{
public:
    EchoSession(const std::shared_ptr<Connection>& con);
    virtual ~EchoSession();

    class Creator: public SessionCreater{
        public:
            virtual Session::Ptr create(const std::shared_ptr<Connection>& con) const{
                Session::Ptr sess = std::make_shared<EchoSession>(con);
                
                return sess;
            }
    };
public:
    void onReceive(char* buf,std::size_t length) override;
};