#pragma once
#include <string>
#include <time.h>
#include <unordered_map>
#include <list>
#include <vector>

class BaseUtils {
private:
    BaseUtils();
    virtual ~BaseUtils();

public:
    // 浮点数最小容差值
    static const float FLOAT_TOLERANCE;

    // 浮点数最小容差值
    static const double DOUBLE_TOLERANCE;

    // 空字符串
    static const std::string EMPTY_STRING;

    // 1000以内质数表
    static const int PRIME_NUMBERS[168];

private:
    static int _seedIndex;

public:
    // 判定字符串以XXX结尾的
    static bool endWith(const std::string &str, const std::string &end);

    // 从文件路径中获取文件名
    static void filename(const std::string path, std::string &file);

    template <typename T>
    static bool contain(const std::vector<T> &arr, const T &val) {
        typename std::vector<T>::const_iterator it = arr.begin();
        while (it != arr.end()) {
            if (*it == val)
                return true;
            ++it;
        }
        return false;
    }

    /**
     * @brief 顺序依次从Map映射表中取出值
     * 
     * @tparam T1 
     * @tparam T2 
     * @param dataMap 
     * @param sequence 
     * @return T2 
     */
    template<typename T1,typename T2>
    static T2 getInSequence(std::unordered_map<T1, T2>&dataMap,std::list<T1>& sequence){
        typename std::unordered_map<T1, T2>::const_iterator it;
        if(sequence.empty()){
            it = dataMap.begin();
            while (it!=dataMap.end()) {
                sequence.push_back(it->first);
                ++it;
            }
        }
        T2 ret;
        while(!sequence.empty()){
            T1 key = sequence.front();
            sequence.pop_front();
            it = dataMap.find(key);
            if(it!=dataMap.end()){
                ret = it->second;
                break;
            }
            // 没找到 → 这个 key 已失效（连接已被删），继续取下一个
        }
        return ret;
    }

        /**
         * 获取当前时间，单位毫秒
         * 从1970-01-01 00:00:00 UTC到现在的毫秒数
         * @return 当前时间
         */
        static time_t
        getCurrentMillisecond();

    /**
     * 获取当前时间，单位秒
     * 从1970-01-01 00:00:00 UTC到现在的秒数
     * @return 当前时间
     */
    static time_t getCurrentSecond();

    // 内存转换为Base64
    static bool encodeBase64(std::string &base64, const char *input, int len);
    // 从base64解码
    static bool decodeBase64(const std::string &input, std::string &output);

    /**
     * 字符串拷贝
     * @param str 源字符串
     * @return 调用c函数拷贝字符串，需要调用free函数释放
     */
    static char *strdup(const char *str);
};