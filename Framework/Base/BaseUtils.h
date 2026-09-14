#pragma once
#include <string>
#include <time.h>

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
    /**
     * 获取当前时间，单位毫秒
     * 从1970-01-01 00:00:00 UTC到现在的毫秒数
     * @return 当前时间
     */
    static time_t getCurrentMillisecond();

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