#pragma once

#include "MsgBase.h"
#include "MsgWrapper.h"
#include <memory>

class IMsgCreatot {
  public:
    IMsgCreatot() = default;
    virtual ~IMsgCreatot() = default;

    typedef std::shared_ptr<IMsgCreatot> Ptr;

  public:
    virtual MsgBase::Ptr deserialize(const MsgWrapper::Ptr &wrapper) const = 0;
};

template <typename T> class MsgCreatot : public IMsgCreatot {
  public:
    MsgCreatot() = default;
    virtual ~MsgCreatot() = default;

  public:
    virtual MsgBase::Ptr
    deserialize(const MsgWrapper::Ptr &wrapper) const override {
        std::unique_ptr<T> ptr(new T());
        if (!wrapper->unpackMessage(*ptr)) {
            return nullptr;
        }
        return MsgBase::Ptr(ptr.release());
    };
};