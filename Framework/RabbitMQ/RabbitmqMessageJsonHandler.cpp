#include "RabbitmqMessageJsonHandler.h"
#include <sstream>
#include "Base/BaseUtils.h"
#include "jsoncpp/include/json/json.h"


void RabbitmqMesaageJsonHandler::handleImpl(const std::string& message){
    std::stringstream ss(message);
    Json::Value jsonObj;
    ss >> jsonObj;
    Json::Value& typeObj = jsonObj["msgType"];
    if(!typeObj.isString())
        return;
    Json::Value& package = jsonObj["msgPack"];
    if(!package.isString())
        return;
    std::string msgType = typeObj.asString();
    std::string msgPack = package.asString();
    if(msgType.empty() && msgPack.empty())
        return;
    std::string json;
    if(!BaseUtils::decodeBase64(msgPack, json))
        return;
    if(json.empty())
        return;
    handleImpl(msgType,json);
}