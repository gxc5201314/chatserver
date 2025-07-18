#include "redis.hpp"
#include <iostream>
#include <cstring>

using namespace std;

// 构造函数
Redis::Redis()
    : _publish_context(nullptr), _subscribe_context(nullptr)
{
}

// 析构函数
Redis::~Redis()
{
    if (_publish_context != nullptr)
    {
        redisFree(_publish_context);
    }

    if (_subscribe_context != nullptr)
    {
        redisFree(_subscribe_context);
    }
}

// 连接 Redis 服务器
bool Redis::connect()
{
    // 负责 publish 发布消息的上下文连接
    _publish_context = redisConnect("127.0.0.1", 6379);
    if (nullptr == _publish_context)
    {
        cerr << "Connect redis failed: " << endl;

        return false;
    }

    // 负责 subscribe 订阅消息的上下文连接
    _subscribe_context = redisConnect("127.0.0.1", 6379);
    if (nullptr == _subscribe_context)
    {
        cerr << "Connect redis failed: " << endl;
        return false;
    }

    // 在独立的线程中，监听订阅通道上的事件
    // 这里的 observer_channel_message 是一个阻塞函数，因此必须在新线程中运行
    thread t([&]() {
        observer_channel_message();
    });
    t.detach(); // 使用分离线程，让它在后台独立运行

    cout << "Connect redis-server success!" << endl;

    return true;
}

// 向 redis 指定的通道 channel 发布消息
bool Redis::publish(int channel, string message)
{
    // 使用 redisCommand 执行 PUBLISH 命令
    redisReply *reply = (redisReply *)redisCommand(_publish_context, "PUBLISH %d %s", channel, message.c_str());
    if (nullptr == reply)
    {
        cerr << "publish command failed!" << endl;
        return false;
    }

    // redisCommand 返回的 redisReply 对象需要手动释放
    freeReplyObject(reply);
    return true;
}

// 向 redis 指定的通道 subscribe 订阅消息
// 注意：此函数只发送 SUBSCRIBE 命令，真正的消息接收在 observer_channel_message 中进行
bool Redis::subscribe(int channel)
{
    // hiredis 在执行 SUBSCRIBE 命令时，是同步阻塞的，不会影响其他线程
    // 这里我们只发送命令，不处理返回。真正的返回处理在 observer_channel_message 循环中
    // redisAppendCommand 是非阻塞的，只将命令放入缓冲区
    if (REDIS_ERR == redisAppendCommand(this->_subscribe_context, "SUBSCRIBE %d", channel))
    {
        cerr << "subscribe command failed!" << endl;
        return false;
    }
    
    // redisBufferWrite 会将缓冲区中的命令发送出去
    int done = 0;
    while (!done) {
        if (REDIS_ERR == redisBufferWrite(this->_subscribe_context, &done)) {
            cerr << "subscribe command failed (buffer write)!" << endl;
            return false;
        }
    }
    // 注意：这里不需要调用 redisGetReply，因为它会阻塞。这个工作交给 observer 线程。
    return true;
}

// 向 redis 指定的通道 unsubscribe 取消订阅消息
bool Redis::unsubscribe(int channel)
{
    // 与 subscribe 类似，只发送命令
    if (REDIS_ERR == redisAppendCommand(this->_subscribe_context, "UNSUBSCRIBE %d", channel))
    {
        cerr << "unsubscribe command failed!" << endl;
        return false;
    }

    int done = 0;
    while (!done) {
        if (REDIS_ERR == redisBufferWrite(this->_subscribe_context, &done)) {
            cerr << "unsubscribe command failed (buffer write)!" << endl;
            return false;
        }
    }
    return true;
}


// 在独立线程中接收订阅通道中的消息
void Redis::observer_channel_message()
{
    redisReply *reply = nullptr;
    while (REDIS_OK == redisGetReply(this->_subscribe_context, (void **)&reply))
    {
        // 返回的 reply 是一个三元数组
        // 1. "message"
        // 2. channel name
        // 3. a message
        if (reply != nullptr && reply->type == REDIS_REPLY_ARRAY && reply->elements == 3)
        {
            // 确保是消息类型
            if (strcmp(reply->element[0]->str, "message") == 0)
            {
                // 检查回调函数是否已注册
                if (_notify_message_handler)
                {
                    // 调用回调函数，上报收到的消息
                    // reply->element[1]->str 是通道号，reply->element[2]->str 是具体消息
                    int channel = atoi(reply->element[1]->str);
                    string message = reply->element[2]->str;
                    _notify_message_handler(channel, message);
                }
            }
        }
        
        // 每次循环后必须释放 reply 对象，否则会导致内存泄漏
        freeReplyObject(reply);
    }
    
    cerr << ">>>>>>>>>> observer_channel_message quit <<<<<<<<<<" << endl;
}

// 初始化向业务层上报通道消息的回调对象
void Redis::init_notify_handler(function<void(int, string)> fn)
{
    this->_notify_message_handler = fn;
}