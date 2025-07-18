#include "json.hpp"
using json = nlohmann::json;

#include <iostream>
#include <vector>
#include <map>
using namespace std;

// json序列化实例1
string func1(){
    json js;
    js["msg_type"] = 2;
    js["from"] = "zhangsan";
    js["to"] = "lisi";
    js["msg"] = "hello!";
    string sendBuf = js.dump();
    return sendBuf;
    // {"id":[1,2,3,4],"msg":{"lisi":"hello china","zhangsan":"hello world"},"name":"zhangsan"}
}
// json序列化实例2
string func2(){
    json js;
    js["id"] = {1,2,3,4};
    js["msg"] = {{"zhangsan","hello world"},{"lisi","hello china"}};
    js["name"] = "zhangsan";
    return js.dump();
    // {"id":[1,2,3,4],"msg":{"lisi":"hello china","zhangsan":"hello world"},"name":"zhangsan"}
}
// json序列化实例3
string func3(){
    json js;
    vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);
    js["list"] = vec;

    // 序列化map容器
    map<int,string> mp;
    mp.insert({1,"黄山"});
    mp.insert({2,"华山"});
    mp.insert({3,"泰山"});
    js["path"] = mp;
    string sendBuf = js.dump();

    return sendBuf;
    // {"list":[1,2,3],"path":[[1,"黄山"],[2,"华山"],[3,"泰山"]]}

}

int main(){
    string recvBuf = func3();

    // 数据的反序列化
    json jsbuf = json::parse(recvBuf);
    // cout << jsbuf["msg_type"] << endl;
    // cout << jsbuf["from"] << endl;
    // cout << jsbuf["to"] << endl;
    // cout << jsbuf["msg"] << endl;
    
    vector<int> vec = jsbuf["list"];
    cout << vec[1] << endl;
    map<int,string> mp = jsbuf["path"];
    for(auto it : mp){
        cout << it.first << " " << it.second << endl;
    }


    return 0;
}