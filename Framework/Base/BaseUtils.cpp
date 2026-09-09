#include "BaseUtils.h"
#include "openssl/evp.h"
#include <cstddef>
#include <openssl/pem.h>
#include <openssl/md5.h>
#include <chrono>
#include <random>


bool BaseUtils::endWith(const std::string& str,const std::string& end){
    if(str.empty() && end.empty()){
        return false;
    }
    if(str.length() < end.length())
        return false;
    std::string sub = str.substr(str.length() - end.length());
    if(sub == end){
        return true;
    }
    return false;
}

// E:\cppsoft\boost_1_88_0   windows
// /home/zzp/log             linux
void BaseUtils::filename(const std::string path,std::string& fileName){
    fileName.clear();
    if(path.empty())
        return;
    size_t pos = fileName.rfind("/");
    if(pos == path.npos)
        pos = fileName.rfind("\\");
    if(pos == path.npos)
        fileName = path;
    else
        fileName = path.substr(pos + 1);

}

time_t BaseUtils::getCurrentMillisecond(){
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::chrono::duration<double> time_since_epoch = now.time_since_epoch();
    time_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_since_epoch).count();
    return ms;
}

time_t BaseUtils::getCurrentSecond(){
    time_t s = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    return s;
}

//内存转换为Base64
bool BaseUtils::encodeBase64(std::string& base64,const char* input,int len){
    base64.clear();
    size_t base64_len = static_cast<size_t>((len+2) / 3 * 4);
    if(base64_len == 0)
        return false;

    unsigned char* buf = new unsigned char[base64_len + 16];
    int ret = EVP_EncodeBlock(buf,reinterpret_cast<const unsigned char*>(input),len);
    if(ret > 0)
        base64.assign(reinterpret_cast<char*>(buf),static_cast<size_t>(ret));
    delete[] buf;
    return (ret > 0);    
}

//从base64解码
bool BaseUtils::decodeBase64(const std::string& input,std::string& output){
    output.clear();
    size_t input_len = input.size();
    if(input_len%4 != 0){
        return false;
    }

    size_t output_len = (input_len/4) * 3;
    unsigned char* buf = new unsigned char[output_len];
    int ret = EVP_DecodeBlock(buf, (const unsigned char*)input.data(), (int)output_len);

    if(ret == -1){
        //base64 decode failed;
        delete[] buf;
        return false;
    }
    //需要注意的是，被编码的数据大小不是3字节的整数倍时，base64后将会有一个“=”或两个“=”跟在后面
    //这样的话需要在解码之后看一下后面有几个“=”，再把解码过的数据进行删减
    int nums = 0;
    while(input.at(--input_len) == '='){
        ret--;
        nums++;
        if(nums > 2){
            //input可能不是base64
            delete[] buf;
            return false;
        }
    }
    output_len = static_cast<size_t>(ret);
    output.assign(reinterpret_cast<const char*>(buf),output_len);
    delete[] buf;
    return (ret > 0);
}