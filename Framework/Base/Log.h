#include "Singleton.h"
#include <memory>
#include <string>
#include <sstream>
#include <atomic>

namespace LogLevel{
    //日志等级
    enum{
        Debug,
        Info,
        Warning,
        Error
    };
}

//日志管理者

class Logger;
class LogManager:public Singleton<LogManager> {
private:
    LogManager();
    ~LogManager();
    friend class Singleton<LogManager>;

public:
    //初始化
    void initialize(const std::string& logFile);

    //关闭
    void stop();

    //调试日志
    void logDebug(const std::string& msg,const std::string& file,int line);

    //常规日志
    void logInfo(const std::string& msg,const std::string& file,int line);

    //警告日志
    void logWarning(const std::string& msg,const std::string& file,int line);

    //错误日志
    void logError(const std::string& msg,const std::string& file,int line);

private:
    std::shared_ptr<Logger> _logger;
    std::atomic<bool> _initialized;

};


class LogStream{
public:
    LogStream(const std::string& file,int line,int level);
    virtual ~LogStream();

private:
    std::ostringstream _oss;
    
    //源文件
    std::string _file;
    //行号
    int _line;
    //等级
    const int _level;

public:
    template<typename T>
    LogStream& operator<<(T&& data){
        _oss<<std::forward<T>(data);
        return *this;
    }
    // 当遇到 std::endl 时，触发 flush()，立即将当前缓冲区内容输出到 LogManager
    LogStream& operator<<(std::ostream& (*f)(std::ostream&));

    void clear();

private:
    void flush();

};

//输出日志
#define LOG_DEBUG(msg)    LogManager::getSingleton().logDebug(msg,__FILE__,__LINE__)
#define LOG_INFO(msg)     LogManager::getSingleton().logInfo(msg,__FILE__,__LINE__)
#define LOG_WARNING(msg)  LogManager::getSingleton().logWarning(msg,__FILE__,__LINE__)
#define LOG_ERROR(msg)    LogManager::getSingleton().logError(msg,__FILE__,__LINE__)

//流形式输出日志
#define DebugS      LogStream(__FILE__,__LINE__,LogLevel::Debug)
#define InfoS       LogStream(__FILE__,__LINE__,LogLevel::Info)
#define WarningS    LogStream(__FILE__,__LINE__,LogLevel::Warning)
#define ErrorS      LogStream(__FILE__,__LINE__,LogLevel::Error)