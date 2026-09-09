#include "MysqlQueryTask.h"
#include <mysql/jdbc.h>


MysqlCountTask::MysqlCountTask(const std::string& sql):MysqlCommandTask(sql,QueryType::Select){

}

int MysqlCountTask::getCount() const{
    return _count;
}

int MysqlCountTask::fetchResult(sql::ResultSet*res){
    int row = 0;
    if(res->next()){
        _count =  res->getInt(1);
        row++;
    }
    return row;
}