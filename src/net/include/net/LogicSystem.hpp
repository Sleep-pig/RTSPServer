#pragma once

#include "const.hpp"
#include "net/MsgNode.hpp"
#include "net/SingleTon.hpp"
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

class RtspConnect;
class Recv_Node;

class LogicNode {
public:
    LogicNode(std::shared_ptr<RtspConnect> connect, std::shared_ptr<msgNode> node)
        : connect_(connect),
          node_(node) {}

    std::shared_ptr<RtspConnect> connect_;
    std::shared_ptr<msgNode> node_;
};

using callbackfunc = std::function<void(std::shared_ptr<RtspConnect> , char const *,size_t)>;

class LogicSystem : public SingleTon<LogicSystem> {
    friend class SingleTon<LogicSystem>;

public:
    void Register();

    void PushMsg(std::shared_ptr<LogicNode> node);
    ~LogicSystem();

private:
    LogicSystem();
    void DealMsg();
    void HandleRequest(std::shared_ptr<RtspConnect> connect, char const *msg,size_t size);
    void HandleSendPacket(std::shared_ptr<RtspConnect> connect, char const *msg,size_t size);

    std::mutex mtx_;
    bool b_stop;
    std::condition_variable consume_;
    std::thread work_thread_;

    std::map<MSG_IDS, callbackfunc> callbacks_;
    std::queue<std::shared_ptr<LogicNode>> msg_que_;
};
