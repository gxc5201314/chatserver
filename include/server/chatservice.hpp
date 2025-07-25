#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include <muduo/net/TcpConnection.h>
#include <unordered_map>
#include <functional>
#include <mutex>
using namespace std;
using namespace muduo;
using namespace muduo::net;

#include "redis.hpp"
#include "usermodel.hpp"
#include "groupmodel.hpp"
#include "offlinemessagemodel.hpp"
#include "friendmodel.hpp"
#include"json.hpp"
using json = nlohmann::json;

// 表示处理消息的事件回调方法类型
using MsgHandler = std::function<void(const TcpConnectionPtr &conn, json &js, Timestamp)>;

// 聊天服务器业务类
class ChatService
{
private:
    ChatService();
    // 存储消息id和其对应的事件处理方法
    unordered_map<int, MsgHandler> _msgHandlerMap;

    // 存储在线用户的通信连接
    unordered_map<int, TcpConnectionPtr> _userConnMap;

    // 定义互斥锁，保证_userConnMap的线程安全
    mutex _connMutex;

    // 数据操作类对象
    UserModel _userModel;
    OfflineMsgModel _offlineMsgModel;
    FriendModel _friendModel;
    GroupModel _groupModel;

    // redis操作对象
    Redis _redis;
    
public:
    // 获取单例对象的接口函数
    static ChatService* instance();
    // 处理登录业务
    void login(const TcpConnectionPtr &conn, json &js, Timestamp);
    // 处理注业务
    void reg(const TcpConnectionPtr &conn, json &js, Timestamp);
    // 一对一聊天业务
    void oneChat(const TcpConnectionPtr &conn, json &js, Timestamp);
    // 添加好友业务
    void addFriend(const TcpConnectionPtr &conn, json &js, Timestamp);
    // 创建群组业务
    void creatGroup(const TcpConnectionPtr &conn, json &js, Timestamp);
    // 加入群组业务
    void addGroup(const TcpConnectionPtr &conn, json &js, Timestamp);
    // 群组聊天业务
    void groupChat(const TcpConnectionPtr &conn, json &js, Timestamp);
    // 处理注销业务
    void logOut(const TcpConnectionPtr &conn, json &js, Timestamp);
    // 获取消息对应的处理器
    MsgHandler getHandler(int msgid);
    // 处理客户端异常退出
    void clientCloseException(const TcpConnectionPtr &conn);
    // 服务器异常业务重置方法
    void reset();

    //
    void handleRedisSubscribeMessage(int userid, string msg);

};


#endif