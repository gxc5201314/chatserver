#include "db.h"
#include <muduo/base/Logging.h>
// -- 静态的数据库配置信息 --
// 外部可以定义一个Config类来读取配置文件，这里为了简单直接使用静态变量
static std::string server = "127.0.0.1";
static std::string user = "root";
static std::string password = "123456789";
static std::string dbname = "chat";

// 初始化数据库连接
MySQL::MySQL() {
    _conn = mysql_init(nullptr);
}

// 释放数据库连接资源
MySQL::~MySQL() {
    if (_conn != nullptr) {
        mysql_close(_conn);
    }
}

// 连接数据库
bool MySQL::connect() {
    // 使用C API连接数据库
    MYSQL* p = mysql_real_connect(_conn, server.c_str(), user.c_str(),
                                  password.c_str(), dbname.c_str(), 3306, nullptr, 0);

    if (p != nullptr) {
        // C/C++代码默认是ASCII，如果不设置，从MySQL拉下来的中文是乱码
        mysql_query(_conn, "set names utf8"); // 推荐使用utf8而非gbk
        LOG_INFO << "connect mysql success!";
    }
    else{
        LOG_INFO << "connot mysql fail!";
    }
    return p;
}

// 更新操作
bool MySQL::update(std::string sql) {
    if (mysql_query(_conn, sql.c_str())) {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":" << sql << "更新失败！";
        return false;
    }
    return true;
}

// 查询操作
MYSQL_RES* MySQL::query(std::string sql) {
    if (mysql_query(_conn, sql.c_str())) {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":" << sql << "查询失败！";
        return nullptr;
    }
    return mysql_use_result(_conn);
}

// 获取连接句柄
MYSQL* MySQL::getConnection() {
    return _conn;
}
