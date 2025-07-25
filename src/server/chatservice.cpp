#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>

using namespace muduo;
// 获取单例对象的接口函数
ChatService* ChatService::instance()
{
    static ChatService service;
    return &service;
}

// 注册消息以及对应的handler回调
ChatService::ChatService()
{
    // 用户基本业务管理相关事件处理回调注册
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this,_1,_2,_3)});
    _msgHandlerMap.insert({LOGOUT_MSG, std::bind(&ChatService::logOut, this,_1,_2,_3)});
    _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this,_1,_2,_3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this,_1,_2,_3)});
    
    // 群组业务管理相关事件处理回调注册
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::creatGroup, this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this,_1,_2,_3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this,_1,_2,_3)});

    // 连接redis服务器
    if(_redis.connect()){
        // 设置上报消息的回调
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this,_1,_2));
    }
    
}

// 服务器异常业务重置方法
void ChatService::reset(){
    // 把online状态的用户设置为offline
    _userModel.resetState();
}

// 获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    // 记录错误日志msgid没有对应的事件处理回调
    auto it = _msgHandlerMap.find(msgid);
    if(it == _msgHandlerMap.end())
    {
        // 返回一个默认的处理器，空操作       
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp){
            LOG_ERROR << "msgid:" << msgid << " can not find handler!";
        };
    }
    else
    {
        return _msgHandlerMap[msgid];
    }
}

// 处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn){
    // id状态改为offline
    // 删除用户链接表
    User user;
    {
        lock_guard<mutex> lock(_connMutex);        
        for(auto it = _userConnMap.begin(); it != _userConnMap.end(); it++){
            if(it->second == conn){
                // 从map表删除用户的链接信息
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }
    // 用户注销，取消订阅通道redis
    _redis.unsubscribe(user.getId());
    
    // id状态改为offline
    if(user.getId() != -1){
        user.setState("offline");
        _userModel.updateState(user);   
    }  
}

// 处理登录业务
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp)
{
    int id = js["id"];
    string pwd = js["password"];
    User user = _userModel.query(id);
    if(user.getId() == id && user.getPassword() == pwd){
        if(user.getState() == "online"){
            // 用户存在，不允许重复登录
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "this account is using, input another!";
            conn->send(response.dump());
        }
        else{
            // 登录成功，记录用户连接信息
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id, conn});
            }
            // id用户登录成功后，向redis订阅channel（id）
            _redis.subscribe(id);

            // 登录成功，更新用户状态信息 offline->online
            user.setState("online");
            _userModel.updateState(user);

            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();

            // 查询用户是否有离线消息
            vector<string> vec = _offlineMsgModel.query(id);
            if(!vec.empty()){
                response["offlinemsg"] = vec;
                // 获取离线消息后，删除用户的所有离线消息
                _offlineMsgModel.remove(id);
            }

            // 查询用户的好友信息并返回
            vector<User> userVec = _friendModel.query(id);
            if(!userVec.empty()){
                vector<string> vec2;
                for(User &user : userVec){
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
            }

            // 查询用户的群组消息
            vector<Group> groupuserVec = _groupModel.queryGroups(id);
            if(!groupuserVec.empty()){
                vector<string> groupV;
                for(Group &group : groupuserVec){
                    json grpjs;
                    grpjs["id"] = group.getId();
                    grpjs["groupname"] = group.getName();
                    grpjs["groupdesc"] = group.getDesc();

                    vector<string> userV;
                    for(GroupUser &user : group.getUsers()){
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    grpjs["users"] = userV;
                    groupV.push_back(grpjs.dump());
                }
                response["groups"] = groupV;
            }

            conn->send(response.dump());
        }
    }
    else{
        // 该用户不存在登陆失败
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "id or password invalid!";
        conn->send(response.dump());
    }

}

// 处理注销业务
void ChatService::logOut(const TcpConnectionPtr &conn, json &js, Timestamp){
    int userid = js["id"].get<int>();
    // id状态改为offline
    // 删除用户链接表
    {
        lock_guard<mutex> lock(_connMutex);
        auto it  = _userConnMap.find(userid);
        if(it != _userConnMap.end()){
             _userConnMap.erase(it);
        }        
    }
    // 用户注销，取消订阅通道redis
    _redis.unsubscribe(userid);
    // id状态改为offline
    User user(userid, "", "", "offline");
    _userModel.updateState(user);   

}

// 处理注册业务 name password
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp)
{
    string name = js["name"];
    string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPassword(pwd);
    bool state = _userModel.insert(user);
    if(state){
        // 注册成功
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump());
    }else{
        // 注册失败
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        conn->send(response.dump());
    }
}

// 一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp){
    int toid = js["toid"].get<int>();

    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if(it != _userConnMap.end()){    
            // 在线，转发消息 服务器主动推送消息给toid用户
            it->second->send(js.dump());
            return;       
        }     
    }
    // 查询toid是否在线
    User user = _userModel.query(toid);
    if(user.getState() == "online"){
        // 在线, 不同服务器，向redis推送
        _redis.publish(toid,js.dump());
        return;
    }
    // toid不在线,存储离线消息
    _offlineMsgModel.insert(toid, js.dump());

}

// 添加好友业务 msgid id friendid
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp){
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    // 存储好友信息
    _friendModel.insert(userid, friendid);
    
}

// 创建群组业务
void ChatService::creatGroup(const TcpConnectionPtr &conn, json &js, Timestamp){
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    // 存储新创建的群组信息
    Group group(-1, name, desc);
    if (_groupModel.createGroup(group))
    {
        // 存储群组创建人信息
        _groupModel.addGroup(userid, group.getId(), "creator");
    } 
}
// 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp){
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModel.addGroup(userid, groupid, "normal");
}
// 群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp){
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);

    lock_guard<mutex> lock(_connMutex);
    for (int id : useridVec)
    {
        auto it = _userConnMap.find(id);
        if (it != _userConnMap.end())
        {
            // 转发群消息
            it->second->send(js.dump());
        }
        else
        {
            // 查询toid是否在线
            User user = _userModel.query(id);
            if(user.getState() == "online"){
                _redis.publish(id,js.dump());
            }
            // 存储离线群消息
            _offlineMsgModel.insert(id, js.dump());
        }
    }
}

// 从redis消息队列中获取订阅的消息
// 这个函数通常作为回调，由 Redis 订阅模块在收到消息时调用
void ChatService::handleRedisSubscribeMessage(int userid, string msg)
{

    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if (it != _userConnMap.end())
    {
        // 用户在线，直接通过其对应的 TCP 连接将消息转发过去
        // it->second 应该是该用户对应的 TcpConnectionPtr
        // js.dump() 将 JSON 对象序列化为字符串
        it->second->send(msg);
        return; // 消息已发送，函数结束
    }

    // 用户不在线，存储该用户的离线消息
    // _offlineMsgModel 是一个数据模型对象，负责将消息写入数据库或其它持久化存储
    _offlineMsgModel.insert(userid, msg);
}