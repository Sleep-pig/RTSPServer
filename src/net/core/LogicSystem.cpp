#include "Log/logger.hpp"
#include "net/const.hpp"
#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <net/LogicSystem.hpp>
#include <net/MsgNode.hpp>
#include <net/RtspConnection.hpp>

LogicSystem::LogicSystem() : b_stop(false) {
    work_thread_ = std::thread(&LogicSystem::DealMsg, this);
    Register();
}

LogicSystem::~LogicSystem() {
    b_stop = true;
    consume_.notify_one();
    work_thread_.join();
}

void LogicSystem::Register() {
    callbacks_[MSG_IDS::REQUEST] =
        std::bind(&LogicSystem::HandleRequest, this, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3);
    callbacks_[MSG_IDS::RTP_SEND_PKT] =
        std::bind(&LogicSystem::HandleSendPacket, this, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3);
}

void LogicSystem::PushMsg(std::shared_ptr<LogicNode> node) {
    std::unique_lock<std::mutex> lk(mtx_);
    msg_que_.push(node);

    if (msg_que_.size() == 1) {
        lk.unlock();
        consume_.notify_one();
    }
}

void LogicSystem::DealMsg() {
    for (;;) {
        std::unique_lock<std::mutex> lk(mtx_);
        consume_.wait(lk, [this] { return b_stop || !msg_que_.empty(); });

        if (b_stop) {
            while (!msg_que_.empty()) {
                auto &msg = msg_que_.front();
                auto iter = callbacks_.find(msg->node_->id_);
                if (iter != callbacks_.end()) {
                    iter->second(msg->connect_, msg->node_->Getdata(),
                                 msg->node_->GetLen());
                }
                msg_que_.pop();
            }
            break;
        }

        auto &msg = msg_que_.front();
        auto iter = callbacks_.find(msg->node_->id_);
        if (iter == callbacks_.end()) {
            LOG_DEBUG("callback func is invalid");
            msg_que_.pop();
            continue;
        }
        iter->second(msg->connect_, msg->node_->Getdata(),
                     msg->node_->GetLen());
        msg_que_.pop();
    }
}

void LogicSystem::HandleRequest(std::shared_ptr<RtspConnect> conn,
                                char const *msg, size_t len) {
    bool ret = conn->ParseRequest(msg);
    if (!ret) {
        LOG_DEBUG("Error:cannot parseRequest");
        return;
    }
    ret = conn->HandleRequest();
    if (!ret) {
        LOG_DEBUG("Error:cannot HandleRequest");
        return;
    }
}

void LogicSystem::HandleSendPacket(std::shared_ptr<RtspConnect> conn,
                                   char const *msg, size_t len) {    
   conn->Send(msg, len);
}
