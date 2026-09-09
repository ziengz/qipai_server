#pragma once
#include <memory>
#include <mutex>

class RabbitmqConnection{
private:
    bool _ok;
    mutable std::mutex _mtx;
public:
    RabbitmqConnection();
    virtual ~RabbitmqConnection();
    typedef std::shared_ptr<RabbitmqConnection> Ptr;

public:
    virtual void* getState() = 0;
    bool isOk() const;
    virtual void setOk(bool seeting);

};