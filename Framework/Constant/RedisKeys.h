#pragma once

#include <string>

class RedisKeys {
  private:
    RedisKeys();

  public:
    virtual ~RedisKeys();

    // 玩家消息密钥，后跟玩家ID
    static const std::string PLAYER_MESSAGE_SECRET;

    // IP黑名单
    static const std::string IP_BLACKLIST;
};