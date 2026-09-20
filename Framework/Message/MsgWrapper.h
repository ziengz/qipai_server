#pragma once
#include "Base/BaseUtils.h"
#include "msgpack/adaptor/define_decl.hpp"
#include "msgpack/msgpack.hpp"
#include "msgpack/v3/object_decl.hpp"
#include "msgpack/v3/unpack.hpp"

#include <exception>
#include <memory>
#include <string>

class MsgWrapper {
  private:
    // 消息体类型
    std::string msgType;

    // 消息体MessagePack打包数据
    std::string msgPack;

    MSGPACK_DEFINE_MAP(msgType, msgPack);

  public:
    MsgWrapper();
    MsgWrapper(const std::string &type);
    MsgWrapper(const std::string &type, const std::string &pack);

    virtual ~MsgWrapper();
    typedef std::shared_ptr<MsgWrapper> Ptr;

  public:
    const std::string &getType() const;
    void setType(const std::string &type);
    const std::string &getPack() const;
    void setPack(const std::string &pack);

    template <typename T> bool packMessage(const T &msg, std::string &pack) {
        try {
            msgpack::sbuffer sbuf1;
            msgpack::pack(sbuf1, msg);
            std::string content(sbuf1.data(), sbuf1.size());
            if (!BaseUtils::encodeBase64(msgPack, content.data(),
                                         static_cast<int>(content.size())))
                return false;
            msgType = msg.getType();
            msgpack::sbuffer sbuf2;
            msgpack::pack(sbuf2, *this);
            pack.assign(sbuf2.data(), sbuf2.size());

        } catch (std::exception &ex) {
            return false;
        }
        return true;
    }

    template <typename T> bool unpackMessage(T &msg) {
        try {
            std::string content;
            if (!BaseUtils::decodeBase64(msgPack, content))
                return false;
            msgpack::object_handle oh =
                msgpack::unpack(content.data(), content.size());
            const msgpack::object &obj = oh.get();
            obj.convert(msg);
        } catch (std::exception &ex) {
            return false;
        }
        return true;
    }
};