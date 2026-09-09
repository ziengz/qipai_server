#include "Log.h"
#include "BaseUtils.h"

#include <boost/log/trivial.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/core/null_deleter.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/async_frontend.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/thread/thread.hpp>
#include <boost/process.hpp>



class Logger{
private:
    typedef boost::log::sinks::asynchronous_sink<boost::log::sinks::text_file_backend> sink_file;
    typedef boost::log::sinks::asynchronous_sink<boost::log::sinks::text_ostream_backend> sink_print;
    boost::shared_ptr<sink_file> _sink_file;
    boost::shared_ptr<sink_print> _sink_print;
public:
    boost::log::sources::severity_logger<boost::log::trivial::severity_level> logger;

public:
    Logger(std::string& fileName){
        namespace expr = boost::log::expressions;
        namespace keywords = boost::log::keywords;
        namespace attr = boost::log::attributes;

        boost::log::add_common_attributes();

        std::string logName = fileName + "_%Y-%m-%d_%N.log";
        boost::shared_ptr<boost::log::sinks::text_file_backend> backend_file = 
            boost::make_shared<boost::log::sinks::text_file_backend>(
                keywords::file_name = logName,
                keywords::open_mode = std::ios_base::out | std::ios_base::app,
                keywords::rotation_size = 30 * 1024 * 1024,   //不超过30M滚动log
                keywords::time_based_rotation = boost::log::sinks::file::rotation_at_time_point(0,0,0)//在每天 0 点 0 分 0 秒时，强制滚动日志
        );
        backend_file->auto_flush(true);
        
        //创建一个后端并附加几个流到它上面
        boost::shared_ptr<boost::log::sinks::text_ostream_backend> backend_print = 
            boost::make_shared<boost::log::sinks::text_ostream_backend>();
        
        // 控制台打印，并使用null_deleter()空删除器，不会被销毁
        backend_print->add_stream(boost::shared_ptr<std::ostream>(&std::clog,boost::null_deleter()));
        backend_print->auto_flush(true);

        // 包装到前端并注册到内核
        _sink_file = boost::make_shared<sink_file>(backend_file);
        _sink_print = boost::make_shared<sink_print>(backend_print);

        // 可以通过 sink 接口管理过滤和格式化
        boost::log::formatter fmt = expr::stream
            << "[" << expr::attr<attr::current_thread_id::value_type>("ThreadID")
            << "] " <<expr::format_date_time<boost::posix_time::ptime>("TimeStramp","%m-%dT%H:%M:%S.%f")
            << " " <<boost::log::trivial::severity
            << " " <<expr::message;
        _sink_file->set_formatter(fmt);
        _sink_print->set_formatter(fmt);

        boost::shared_ptr<boost::log::core> core = boost::log::core::get();
        core->set_filter(boost::log::trivial::severity >= boost::log::trivial::trace);
        core->add_sink(_sink_file);
        core->add_sink(_sink_print);
    }

    virtual ~Logger(){
        boost::shared_ptr<boost::log::core> core = boost::log::core::get();
        if(_sink_print){
            //从内核中移除sink,因此没有记录会被传递给它
            core->remove_sink(_sink_print);
            
            _sink_print->stop();

            //刷新所有可能在缓冲中剩余的日志记录
            _sink_print->flush();
            _sink_print.reset();
        }
        if(_sink_file){
            core->remove_sink(_sink_file);
            _sink_file->stop();
            _sink_file->flush();
            _sink_file.reset();
        }
    }
};

template<> LogManager* Singleton<LogManager>::_inst = nullptr;

LogManager::LogManager()
    :_initialized(false)
{}

LogManager::~LogManager(){}

//初始化
void LogManager::initialize(const std::string& fileName)
{
    if(_initialized)
        return;
    _initialized = true;
    if(fileName.empty())
        throw std::runtime_error("specific log file name is empty");
    std::string tmpName;
    if(BaseUtils::endWith(fileName,".log")){
        tmpName = fileName.substr(0,fileName.length() - 4);
    }
    else
        tmpName = fileName;
    _logger = std::make_shared<Logger>(tmpName);
}
//关闭
void LogManager::stop(){
    if(!_initialized)
        return;
    _initialized = false;
    _logger.reset();
}

void LogManager::logDebug(const std::string& msg,const std::string& file,int line){
    if(!_initialized)
        return;
    std::string text;
    BaseUtils::filename(file,text);
    text = text + ":" + std::to_string(line);
    text = text + " " + msg;
    BOOST_LOG_SEV(_logger->logger,boost::log::trivial::debug)<<text;
}
void LogManager::logInfo(const std::string& msg,const std::string& file,int line){
    if(!_initialized)
        return;
    std::string text;
    BaseUtils::filename(file,text);
    text = text + ":" + std::to_string(line);
    text = text + " " + msg;
    BOOST_LOG_SEV(_logger->logger,boost::log::trivial::info)<<text;
}
void LogManager::logWarning(const std::string& msg,const std::string& file,int line){
    if(!_initialized)
        return;
    std::string text;
    BaseUtils::filename(file,text);
    text = text + ":" + std::to_string(line);
    text = text + " " + msg;
    BOOST_LOG_SEV(_logger->logger,boost::log::trivial::warning)<<text;
}
void LogManager::logError(const std::string& msg,const std::string& file,int line){
    if(!_initialized)
        return;
    std::string text;
    BaseUtils::filename(file,text);
    text = text + ":" + std::to_string(line);
    text = text + " " + msg;
    BOOST_LOG_SEV(_logger->logger,boost::log::trivial::error)<<text;
}


LogStream::LogStream(const std::string& file,int line,int level):
    _file(file),_line(line),_level(level)
{}

LogStream& LogStream::operator<<(std::ostream& (*f)(std::ostream&)){
    flush();
    return *this;
}

LogStream::~LogStream(){
    flush();
}

void LogStream::clear(){
    _oss.str("");
}

void LogStream::flush(){
    std::string msg = _oss.str();
    if(msg.empty())
        return;
    if(_level == LogLevel::Debug)
        LogManager::getSingleton().logDebug(msg,_file,_line);
    if(_level == LogLevel::Info)
        LogManager::getSingleton().logInfo(msg,_file,_line);
    if(_level == LogLevel::Warning)
        LogManager::getSingleton().logWarning(msg,_file,_line);
    if(_level == LogLevel::Error)
        LogManager::getSingleton().logError(msg,_file,_line);

    //清空已输出日志
    _oss.str("");
}